#!/usr/bin/env python3
"""Execute independent old CCfg and current production-reader probes on small files.

Compare live results, and assert deliberately enumerated migration differences.
No hardware, network, raw DAQ files, or inferred whole-library compatibility.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
from xml.sax.saxutils import escape, quoteattr


TYPES = {'integer': 'int', 'hexadecimal': 'hex', 'real number': 'real',
         'boolean': 'bool', 'string': 'string'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('legacy', type=Path)
    parser.add_argument('current', type=Path)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    legacy, current = args.legacy.resolve(), args.current.resolve()
    args.evidence.mkdir(parents=True, exist_ok=True)
    traces = []
    cases = []

    def scalar(name, value, kind='', bounds='', expected='equal', markup=False):
        attributes = (' type=' + quoteattr(kind) if kind else '')
        attributes += (' range=' + quoteattr(bounds) if bounds else '')
        xml = '<Root><x' + attributes + '>' + (value if markup else escape(value)) + '</x></Root>'
        cases.append({'name': name, 'files': {'main.xcfg': xml}, 'queries': ['x'], 'expected': expected})

    for i, value in enumerate(['0', '+01', '-0002', '-2147483648', '2147483647',
                               '0x0', '0xFF', '0xffffffff', 'TRUE', 'False', 'true',
                               '1.0', '.5', '1.', '1e-3', '-0.0', 'hello world',
                               '"quoted"', 'a\\nline', '0x', 'nan', '']):
        scalar('auto-%02d' % i, value)
    for kind, values in [('int', ['0', '+002', '-2147483648', '2147483647']),
                         ('hex', ['0', '10', '0x10', '0X10', 'ffffffff']),
                         ('real', ['0', '1', '-0.0', '1.5', '1e-3', '1e-999']),
                         ('bool', ['true', 'false']),
                         ('string', ['', '001', 'true', '中文', 'a&b<z>', '"quoted"'])]:
        for i, value in enumerate(values):
            scalar('%s-explicit-%02d' % (kind, i), value, kind)
    for name, markup in [('literal', '  a   b\t c\n d  '),
                         ('references', '&#32;&#32;a&#32;&#32;b&#10;c&#32;'),
                         ('cdata', '<![CDATA[  a   b\t c\n d  ]]>'),
                         ('unicode', '\u00a0a\u00a0\u00a0b\u00a0'),
                         ('comment-tail', ' a   b <!-- keep -->'),
                         ('comment-head', '<!-- keep --> a   b '),
                         ('literal-reference', ' a &#32; b ')]:
        scalar('xml-space-' + name, markup, 'string', markup=True)

    # Original int32/uint32 values; vary boundaries, open/closed, and item order.
    ranges = ['[0,10]', ']0,10[', '[0,10[', ']0,10]', '[-inf,10]',
              '[0,+inf]', '[-inf,+inf]', '{3,1,2,2}', '{}', '{-2,0,2,10}']
    for r, bounds in enumerate(ranges):
        for value in [-2, -1, 0, 1, 2, 3, 9, 10, 11]:
            scalar('int-range-%02d-%d' % (r, value), str(value), 'int', bounds)
    for kind, bounds, values in [
            ('hex', '[0x0,0xff]', ['0', '1', 'ff', '100']),
            ('hex', '{0xff,0x0,0xff}', ['0', '1', 'ff']),
            ('real', '[0.0,1.0]', ['-1.0', '0.0', '0.5', '1.0', '2.0']),
            ('bool', '{true,false,true}', ['true', 'false']),
            ('string', '{"fast, slow","a\\\"b","a\\nline","\\x41\\101"}',
             ['fast, slow', 'a"b', 'a\nline', 'AA', 'absent']),
            ('string', '["a","z"]', ['a', 'beta', 'z', 'zz'])]:
        for i, value in enumerate(values):
            # Character references keep a newline significant to the string range.
            scalar('typed-range-%s-%s-%d' % (kind, len(cases), i),
                   escape(value).replace('\n', '&#10;'), kind, bounds, markup=True)

    # Explicit differences: never silently count them as old-compatible cases.
    for name, value, kind in [('signed32-extension', '2147483648', 'int'),
                               ('unsigned32-extension', '100000000', 'hex'),
                               ('auto-signed32-extension', '2147483648', ''),
                               ('auto-unsigned32-extension', '0x100000000', ''),
                               ('auto-upper-prefix-extension', '0XFF', ''),
                               ('typed-bool-case-extension', 'TRUE', 'bool')]:
        scalar(name, value, kind, expected='documented-extension')
    for name, value, kind, bounds, markup in [
            ('negative-hex', '-1', 'hex', '', False),
            ('nonfinite-auto-real', '1e309', '', '', False),
            ('range-trailing-enum', '2', 'int', '{1,2}tail', False),
            ('range-trailing-interval', '2', 'int', '[0,10]tail', False),
            ('range-incomplete', '2', 'int', '{1,}', False)]:
        scalar(name, value, kind, bounds, expected='strict-rejection', markup=markup)
    # Legacy typed visitors kept the final text token. The new reader preserves
    # that value and refuses unsafe in-place editing of the split source.
    scalar('typed-split-text', 'a<!-- comment -->b', 'string', '', expected='equal', markup=True)
    for name, value, kind, bounds in [
            ('bad-int', '1.0', 'int', ''), ('bad-bool', '0', 'bool', ''),
            ('bad-real', 'nan', 'real', ''), ('real-overflow', '1e309', 'real', ''),
            ('mixed-enum', '2', 'int', '{1,1.0}')]:
        scalar(name, value, kind, bounds, expected='both-reject')

    cases += [
        {'name': 'multilevel-default-backtrack', 'files': {'main.xcfg':
         '<Root><A id="*"><B id="*"><x>1</x><y>2</y></B><B id="0"><x>3</x></B></A>'
         '<A id="0"><B id="1"><y>4</y></B></A></Root>'},
         'queries': ['A[0].B[1].x', 'A[0].B[1].y', 'A[0].B[0].x', 'A[0].B[0].y',
                     'A[1].B[0].x', 'A[0].B[9].x'], 'expected': 'equal'},
        {'name': 'shared-class-unit-range', 'files': {'main.xcfg':
         '<Root><Board id="*"><gain type="int" unit="V" range="[0,9]">2</gain></Board>'
         '<Board id="0"><gain unit="mV">3</gain></Board><Board id="1"/></Root>'},
         'queries': ['Board[0].gain', 'Board[1].gain'], 'expected': 'equal'},
        {'name': 'nested-include-context', 'files': {
         'main.xcfg': '<Root><Board id="*" include="sub/template.xcfg"/><Board id="0"/></Root>',
         'sub/template.xcfg': '<Template><gain type="int" unit="mV" range="[0,9]">4</gain>'
                              '<module include="nested.xcfg"/></Template>',
         'sub/nested.xcfg': '<Other><enabled>true</enabled></Other>'},
         'queries': ['Board[0].gain', 'Board[0].module.enabled'], 'expected': 'equal'},
        {'name': 'include-local-children', 'files': {
         'main.xcfg': '<Root><Board id="0" include="template.xcfg"><local>7</local></Board></Root>',
         'template.xcfg': '<Other><included>3</included></Other>'},
         'queries': ['Board[0].local', 'Board[0].included'], 'expected': 'equal'},
        {'name': 'include-cycle', 'files': {
         'main.xcfg': '<Root><Board include="other.xcfg"/></Root>',
         'other.xcfg': '<Other><nested include="main.xcfg"/></Other>'},
         'queries': ['Board'], 'expected': 'both-reject'},
        {'name': 'missing-include', 'files': {'main.xcfg': '<Root><Board include="missing.xcfg"/></Root>'},
         'queries': ['Board'], 'expected': 'both-reject'},
        {'name': 'equivalent-imported-class-range', 'files': {
         'main.xcfg': '<Root><Board id="*"><gain unit="mV" range="{1,2,3}">2</gain></Board>'
                      '<Board id="0" include="template.xcfg"/></Root>',
         'template.xcfg': '<Other><gain unit="mV" range="{3,1,2,2}">3</gain></Other>'},
         'queries': ['Board[0].gain'], 'expected': 'equal'},
        {'name': 'conflicting-imported-class-range', 'files': {
         'main.xcfg': '<Root><Board id="*"><gain range="[0,9]">2</gain></Board>'
                      '<Board id="0" include="template.xcfg"/></Root>',
         'template.xcfg': '<Other><gain range="[0,10]">3</gain></Other>'},
         'queries': ['Board[0].gain'], 'expected': 'both-reject'},
        {'name': 'duplicate-declaration', 'files': {
         'main.xcfg': '<Root><Board id="0"><x>1</x><x>2</x></Board></Root>'},
         'queries': ['Board[0].x'], 'expected': 'strict-rejection'},
        {'name': 'local-include-collision', 'files': {
         'main.xcfg': '<Root><Board id="0" include="template.xcfg"><x>1</x></Board></Root>',
         'template.xcfg': '<Other><x>2</x></Other>'},
         'queries': ['Board[0].x'], 'expected': 'strict-rejection'},
    ]

    def execute(program, path, queries):
        result = subprocess.run([str(program), str(path)] + queries, capture_output=True, text=True, timeout=10)
        assert result.returncode in (0, 1), (program, result.returncode, result.stderr)
        answer = json.loads(result.stdout)
        assert answer['loaded'] == (result.returncode == 0), (program, result.returncode, answer, result.stderr)
        return {'returncode': result.returncode, 'answer': answer, 'stderr': result.stderr}

    def normalize(answer):
        if not answer['loaded']:
            return [{'ok': False}]
        results = []
        for q in answer['queries']:
            if not q['ok']:
                results.append({'ok': False})
                continue
            item = {k: q[k] for k in ('ok', 'object')}
            if not q['object']:
                kind = TYPES.get(q['type'], q['type'])
                value = q['value']
                if kind == 'hex':
                    value = hex(int(value, 16) if value.lower().startswith('0x') else int(value))
                elif kind == 'real':
                    value = float(value)
                    if value == 0:
                        value = 0.0
                item.update(type=kind, value=value, unit=q['unit'], in_range=q['in_range'])
            results.append(item)
        return results

    try:
        with tempfile.TemporaryDirectory(prefix='getdaq-legacy-configuration-') as directory:
            work = Path(directory)
            for index, case in enumerate(cases):
                fixture = work / str(index)
                fixture.mkdir()
                before = {}
                for name, text in case['files'].items():
                    path = fixture / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text(text, encoding='utf-8')
                    before[path] = path.read_bytes()
                assert sum(len(b) for b in before.values()) <= 1024 * 1024
                old = execute(legacy, fixture / 'main.xcfg', case['queries'])
                new = execute(current, fixture / 'main.xcfg', ['Root/' + q.replace('.', '/') for q in case['queries']])
                old_values, new_values = normalize(old['answer']), normalize(new['answer'])
                record = dict(case, legacy=old, current=new, legacy_normalized=old_values, current_normalized=new_values)
                traces.append(record)
                expected = case['expected']
                old_ok, new_ok = all(q['ok'] for q in old_values), all(q['ok'] for q in new_values)
                if expected == 'equal':
                    assert old_values == new_values, record
                elif expected == 'both-reject':
                    assert not old_ok and not new_ok, record
                elif expected == 'strict-rejection':
                    assert old_ok and not new_ok, record
                elif expected == 'documented-extension':
                    assert new_ok and old_values != new_values, record
                else:
                    raise AssertionError(expected)
                assert all(p.read_bytes() == content for p, content in before.items()), 'Probe mutated reference input'
    finally:
        (args.evidence / 'processes.json').write_text(json.dumps(traces, indent=2, ensure_ascii=False) + '\n')
    counts = Counter(case['expected'] for case in cases)
    scope = {'cases': len(cases), 'processes': 2 * len(cases), 'categories': dict(counts),
             'legacy_program_sha256': hashlib.sha256(legacy.read_bytes()).hexdigest(),
             'current_program_sha256': hashlib.sha256(current.read_bytes()).hexdigest(),
             'legacy_original_library_executed': True, 'legacy_binary_database_network_tested': False,
             'temporary_configs_deleted': True, 'raw_data_transfer': False,
             'platform': 'local environment only', 'whole_library_compatibility_proven': False}
    (args.evidence / 'scope.json').write_text(json.dumps(scope, indent=2) + '\n')
    print('PASS live original CCfg/Spirit versus current: %d cases, %d processes; %s; temporary configs deleted' %
          (len(cases), 2 * len(cases), dict(counts)))


if __name__ == '__main__':
    main()
