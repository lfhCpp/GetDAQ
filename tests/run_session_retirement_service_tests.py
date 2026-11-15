#!/usr/bin/env python3
"""Authenticated data-service retirement with actual owned receiver processes."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import tempfile

from run_supervisor_control_tests import Rig, frame, process_identity, require, wait_for
from run_receiver_observation_tests import interrupted

CASES = ["success", "local-failure", "lock-held", "abort-audit", "retirement-proof",
         "independent-hold", "cold-aborted", "cold-retired"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ports():
    # Reserve all five probe ports together. Rig has a bounded startup deadline;
    # a subsequent external collision fails instead of claiming acceptance.
    held = []
    try:
        for _ in range(5):
            sock = socket.socket()
            sock.bind(("127.0.0.1", 0))
            held.append(sock)
        return [sock.getsockname()[1] for sock in held]
    finally:
        for sock in held:
            sock.close()


def failed_request(rig, command, operation, session, parent):
    request = rig.make_request(command, operation)
    request["expected_session_id"] = session
    if parent:
        request["parent_failure_operation_id"] = parent
    return request


def run_case(args, root, name):
    rig = Rig.__new__(Rig)
    lease = None
    try:
        rig.__init__(args, root / name, ports())
        _, started = rig.start()
        old = started["result"]
        session = old["session_id"]
        parent = "original-failed-run" if name != "local-failure" else ""
        early = failed_request(rig, "retire-failed-session", "before-abort", session, parent)
        require(rig.request("POST", "/v1/operations", early)[0] == 409, "Retirement adopted a healthy session")
        # This SIGKILL is fault injection for this fixture's recorded process,
        # never a product retirement operation.
        pid = old["receivers"][0]["pid"]
        require(process_identity(pid) == rig.children[pid], "Lost owned receiver before fault injection")
        os.kill(pid, signal.SIGKILL)
        connection = socket.create_connection(("127.0.0.1", rig.plan["sources"][1]["data_port"]), timeout=2)
        rig.sockets.append(connection)
        payload = frame(1)
        connection.sendall(payload)
        def received():
            import http.client
            client = http.client.HTTPConnection("127.0.0.1", rig.plan["sources"][1]["status_port"], timeout=2)
            try:
                client.request("GET", "/status")
                return int(json.loads(client.getresponse().read())["received_bytes"]) == len(payload)
            finally:
                client.close()
        wait_for(received)
        abort = failed_request(rig, "abort-failed-session", "abort", session, parent)
        rig.submit(abort)
        stopped = rig.completed("abort")["result"]
        require(stopped["state"] == "aborted" and stopped["needs_reconciliation"], "Abort cleared data hold")
        require(all(r["reaped"] and not r["seal_verified"] for r in stopped["receivers"]), "Abort invented a seal")
        old_paths = [Path(r["output_directory"]) for r in stopped["receivers"]]
        old_raw = {str(p): sha(p) for directory in old_paths for p in directory.glob("*.graw")}
        require(old_raw and sum(Path(p).stat().st_size for p in old_raw) == len(payload), "Failed raw was lost before retirement")
        require(rig.request("POST", "/v1/operations", rig.make_request("start", "before-retire-start"))[0] == 409, "Abort enabled positive restart")
        retire = failed_request(rig, "retire-failed-session", "retire", session, parent)
        def submit_retirement():
            request_file = rig.root / "retirement-request.json"
            request_file.write_text(json.dumps(retire, indent=2)+"\n")
            if name in ("success", "local-failure", "lock-held", "abort-audit"):
                response = rig.native("submit", request_file)
                require(response["code"] == 202, "Native client retirement was not accepted")
                accepted = json.loads(response["body"])
            else:
                accepted = rig.query("submit", "--request-file", request_file)
            require(accepted["operation_id"] == "retire", "Actual CLI submitted a different retirement")
            (rig.root / "retirement-client-response.json").write_text(json.dumps(accepted, indent=2)+"\n")
        for field, value in [("expected_session_id", "0"*64), ("controller_epoch", "1"*64),
                             ("controller_snapshot_sha256", "2"*64), ("instance_id", "3"*64)]:
            wrong = dict(retire, operation_id="wrong-"+field)
            wrong[field] = value
            code, response = rig.request("POST", "/v1/operations", wrong)
            require(code in (400, 409, 412), "Retirement accepted foreign scope: " + field + " " + str(code))
        if name in ("cold-aborted", "cold-retired"):
            if name == "cold-retired":
                submit_retirement()
                require(rig.completed("retire")["result"]["state"] == "retired", "Initial retirement failed")
            rig.stop_service()
            rig.start_service()
            cold = failed_request(rig, "retire-failed-session", "cold-retire", session, parent)
            require(rig.request("POST", "/v1/operations", cold)[0] == 409, "Cold service adopted old receiver ownership")
            require(rig.status()["control_needs_reconciliation"], "Cold service cleared hold from old retirement")
        elif name == "independent-hold":
            # A new, invalid mutating intent after abort creates a separate hold.
            # It must not be erased by the existing abort completion.
            repeat = failed_request(rig, "abort-failed-session", "independent-failed-abort", session, parent)
            rig.submit(repeat)
            rig.completed(repeat["operation_id"], "failed")
            require(rig.request("POST", "/v1/operations", retire)[0] == 409, "Retirement cleared another failed intent")
        elif name == "abort-audit":
            audit = Path(rig.plan["audit_root"]) / "control/operations/abort/receiver/data-supervisor.jsonl"
            with audit.open("a") as handle:
                handle.write(" ")
            submit_retirement()
            rig.completed("retire", "failed")
            require(rig.status()["control_needs_reconciliation"], "Changed abort proof cleared hold")
        else:
            if name == "lock-held":
                lease = (old_paths[1] / ".getdaq-receiver.lock").open("r+")
                fcntl.flock(lease.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                submit_retirement()
                rig.completed("retire", "failed")
                require(rig.status()["control_needs_reconciliation"], "Held child lock cleared hold")
            else:
                submit_retirement()
                retired = rig.completed("retire")["result"]
                require(retired["state"] == "retired" and not retired["needs_reconciliation"], "Data retirement not committed")
                require(retired["session_id"] == session and retired["receivers"] == stopped["receivers"], "Retirement erased history or invented completion")
                require(not rig.status()["control_needs_reconciliation"], "Corresponding abort hold remained")
                actual_status = rig.query("status")
                native_status = rig.native("status")
                require(actual_status["lifecycle"]["state"] == "retired" and
                        json.loads(native_status["body"])["lifecycle"]["state"] == "retired", "Actual native clients did not decode retirement")
                require(rig.submit(retire)["phase"] == "complete", "Same operation did not return original terminal result")
                require(sum(row["event"] == "accepted" and row.get("operation_id") == "retire" for row in rig.journal()) == 1,
                        "Retirement intent replayed")
                if name == "retirement-proof":
                    audit = Path(rig.plan["audit_root"]) / "control/operations/retire/receiver/data-supervisor.jsonl"
                    original = audit.read_bytes()
                    with audit.open("a") as handle:
                        handle.write(" ")
                    rig.submit(rig.make_request("start", "proof-changed-start"))
                    rig.completed("proof-changed-start", "failed")
                    audit.write_bytes(original)
                    require(rig.request("POST", "/v1/operations", rig.make_request("start", "restored-start"))[0] == 409,
                            "Restoring proof erased fault")
                else:
                    _, new = rig.start("new-cycle")
                    require(new["result"]["session_id"] != session, "New cycle reused failed session")
                    rig.connect_senders()
                    rig.finish([frame(7), frame(8, True)], "new-finish")
                    require(all(sha(Path(p)) == digest for p, digest in old_raw.items()), "New cycle altered original failed raw")
        require(all(sha(Path(p)) == digest for p, digest in old_raw.items()), "Retirement modified failed payload")
        require(all(not (p / ".getdaq-receiver-seal.json").exists() for p in old_paths), "Retirement fabricated a seal")
        return {"scenario": name, "passed": True, "old_session_id": session, "raw_preserved_until_fixture_cleanup": True}
    finally:
        if lease:
            lease.close()
        if hasattr(rig, "process"):
            rig.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("service", "fixture", "query", "receiver"):
        parser.add_argument(name, type=Path)
    parser.add_argument("--output-parent", type=Path, required=True)
    args = parser.parse_args()
    for name in ("service", "fixture", "query", "receiver"):
        setattr(args, name, getattr(args, name).resolve())
    args.output_parent.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="retirement-service-", dir=args.output_parent)).resolve()
    signal.signal(signal.SIGINT, interrupted)
    signal.signal(signal.SIGTERM, interrupted)
    results, removed = [], []
    try:
        for name in CASES:
            results.append(run_case(args, root, name))
    finally:
        for path in root.rglob("*.graw"):
            if path.is_file() and not path.is_symlink():
                removed.append({"path": str(path.relative_to(root)), "bytes": path.stat().st_size, "sha256": sha(path)})
                path.unlink()
        for path in root.rglob("credential"):
            path.unlink()
        remaining = list(root.rglob("*.graw"))
        (root / "cleanup.json").write_text(json.dumps({"removed": removed, "raw_remaining": len(remaining)}, indent=2)+"\n")
        (root / "result.json").write_text(json.dumps({"passed": len(results) == len(CASES), "cases": results,
            "program_sha256": {name: sha(getattr(args, name)) for name in ("service", "fixture", "query", "receiver")},
            "runner_sha256": sha(Path(__file__)), "maximum_aggregate_test_bytes": 32*1024**2,
            "raw_remaining": len(remaining), "real_hardware": False, "parent_ecc_or_run_recovered": False}, indent=2)+"\n")
    assert not remaining
    print("PASS", len(results), "actual authenticated service retirement cases; owned raw removed;", root)


if __name__ == "__main__":
    main()
