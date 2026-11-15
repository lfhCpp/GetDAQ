#!/usr/bin/env python3
"""Real parent library/HTTP nodes: observe an explicit failed-retirement repair."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile

from run_receiver_observation_tests import interrupted, sha, stop_owned_group


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['fixture', 'receiver', 'service', 'injector', 'evidence']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--sanitized', action='store_true')
    args = parser.parse_args()
    programs = {name: getattr(args, name).resolve(strict=True)
                for name in ['fixture', 'receiver', 'service', 'injector']}
    before = {name: sha(path) for name, path in programs.items()}
    args.evidence.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='failed-retirement-parent-', dir=args.evidence)).resolve()
    root = Path(__file__).resolve().parents[1]
    env = dict(os.environ, GETDAQ_TEST_RETIREMENT_INJECTOR=str(programs['injector']),
               ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    if args.sanitized:
        linked = subprocess.check_output(['ldd', str(programs['service'])], text=True)
        env['GETDAQ_TEST_ASAN_RUNTIME'] = next(line.split('=>')[1].strip().split()[0]
                                              for line in linked.splitlines() if 'libasan.so' in line)
    for sig in [signal.SIGINT, signal.SIGTERM]:
        signal.signal(sig, interrupted)
    results, deleted, process = [], [], None
    try:
        for minor in [0, 1]:
            scenario = 'handoff-retire-failed-' + str(minor)
            case = folder / scenario
            command = [str(programs['fixture']), str(root/'tests/data/cobo-hardware.json'),
                       str(root/'tests/data/mutant-hardware.json'), str(programs['receiver']),
                       str(programs['service']), str(folder/(scenario+'.index')), '--scenario', scenario, str(case)]
            with (folder/(scenario+'.log')).open('w') as log:
                process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                           env=env, start_new_session=True)
                code = process.wait(timeout=120)
            stop_owned_group(process)
            process = None
            assert code == 0, (folder/(scenario+'.log')).read_text()[-3000:]
            result = json.loads((case/'failed-retirement-parent-result.json').read_text())
            assert result['passed'] and result['original_failure_preserved'] and result['old_control_fenced']
            assert result['operator_node_repair_count'] == 1 and result['parent_repair_posts'] == 0
            assert result['hardware_writes'] == 0 and not result['raw_forwarded']
            results.append(dict(scenario=scenario, command=command, exit_code=code, result=result))
    finally:
        if process is not None:
            stop_owned_group(process)
        # Only private fixtures below this unique root; processes have stopped.
        raw = list(folder.rglob('*.graw'))
        assert sum(path.stat().st_size for path in raw) <= 64*1024**2
        for path in raw:
            assert folder in path.resolve().parents and not path.is_symlink()
            deleted.append(dict(path=str(path.relative_to(folder)), bytes=path.stat().st_size, sha256=sha(path)))
            path.unlink()
        for path in folder.glob('*/node-*/credential'):
            path.unlink()
        (folder/'cleanup.json').write_text(json.dumps(dict(deleted=deleted,
            raw_remaining=len(list(folder.rglob('*.graw')))), indent=2)+'\n')
    for path in folder.rglob('*.log'):
        text = path.read_text(errors='replace')
        assert not any(marker in text for marker in ['ERROR: AddressSanitizer', 'ERROR: LeakSanitizer',
            'AddressSanitizer:DEADLYSIGNAL', 'runtime error:']), str(path)
    assert before == {name: sha(path) for name, path in programs.items()}
    (folder/'result.json').write_text(json.dumps(dict(passed=True, cases=results,
        program_sha256=before, runner_sha256=sha(Path(__file__)), sanitized=args.sanitized,
        raw_remaining=0, maximum_aggregate_test_bytes=64*1024**2, real_hardware=False), indent=2)+'\n')
    print('PASS two parent/real-node explicit retirement completion scenarios; raw removed:', folder)


if __name__ == '__main__':
    main()
