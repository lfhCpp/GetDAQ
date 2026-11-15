#!/usr/bin/env python3
"""Independently check recorded source retirement files and original-failure bindings."""
import argparse
import hashlib
import json
from pathlib import Path


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def child(root, name):
    path = root / name
    assert not Path(name).is_absolute() and ".." not in Path(name).parts
    assert path.is_file() and not path.is_symlink()
    assert root.resolve() in path.resolve().parents
    return path


def check(controller, retirement_id, recovery_id, original_id):
    retirement = controller / "operations" / retirement_id
    manifest_file = retirement / "source-retirement-manifest.json"
    manifest = read(manifest_file)
    assert manifest["operation_id"] == retirement_id and manifest["failed_operation_id"] == original_id
    rows = [json.loads(line) for line in (retirement / "operation.jsonl").read_text().splitlines()]
    assert [row["phase"] for row in rows] == ["intent", "backend_complete"]
    assert rows[-1]["source_retirement_manifest_sha256"] == sha(manifest_file)
    assert rows[-1]["original_source_connections_retired"] and rows[-1]["parent_unknown_preserved"]
    assert not rows[-1]["source_end_requested"] and not rows[-1]["run_complete"]
    names = set()
    source_count = 0
    for entry in manifest["audits"]:
        assert entry["file"] not in names
        names.add(entry["file"])
        audit = child(retirement, entry["file"])
        assert sha(audit) == entry["sha256"]
        assert len(entry["writer_chain_sha256"]) == 64
        events = [json.loads(line) for line in audit.read_text().splitlines()]
        acknowledged = [row for row in events if row["phase"] == "failed-source-retirement-acknowledged"]
        if acknowledged:
            assert len(acknowledged) == 1
            row = acknowledged[0]
            assert row["producer_idle_verified"] and row["data_connection_closed"] and row["outcome_unknown"]
            assert not row["source_end_requested"] and not row["source_finish_issued"] and not row["source_end_acknowledged"]
            assert row["original_error"] and int(row["issued_bytes"]) == int(row["locally_sent_bytes"])
            source_count += 1
    recovery = controller / "operations" / recovery_id
    proof = read(recovery / "recovery-proof.json")
    assert proof["failed_operation_id"] == original_id and proof["source_retirement_operation_id"] == retirement_id
    assert proof["read_only"] and proof["target_state"] == 2 and proof["original_source_unknown_retained"]
    assert proof["original_raw_retained"] and not proof["original_run_complete"] and proof["fresh_prepare_required"]
    observation = child(recovery, proof["source_retirement_observation_file"])
    assert sha(observation) == proof["source_retirement_observation_sha256"]
    sources = read(observation)
    assert source_count == len(sources) == 2
    assert len({source["receiver_session"] for source in sources}) == len(sources)
    for source in sources:
        assert source["state"] == "unknown" and source["original_error"]
        assert source["failure_stop_acknowledged"] and source["data_connection_closed"] and source["failure_retired"]
        assert not source["source_finish_issued"] and not source["source_end_acknowledged"]
    return source_count


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    args = parser.parse_args()
    result = read(args.evidence / "result.json")
    assert result["passed"] and result["raw_remaining"] == 0
    for run in result["runs"]:
        assert run["scenario"] in ("ice-source-recovery-0", "ice-source-recovery-1")
        check(args.evidence / run["scenario"] / "controller", "original-source-retirement", "verified-source-recovery", "start")
    print("PASS original source retirement manifests, child file digests and read-only recovery bindings")
