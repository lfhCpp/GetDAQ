#!/usr/bin/env python3
"""Original-owner handoff using authenticated services and real, normally sealed receivers."""
import argparse
import fcntl
import hashlib
import http.client
import http.server
import socket
import threading
import json
import signal
import tempfile
from pathlib import Path

from run_receiver_observation_tests import interrupted
from run_session_retirement_service_tests import ports
from run_supervisor_control_tests import Rig, frame, require

CASES = ["success", "ack-loss", "cold", "proof-tamper", "request-tamper", "directory-tamper", "seal-tamper", "lock-held", "independent-hold"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class DropHandoffReply:
    def __init__(self, rig):
        self.posts = 0
        relay = self
        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"
            def log_message(self, *args): pass
            def forward(self):
                body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
                client = http.client.HTTPConnection("127.0.0.1", rig.ports[0], timeout=3)
                try:
                    client.request(self.command, self.path, body=body,
                        headers={"Authorization":self.headers.get("Authorization", ""),"Content-Type":"application/json"})
                    response = client.getresponse(); payload = response.read()
                    if self.command == "POST":
                        relay.posts += 1
                        self.close_connection = True
                        self.connection.shutdown(socket.SHUT_RDWR)
                        return
                    self.send_response(response.status)
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Content-Length", str(len(payload)))
                    self.send_header("Connection", "close"); self.end_headers(); self.wfile.write(payload)
                    self.close_connection = True
                finally: client.close()
            do_GET = forward
            do_POST = forward
        self.server = http.server.ThreadingHTTPServer(("127.0.0.1",0), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever,daemon=True)
        self.thread.start()
    def close(self):
        self.server.shutdown(); self.server.server_close(); self.thread.join(timeout=3)
        require(not self.thread.is_alive(), "Reply-loss relay did not stop")


def run(args, root, mode):
    rig = Rig.__new__(Rig)
    lease = None
    try:
        rig.__init__(args, root / mode, ports())
        original_request = rig.make_request
        def handoff(ident):
            request = original_request("handoff-control", ident)
            request.update(successor_controller_snapshot_sha256="e"*64, successor_controller_epoch="f"*64)
            return request
        require(rig.request("POST", "/v1/operations", handoff("unowned"))[0] == 409, "Unowned Fresh service handed off control")
        rig.start()
        require(rig.request("POST", "/v1/operations", handoff("running"))[0] == 409, "Running receivers handed off control")
        expected = [frame(1), frame(2, True)]
        rig.connect_senders()
        finished = rig.finish(expected)
        before = rig.status()
        raw = {p: sha(p) for p in rig.root.rglob("*.graw")}
        require(sum(p.stat().st_size for p in raw) == sum(map(len, expected)), "Unexpected test payload volume")
        request = handoff("handoff")
        bad = dict(request, operation_id="foreign", controller_epoch="0"*64)
        require(rig.request("POST", "/v1/operations", bad)[0] == 409, "Foreign controller transferred ownership")
        require(rig.request("POST", "/v1/operations", request, auth=False)[0] == 401, "Unauthenticated handoff")
        malformed = dict(request, successor_controller_epoch="d"*64, successor_controller_snapshot_sha256="c"*64)
        require(rig.request("POST", "/v1/operations", malformed)[0] == 400, "Self handoff accepted")
        malformed = dict(request); malformed.pop("successor_controller_epoch")
        require(rig.request("POST", "/v1/operations", malformed)[0] == 400, "Incomplete successor accepted")
        require(rig.status()["lifecycle"] == before["lifecycle"], "Preflight handoff rejection changed receiver state")
        if mode in ("seal-tamper", "lock-held", "independent-hold"):
            changed = None
            if mode == "seal-tamper":
                changed = next(iter(raw)); saved = changed.read_bytes(); changed.write_bytes(saved[:-1])
            elif mode == "lock-held":
                directory = finished["result"]["receivers"][0]["output_directory"]
                lease = open(Path(directory)/".getdaq-receiver.lock", "r+b")
                fcntl.flock(lease, fcntl.LOCK_EX|fcntl.LOCK_NB)
            else:
                rig.submit(original_request("finish", "invalid-extra-finish")); rig.completed("invalid-extra-finish", "failed")
            require(rig.request("POST", "/v1/operations", request)[0] == 409, "Unconfirmed/held data handed off ownership")
            if changed: changed.write_bytes(saved)
            if lease: lease.close(); lease = None
            require(rig.request("POST", "/v1/operations", handoff("restored"))[0] == 409, "Restoration cleared handoff hold")
            held = rig.status()
            require(held["controller_epoch"] == "d"*64 and held["control_needs_reconciliation"], "Handoff hold lost original owner")
            return {"case": mode, "passed": True, "old_owner_preserved": True}
        request_path = rig.root/"handoff-request.json"
        request_path.write_text(json.dumps(request, indent=2)+"\n")
        if mode == "ack-loss":
            relay = DropHandoffReply(rig)
            try:
                rig.query("submit", "--request-file", request_path,
                    override={"--port":str(relay.server.server_port)},okay=False)
            finally: relay.close()
            require(relay.posts == 1, "Native client repeated the handoff POST after reply loss")
        else:
            rig.query("submit", "--request-file", request_path)
        result = rig.completed("handoff")
        require(result["result"] == before["lifecycle"], "Handoff changed old completed receiver session")
        state = rig.status()
        require(state["controller_snapshot_sha256"] == "e"*64 and state["controller_epoch"] == "f"*64, "Successor not published")
        require(not state["control_needs_reconciliation"], "Successful handoff latched a control hold")
        proof = Path(rig.plan["audit_root"])/"control/operations/handoff/handoff-proof.json"
        require(sha(proof) == result["handoff"]["proof_sha256"], "Handoff proof digest differs")
        require(rig.query("operation", "--operation-id", "handoff") == result, "Native CLI cannot read handoff outcome")
        require(rig.request("POST", "/v1/operations", request)[1] == result, "Same handoff ID was replayed")
        require(sum(row["event"] == "accepted" and row.get("operation_id") == "handoff" for row in rig.journal()) == 1, "Duplicate handoff intent")
        require(rig.request("POST", "/v1/operations", original_request("start", "old-owner-start"))[0] == 409, "Old owner started another receiver")
        altered = dict(request, successor_controller_epoch="9"*64)
        require(rig.request("POST", "/v1/operations", altered)[0] == 412, "Same ID changed successor")
        def successor(command, ident):
            value = original_request(command, ident)
            value.update(controller_snapshot_sha256="e"*64, controller_epoch="f"*64)
            return value
        rig.make_request = successor
        if mode in ("proof-tamper", "request-tamper", "directory-tamper"):
            target = proof if mode == "proof-tamper" else proof.parent/"request.json"
            saved = target.read_bytes()
            if mode == "directory-tamper":
                old_directory = proof.parent.with_name("moved-original-handoff")
                proof.parent.rename(old_directory); proof.parent.mkdir(mode=0o700)
                for path in old_directory.iterdir(): path.rename(proof.parent/path.name)
            else:
                target.write_bytes(saved+b" ")
            require(rig.request("POST", "/v1/operations", successor("verify-stopped", "tampered-proof"))[0] == 409, "Modified handoff proof was ignored")
            if mode == "directory-tamper":
                for path in proof.parent.iterdir(): path.rename(old_directory/path.name)
                proof.parent.rmdir(); old_directory.rename(proof.parent)
            else: target.write_bytes(saved)
            require(rig.request("POST", "/v1/operations", successor("start", "restored-proof"))[0] == 409, "Restored proof cleared storage hold")
        elif mode == "cold":
            rig.stop_service(); rig.start_service()
            cold = rig.status()
            require(cold["controller_epoch"] == "f"*64 and cold["control_needs_reconciliation"], "Cold replay lost successor or adopted old receivers")
            require(rig.operation("handoff") == result, "Cold handoff result changed")
            require(rig.request("POST", "/v1/operations", successor("start", "cold-start"))[0] == 409, "Cold service replayed transfer into new acquisition")
        else:
            _, started = rig.start("successor-start")
            require(started["result"]["session_id"] != before["lifecycle"]["session_id"], "Successor reused old session")
            rig.connect_senders(); rig.finish([frame(3), frame(4, True)], "successor-finish")
        require(all(sha(p) == expected_sha for p, expected_sha in raw.items()), "Old raw bytes changed across handoff")
        return {"case": mode, "passed": True, "successor_explicit": True, "old_raw_retained": True}
    finally:
        if lease: lease.close()
        rig.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("service", type=Path); parser.add_argument("query", type=Path); parser.add_argument("receiver", type=Path)
    parser.add_argument("evidence", type=Path); parser.add_argument("--cases", nargs="+", choices=CASES, default=CASES)
    args = parser.parse_args()
    for name in ("service", "query", "receiver"): setattr(args, name, getattr(args, name).resolve(strict=True))
    args.file_naming="native"
    args.evidence.mkdir(parents=True, exist_ok=True)
    root=Path(tempfile.mkdtemp(prefix="controller-handoff-", dir=args.evidence)).resolve()
    signal.signal(signal.SIGINT, interrupted); signal.signal(signal.SIGTERM, interrupted)
    results=[]; deleted=[]
    try:
        for mode in args.cases: results.append(run(args, root, mode))
    finally:
        for path in root.rglob("*.graw"):
            deleted.append({"path":str(path.relative_to(root)), "bytes":path.stat().st_size, "sha256":sha(path)}); path.unlink()
        (root/"cleanup.json").write_text(json.dumps({"deleted":deleted,"raw_remaining":len(list(root.rglob("*.graw")))},indent=2)+"\n")
    (root/"result.json").write_text(json.dumps({"passed":True,"cases":results,"program_sha256":{name:sha(getattr(args,name)) for name in ("service","query","receiver")},
        "maximum_aggregate_test_bytes":2*1024**2,"aggregate_limit_bytes":20*1024**3,"raw_remaining":0,"real_hardware":False},indent=2)+"\n")
    print("PASS",len(results),"authenticated original-controller handoff cases; old raw preserved during test, own raw removed;",root)


if __name__ == "__main__": main()
