#!/usr/bin/env python3
"""Exercise the offline CLI against supplied GET files and malformed experiment settings."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[1]
    configs = root / 'source/get/GetBench-software-release-20210208/data/config'
    pedestal = configs / 'configure-pedestals.xcfg'
    original = pedestal.read_text()
    original_bytes = pedestal.read_bytes()
    with tempfile.TemporaryDirectory(prefix='getdaq-aget-') as temporary:
        directory = Path(temporary)

        def run(source, output, cobo='0', asad='0', aget='0', version='0x202', expected=0):
            result = subprocess.run([str(binary), 'compile-aget', str(source), cobo, asad, aget, version, str(output)],
                                    capture_output=True, text=True, timeout=15)
            assert result.returncode == expected, (result.args, result.returncode, result.stdout, result.stderr)
            return result

        output = directory / 'pedestal.json'
        run(pedestal, output)
        image = json.loads(output.read_text())
        values = {reg['name']: int(reg['value']) for reg in image['registers']}
        expected = {'reg1': 0x283ac079, 'reg2': 0x28a187, 'reg3': 0, 'reg4': 0,
                    'reg6': 0xaaaaaaaaaaaaaaaa, 'reg7': 0xaaaaaaaaaaaaaaaa,
                    'reg8': 0xffffffffffffffff, 'reg8msb': 0xffffffffffffffff,
                    'reg9': 0xffffffffffffffff, 'reg9msb': 0xffffffffffffffff,
                    'reg10': 0, 'reg11': 0, 'reg12': 0}
        assert values == expected, values
        assert image['kind'] == 'getdaq.aget-register-image' and image['schema_version'] == 1
        assert image['hardware_deployment_supported'] is False
        assert image['register_stage_depth'] == image['module_readout_depth'] == 512
        assert image['sources'][0]['sha256'] == hashlib.sha256(original_bytes).hexdigest()
        data = output.read_bytes()
        run(pedestal, output, expected=1)
        assert output.read_bytes() == data, 'existing artifact was changed'
        for index, (asad, aget) in enumerate([('0', '1'), ('0', '2'), ('0', '3')]):
            path = directory / f'other-chip-{index}.json'
            run(pedestal, path, asad=asad, aget=aget)
            assert {r['name']: int(r['value']) for r in json.loads(path.read_text())['registers']} == expected
        external = configs / 'configure-ExternalTrigger-2CoBo_1Mutant.xcfg'
        run(external, directory / 'external.json', cobo='Crate00_Slot00')
        for cobo, asad, aget, version, code in [('missing', '0', '0', '0x202', 1), ('*', '0', '0', '0x202', 1),
                ('0', '4', '0', '0x202', 2), ('0', '0', '04', '0x202', 2), ('0', '0', '0', '0x999', 2),
                ('0', '0', '0', '0x203', 1), ('0', '3', '0', '0x202', 1)]:
            path = directory / 'rejected.json'
            run(pedestal, path, cobo, asad, aget, version, code)
            assert not path.exists()
        run(external, directory / 'inactive.json', cobo='Crate00_Slot01', expected=1)
        assert not (directory / 'inactive.json').exists()

        def override(name, content, expected_code=0, expected_error=None, module=''):
            path = directory / f'{name}.xcfg'
            selected = '<Instance id="0">' + module + '<AsAd id="0"><Aget id="0">' + content + '</Aget></AsAd></Instance>'
            path.write_text(original.replace('<Instance id="0" />', selected), encoding='utf-8')
            output = directory / f'{name}.json'
            result = run(path, output, expected=expected_code)
            if expected_error:
                assert expected_error in result.stderr, result.stderr
            if expected_code:
                assert not output.exists(), 'failed compilation created an artifact'
                return None
            return json.loads(output.read_text())

        changed = override('override', '<Global><Reg1><peackingTime unit="ns">70</peackingTime></Reg1></Global>'
                           '<channel id="0"><Reading>always</Reading></channel>')
        assert changed['registers'][0]['hex'] == '0x283ac001'
        assert changed['readout_masks'][0]['read_always'] == '8'
        assert changed['readout_masks'][0]['read_if_hit'] == '7'
        query = next(p for p in changed['parameters'] if p['query_path'].endswith('/Global/Reg1/peackingTime'))
        assert 'Instance[0]/AsAd[0]/Aget[0]' in query['source_path']
        disabled = override('disabled-channel', '<channel id="0"><isActive>false</isActive><Gain>invalid</Gain></channel>')
        assert disabled['readout_masks'][0]['read_if_hit'] == '7'
        assert int(disabled['registers'][4]['value']) == 0x2aaaaaaaaaaaaaaa
        short = override('short-depth', '', module='<Module><readoutDepth>500</readoutDepth></Module>')
        assert short['register_stage_depth'] == 512 and short['module_readout_depth'] == 500
        assert any('overwrites' in w for w in short['warnings'])
        for name, contents, error, module in [
            ('threshold-overflow', '<Global><Reg1><GlobalThresholdValue>8</GlobalThresholdValue></Reg1></Global>', 'GlobalThresholdValue', ''),
            ('channel-overflow', '<channel id="0"><LSBThresholdValue>16</LSBThresholdValue></channel>', 'LSBThresholdValue', ''),
            ('extra-channel', '<channel id="68"/>', '68 concrete', ''),
            ('duplicate-channel', '<channel id="0"/><channel id="0"/>', 'Duplicate', ''),
            ('bad-unit', '<Global><Reg1><peackingTime unit="us">1014</peackingTime></Reg1></Global>', 'Unsupported unit', ''),
            ('bad-type', '<Global><Reg1><TestModeRange type="bool">120fF</TestModeRange></Reg1></Global>', 'Boolean', ''),
            ('class', '<Global><Reg1><TestModeRange class="shared">120fF</TestModeRange></Reg1></Global>', 'class', ''),
            ('range', '<Global><Reg1><GlobalThresholdValue range="[0, 3]">7</GlobalThresholdValue></Reg1></Global>', 'interval', ''),
            ('bad-veto', '<Global><Reg1><TriggerVetoOption>Undefined</TriggerVetoOption></Reg1></Global>', 'TriggerVetoOption', ''),
            ('offset-overflow', '<Global><ReadoutPointerOffset>1</ReadoutPointerOffset></Global>', 'readoutDepth', ''),
            ('two-pulse-mismatch', '', 'SCA_Splitting', '<Module><enableMem2pMode>true</enableMem2pMode><readoutDepth>256</readoutDepth></Module>'),
            ('false-active', '<Control><isActive>false</isActive></Control>', 'active', ''),
        ]:
            override(name, contents, expected_code=1, expected_error=error, module=module)

        # No implicit instances or channels from an uninstantiated '*' declaration.
        no_channels = directory / 'no-channels.xcfg'
        import re
        no_channels.write_text(re.sub(r'<channel id="[0-9]+"\s*/>', '', original), encoding='utf-8')
        run(no_channels, directory / 'no-channels.json', expected=1)
        assert not (directory / 'no-channels.json').exists()
        # Three optional old parameters have explicit, auditable code defaults.
        defaults = directory / 'old-defaults.xcfg'
        defaults.write_text(re.sub(r'<(inputDynamicRange_5pct|triggerLvdsThreshold|multiplicityLevel)\b[^>]*>[^<]*</\1>', '', original), encoding='utf-8')
        defaults_output = directory / 'old-defaults.json'
        run(defaults, defaults_output)
        entries = [p for p in json.loads(defaults_output.read_text())['parameters'] if p['default_from_code']]
        assert len(entries) == 3
        # Import at the AGET boundary; duplicate siblings across include are deliberately rejected.
        begin = original.index('<Aget id="*">')
        end = original.index('</Aget>', begin) + len('</Aget>')
        (directory / 'aget-defaults.xcfg').write_text(original[begin:end], encoding='utf-8')
        imported = directory / 'imported.xcfg'
        imported.write_text('<Setup><Node id="CoBo"><Instance id="0"><isActive>true</isActive>'
                            '<Module><enableMem2pMode>false</enableMem2pMode></Module><AsAd id="0"><Control><isActive>true</isActive></Control>'
                            '<Aget id="0" include="aget-defaults.xcfg"/></AsAd></Instance></Node></Setup>', encoding='utf-8')
        imported_output = directory / 'imported.json'
        run(imported, imported_output)
        imported_image = json.loads(imported_output.read_text())
        assert len(imported_image['sources']) == 2
        assert {r['name']: int(r['value']) for r in imported_image['registers']} == expected
        assert any(p['default_from_code'] and p['query_path'].endswith('/Module/readoutDepth') for p in imported_image['parameters'])
        def board(source, output, versions='0=0x202,1=0x202,2=0x202,3=0x202', expected_code=0):
            result = subprocess.run([str(binary), 'compile-cobo-aget', str(source), '0', versions, str(output)],
                                    capture_output=True, text=True, timeout=15)
            assert result.returncode == expected_code, (result.args, result.returncode, result.stderr)
            if expected_code:
                assert not output.exists()
                return None
            return json.loads(output.read_text())
        whole = board(pedestal, directory / 'whole.json')
        assert whole['kind'] == 'getdaq.cobo-aget-register-image' and whole['asad_mask'] == 1 and whole['aget_mask'] == 15
        assert whole['readout_depth'] == 512 and whole['readout_offset'] == 0 and whole['fpn_read'] is True
        assert len(whole['chips']) == 4 and all({r['name']: int(r['value']) for r in c['registers']} == expected for c in whole['chips'])
        for version_map, code in [('0=0x202',1), ('0=0x202,1=0x202,2=0x202,3=0x202,15=0x202',1),
                ('0=0x202,0=0x202',2), ('00=0x202',2), ('16=0x202',2), ('0=0x999',2), ('',2)]:
            board(pedestal, directory / 'bad-board.json', version_map, code)
        override('mixed-board', '<Global><Reg1><peackingTime>68</peackingTime></Reg1></Global>', expected_code=1, expected_error='peackingTime')
        mixed = directory / 'mixed-board.xcfg'
        # override() changes chip 0; compile the whole board with only that chip ASTRE.
        whole_mixed = board(mixed, directory / 'whole-mixed.json', '0=0x203,1=0x202,2=0x202,3=0x202')
        assert whole_mixed['chips'][0]['chip_version'] == '0x203'
        inconsistent = directory / 'inconsistent.xcfg'
        inconsistent.write_text(original.replace('<Instance id="0" />', '<Instance id="0"><Module><readoutDepth>500</readoutDepth></Module>'
            '<AsAd id="0"><Aget id="3"><Global><ReadoutPointerOffset>12</ReadoutPointerOffset></Global></Aget></AsAd></Instance>'), encoding='utf-8')
        board(inconsistent, directory / 'inconsistent.json', expected_code=1)
        assert pedestal.read_bytes() == original_bytes, 'source experiment was modified'
        help_result = subprocess.run([str(binary), '--help'], capture_output=True, text=True, timeout=15)
        assert help_result.returncode == 0 and 'compile-aget' in help_result.stderr
    print('PASS: single/whole-CoBo offline AGET CLI, golden registers, mixed versions, shared constraints, provenance and failure without output')


if __name__ == '__main__':
    main()
