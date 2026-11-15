#!/usr/bin/env python3
"""ECC网络入口的控制边界检查；不发送有效硬件命令，不替代完整运行联调。"""
import argparse
import http.client
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
import time


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("service", type=Path)
    parser.add_argument("config_template", type=Path, help="Valid fixed configuration produced by fixed-ecc-configuration-tests")
    parser.add_argument("--output-parent", type=Path, required=True)
    args = parser.parse_args()
    parent = args.output_parent.resolve()
    parent.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="ecc-http-boundary-", dir=str(parent)))
    config = json.loads(args.config_template.read_text())
    with socket.socket() as candidate:
        candidate.bind(("127.0.0.1", 0))
        port = candidate.getsockname()[1]
    credential = root / "credential"
    credential.write_text(os.urandom(32).hex() + "\n")
    os.chmod(credential, 0o600)
    token = credential.read_text().strip()
    config.update(port=port, credential_file=str(credential), audit_root=str(root / "controller"))
    file = root / "service.json"
    file.write_text(json.dumps(config, indent=2) + "\n")
    executable = str(args.service.resolve())
    check = subprocess.run([executable, "--config", str(file), "--check-config"], capture_output=True, text=True, timeout=10)
    (root / "check-config.log").write_text(check.stdout + check.stderr)
    require(check.returncode == 0 and not Path(config["audit_root"]).exists(), "Offline service check created state or failed")
    logs, process = [], None
    def request(method, path, body=None, auth=True):
        headers = {"Content-Type": "application/json"}
        if auth:
            headers["Authorization"] = "Bearer " + token
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
        try:
            connection.request(method, path, json.dumps(body) if body is not None else None, headers)
            response = connection.getresponse()
            content = response.read().decode()
            require(token not in content, "HTTP response exposed credential")
            return response.status, json.loads(content)
        finally:
            connection.close()
    def start():
        nonlocal process
        log = (root / ("service-" + str(len(logs)) + ".log")).open("w")
        logs.append(log)
        process = subprocess.Popen([executable, "--config", str(file)], stdout=log, stderr=subprocess.STDOUT)
        end = time.monotonic() + 8
        while True:
            require(process.poll() is None, "ECC service startup exited")
            try:
                code, identity = request("GET", "/v1/identity")
                if code == 200:
                    return identity
            except (OSError, http.client.HTTPException):
                pass
            require(time.monotonic() < end, "ECC service startup deadline")
            time.sleep(.02)
    def stop():
        nonlocal process
        if process is not None and process.poll() is None:
            process.send_signal(signal.SIGTERM)
            process.wait(timeout=10)
            require(process.returncode == 0, "ECC service normal shutdown failed")
    try:
        identity = start()
        (root / "identity.json").write_text(json.dumps(identity, indent=2) + "\n")
        require(identity["service"] == "getdaq-ecc-server", "Wrong service identity")
        require(request("GET", "/v1/status", auth=False)[0] == 401, "Unauthenticated status exposed")
        scan_identity_request = {"schema_version": 1, "kind": "getdaq.scan-runtime-point-identity-request",
            "execution_snapshot_sha256": "0"*64, "point_index": 0,
            "configuration_sha256": "1"*64, "artifact_sha256": "2"*64}
        require(request("POST", "/v1/scan-runtime-identity", scan_identity_request, auth=False)[0] == 401,
                "Unauthenticated scan runtime identity observation exposed")
        require(request("POST", "/v1/scan-runtime-identity", {"schema_version": 1})[0] == 400,
                "Malformed scan runtime identity request accepted")
        base = {"schema": 1, "operation_id": "illegal-start", "command": "start", "instance_id": identity["instance_id"],
                "snapshot_sha256": identity["snapshot_sha256"], "client_id": config["client_id"], "client_epoch": "a"*64}
        for key in ("instance_id", "snapshot_sha256", "client_id"):
            wrong = dict(base)
            wrong[key] = "other-client" if key == "client_id" else "0"*64
            require(request("POST", "/v1/operations", wrong)[0] == 412, "Invalid service precondition accepted")
        require(request("POST", "/v1/operations", dict(base, raw_frame="forbidden"))[0] == 400, "Raw data field accepted")
        require(request("POST", "/v1/operations", base)[0] == 202, "Failed-state test intent not accepted")
        end = time.monotonic() + 5
        while True:
            code, result = request("GET", "/v1/operations/illegal-start")
            if result["phase"] in ("complete", "failed", "unknown"):
                break
            require(time.monotonic() < end, "ECC command observation deadline")
            time.sleep(.02)
        require(code == 200 and result["phase"] == "failed" and result["result"]["state"] == 1, "Illegal Idle->Start fabricated hardware success")
        (root / "operation.json").write_text(json.dumps(result, indent=2) + "\n")
        duplicate = request("POST", "/v1/operations", base)
        require(duplicate[0] == 202 and duplicate[1]["phase"] == "failed", "Duplicate failed operation replayed")
        require(request("POST", "/v1/operations", dict(base, command="prepare"))[0] == 412, "Reused ID with different request accepted")
        require(request("POST", "/v1/operations", dict(base, operation_id="other", client_epoch="b"*64))[0] == 409, "Client takeover accepted")
        status = request("GET", "/v1/status")[1]
        require(status["controller"]["state"] == 1 and not status["operation_active"], "Metadata tests mutated ECC state")
        native_journal = Path(config["audit_root"]) / "ecc.jsonl"
        require(not any(json.loads(row).get("phase") == "intent" for row in native_journal.read_text().splitlines()), "Metadata tests started hardware workflow")
        stop()
        reopened = start()
        require(reopened["instance_id"] != identity["instance_id"], "ECC server reused old instance")
        require(request("GET", "/v1/operations/illegal-start")[1]["phase"] == "failed", "Reopen lost operation record")
        require(request("GET", "/v1/status")[1]["control_needs_reconciliation"], "Reopen cleared reconciliation gate")
        require(request("POST", "/v1/operations", base)[0] == 412, "Stale instance request accepted")
        current = dict(base, instance_id=reopened["instance_id"], operation_id="new-start")
        require(request("POST", "/v1/operations", current)[0] == 409, "Reopened server performed positive command")
        stop()
    finally:
        stop()
        for log in logs:
            log.close()
            require(token not in Path(log.name).read_text(), "Service log exposed credential")
    (root / "scope.json").write_text(json.dumps({"passed": True, "hardware_commands_sent": False,
        "full_ecc_workflow_tested": False, "raw_receiver_tested": False, "scope": "actual HTTP process and control boundary only"}, indent=2) + "\n")
    print("PASS actual ECC HTTP process: offline/no-state, auth, strict scope, dedup, owner, illegal transition and reopen; no hardware/full acquisition claim")
    print("Evidence:", root)


if __name__ == "__main__":
    main()
