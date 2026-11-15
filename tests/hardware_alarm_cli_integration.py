#!/usr/bin/env python3
"""Native process/callback integration; only a loopback fixture, no actual boards."""
import json
from pathlib import Path
import select
import subprocess
import sys
import tempfile


def main():
    client, fixture = (str(Path(p).resolve()) for p in sys.argv[1:])
    server = subprocess.Popen([fixture, '--serve'], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    with tempfile.TemporaryDirectory(prefix='getdaq-alarm-cli-') as temporary:
        folder = Path(temporary)
        try:
            assert select.select([server.stdout], [], [], 10)[0], 'alarm fixture did not listen'
            address = server.stdout.readline().strip()
            serial = 0

            def arguments(minor, seconds='1', identity='CoBo0'):
                nonlocal serial
                paths = [folder / ('%d-%s.jsonl' % (serial, name)) for name in ('event', 'subscribe', 'unsubscribe')]
                serial += 1
                return [client, '--encoding', '1.'+str(minor), 'watch-asad-alarms', address, identity,
                        'tcp -h 127.0.0.1 -p 0', seconds, *map(str, paths)], paths

            def status(command='status'):
                server.stdin.write(command+'\n'); server.stdin.flush()
                assert select.select([server.stdout], [], [], 10)[0], 'fixture status timeout'
                return json.loads(server.stdout.readline())

            for minor in (0, 1):
                args, paths = arguments(minor)
                quiet = subprocess.run(args, capture_output=True, text=True, timeout=10)
                assert quiet.returncode == 0, quiet.stderr
                final = json.loads(quiet.stdout.splitlines()[-1])
                assert final['subscription'] == 'closed' and final['received'] == '0'
                assert not final['callback_health_verified'] and not final['hardware_stop_performed']
                assert status()['subscribers'] == 1
                args, paths = arguments(minor, seconds='10')
                watching = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                try:
                    assert select.select([watching.stdout], [], [], 10)[0], 'watch command failed to subscribe'
                    ready = json.loads(watching.stdout.readline())
                    assert ready['subscription'] == 'active', ready
                    assert status('alarm')['resets'] == 0
                    output, errors = watching.communicate(timeout=10)
                    assert watching.returncode == 3, (output, errors)
                    rows = [json.loads(line) for line in output.splitlines()]
                    assert rows[0]['phase'] == 'alarm' and rows[0]['alarm_bits'] == 8
                    assert rows[-1]['subscription'] == 'closed' and rows[-1]['stop_requested']
                    assert rows[-1]['received'] == '1' and rows[-1]['durable'] == '1'
                    assert not rows[-1]['hardware_stop_performed']
                    events = [json.loads(line) for line in paths[0].read_text().splitlines()]
                    assert any(row['phase'] == 'alarm' and row['alarm_bits'] == '8' for row in events)
                    for path in paths[1:]:
                        assert [row['phase'] for row in map(json.loads, path.read_text().splitlines())] == ['intent', 'begin', 'acknowledged']
                    assert status()['subscribers'] == 1
                finally:
                    if watching.poll() is None:
                        watching.kill(); watching.communicate()

            before = status()['subscribes']
            for seconds in ('0', '-1', '86401', 'abc'):
                args, paths = arguments(0, seconds)
                result = subprocess.run(args, capture_output=True, timeout=10)
                assert result.returncode == 2 and not any(p.exists() for p in paths)
            args, paths = arguments(0, identity='wrong')
            result = subprocess.run(args, capture_output=True, text=True, timeout=10)
            assert result.returncode == 1 and 'name differs' in result.stderr
            assert json.loads(result.stdout)['subscription'] == 'fresh'
            args, paths = arguments(0); args[-1] = args[-2]
            result = subprocess.run(args, capture_output=True, timeout=10)
            assert result.returncode == 2 and not any(p.exists() for p in paths)
            final = status()
            assert final['subscribes'] == before and final['resets'] == 0 and final['subscribers'] == 1
            print('PASS alarm CLI: both encodings, quiet health uncertainty, alarm exit 3, durable audits, owner-only close and preflight rejection')
        finally:
            if server.poll() is None:
                try:
                    server.communicate('quit\n', timeout=5)
                except subprocess.TimeoutExpired:
                    server.kill(); server.communicate()
            assert server.returncode == 0, 'alarm fixture failed'


if __name__ == '__main__':
    main()
