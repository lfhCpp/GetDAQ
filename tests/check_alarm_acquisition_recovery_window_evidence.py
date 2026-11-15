#!/usr/bin/env python3
"""Check ten actual GUI alarm-recovery intents against twelve backend operations."""
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
        assert scenario in ("qt-alarm-acquisition-recovery-0", "qt-alarm-acquisition-recovery-1")
        root = folder / scenario
        proof = read_json(root / "alarm-acquisition-recovery-window-result.json")
        assert proof["passed"] and proof["encoding_minor"] == int(scenario[-1])
        assert proof["real_main_window_and_runcontrol"] and proof["run_incomplete"]
        run_journal = read_json(root / "gui-runs/hardware/NativeIntegration/run_0000/journal.json")
        saved = json.loads(run_journal["native_acquisition_recovery_operation"])
        original = saved["request"]["failed_operation_id"]
        assert run_journal["hardware_recovery_complete"]
        assert saved == json.loads(run_journal["native_alarm_acknowledgement_operation"])
        assert saved["request"]["command"] == "reconcile-acquisition-with-alarm"
        assert saved["result"]["state"] == 2 and not saved["result"]["outcome_unknown"]
        assert saved["result"]["operation_id"] == saved["request"]["operation_id"]
        assert run_journal["phase"] == "Interrupted" and run_journal["duration_incomplete"]
        assert not (root / "gui-runs/hardware/NativeIntegration/run_0000/run.csv").exists()
        before = read_json(root / "gui-before-receiver-failure.json")
        after = read_json(root / "final-status.json")
        assert after["instance_id"] == before["instance_id"] and after["snapshot_sha256"] == before["snapshot_sha256"]
        assert after["controller"]["state"] == 3 and not after["controller"]["outcome_unknown"]
        assert not after["controller"]["needs_reconciliation"] and not after["control_needs_reconciliation"]
        assert not after["operation_active"] and not after["data"]["needs_reconciliation"]
        assert len(after["data"]["nodes"]) == len(before["data"]["nodes"]) == 2
        for current, original_node in zip(after["data"]["nodes"], before["data"]["nodes"]):
            assert current["agent_id"] == original_node["agent_id"] and current["instance_id"] == original_node["instance_id"]
            now, old = current["lifecycle"], original_node["lifecycle"]
            assert now["state"] == "retired" and now["session_id"] == old["session_id"]
            assert len(now["receivers"]) == len(old["receivers"]) == 1
            assert now["receivers"][0]["pid"] == old["receivers"][0]["pid"]
            assert now["receivers"][0]["output_directory"] == old["receivers"][0]["output_directory"]
            assert now["receivers"][0]["reaped"] and not now["receivers"][0]["seal_verified"]
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
        assert len(accepted) == len(terminal) == 12
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
        assert set(intents) == set(observations) and len(intents) == 10
        automatic = set(accepted) - set(intents)
        assert len(automatic) == 2
        receiver_ids = [ident for ident in automatic if ident.startswith("internal-receiver-")]
        alarm_ids = [ident for ident in automatic if ident.startswith("internal-alarm-")]
        assert len(receiver_ids) == len(alarm_ids) == 1
        automatic_id, alarm_id = receiver_ids[0], alarm_ids[0]
        assert all(accepted[ident][0]["command"] == "stop-production-after-failure" and terminal[ident][0] == "complete" for ident in automatic)
        assert saved["request"]["alarm_operation_id"] == alarm_id
        assert observations[original]["request"]["command"] == "start"
        assert observations[original]["phase"] == "complete" and observations[original]["result"]["state"] == 5
        from collections import Counter
        assert Counter(v["command"] for v in intents.values()) == Counter({"describe": 1,"prepare": 2,"configure": 1,
            "start": 1,"breakup-after-failure": 1,"abort-failed-receivers": 1,"retire-failed-receivers": 1,
            "unprepare-acquisition-after-failure": 1,"reconcile-acquisition-with-alarm": 1})
        preparations = [op for op in observations.values() if op["request"]["command"] == "prepare"]
        assert len(preparations) == 2 and all(op["phase"] == "complete" and op["result"]["state"] == 3 for op in preparations)
        recovery_rows = [json.loads(line) for line in backend.read_text().splitlines()
                         if json.loads(line)["event"] == "complete" and json.loads(line)["operation_id"] == saved["request"]["operation_id"]]
        assert len(recovery_rows) == 1 and recovery_rows[0]["receiver_policy_reset"]
        assert recovery_rows[0]["receiver_stop_operation_id"] == automatic_id
        assert recovery_rows[0]["alarm_policy_reset"] and recovery_rows[0]["alarm_stop_operation_id"] == alarm_id
        alarm_rows = [json.loads(line) for line in backend.read_text().splitlines() if json.loads(line)["event"] == "alarm-held"]
        assert len(alarm_rows) == 1 and alarm_rows[0]["operation_id"] == alarm_id
        assert recovery_rows[0]["alarm_observation_sha256"] == digest(b"getdaq.ecc-alarm-observation:1\n" + alarm_rows[0]["evidence"].encode())

        parent_rows = [json.loads(line) for line in (root / "controller/ecc.jsonl").read_text().splitlines()]
        assert [r["phase"] for r in parent_rows if r.get("id") == original] == ["intent", "complete", "held"]
        assert any(r["phase"] == "reconciled" and r["id"] == saved["request"]["operation_id"] for r in parent_rows)

        for value in observations.values():
            if value["request"]["command"] in ("breakup-after-failure", "abort-failed-receivers", "retire-failed-receivers", "unprepare-acquisition-after-failure"):
                assert value["phase"] == "complete" and value["result"]["outcome_unknown"]
                assert value["result"]["operation_id"] == original
        assert saved == observations[saved["request"]["operation_id"]]
        output = {"passed": True, "encoding_minor": int(scenario[-1]),
                  "actual_gui_intents": 10, "matching_terminal_observations": 10, "backend_operations": 12,
                  "original_running_failure_bound": True, "bound_described_recovery_verified": True, "original_alarm_acknowledged": True, "fresh_prepare_verified": True,
                  "run_interrupted": True, "no_run_csv": True, "raw_remaining": 0,
                  "client_journal_sha256": digest(journal.read_bytes()),
                  "backend_journal_sha256": digest(backend.read_bytes()),
                  "verifier_sha256": digest(Path(__file__).read_bytes())}
        (root / "window-store-binding-result.json").write_text(json.dumps(output, indent=2) + "\n")
    print("PASS actual GUI request files, durable intents and terminal observations match ECC; " + str(folder))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    check(parser.parse_args().evidence.resolve())
