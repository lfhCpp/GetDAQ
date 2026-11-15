#!/usr/bin/env python3
"""Independently bind committed control handoffs to original requests and sealed sessions."""
import argparse
import hashlib
import json
from pathlib import Path


def sha(data):return hashlib.sha256(data).hexdigest()


def check_node(directory):
    policy=json.loads((directory/"policy.json").read_text())
    owner=None;accepted={};completed=0
    for sequence,line in enumerate((directory/"operations.jsonl").read_text().splitlines()):
        row=json.loads(line)
        assert row["sequence"]==sequence and row["plan_sha256"]==policy["plan_sha256"]
        event=row["event"]
        if event=="owner":
            assert owner is None
            owner=(row["controller_snapshot_sha256"],row["controller_epoch"])
        elif event=="accepted":
            canonical=row["request"].encode();request=json.loads(canonical)
            ident=row["operation_id"]
            assert ident not in accepted and request["operation_id"]==ident
            assert row["request_key"]==sha(b"getdaq.supervisor-request:1\n"+canonical)
            assert (directory/"operations"/ident/"request.json").read_bytes()==canonical
            if owner is None: assert request["command"] in ("validate","verify-ready","verify-stopped")
            else: assert owner==(request["controller_snapshot_sha256"],request["controller_epoch"])
            accepted[ident]=request
        elif event=="complete" and accepted[row["operation_id"]]["command"]=="handoff-control":
            request=accepted[row["operation_id"]]
            proof=(directory/"operations"/row["operation_id"]/"handoff-proof.json").read_bytes()
            assert sha(proof)==row["handoff_proof_sha256"]
            value=json.loads(proof);status=json.loads(row["result"])
            assert value["schema"]==1 and value["kind"]=="getdaq.supervisor-control-handoff"
            assert value["operation_id"]==row["operation_id"] and value["request_key"]==row["request_key"]
            for key in ["instance_id","plan_sha256","controller_id","controller_snapshot_sha256","controller_epoch", "successor_controller_snapshot_sha256","successor_controller_epoch"]:
                assert value[key]==request[key]
            assert value["lifecycle"]==status and status["state"]=="stopped" and not status["needs_reconciliation"]
            assert status["receivers"] and all(r["reaped"] and r["seal_verified"] and r["exit_code"]==0 for r in status["receivers"])
            owner=(request["successor_controller_snapshot_sha256"],request["successor_controller_epoch"])
            completed+=1
    return completed


def check(root):
    result=json.loads((root/"result.json").read_text());assert result["passed"] and result["raw_remaining"]==0
    assert not list(root.rglob("*.graw"))
    completions=0
    for policy in root.rglob("control/policy.json"):
        completions+=check_node(policy.parent)
    assert completions>0
    for receipt_file in root.glob("*/control-handoff-result.json"):
        receipt=json.loads(receipt_file.read_text())
        assert receipt["kind"]=="getdaq.remote-control-handoff" and len(receipt["nodes"])==2
        for node in receipt["nodes"]:
            request=json.loads(node["request"])
            assert node["request_key"]==sha(b"getdaq.supervisor-request:1\n"+node["request"].encode())
            assert request["successor_controller_snapshot_sha256"]==receipt["successor_scope_sha256"]
            assert request["successor_controller_epoch"]==receipt["successor_epoch"]
            control=receipt_file.parent/node["agent_id"]/"deployment/control"
            proof=control/"operations"/node["operation_id"]/"handoff-proof.json"
            assert sha(proof.read_bytes())==node["proof_sha256"]
        adopted=receipt_file.parent/"successor-controller/controller-handoff.json"
        assert adopted.read_bytes()==receipt_file.read_bytes()
    print("PASS",len(result["cases"]),"cases;",completions,"committed handoffs bind actual original requests, proof digests and normal sealed sessions")


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("evidence",type=Path)
    check(parser.parse_args().evidence.resolve())
