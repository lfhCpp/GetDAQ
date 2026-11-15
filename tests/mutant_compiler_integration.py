#!/usr/bin/env python3
"""Exercise real MuTaNT configurations, provenance and rejected compiler inputs."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def main():
    root = Path(__file__).resolve().parents[1]
    executable = Path(sys.argv[1]).resolve()
    original_dir = root / 'source/get/GetBench-software-release-20210208'
    production = original_dir / 'data/config/configure-ExternalTrigger-2CoBo_1Mutant.xcfg'
    old_test = original_dir / 'test/data/mutant-config.xcfg'
    description = original_dir / 'data/config/hardwareDescription_Mutant.xcfg'
    originals = [production, old_test, description]
    digests = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in originals}
    count = 0
    with tempfile.TemporaryDirectory(prefix='getdaq-mutant-cli-') as temporary:
        folder = Path(temporary)

        def run(source, instance='master', modules='2', gmc='0xb', wsca='7', layout='modern', policy='strict', success=True):
            nonlocal count
            output = folder / ('image-%d.json' % count)
            args = [str(executable), 'compile-mutant', str(source), instance, modules, gmc, wsca, layout, policy, str(output)]
            result = subprocess.run(args, capture_output=True, text=True, timeout=20)
            count += 1
            if not success:
                assert result.returncode != 0 and not output.exists(), result.stdout
                return result
            assert result.returncode == 0, result.stderr
            image = json.loads(output.read_text())
            assert image['kind'] == 'getdaq.mutant-image'
            assert not image['complete_prepare'] and not image['alignment_verified'] and not image['hardware_deployment_supported']
            before = output.read_bytes()
            again = subprocess.run(args, capture_output=True, timeout=20)
            assert again.returncode != 0 and output.read_bytes() == before
            return image

        def registers(image):
            return {(r['register'], r.get('field', '')): int(r['value']) for r in image['registers']}

        image = run(production)
        assert image['shelf_mask'] == 3 and image['sources'][0]['sha256'] == digests[production]
        assert registers(image)['GLOBAL_CONF', ''] == 0x411
        assert image['timings']['level0_duration']['code'] == 65534  # Original float multiplication/floor.
        assert not image['clock_source_fallback'] and registers(image)['TRIGGER_CTRL', 'read_mode'] == 0
        assert image['clock_path_update']['mask'] == 5 and image['clock_path_update']['value'] == 0
        image = run(production, layout='legacy-names')
        assert ('MASTER_MASK', '') in registers(image) and ('SHELF_MASK', '') not in registers(image)

        compiled_description = folder / 'description.json'
        result = subprocess.run([str(executable), 'compile-hardware', str(description), str(compiled_description)], capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, result.stderr
        actual = json.loads(compiled_description.read_text())
        fixture = json.loads((root / 'tests/data/mutant-hardware.json').read_text())
        assert actual['devices'] == fixture['devices'] and actual['node_id'] == fixture['node_id']
        assert fixture['sources'][0]['sha256'] == digests[description]

        base = ET.parse(old_test).getroot()
        included = folder / 'defaults.xcfg'
        source = folder / 'experiment.xcfg'
        source.write_text('<Setup><Node id="Mutant"><Instance id="*" include="defaults.xcfg"/><Instance id="master"/></Node></Setup>')

        def write(tree):
            tree.tag = 'Defaults'
            included.write_bytes(ET.tostring(tree, encoding='utf-8'))

        write(copy.deepcopy(base))
        run(source, success=False)  # Old optical names are not a current mapping.
        image = run(source, policy='legacy-vhdci')
        assert image['clock_source_fallback'] and image['clock_source_code'] == 2
        assert image['preload_timestamp'] and image['preload_event_number'] and image['clear_counters']
        assert len(image['sources']) == 2 and any(p.get('source_file') == str(included) for p in image['parameters'])
        assert any(p.get('default_from_code') and p['query_path'].endswith('/preLoadTimestamp') for p in image['parameters'])
        base.find('GlobalMasterClock/externalClockSelection').attrib.clear()
        base.find('GlobalMasterClock/externalClockSelection').text = 'VHDCI'
        base.find('Scalers/clearCounters').text = 'false'
        write(copy.deepcopy(base)); image = run(source)
        assert not image['clear_scalers'] and not image['clear_counters']
        for source_id, expected in [('0x10', 16), ('255', 255)]:
            tree = copy.deepcopy(base); tree.find('sourceId').text = source_id
            if source_id.startswith('0x'):
                tree.find('sourceId').set('type', 'hex')
                write(tree); run(source, success=False)  # HEX value must not silently enter inherited INTEGER range.
                tree.find('sourceId').set('range', '[0x0,0xff]')
            write(tree); assert registers(run(source))['SOURCE_SYSTEM', 'source'] == expected
        tree = copy.deepcopy(base)
        for name, value in [('low', '10'), ('mid', '1234'), ('high', 'ffff')]:
            tree.find('preLoadedTimestamp/' + name).text = value
            tree.find('preLoadedTimestamp/' + name).set('type', 'hex')
        tree.find('preLoadedEventNumber').text = '76543210'; tree.find('preLoadedEventNumber').set('type', 'hex')
        ET.SubElement(tree, 'vcxoControl', type='hex').text = '80'
        write(tree); image = run(source)
        assert int(image['timestamp_10ns']) == 0xffff12340010 and int(image['event_number']) == 0x76543210
        assert registers(image)['VCXO_CONTROL', ''] == 0x80
        tree.find('preLoadedTimestamp/high').attrib = {'range': '[0x0,0xffff]'}
        write(tree); run(source, success=False)  # Untyped ffff is not a legacy HEX value.
        tree.find('preLoadedTimestamp/high').text = '0xffff'
        write(tree); image = run(source)
        assert int(image['timestamp_10ns']) == 0xffff12340010
        tree.find('preLoadedTimestamp/low').attrib = {'type': 'int', 'range': '[0,15]'}
        write(tree); run(source, success=False)
        # Explicit hex is interpreted consistently for range validation and compilation.
        tree.find('preLoadedTimestamp/low').attrib = {'type': 'hex', 'range': '[0x0, 0xF]'}
        write(tree); run(source, success=False)
        tree = copy.deepcopy(base); tree.find('isSlave').text = 'true'
        for level in tree.findall('Level'): level.find('isEnabled').text = 'false'
        tree.find('Slave/slavePort').text = 'B'; tree.find('Slave/slaveId').text = '1'
        tree.find('GlobalMasterClock').remove(tree.find('GlobalMasterClock/externalClockEnabled'))
        write(tree); image = run(source)
        assert image['is_slave'] and registers(image)['GMC_WSCA_SEL', 'WSCA'] == 2
        assert image['external_clock_enabled'] and image['clock_path_update']['value'] == 5
        assert registers(image)['GLOBAL_CONF', ''] & 15 == 10
        tree.find('centrumScalerInterface').text = 'CENTRUM'; write(tree); run(source, success=False)
        tree = copy.deepcopy(base); tree.find('TwoProtonDecayMode/isEnabled').text = 'true'; tree.find('TwoProtonDecayMode/halfEventReadout').text = 'true'
        tree.find('Level[@id="1"]/L1B/timeout').text = '167769.6'; write(tree); image = run(source)
        assert image['timings']['level1b_timeout']['code'] == 65535 and registers(image)['GLOBAL_CONF', ''] & (1 << 12)
        tree.find('Level[@id="1"]/L1B/timeout').text = '167770'; write(tree); run(source, success=False)
        # Disabled L1 still configures inspection parameters; malformed active L1A must fail.
        mutations = [
            ('Level[@id="0"]/gateDelay', '-0.1', {}), ('Level[@id="0"]/gateDuration', '0', {}),
            ('Level[@id="0"]/gateDelay', '1', {'unit': 'ms'}), ('Level[@id="0"]/gateDelay', 'nan', {}),
            ('Level[@id="1"]/L1A/lowNumBuckets', '512', {}), ('Level[@id="1"]/L1A/lowNumBuckets', '0', {}),
            ('Level[@id="1"]/selection', 'INVALID', {}), ('sourceId', '256', {}),
            ('frameType', '4', {}), ('frameType', '3', {}), ('testModeEnabled', '1', {}), ('Scalers/clearCounters', '1', {}),
            ('Slave/slavePort', 'C', {}), ('preLoadedTimestamp/high', '0x10000', {}),
            ('preLoadedEventNumber', '0x100000000', {}), ('levelPriority', 'UNKNOWN', {}),
            ('centrumScalerInterface', 'Other', {}), ('Inspection/frontPanelInsp1', '256', {}),
        ]
        for path, value, attributes in mutations:
            tree = copy.deepcopy(base); node = tree.find(path); node.text = value; node.attrib.update(attributes)
            if path.startswith('Slave/'):
                tree.find('isSlave').text = 'true'
                for level in tree.findall('Level'): level.find('isEnabled').text = 'false'
            tree.find('Level[@id="1"]/isEnabled').text = 'false'
            write(tree); run(source, success=False)
        for field in ['preLoadTimestamp', 'preLoadEventNumber', 'readoutModeEnabled']:
            tree = copy.deepcopy(base); node = tree.find(field)
            if node is None: node = ET.SubElement(tree, field)
            node.text = 'false'; write(tree); image = run(source)
            assert not image[{'preLoadTimestamp': 'preload_timestamp', 'preLoadEventNumber': 'preload_event_number', 'readoutModeEnabled': 'readout_enabled'}[field]]
        tree = copy.deepcopy(base); tags = ET.SubElement(tree, 'AmcTagOnly'); ET.SubElement(tags, 'ATO', id='8').text = 'true'
        write(tree); image = run(source)
        assert image['amc_tag_only_mask'] == 128 and image['shelf_mask'] == 3 and image['final_gmc_output_mask'] == 139
        ET.SubElement(tags, 'ATO', id='13').text = 'true'; write(tree); run(source, success=False)
        write(copy.deepcopy(base))
        for kwargs in [{'modules': '0'}, {'modules': '02'}, {'modules': '4097'}, {'gmc': '0x1000'}, {'wsca': '-1'},
                       {'layout': 'auto'}, {'policy': 'auto'}, {'instance': '*'}, {'instance': 'missing'}]:
            run(source, success=False, **kwargs)
        source.write_text('<Setup><Node id="Mutant"><Instance id="*" include="defaults.xcfg"/></Node></Setup>')
        run(source, success=False)
    assert all(hashlib.sha256(p.read_bytes()).hexdigest() == digest for p, digest in digests.items())
    print('PASS MuTaNT compiler CLI: %d contracts; original sources unchanged, descriptor fixture, include/default provenance, units/masks/counters and no overwrite' % count)


if __name__ == '__main__':
    main()
