#!/usr/bin/env python3
"""Check eight actual GUI unprepare intents against nine backend operations."""
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
        assert scenario in ("qt-acquisition-unprepare-0", "qt-acquisition-unprepare-1")
        root = folder / scenario
        proof = read_json(root / "acquisition-unprepare-window-result.json")
        assert proof["passed"] and proof["encoding_minor"] == int(scenario[-1])
        assert proof["real_main_window_and_runcontrol"] and proof["run_incomplete"]
        run_journal = read_json(root / "gui-runs/hardware/NativeIntegration/run_0000/journal.json")
        saved = json.loads(run_journal["native_acquisition_unprepare_operation"])
        original = saved["result"]["operation_id"]
        assert run_journal["phase"] == "Interrupted" and run_journal["duration_incomplete"]
        assert not (root / "gui-runs/hardware/NativeIntegration/run_0000/run.csv").exists()
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
        assert len(accepted) == len(terminal) == 9
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
        assert set(intents) == set(observations) and len(intents) == 8
        automatic = set(accepted) - set(intents)
        assert len(automatic) == 1
        automatic_id = next(iter(automatic))
        assert accepted[automatic_id][0]["command"] == "stop-production-after-failure"
        assert automatic_id.startswith("internal-receiver-") and terminal[automatic_id][0] == "complete"
        assert observations[original]["request"]["command"] == "start"
        assert observations[original]["phase"] == "complete" and observations[original]["result"]["state"] == 5
        from collections import Counter
        assert Counter(v["command"] for v in intents.values()) == Counter({name: 1 for name in
            ("describe", "prepare", "configure", "start", "breakup-after-failure", "abort-failed-receivers", "retire-failed-receivers", "unprepare-acquisition-after-failure")})
        for value in observations.values():
            if value["request"]["command"] in ("breakup-after-failure", "abort-failed-receivers", "retire-failed-receivers", "unprepare-acquisition-after-failure"):
                assert value["phase"] == "complete" and value["result"]["outcome_unknown"]
                assert value["result"]["operation_id"] == original
        assert saved == observations[saved["request"]["operation_id"]]
        output = {"passed": True, "encoding_minor": int(scenario[-1]),
                  "actual_gui_intents": 8, "matching_terminal_observations": 8, "backend_operations": 9,
                  "original_running_failure_bound": True, "parent_unknown_preserved": True,
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
