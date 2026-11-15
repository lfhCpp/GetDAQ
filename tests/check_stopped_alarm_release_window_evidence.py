#!/usr/bin/env python3
"""Check six actual stopped-alarm GUI intents against seven backend operations."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def check(folder):
    result = read_json(folder / "result.json")
    assert result["passed"] and result["raw_remaining"] == 0
    assert read_json(folder / "cleanup.json")["remaining_raw"] == []
    for run in result["runs"]:
        scenario = run["scenario"]
        assert scenario in ("qt-stopped-alarm-release-0", "qt-stopped-alarm-release-1")
        root = folder / scenario
        proof = read_json(root / "stopped-alarm-window-result.json")
        assert proof["passed"] and proof["encoding_minor"] == int(scenario[-1])
        assert proof["real_main_window_and_runcontrol"] and proof["original_run_interrupted"]
        run_journal = read_json(root / "gui-runs/hardware/NativeIntegration/run_0000/journal.json")
        saved = json.loads(run_journal["native_acquisition_recovery_operation"])
        original = saved["request"]["failed_operation_id"]
        assert run_journal["hardware_recovery_complete"]
        assert saved == json.loads(run_journal["native_alarm_acknowledgement_operation"])
        assert saved["request"]["command"] == "release-stopped-alarm"
        assert saved["result"]["state"] == 2 and not saved["result"]["outcome_unknown"]
        assert saved["result"]["operation_id"] == saved["request"]["operation_id"]
        assert run_journal["phase"] == "Interrupted" and run_journal["duration_incomplete"]
        assert not (root / "gui-runs/hardware/NativeIntegration/run_0000/run.csv").exists()
        before = read_json(root / "gui-before-alarm-status.json")
        after = read_json(root / "final-status.json")
        assert after["instance_id"] == before["instance_id"] and after["snapshot_sha256"] == before["snapshot_sha256"]
        assert after["controller"]["state"] == 3 and not after["controller"]["outcome_unknown"]
        assert not after["controller"]["needs_reconciliation"] and not after["control_needs_reconciliation"]
        assert not after["operation_active"] and not after["data"]["needs_reconciliation"]
        assert len(after["data"]["nodes"]) == len(before["data"]["nodes"]) == 2
        for current, original_node in zip(after["data"]["nodes"], before["data"]["nodes"]):
            assert current["agent_id"] == original_node["agent_id"] and current["instance_id"] == original_node["instance_id"]
            now, old = current["lifecycle"], original_node["lifecycle"]
            assert now["state"] == "stopped" and now["session_id"] == old["session_id"]
            assert len(now["receivers"]) == len(old["receivers"]) == 1
            assert now["receivers"][0]["pid"] == old["receivers"][0]["pid"]
            assert now["receivers"][0]["output_directory"] == old["receivers"][0]["output_directory"]
            assert now["receivers"][0]["reaped"] and now["receivers"][0]["seal_verified"]
        accepted, terminal = {}, {}
        backend = root / "controller/network/operations.jsonl"
        for line in backend.read_text(encoding="utf-8").splitlines():
            row = json.loads(line)
            if row["event"] == "accepted":
                request = json.loads(row["request"])
                ident = request["operation_id"]
                assert ident not in accepted
                accepted[ident] = (request, row["request_key"])
            elif row["event"] in ("complete", "failed"):
                ident = row["operation_id"]
                assert ident not in terminal
                terminal[ident] = (row["event"], json.loads(row["result"]))
        assert len(accepted) == len(terminal) == 7
        intents, observations = {}, {}
        journal = root / "gui-client-records/observations.jsonl"
        for sequence, line in enumerate(journal.read_text(encoding="utf-8").splitlines()):
            row = json.loads(line)
            assert row["schema"] == 1 and type(row["sequence"]) is int
            assert row["sequence"] == sequence
            value = json.loads(row["payload"])
            if row["kind"] == "intent":
                ident = value["operation_id"]
                assert re.fullmatch(r"[A-Za-z0-9_-]{1,128}", ident)
                assert ident not in intents
                canonical = row["payload"].encode("utf-8")
                assert (root / "gui-client-records/requests" / (ident + ".json")).read_bytes() == canonical
                key = digest(b"getdaq.ecc-control-request:1\n" + canonical)
                assert key == row["request_key"] == accepted[ident][1]
                assert value == accepted[ident][0]
                intents[ident] = value
            else:
                assert row["kind"] == "observation"
                ident = value["request"]["operation_id"]
                assert value["request"] == intents[ident]
                assert value["request_key"] == row["request_key"] == accepted[ident][1]
                if value["phase"] in ("complete", "failed"):
                    assert (value["phase"], value["result"]) == terminal[ident]
                    if ident in observations:
                        assert observations[ident] == value
                    observations[ident] = value
        assert set(intents) == set(observations) and len(intents) == 6
        automatic = set(accepted) - set(intents)
        assert len(automatic) == 1 and original == next(iter(automatic))
        assert accepted[original][0]["command"] == "stop" and terminal[original][0] == "complete"
        assert not terminal[original][1]["outcome_unknown"] and terminal[original][1]["state"] == 4
        assert saved["request"]["alarm_operation_id"] == original
        events = [json.loads(line) for line in backend.read_text().splitlines()]
        reset = next(row for row in events if row["event"] == "complete" and row["operation_id"] == saved["request"]["operation_id"])
        assert reset["alarm_policy_reset"] and reset["alarm_stop_operation_id"] == original
        assert "receiver_policy_reset" not in reset
        held = next(row for row in events if row["event"] == "alarm-held")
        assert reset["alarm_observation_sha256"] == digest(b"getdaq.ecc-alarm-observation:1\n" + held["evidence"].encode())
        native = [json.loads(line) for line in (root / "controller/ecc.jsonl").read_text().splitlines()]
        assert any(row["phase"] == "complete" and row.get("id") == original for row in native)
        assert not any(row["phase"] == "reconciled" for row in native)
    print("PASS six GUI intents and terminal results match seven ECC operations; normal seals and interrupted Run retained; " + str(folder))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    check(parser.parse_args().folder.resolve())
