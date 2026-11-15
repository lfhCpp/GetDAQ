#!/usr/bin/env python3
"""Persistent ECC journal: real process termination and read-only CLI inspection."""
import json
import pathlib
import select
import subprocess
import sys
import tempfile

if len(sys.argv) != 3:
    raise SystemExit('usage: ecc_journal_integration.py GETDAQ_ECC_JOURNAL ECC_JOURNAL_TESTS')
inspector, fixture = (str(pathlib.Path(p).resolve()) for p in sys.argv[1:])
scope = 'test-CoBo-0-configuration-v1'

def inspect(path, *args, code=0, target_scope=scope):
    result = subprocess.run([inspector, 'inspect', str(path), target_scope, *args], capture_output=True, text=True, timeout=10)
    assert result.returncode == code, (result.returncode, result.stdout, result.stderr)
    return json.loads(result.stdout) if code == 0 else result.stderr

with tempfile.TemporaryDirectory(prefix='getdaq-ecc-journal-cli-') as directory:
    root = pathlib.Path(directory)
    completed = root / 'history.jsonl'
    result = subprocess.run([fixture, '--fixture', str(completed)], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stderr
    original = completed.read_bytes()
    result = inspect(completed)
    assert result['state'] == 5 and result['transition'] == 0 and result['error'] == 6
    assert result['outcome_unknown'] and result['needs_reconciliation']
    assert result['operation_id'] == 'pause-1'
    assert len(result['operations']) == 5
    assert [op['phase'] for op in result['operations']] == ['complete'] * 4 + ['unknown']
    pause = inspect(completed, '--operation', 'pause-1')
    assert pause['command'] == 6 and pause['from'] == 5 and pause['to'] == 6
    assert '中文' in pause['message']
    assert 'scope differs' in inspect(completed, code=1, target_scope='another-board')
    assert 'not recorded' in inspect(completed, '--operation', 'missing', code=1)
    assert completed.read_bytes() == original, 'Inspection changed history'
    missing = root / 'missing.jsonl'
    inspect(missing, code=1)
    assert not missing.exists(), 'Inspector created a journal'
    corrupt = root / 'truncated.jsonl'; corrupt.write_bytes(original[:-1])
    inspect(corrupt, code=1)
    assert corrupt.read_bytes() == original[:-1], 'Inspector repaired/truncated history'

    pending = root / 'pending.jsonl'
    process = subprocess.Popen([fixture, '--serve-pending', str(pending)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        assert select.select([process.stdout], [], [], 10)[0], 'Pending fixture did not start'
        assert process.stdout.readline().strip() == 'ready'
        assert 'locked' in inspect(pending, code=1)
        process.kill(); process.communicate(timeout=5)
        assert process.returncode == -9, 'Fixture was not terminated at the intended stage'
        bytes_before = pending.read_bytes()
        recovered = inspect(pending)
        assert recovered['state'] == 1 and recovered['error'] == 1
        assert recovered['outcome_unknown'] and recovered['needs_reconciliation']
        op = inspect(pending, '--operation', 'describe-1')
        assert op['phase'] == 'intent', 'An unconfirmed intent was invented as completion'
        assert pending.read_bytes() == bytes_before, 'Recovery inspection mutated pending intent'
    finally:
        if process.poll() is None:
            process.kill(); process.communicate(timeout=5)
    print('PASS ECC journal CLI: unknown outcomes, history query, target binding, locks, SIGKILL recovery, immutable inspection')
