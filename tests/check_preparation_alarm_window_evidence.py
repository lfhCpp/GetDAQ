#!/usr/bin/env python3
"""Check actual GUI intents/observations against the ECC's accepted results."""
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
        assert scenario in ("preparation-alarm-recovery-window-0", "preparation-alarm-recovery-window-1")
        root = folder / scenario
        proof = read_json(root / "preparation-window-result.json")
        assert proof["passed"] and proof["encoding_minor"] == int(scenario[-1])
        assert proof["real_main_window_and_runcontrol"] and proof["no_receivers_started"]
        gui = read_json(root / "window-preparation-proof.json")
        original = gui["original_failed_operation_id"]
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
        assert len(accepted) == len(terminal) == 6
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
        assert set(intents) == set(observations) and len(intents) == 5
        automatic = set(accepted) - set(intents)
        assert len(automatic) == 1
        alarm_id = next(iter(automatic))
        assert accepted[alarm_id][0]["command"] == "stop-production-after-failure"
        assert terminal[alarm_id][0] == "complete"
        assert terminal[alarm_id][1]["outcome_unknown"] and terminal[alarm_id][1]["operation_id"] == original
        failed = observations[original]
        assert failed["request"]["command"] == "prepare" and failed["phase"] == "failed"
        assert failed["result"]["outcome_unknown"] and failed["result"]["operation_id"] == original
        recovery = [value for value in observations.values() if value["request"]["command"] == "reconcile-preparation-with-alarm"]
        assert len(recovery) == 1 and recovery[0]["phase"] == "complete"
        assert recovery[0]["request"]["failed_operation_id"] == original
        assert recovery[0]["request"]["alarm_operation_id"] == alarm_id
        events = [json.loads(line) for line in backend.read_text().splitlines()]
        reset = next(row for row in events if row["event"] == "complete" and row["operation_id"] == recovery[0]["request"]["operation_id"])
        assert reset["alarm_policy_reset"] and reset["alarm_stop_operation_id"] == alarm_id
        assert "receiver_policy_reset" not in reset
        held = next(row for row in events if row["event"] == "alarm-held" and row["operation_id"] == alarm_id)
        assert reset["alarm_observation_sha256"] == digest(b"getdaq.ecc-alarm-observation:1\n" + held["evidence"].encode())
        restored = recovery[0]["result"]
        assert restored["state"] == 2 and not restored["outcome_unknown"]
        assert not restored["needs_reconciliation"] and restored["transition"] == 0
        assert restored["operation_id"] == recovery[0]["request"]["operation_id"]
        for value in observations.values():
            if value["request"]["command"] in ("stop-production-after-failure", "unprepare-after-failure"):
                assert value["phase"] == "complete"
                assert value["result"]["state"] == 2 and value["result"]["outcome_unknown"]
                assert value["result"]["operation_id"] == original
        fresh = [value for ident, value in observations.items() if value["request"]["command"] == "prepare" and ident != original]
        assert len(fresh) == 1 and fresh[0]["phase"] == "complete"
        assert fresh[0]["result"]["state"] == 3 and not fresh[0]["result"]["outcome_unknown"]
        output = {"passed": True, "encoding_minor": int(scenario[-1]),
                  "actual_gui_intents": 5, "matching_terminal_observations": 5, "backend_operations": 6,
                  "original_failed_prepare_bound": True, "recovery_target_described": True,
                  "new_prepare_complete": True, "raw_remaining": 0,
                  "client_journal_sha256": digest(journal.read_bytes()),
                  "backend_journal_sha256": digest(backend.read_bytes()),
                  "verifier_sha256": digest(Path(__file__).read_bytes())}
        (root / "window-store-binding-result.json").write_text(json.dumps(output, indent=2) + "\n")
    print("PASS five actual GUI intents/terminal results match six ECC operations, original alarm and atomic reset; " + str(folder))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    check(parser.parse_args().evidence.resolve())
