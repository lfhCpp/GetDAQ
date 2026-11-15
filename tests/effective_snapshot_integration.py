#!/usr/bin/env python3
"""真实离线CLI与独立SHA-256协议参考；不连接ECC/硬件/receiver。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def field(value):
    data = value if isinstance(value, bytes) else str(value).encode('utf-8')
    return str(len(data)).encode('ascii') + b':' + data


def expected_digest():
    # 独立固定参考：按规范手工给出有效对象、值及范围，未调用C++生成期望。
    data = b'getdaq.effective-configuration:1\n' + field('Setup') + field(3)
    for path in ('Setup', 'Setup/Board[0]', 'Setup/Board[1]'):
        data += field(path)
    data += field(4)
    integer_range = b''.join(field(x) for x in ('interval', 'closed', 'int', '0', 'closed', 'int', '10'))
    hex_range = b''.join(field(x) for x in ('interval', 'closed', 'hex', '0x0', 'closed', 'hex', '0xffffffffffffffff'))
    for board in (0, 1):
        for name, kind, value, unit, bounds in [('gain', 'int', '2', 'mV', integer_range), ('mask', 'hex', '0xffffffffffffffff', '', hex_range)]:
            data += b''.join(field(x) for x in (f'Setup/Board[{board}]/{name}', kind, value, unit, bounds))
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('program', type=Path)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    program = args.program.resolve()
    args.evidence.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='effective-snapshot-', dir=args.evidence))
    traces = []

    def run(command, success=True):
        result = subprocess.run([str(program)] + list(map(str, command)), capture_output=True, text=True)
        traces.append({'args': list(map(str, command)), 'returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
        assert (result.returncode == 0) == success, traces[-1]

    source, before, after, review = [root / name for name in ('input.xcfg', 'before.json', 'after.json', 'review.json')]
    original = b'<Setup><Board id="*"><gain range="[0,10]" unit="V">02</gain><mask type="hex" range="[0x0,0xffffffffffffffff]">FFFFFFFFFFFFFFFF</mask></Board><Board id="0"/><Board id="1"><gain unit="mV">2</gain></Board></Setup>'
    source.write_bytes(original)
    run(['snapshot', source, before])
    snapshot = json.loads(before.read_bytes())
    assert snapshot['effective_sha256'] == expected_digest(), 'Independent semantic digest differs'
    assert len(snapshot['scalars']) == 4 and all('[*]' not in p['path'] for p in snapshot['scalars'])
    saved = before.read_bytes()
    run(['snapshot', source, before], False)
    assert before.read_bytes() == saved and source.read_bytes() == original
    source.write_bytes(original.replace(b'<Setup>', b'<Setup><!-- comment -->').replace(b'[0,10]', b'[0, 010]').replace(b'>02<', b'>2<'))
    run(['snapshot', source, after])
    run(['diff-snapshot', before, after, review])
    report = json.loads(review.read_bytes())
    assert not report['effective_changed'] and report['snapshot_changed'] and len(report['source_changes']) == 1
    assert report['scalar_changes'] and all('value' not in item['fields'] for item in report['scalar_changes'])
    saved_review = review.read_bytes()
    run(['diff-snapshot', before, after, review], False)
    assert review.read_bytes() == saved_review
    source.write_bytes(original.replace(b'>02<', b'>3<'))
    changed, changed_review = root / 'changed.json', root / 'changed-review.json'
    run(['snapshot', source, changed])
    run(['diff-snapshot', before, changed, changed_review])
    report = json.loads(changed_review.read_bytes())
    assert report['effective_changed'] and len(report['scalar_changes']) == 1
    assert report['scalar_changes'][0]['path'] == 'Setup/Board[0]/gain'
    for name, bad in [('digest', dict(snapshot, effective_sha256='0' * 64)), ('unknown', dict(snapshot, unexpected=True)), ('schema', dict(snapshot, schema_version=2))]:
        bad_file, absent = root / (name + '.json'), root / (name + '-review.json')
        bad_file.write_text(json.dumps(bad))
        run(['diff-snapshot', bad_file, after, absent], False)
        assert not absent.exists()
    duplicate, absent = root / 'duplicate.json', root / 'duplicate-review.json'
    duplicate.write_bytes(b'{"schema_version":1,' + saved.lstrip()[1:])
    run(['diff-snapshot', duplicate, after, absent], False)
    assert not absent.exists()
    source.write_bytes(original.replace(b'>02<', b'>11<'))
    absent = root / 'invalid.json'
    run(['snapshot', source, absent], False)
    assert not absent.exists()
    source.write_bytes(original)
    (root / 'processes.json').write_text(json.dumps(traces, indent=2) + '\n')
    (root / 'scope.json').write_text(json.dumps({'offline_only': True, 'hardware_fixture': False, 'raw_data_transfer': False, 'program_sha256': hashlib.sha256(program.read_bytes()).hexdigest(), 'expected_effective_sha256': expected_digest(), 'process_cases': len(traces)}, indent=2) + '\n')
    print(f'PASS effective snapshot CLI: {len(traces)} process cases; independent typed SHA-256, provenance, default-instance changes, tampering and no overwrite; evidence {root}')


if __name__ == '__main__':
    main()
