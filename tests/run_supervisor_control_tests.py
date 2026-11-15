#!/usr/bin/env python3
"""真实控制服务/原生客户端/receiver联调；只使用本机合成帧，不代表硬件或吞吐验收。"""
import argparse
import atexit
import concurrent.futures
import hashlib
import http.client
import json
import os
import re
from pathlib import Path
import signal
import socket
import stat
import subprocess
import tempfile
import threading
import time

SCENARIOS = ["normal", "disconnect", "timeout", "ownership", "journal-fault",
             "request-fault", "restart", "restart-unknown", "credential-fault", "partial-start", "preconditions",
             "catalog", "catalog-empty", "catalog-session", "catalog-restart"]
SCENARIOS += ["observe-clean", "observe-error", "observe-signal", "observe-restart"]
MAX_SYNTHETIC_RAW_BYTES = 20 * 1024 ** 3
OWNED_RAW_ROOTS = []


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def cleanup_owned_raw_files(output):
    """Remove only regular .graw files and symlinks below our private root."""
    for path in Path(output).rglob("*.graw"):
        try:
            info = path.lstat()
            if stat.S_ISREG(info.st_mode) or stat.S_ISLNK(info.st_mode):
                path.unlink()
        except FileNotFoundError:
            pass


def register_owned_raw_cleanup(output):
    root = Path(output)
    OWNED_RAW_ROOTS.append(root)
    atexit.register(cleanup_owned_raw_files, root)


def run_with_keyboard_interrupt_cleanup(action):
    try:
        return action()
    except KeyboardInterrupt:
        for root in OWNED_RAW_ROOTS:
            cleanup_owned_raw_files(root)
        raise SystemExit(130)


def wait_for(action, timeout=8):
    end = time.monotonic() + timeout
    while True:
        result = action()
        if result:
            return result
        require(time.monotonic() < end, "Fixture observation deadline")
        time.sleep(.015)


def frame(event, little=False):
    data = bytearray([event & 255] * 4096)
    data[:12] = bytes([6, 0, 0, 64, 7, 0, 1, 4, 0, 2, 0, 4])
    data[12:16] = bytes([0, 0, 3, 224])
    data[22:26] = event.to_bytes(4, "big")
    data[26:28] = bytes(2)
    if little:
        data[:4] = bytes([134, 64, 0, 0])
        for offset in (5, 8, 10):
            data[offset:offset+2] = data[offset:offset+2][::-1]
        data[12:16] = data[12:16][::-1]
        data[22:26] = data[22:26][::-1]
    return bytes(data)


def process_identity(pid):
    path = Path("/proc") / str(pid)
    try:
        return str((path / "exe").resolve(strict=True)), (path / "stat").read_text().rsplit(") ", 1)[1].split()[19]
    except (FileNotFoundError, ProcessLookupError):
        return None


def socket_identities(pid):
    sockets = set()
    for path in (Path("/proc") / str(pid) / "fd").iterdir():
        try:
            value = os.readlink(path)
            if value.startswith("socket:["):
                sockets.add(value)
        except FileNotFoundError:
            pass  # 同时关闭的状态连接不属于仍被继承的描述符。
    return sockets


class Rig:
    def __init__(self, args, root, ports):
        self.args, self.root, self.ports = args, root, ports
        self.process, self.token = None, None
        self.logs, self.sockets, self.children = [], [], {}
        root.mkdir(mode=0o700)
        (root / "outputs").mkdir(mode=0o700)
        self.token_file = root / "credential"
        result = subprocess.run([str(args.service), "--generate-token", str(self.token_file)], capture_output=True, text=True, timeout=10)
        (root / "provision.log").write_text(result.stdout + result.stderr)
        require(result.returncode == 0 and self.token_file.stat().st_mode & 0o777 == 0o600, "Credential provisioning failed")
        self.token = self.token_file.read_text().strip()
        require(self.token not in result.stdout + result.stderr, "Provisioning disclosed token")
        self.plan = {
            "schema": 1, "kind": "getdaq.data-supervisor-plan", "deployment_id": "fixture",
            "audit_root": str(root / "deployment"), "executable": str(args.receiver),
            "executable_sha256": hashlib.sha256(args.receiver.read_bytes()).hexdigest(),
            "configuration_sha256": "a" * 64, "artifact_sha256": "b" * 64,
            "startup_timeout_ms": 2000, "finish_timeout_ms": 1200, "status_timeout_ms": 500,
            "max_total_queue_mib": 8, "max_total_frame_mib": 8, "sources": []}
        for index in range(2):
            self.plan["sources"].append({"id": "C" + str(index), "receiver_address": "127.0.0.1:" + str(ports[1+2*index]),
                "bind": "127.0.0.1", "output_root": str(root / "outputs"), "data_port": ports[1+2*index],
                "status_port": ports[2+2*index], "export_port": 0, "queue_mib": 1, "max_frame_mib": 1,
                "rotate_mib": 1, "export_queue_mib": 1})
        self.plan["sources"][0]["min_free_mib"] = 1
        if getattr(args,"file_naming","native")!="native":
            for source in self.plan["sources"]:source["file_naming"]=args.file_naming
        if root.name.startswith("catalog"):
            (root / "catalog").mkdir(mode=0o700)
            self.plan["catalog_root"] = str(root / "catalog")
        self.config = {"schema": 1, "kind": "getdaq.supervisor-service", "agent_id": "data-node-fixture",
            "controller_id": "ecc-fixture", "bind": "127.0.0.1", "port": ports[0],
            "credential_file": str(self.token_file), "receiver_plan": self.plan}
        self.config_file = root / "service.json"
        self.config_file.write_text(json.dumps(self.config, indent=2) + "\n")
        check = subprocess.run([str(args.service), "--config", str(self.config_file), "--check-config"], capture_output=True, text=True, timeout=10)
        check_output = check.stdout + check.stderr
        (root / "check-config.log").write_text(check_output)
        require(check.returncode == 0 and not Path(self.plan["audit_root"]).exists(), "Offline check failed or created audit root")
        digest_match = re.search(r"plan_sha256=([0-9a-f]{64})", check_output)
        require(digest_match is not None, "Offline check omitted its canonical plan digest")
        planned = subprocess.run([str(args.service), "--config", str(self.config_file), "--plan-config"],
                                 capture_output=True, text=True, timeout=10)
        (root / "plan-config.log").write_text(planned.stdout + planned.stderr)
        require(planned.returncode == 0, "Offline plan generation failed")
        require(hashlib.sha256(planned.stdout.encode()).hexdigest() == digest_match.group(1),
                "Plan output bytes differ from the checked plan digest")
        require(json.loads(planned.stdout) == self.plan, "Offline plan output differs from the supplied receiver plan")
        require(self.token not in planned.stdout + planned.stderr, "Offline plan disclosed the service credential")
        require(not Path(self.plan["audit_root"]).exists() and not any((root / "outputs").iterdir()),
                "Offline configuration command created receiver state or data")
        self.start_service()

    def request(self, method, path, payload=None, auth=True):
        headers = {"Content-Type": "application/json"}
        if auth:
            headers["Authorization"] = "Bearer " + self.token
        data = json.dumps(payload).encode() if payload is not None else None
        connection = http.client.HTTPConnection("127.0.0.1", self.ports[0], timeout=3)
        try:
            connection.request(method, path, data, headers)
            response = connection.getresponse()
            body = response.read()
            require(self.token.encode() not in body, "HTTP response disclosed credential")
            return response.status, json.loads(body)
        finally:
            connection.close()

    def start_service(self):
        log = (self.root / ("service-" + str(len(self.logs)) + ".log")).open("w")
        self.logs.append(log)
        self.process = subprocess.Popen([str(self.args.service), "--config", str(self.config_file)], stdout=log, stderr=subprocess.STDOUT)
        def identity():
            require(self.process.poll() is None, "Control service exited during startup")
            try:
                code, value = self.request("GET", "/v1/identity")
                return value if code == 200 else None
            except (ConnectionError, OSError, http.client.HTTPException):
                return None
        self.identity = wait_for(identity)
        require(self.identity["agent_id"] == "data-node-fixture" and self.identity["controller_id"] == "ecc-fixture", "Service scope differs")
        manifest = Path(self.plan["audit_root"]) / "plan.json"
        require(hashlib.sha256(manifest.read_bytes()).hexdigest() == self.identity["plan_sha256"], "Service plan digest differs from durable bytes")
        require(json.loads(manifest.read_text()) == self.plan == self.identity["plan"], "Service changed the fixed receiver plan")

    def stop_service(self):
        if self.process and self.process.poll() is None:
            self.process.send_signal(signal.SIGTERM)
            self.process.wait(timeout=10)
            require(self.process.returncode == 0, "Graceful service exit failed")

    def status(self):
        code, result = self.request("GET", "/v1/status")
        require(code == 200, "Status unavailable")
        require(result["instance_id"] == self.identity["instance_id"] and result["plan_sha256"] == self.identity["plan_sha256"], "Status scope differs")
        self.remember(result["lifecycle"])
        return result

    def remember(self, lifecycle):
        for receiver in lifecycle["receivers"]:
            if receiver["pid"] > 0 and receiver["pid"] not in self.children:
                identity = process_identity(receiver["pid"])
                if identity:
                    require(identity[0] == str(self.args.receiver), "Fixture was assigned an unrelated process")
                    self.children[receiver["pid"]] = identity

    def make_request(self, command, operation):
        return {"schema": 1, "operation_id": operation, "command": command, "plan_sha256": self.identity["plan_sha256"],
            "instance_id": self.identity["instance_id"], "controller_id": "ecc-fixture", "controller_snapshot_sha256": "c" * 64,
            "controller_epoch": "d" * 64, "configuration_sha256": "a" * 64, "artifact_sha256": "b" * 64,
            "parent_audit_reference": "fixture-parent-operation:" + operation}

    def submit(self, request):
        code, result = self.request("POST", "/v1/operations", request)
        require(code == 202 and result["operation_id"] == request["operation_id"], "Expected accepted operation, code " + str(code))
        return result

    def operation(self, operation):
        code, result = self.request("GET", "/v1/operations/" + operation)
        if code == 404:
            return None
        require(code == 200, "Operation observation failed")
        if isinstance(result["result"], dict) and "receivers" in result["result"]:
            self.remember(result["result"])
        return result

    def completed(self, operation, phase="complete"):
        def poll():
            result = self.operation(operation)
            return result if result and result["phase"] in ("complete", "failed", "unknown") else None
        result = wait_for(poll)
        require(result["phase"] == phase, "Unexpected terminal phase: " + result["phase"] + " " + result["message"])
        (self.root / (operation + "-result.json")).write_text(json.dumps(result, indent=2) + "\n")
        return result

    def start(self, operation="start"):
        request = self.make_request("start", operation)
        self.submit(request)
        result = self.completed(operation)
        require(result["result"]["state"] == "ready", "Receivers not ready")
        for source in self.plan["sources"]:
            connection = http.client.HTTPConnection("127.0.0.1", source["status_port"], timeout=2)
            try:
                connection.request("GET", "/status")
                status = json.loads(connection.getresponse().read())
                require(status["connections"] == 0, "Readiness used a raw data connection")
                expected_reserve = source.get("min_free_mib", 0) * 1024 * 1024
                require(status.get("minimum_free_bytes") == expected_reserve, "Fixed disk reserve was not passed to the receiver")
            finally:
                connection.close()
        return request, result

    def connect_senders(self):
        self.sockets = [socket.create_connection(("127.0.0.1", source["data_port"]), timeout=3) for source in self.plan["sources"]]

    def eof(self, expected):
        for index, connection in enumerate(self.sockets):
            for offset in range(0, len(expected[index]), 317):
                connection.sendall(expected[index][offset:offset+317])
            connection.shutdown(socket.SHUT_WR)
            connection.close()
        self.sockets = []

    def finish(self, expected, operation="finish"):
        self.eof(expected)
        self.submit(self.make_request("finish", operation))
        result = self.completed(operation)
        require(result["result"]["state"] == "stopped" and not result["result"]["needs_reconciliation"], "EOF did not commit stopped")
        for index, receiver in enumerate(result["result"]["receivers"]):
            output = Path(receiver["output_directory"])
            seal = json.loads((output / ".getdaq-receiver-seal.json").read_text())
            actual = b"".join((output / item["name"]).read_bytes() for item in seal["files"])
            require(actual == expected[index], "Independent raw frame bytes differ")
            require(receiver["reaped"] and receiver["exit_code"] == 0 and receiver["seal_verified"], "Exit/seal not verified")
            require(isinstance(receiver["received_bytes"], str) and int(receiver["received_bytes"]) == len(actual), "Received count encoding differs")
            require(seal["sender_eof_confirmed"] and seal["session_id"] == result["result"]["session_id"] and seal["sender_id"] == "C" + str(index), "Seal identity/EOF differs")
        return result

    def journal(self):
        return [json.loads(row) for row in (Path(self.plan["audit_root"]) / "control/operations.jsonl").read_text().splitlines()]

    def native(self, method, argument=None, timeout=1000, okay=True):
        command = [str(self.args.fixture), "http", "127.0.0.1", str(self.ports[0]), str(self.token_file), method]
        if argument is not None:
            command.append(str(argument))
        command.append(str(timeout))
        result = subprocess.run(command, capture_output=True, text=True, timeout=5)
        require(self.token not in result.stdout + result.stderr, "Native client disclosed credential")
        require((result.returncode == 0) == okay, "Native HTTP outcome differs: " + result.stderr)
        return json.loads(result.stdout) if okay else result.stderr

    def query(self, method, option=None, argument=None, override=None, okay=True):
        fields = {"--host": "127.0.0.1", "--port": str(self.ports[0]), "--credential-file": str(self.token_file),
                  "--expected-agent": "data-node-fixture", "--controller": "ecc-fixture", "--expected-plan": self.identity["plan_sha256"]}
        if override:
            fields.update(override)
        if option:
            fields[option] = str(argument)
        command = [str(self.args.query), method]
        for key, value in fields.items():
            command.extend([key, value])
        result = subprocess.run(command, capture_output=True, text=True, timeout=5)
        require(self.token not in result.stdout + result.stderr, "Query CLI disclosed credential")
        require((result.returncode == 0) == okay, "Query CLI outcome differs: " + result.stderr)
        return json.loads(result.stdout) if okay else result.stderr

    def close(self):
        for connection in self.sockets:
            connection.close()
        self.sockets = []
        # 只针对本夹具从实际receiver记录的PID+可执行文件+启动时刻清理故障测试；生产服务不这样kill。
        for pid, identity in self.children.items():
            if process_identity(pid) == identity:
                os.kill(pid, signal.SIGTERM)
        self.stop_service()
        for log in self.logs:
            log.close()
            if self.token:
                require(self.token not in Path(log.name).read_text(), "Service log disclosed credential")


def scenario(args, output, name, ports):
    started = time.monotonic()
    rig = None
    result = {"scenario": name, "exit_code": 0}
    try:
        rig = Rig.__new__(Rig)
        rig.__init__(args, output / name, ports)
        if name.startswith("observe-"):
            require(rig.request("GET", "/v1/receivers", auth=False)[0] == 401, "Unauthenticated observation exposed")
            fresh = rig.query("receivers")["observation"]
            require(not fresh["busy"] and not fresh["receivers"] and fresh["lifecycle"]["state"] == "fresh", "Fresh observation invented children")
            rig.start()
            running = rig.query("receivers")["observation"]
            require(all(r["state"] == "running" and r["exit_code"] == -1 for r in running["receivers"]), "Live receivers falsely exited")
            require(rig.request("GET", "/v1/receiver-events/0", auth=False)[0] == 401, "Unauthenticated receiver event count exposed")
            code, initial_events = rig.request("GET", "/v1/receiver-events/0")
            require(code == 200 and initial_events["service"] == "getdaq-supervisor" and
                    initial_events["agent_id"] == "data-node-fixture" and initial_events["instance_id"] == rig.identity["instance_id"] and
                    initial_events["plan_sha256"] == rig.identity["plan_sha256"] and
                    initial_events["events"] == {"source_id": "C0", "session_id": running["lifecycle"]["session_id"],
                        "pid": running["receivers"][0]["pid"], "available": True, "acquisition_events": 0,
                        "observed_at_unix_ms": initial_events["events"]["observed_at_unix_ms"]},
                    "Targeted receiver event response lost its fixed identity or initial count")
            require(rig.request("GET", "/v1/receiver-events/00")[0] == 400 and
                    rig.request("GET", "/v1/receiver-events/99")[0] == 404,
                    "Invalid receiver event source index was accepted")
            for key, value in (("--expected-agent", "other"), ("--controller", "other"), ("--expected-plan", "0" * 64)):
                rig.query("receivers", override={key: value}, okay=False)
            children = rig.status()["lifecycle"]["receivers"]
            expected = [frame(1), frame(2, True)]
            if name == "observe-signal":
                os.kill(children[0]["pid"], signal.SIGKILL)
                connection = socket.create_connection(("127.0.0.1", rig.plan["sources"][1]["data_port"]), timeout=3)
                try: connection.sendall(expected[1]); connection.shutdown(socket.SHUT_WR)
                finally: connection.close()
            else:
                rig.connect_senders()
                rig.sockets[0].sendall(expected[0])
                def event_seen():
                    code, value = rig.request("GET", "/v1/receiver-events/0")
                    return value if code == 200 and value["events"]["acquisition_events"] == 1 else None
                observed_events = wait_for(event_seen)
                require(observed_events["events"]["source_id"] == "C0" and observed_events["events"]["session_id"] == running["lifecycle"]["session_id"],
                        "Targeted live event count was not bound to the running source/session")
                rig.eof([(frame(3)[:100] if name == "observe-error" else b""), expected[1]])
            def exited():
                value = rig.query("receivers")["observation"]
                return value if not value["busy"] and all(r["state"] == "exited" for r in value["receivers"]) else None
            observed = wait_for(exited)
            wanted = 1 if name == "observe-error" else 128 + signal.SIGKILL if name == "observe-signal" else 0
            require(observed["receivers"][0]["exit_code"] == wanted and observed["receivers"][1]["exit_code"] == 0, "Actual HTTP exit status differs")
            require(observed["receivers"][0]["signal"] == (signal.SIGKILL if name == "observe-signal" else 0), "Actual HTTP signal differs")
            before = rig.journal()
            for repeat in range(3):
                current = rig.query("receivers")["observation"]
                require(current["receivers"] == observed["receivers"] and current["lifecycle"]["state"] == "ready", "Observation reaped child or changed lifecycle")
            require(rig.journal() == before, "Observation wrote control intent")
            (rig.root / "actual-observation.json").write_text(json.dumps(observed, indent=2) + "\n")
            if name == "observe-restart":
                rig.stop_service(); rig.start_service()
                reopened = rig.query("receivers")["observation"]
                require(not reopened["busy"] and not reopened["receivers"] and reopened["lifecycle"]["needs_reconciliation"], "Restart adopted historical PIDs or cleared gate")
            else:
                rig.submit(rig.make_request("finish", "finish"))
                finished = rig.completed("finish", "failed" if wanted else "complete")
                final = rig.query("receivers")["observation"]
                require(all(r["state"] == "reaped" for r in final["receivers"]), "Finish could not reap observed children")
                require(finished["result"]["state"] == ("failed" if wanted else "stopped"), "Observed EOF bypassed seal gate")
                if not wanted:
                    for i, child in enumerate(finished["result"]["receivers"]):
                        folder = Path(child["output_directory"]); seal = json.loads((folder / ".getdaq-receiver-seal.json").read_text())
                        require(b"".join((folder / f["name"]).read_bytes() for f in seal["files"]) == expected[i], "Observed normal cycle raw bytes differ")
        elif name == "preconditions":
            require(rig.request("GET", "/v1/identity", auth=False)[0] == 401, "Unauthenticated identity exposed")
            base = rig.make_request("start", "invalid")
            for field in ("instance_id", "plan_sha256", "controller_id", "configuration_sha256", "artifact_sha256"):
                bad = dict(base)
                bad[field] = "other-controller" if field == "controller_id" else "0" * 64
                require(rig.request("POST", "/v1/operations", bad)[0] == 412, "Incorrect precondition accepted")
            for extra in ("executable", "output_root", "raw_frame"):
                bad = dict(base, **{extra: "forbidden"})
                require(rig.request("POST", "/v1/operations", bad)[0] == 400, "Unsupported remote field accepted")
            require(not rig.status()["lifecycle"]["receivers"] and not any(row["event"] == "accepted" for row in rig.journal()), "Invalid request created intent/process")
            rig.submit(rig.make_request("validate", "validate"))
            rig.completed("validate")
        elif name == "credential-fault":
            os.chmod(rig.token_file, 0o644)
            require(rig.request("GET", "/v1/status")[0] == 503, "Credential permission mutation ignored")
            os.chmod(rig.token_file, 0o600)
            require(rig.request("GET", "/v1/status")[0] == 503, "Credential restoration cleared fault")
            require(rig.request("POST", "/v1/operations", rig.make_request("start", "denied"))[0] == 503, "Credential fault started receiver")
        elif name == "journal-fault":
            path = Path(rig.plan["audit_root"]) / "control/operations.jsonl"
            original = path.read_bytes()
            with path.open("r+b") as handle:
                handle.write(b"!")
                handle.flush()
                os.fsync(handle.fileno())
            require(rig.request("POST", "/v1/operations", rig.make_request("start", "faulted"))[0] == 409, "Control journal mutation ignored")
            with path.open("r+b") as handle:
                handle.write(original)
                handle.flush()
                os.fsync(handle.fileno())
            require(rig.request("POST", "/v1/operations", rig.make_request("start", "restored"))[0] == 409, "Journal restoration cleared fault")
            require(rig.status()["control_needs_reconciliation"] and not rig.status()["lifecycle"]["receivers"], "Journal fault cleared state/spawned")
        elif name == "partial-start":
            with socket.socket() as occupied:
                occupied.bind(("127.0.0.1", rig.plan["sources"][1]["data_port"]))
                occupied.listen()
                request = rig.make_request("start", "partial")
                rig.submit(request)
                result_start = rig.completed("partial", "failed")
                require(result_start["result"]["state"] == "failed", "Partial start did not preserve failure")
                require(rig.submit(request)["phase"] == "failed", "Duplicate failed start replayed")
                rig.submit(rig.make_request("abort-startup", "abort"))
                aborted = rig.completed("abort")
                require(aborted["result"]["state"] == "aborted" and aborted["result"]["needs_reconciliation"], "Explicit startup abort cleared gate")
        elif name.startswith("catalog"):
            rig.start()
            live = dict(rig.make_request("catalog", "live-catalog"), catalog_experiment="Exp", catalog_run_number=1,
                        catalog_session_id=rig.status()["lifecycle"]["session_id"])
            require(rig.request("POST", "/v1/operations", live)[0] == 412 and rig.operation("live-catalog") is None, "Live catalog accepted/created intent")
            rig.connect_senders()
            expected = [b"", b""] if name == "catalog-empty" else [b"".join(frame(e) for e in range(1, 7)), b"".join(frame(e, True) for e in range(1, 7))]
            stopped = rig.finish(expected)["result"]
            request = dict(rig.make_request("catalog", "catalog-1"), catalog_experiment="Exp", catalog_run_number=1, catalog_session_id=stopped["session_id"])
            if name == "catalog-session":
                request["catalog_session_id"] = "0" * 64
                require(rig.request("POST", "/v1/operations", request)[0] == 412 and rig.operation("catalog-1") is None, "Wrong catalog session created intent")
                require(not (rig.root / "catalog/Exp").exists(), "Wrong session created target")
            else:
                path = rig.root / "catalog-request.json"; path.write_text(json.dumps(request))
                require(rig.native("submit", path)["code"] == 202, "Native typed catalog request rejected")
                rig.submit(request)
                completed = rig.completed("catalog-1")
                receipt = completed["catalog"]
                require(receipt["complete"] and receipt["raw_payload_bytes_read"] == "0" and receipt["payload_method"] == "hardlink", "Catalog result missing/native payload copied")
                require(completed["result"]["state"] == "stopped" and completed["result"]["session_id"] == stopped["session_id"], "Catalog changed receiver identity/state")
                directory = Path(receipt["directory"])
                require(json.loads((directory / "catalog-complete.json").read_text()) == receipt, "HTTP catalog receipt differs from actual record")
                manifest = json.loads((directory / "manifest.json").read_text())
                require(hashlib.sha256((directory / "manifest.json").read_bytes()).hexdigest() == receipt["manifest_sha256"], "Catalog manifest binding differs")
                for index, source in enumerate(manifest["sources"]):
                    require(source["id"] == stopped["receivers"][index]["id"], "Catalog source mapping differs")
                    original = Path(source["original_directory"])
                    for file in source["files"]:
                        before = (original / file["name"]).stat(); after = (directory / source["stored_directory"] / file["name"]).stat()
                        require((before.st_dev, before.st_ino, before.st_size) == (after.st_dev, after.st_ino, int(file["bytes"])) and before.st_nlink == 2, "HTTP catalog moved/copied original data")
                inspect = subprocess.run([str(args.service.parent / "getdaq-catalog"), "--inspect-catalog", str(directory)], capture_output=True, text=True, timeout=5)
                require(inspect.returncode == 0 and json.loads(inspect.stdout) == receipt, "Native catalog inspection failed: " + inspect.stderr)
                require(sum(row["event"] == "accepted" and row.get("operation_id") == "catalog-1" for row in rig.journal()) == 1, "Catalog executed/accepted twice")
                changed = dict(request, catalog_run_number=2)
                require(rig.request("POST", "/v1/operations", changed)[0] == 412, "Catalog ID reused with another Run")
                if name == "catalog-restart":
                    rig.start("replacement-start")
                    rig.connect_senders()
                    replacement_stopped = rig.finish([b"".join(frame(e) for e in range(1, 7)), b"".join(frame(e, True) for e in range(1, 7))], "replacement-finish")["result"]
                    replacement_id = "catalog-replacement-stable-1"
                    prepare = dict(rig.make_request("catalog", "replacement-prepare"), catalog_experiment="Exp",
                        catalog_run_number=1, catalog_session_id=replacement_stopped["session_id"], catalog_replace_existing=True,
                        catalog_phase="prepare", catalog_replacement_id=replacement_id)
                    rig.submit(prepare)
                    require(rig.completed("replacement-prepare")["phase"] == "complete", "Replacement prepare failed")
                    publish = dict(prepare, operation_id="replacement-publish", catalog_phase="publish",
                        parent_audit_reference="fixture-parent-operation:replacement-publish")
                    rig.submit(publish)
                    replacement_published = rig.completed("replacement-publish")
                    require(replacement_published["phase"] == "complete", "Replacement publish failed")
                    require(rig.request("GET", "/v1/catalog-replacement-observations/replacement-prepare")[0] == 412,
                        "Catalog observation accepted a non-Publish operation")
                    rig.query("catalog-replacement-observation", "--operation-id", "replacement-prepare", okay=False)
                    require(rig.request("GET", "/v1/catalog-replacement-observations/catalog-1")[0] == 412,
                        "Catalog observation accepted a non-replacement single-stage catalog operation")
                    rig.query("catalog-replacement-observation", "--operation-id", "catalog-1", okay=False)
                    observation_path = "/v1/catalog-replacement-observations/replacement-publish"
                    code, observed = rig.request("GET", observation_path)
                    require(code == 200 and observed["kind"] == "getdaq.supervisor-catalog-replacement-observation" and
                        observed["agent_id"] == "data-node-fixture" and observed["publish_operation_id"] == "replacement-publish" and
                        observed["request_key"] == replacement_published["request_key"] and
                        observed["catalog_replacement_id"] == replacement_id and observed["operation_phase"] == "complete" and
                        observed["observation"]["phase"] == "published" and observed["observation"]["raw_payload_bytes_read"] == "0" and
                        observed["observation"]["recovery_authorized"] is False,
                        "Read-only replacement observation did not bind the current published node state: HTTP " + str(code) + " " + json.dumps(observed, sort_keys=True))
                    cli_observed = rig.query("catalog-replacement-observation", "--operation-id", "replacement-publish")
                    require(cli_observed["observation"] == observed["observation"] and
                        cli_observed["current_instance_id"] == rig.identity["instance_id"] and
                        cli_observed["original_instance_id"] == publish["instance_id"],
                        "Native query CLI did not verify and preserve the replacement observation bindings")
                    require(rig.request("GET", observation_path, auth=False)[0] == 401,
                        "Unauthenticated replacement observation exposed catalog state")
                    rig.stop_service(); rig.start_service()
                    require(rig.operation("catalog-1")["catalog"] == receipt and rig.status()["control_needs_reconciliation"], "Catalog restart replayed/lost result or cleared gate")
                    restart_code, restart_observation = rig.request("GET", observation_path)
                    require(restart_code == 200 and restart_observation["current_instance_id"] == rig.identity["instance_id"] and
                        restart_observation["original_instance_id"] == publish["instance_id"] and
                        restart_observation["observation"]["phase"] == "published" and
                        restart_observation["observation"]["recovery_authorized"] is False,
                        "Restarted data service could not re-observe the bound replacement journal read-only")
                    cli_after_restart = rig.query("catalog-replacement-observation", "--operation-id", "replacement-publish")
                    require(cli_after_restart["current_instance_id"] == rig.identity["instance_id"] and
                        cli_after_restart["original_instance_id"] == publish["instance_id"] and
                        cli_after_restart["observation"]["phase"] == "published" and
                        cli_after_restart["observation"]["recovery_authorized"] is False,
                        "Native query CLI did not observe the published transaction after service restart")
                else:
                    rig.start("next-cycle"); rig.connect_senders(); rig.finish([b"", b""], "next-finish")
                    require(json.loads((directory / "catalog-complete.json").read_text()) == receipt, "Next cycle changed archived Run")
        elif name == "disconnect":
            request = rig.make_request("start", "lost-response")
            body = json.dumps(request).encode()
            with socket.create_connection(("127.0.0.1", rig.ports[0]), timeout=2) as connection:
                header = ("POST /v1/operations HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer " + rig.token +
                          "\r\nContent-Type: application/json\r\nContent-Length: " + str(len(body)) + "\r\n\r\n").encode()
                connection.sendall(header + body)
                connection.shutdown(socket.SHUT_WR)
            complete = rig.completed("lost-response")
            require(complete["result"]["state"] == "ready", "Disconnected caller cancelled accepted start")
            require(sum(row["event"] == "accepted" and row.get("operation_id") == "lost-response" for row in rig.journal()) == 1, "Lost response caused repeated intent")
            rig.connect_senders()
            rig.finish([frame(1), frame(2, True)])
        else:
            inherited = set()
            pending = None
            if name == "normal":
                pending = socket.create_connection(("127.0.0.1", rig.ports[0]), timeout=2)
                pending.sendall(b"GET /v1/status HTTP/1.1\r\n")
                def accepted_sockets():
                    sockets = socket_identities(rig.process.pid)
                    return sockets if len(sockets) >= 2 else None
                inherited = wait_for(accepted_sockets)
            request, start = rig.start()
            if name == "normal":
                pending.close()
                for pid in rig.children:
                    sockets = socket_identities(pid)
                    require(not sockets.intersection(inherited), "Receiver inherited a control listener/accepted socket")
                require(json.loads(rig.native("identity")["body"]) == rig.identity, "Native client identity differs")
                require(rig.query("identity") == rig.identity and rig.query("status")["lifecycle"]["state"] == "ready", "Production query CLI scope differs")
                for key, value in (("--expected-agent", "other-node"), ("--controller", "other-ecc"), ("--expected-plan", "0"*64)):
                    rig.query("identity", override={key: value}, okay=False)
                require(rig.submit(request)["result"]["session_id"] == start["result"]["session_id"], "Duplicate start spawned again")
                changed = dict(request, parent_audit_reference="different")
                require(rig.request("POST", "/v1/operations", changed)[0] == 412, "Operation ID reused with different intent")
                check = rig.make_request("verify-ready", "native-ready")
                path = rig.root / "native-request.json"
                path.write_text(json.dumps(check))
                require(rig.native("submit", path)["code"] == 202, "Native client did not submit")
                rig.completed("native-ready")
                require(rig.native("operation", "native-ready")["code"] == 200, "Native operation query failed")
                require(rig.query("operation", "--operation-id", "native-ready")["phase"] == "complete", "Production CLI operation query failed")
                cli_request = rig.make_request("verify-ready", "cli-ready")
                path.write_text(json.dumps(cli_request))
                require(rig.query("submit", "--request-file", path)["operation_id"] == "cli-ready", "Production CLI did not submit once")
                rig.completed("cli-ready")
                stale = rig.make_request("start", "cli-stale")
                stale["instance_id"] = "0"*64
                path.write_text(json.dumps(stale))
                rig.query("submit", "--request-file", path, okay=False)
                require(rig.operation("cli-stale") is None, "CLI posted known stale request")
                rig.connect_senders()
                final = rig.finish([b"".join(frame(e) for e in range(1, 7)), b"".join(frame(e, True) for e in range(1, 7))])
                _, second = rig.start("start-2")
                require(second["result"]["session_id"] != final["result"]["session_id"], "Second cycle reused session")
                rig.connect_senders()
                rig.finish([b"", b""], "finish-2")
            elif name == "ownership":
                for field in ("controller_epoch", "controller_snapshot_sha256"):
                    bad = rig.make_request("verify-ready", "other-" + field)
                    bad[field] = "e" * 64
                    require(rig.request("POST", "/v1/operations", bad)[0] == 409, "Ownership changed without recovery")
                rig.submit(rig.make_request("verify-ready", "same-owner"))
                rig.completed("same-owner")
                rig.connect_senders()
                rig.finish([b"", b""])
                new_owner = rig.make_request("start", "takeover-after-stop")
                new_owner["controller_epoch"] = "f" * 64
                require(rig.request("POST", "/v1/operations", new_owner)[0] == 409, "Stopped cycle silently released control ownership")
            elif name == "restart":
                old_instance = rig.identity["instance_id"]
                rig.stop_service()
                require(all(process_identity(pid) == identity for pid, identity in rig.children.items()), "Service exit killed ready receiver")
                rig.native("identity", okay=False)
                rig.start_service()
                require(rig.identity["instance_id"] != old_instance, "Service restart reused instance")
                require(rig.operation("start")["phase"] == "complete", "Restart lost historical operation")
                require(rig.status()["control_needs_reconciliation"], "Restart silently cleared control gate")
                require(rig.request("POST", "/v1/operations", request)[0] == 412, "Stale instance accepted after restart")
                require(rig.request("POST", "/v1/operations", rig.make_request("start", "new-start"))[0] == 409, "Restart launched successor receiver")
            elif name == "restart-unknown":
                rig.connect_senders()
                rig.submit(rig.make_request("finish", "crashed-finish"))
                wait_for(lambda: rig.operation("crashed-finish")["phase"] == "running")
                rig.process.kill()  # 仅本测试明确注入服务崩溃；不向任何receiver发信号。
                rig.process.wait(timeout=5)
                require(rig.process.returncode == -signal.SIGKILL, "Service crash injection failed")
                require(all(process_identity(pid) == identity for pid, identity in rig.children.items()), "Service crash killed receiver")
                rig.start_service()
                operation = rig.operation("crashed-finish")
                require(operation["phase"] == "unknown" and operation["result"] is None, "Restart inferred/replayed unfinished result")
                require(rig.query("operation", "--operation-id", "crashed-finish")["phase"] == "unknown", "Historical unknown query fabricated completion")
                require(rig.status()["control_needs_reconciliation"] and not rig.status()["operation_active"], "Crash restart resumed old worker")
                require(sum(row["event"] == "running" and row.get("operation_id") == "crashed-finish" for row in rig.journal()) == 1, "Crash restart repeated unfinished action")
            else:
                rig.connect_senders()
                finish = rig.make_request("finish", "pending-finish")
                rig.submit(finish)
                wait_for(lambda: rig.operation("pending-finish")["phase"] == "running")
                require(rig.submit(finish)["operation_id"] == "pending-finish", "Duplicate running operation not recognised")
                before = time.monotonic()
                status = rig.status()
                latency = time.monotonic() - before
                require(latency < .5 and status["operation_active"], "Slow operation blocked status query")
                if name == "timeout":
                    wait_for(lambda: rig.status()["lifecycle"]["state"] == "finishing")
                    before = time.monotonic(); observed = rig.query("receivers")["observation"]
                    require(observed["busy"] and not observed["receivers"] and time.monotonic()-before < .5, "HTTP observation blocked on Finish or guessed process state")
                (rig.root / "status-latency.json").write_text(json.dumps({"seconds": latency}) + "\n")
                require(rig.request("POST", "/v1/operations", rig.make_request("start", "busy"))[0] == 409, "Busy operation was queued")
                if name == "request-fault":
                    path = Path(rig.plan["audit_root"]) / "control/operations/pending-finish/request.json"
                    with path.open("r+b") as handle:
                        handle.write(b"!")
                        handle.flush()
                        os.fsync(handle.fileno())
                    rig.eof([frame(7), frame(8, True)])
                    final = rig.completed("pending-finish", "failed")
                    require(final["result"]["state"] == "stopped" and rig.status()["control_needs_reconciliation"], "Late request fault falsely committed control success")
                    require(not any(row["event"] == "complete" and row.get("operation_id") == "pending-finish" for row in rig.journal()), "Late corruption retained positive result")
                elif name == "timeout":
                    final = rig.completed("pending-finish", "failed")
                    require(final["result"]["state"] == "failed" and rig.status()["control_needs_reconciliation"], "Finish timeout cleared gate")
                    require(all(process_identity(pid) == identity for pid, identity in rig.children.items()), "Finish timeout killed receiver")
                    require(rig.submit(finish)["phase"] == "failed", "Repeated timed-out finish executed again")
                    require(rig.request("POST", "/v1/operations", rig.make_request("start", "after-timeout"))[0] == 409, "Finish timeout restarted receiver")
                    rig.eof([b"", b""])
                else:
                    raise AssertionError("Unknown scenario")
    except Exception as error:
        result["exit_code"], result["error"] = 1, str(error)
    finally:
        if rig:
            try:
                rig.close()
            except Exception as error:
                result["exit_code"], result["cleanup_error"] = 1, str(error)
    result["seconds"] = round(time.monotonic() - started, 3)
    (output / (name + "-summary.json")).write_text(json.dumps(result, indent=2) + "\n")
    return result


def transport_cases(args, output, port):
    root = output / "native-transport"
    root.mkdir(mode=0o700)
    token_file = root / "credential"
    token_file.write_text(os.urandom(32).hex() + "\n")
    os.chmod(token_file, 0o600)
    token = token_file.read_text().strip()
    request = {"schema": 1, "operation_id": "single-post", "command": "start", "plan_sha256": "a"*64, "instance_id": "b"*64,
               "controller_id": "fixture", "controller_snapshot_sha256": "c"*64, "controller_epoch": "d"*64,
               "configuration_sha256": "e"*64, "artifact_sha256": "f"*64, "parent_audit_reference": "fixture"}
    request_file = root / "request.json"
    request_file.write_text(json.dumps(request))
    cases = {"fragmented": (b"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}", True),
             "duplicate-length": (b"HTTP/1.1 200 OK\r\nContent-Length: 2\r\ncontent-length: 2\r\n\r\n{}", False),
             "chunked": (b"HTTP/1.1 200 OK\r\nContent-Length: 2\r\nTransfer-Encoding: chunked\r\n\r\n{}", False),
             "truncated": (b"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\n{}", False),
             "oversized": (b"HTTP/1.1 200 OK\r\nContent-Length: 1048577\r\n\r\n", False),
             "redirect": (b"HTTP/1.1 302 Found\r\nContent-Length: 2\r\nLocation: http://127.0.0.1:1\r\n\r\n{}", True),
             "lost-post-response": (b"", False), "deadline": (None, False)}
    results = []
    for name, (response, okay) in cases.items():
        accepted, failures = [], []
        with socket.socket() as listener:
            listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listener.bind(("127.0.0.1", port))
            listener.listen()
            listener.settimeout(2)
            def serve():
                try:
                    connection, _ = listener.accept()
                    accepted.append(1)
                    with connection:
                        connection.settimeout(2)
                        received = b""
                        while b"\r\n\r\n" not in received:
                            received += connection.recv(4096)
                        header, body = received.split(b"\r\n\r\n", 1)
                        content = next(row for row in header.split(b"\r\n") if row.lower().startswith(b"content-length:"))
                        length = int(content.split(b":", 1)[1])
                        while len(body) < length:
                            body += connection.recv(4096)
                        if response is None:
                            time.sleep(.4)
                        elif name == "fragmented":
                            for byte in response:
                                connection.sendall(bytes([byte]))
                        else:
                            connection.sendall(response)
                    listener.settimeout(.2)
                    try:
                        retry, _ = listener.accept()
                        accepted.append(1)
                        retry.close()
                    except socket.timeout:
                        pass
                except Exception as error:
                    failures.append(str(error))
            worker = threading.Thread(target=serve)
            worker.start()
            method = "submit" if name == "lost-post-response" else "identity"
            command = [str(args.fixture), "http", "127.0.0.1", str(port), str(token_file), method]
            if method == "submit":
                command.append(str(request_file))
            command.append("150" if name == "deadline" else "1000")
            started = time.monotonic()
            result = subprocess.run(command, capture_output=True, text=True, timeout=4)
            worker.join(timeout=4)
            require(not worker.is_alive() and not failures, "Native transport fixture failed: " + str(failures))
            require((result.returncode == 0) == okay and len(accepted) == 1, "Native transport accepted/retry policy differs: " + name)
            require(token not in result.stdout + result.stderr, "Native transport disclosed credential")
            if name == "redirect":
                require(json.loads(result.stdout)["code"] == 302, "Native client followed redirect")
            if name == "deadline":
                require(time.monotonic() - started < 1, "Native HTTP deadline unbounded")
            results.append({"case": name, "exit_code": result.returncode, "connections": len(accepted), "expected_success": okay})
            (root / (name + ".log")).write_text(result.stdout + result.stderr)
    (root / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    return {"scenario": "native-transport", "exit_code": 0, "cases": len(results)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("service", type=Path)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("receiver", type=Path)
    parser.add_argument("--query", type=Path, help="Native production CLI; defaults beside service executable")
    parser.add_argument("--output-parent", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=3)
    parser.add_argument("--scenarios", nargs="+", choices=SCENARIOS, default=SCENARIOS)
    parser.add_argument("--file-naming",choices=("native","get"),default="native")
    parser.add_argument("--cleanup-raw", action="store_true",
                        help="Remove only synthetic .graw files from this runner's private evidence directory after recording results and on normal process exit")
    args = parser.parse_args()
    require(1 <= args.jobs <= 8, "Use 1..8 fixture workers")
    for name in ("service", "fixture", "receiver"):
        setattr(args, name, getattr(args, name).resolve(strict=True))
    args.query = (args.query or args.service.parent / "getdaq-supervisor-query").resolve(strict=True)
    parent = args.output_parent.resolve()
    parent.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix="remote-receiver-", dir=str(parent)))
    if args.cleanup_raw:
        # Register before any scenario starts so exceptions and KeyboardInterrupt
        # also remove payloads from this runner-owned evidence root.
        register_owned_raw_cleanup(output)
    ports = []
    # 预先选取与默认Linux客户端临时端口不同的测试端口；本fixture各场景固定独立集合。
    for value in range(10000 + (os.getpid() % 160) * 128, 32000):
        if len(ports) >= len(args.scenarios) * 5 + 1:
            break
        with socket.socket() as candidate:
            try:
                candidate.bind(("127.0.0.1", value))
                ports.append(value)
            except OSError:
                continue
    require(len(ports) == len(args.scenarios) * 5 + 1, "Insufficient isolated fixture ports")
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        futures = [executor.submit(scenario, args, output, name, ports[index*5:index*5+5]) for index, name in enumerate(args.scenarios)]
        results = [future.result() for future in futures]
    try:
        results.append(transport_cases(args, output, ports[-1]))
    except Exception as error:
        results.append({"scenario": "native-transport", "exit_code": 1, "error": str(error)})
    for name in ("codec", "credential"):
        command = [str(args.fixture), name]
        if name == "credential":
            command.append(str(output / "credential-checks"))
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        (output / (name + ".log")).write_text(result.stdout + result.stderr)
        results.append({"scenario": name, "exit_code": result.returncode})
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    raw_paths = sorted(output.rglob("*.graw"))
    raw_bytes = 0
    raw_inventory_error = None
    for path in raw_paths:
        try:
            info = path.lstat()
            if stat.S_ISREG(info.st_mode):
                raw_bytes += info.st_size
            elif not stat.S_ISLNK(info.st_mode):
                raw_inventory_error = "Synthetic raw path is not a regular file: " + str(path)
        except OSError as error:
            raw_inventory_error = "Cannot inspect synthetic raw path: " + str(error)
    budget_ok = raw_bytes <= MAX_SYNTHETIC_RAW_BYTES
    removed = 0
    if args.cleanup_raw:
        for path in raw_paths:
            try:
                path.unlink()
                removed += 1
            except FileNotFoundError:
                pass
            except OSError as error:
                raw_inventory_error = "Cannot remove runner-owned synthetic raw file: " + str(error)
        remaining = list(output.rglob("*.graw"))
    else:
        remaining = raw_paths
    cleanup = {"scenario": "synthetic-raw-budget", "exit_code": 0 if budget_ok and raw_inventory_error is None and (not args.cleanup_raw or not remaining) else 1,
               "maximum_bytes": MAX_SYNTHETIC_RAW_BYTES, "observed_path_bytes": raw_bytes,
               "observed_files": len(raw_paths), "cleanup_requested": args.cleanup_raw,
               "removed_paths": removed, "remaining_paths": len(remaining), "error": raw_inventory_error}
    results.append(cleanup)
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    for result in results:
        print(result)
    print("Evidence:", output)
    if any(result["exit_code"] != 0 for result in results):
        raise SystemExit(1)
    print("PASS", len(args.scenarios), "actual service/receiver scenarios, 8 native HTTP fault cases, codec and credentials; synthetic_raw_bytes=%d; raw_remaining=%d; no hardware/throughput claim" % (raw_bytes, len(remaining)))


if __name__ == "__main__":
    run_with_keyboard_interrupt_cleanup(main)
