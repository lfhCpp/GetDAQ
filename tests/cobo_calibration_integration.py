#!/usr/bin/env python3
"""Validate offline calibration artifacts without channels/register configuration or hardware."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    root = Path(__file__).resolve().parents[1]
    executable = Path(sys.argv[1]).resolve()
    original_dir = root / 'source/get/GetBench-software-release-20210208/data/config'
    originals = [original_dir / name for name in ('configure-pedestals.xcfg', 'configure-ExternalTrigger-2CoBo_1Mutant.xcfg')]
    digests = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in originals}
    count = 0
    with tempfile.TemporaryDirectory(prefix='getdaq-calibration-cli-') as temporary:
        folder = Path(temporary)

        def run(source, instance='0', versions='2=0x201,5=0x202,15=0x203', layout='global', success=True):
            nonlocal count
            output = folder / ('image-%d.json' % count)
            arguments = [str(executable), 'compile-cobo-calibration', str(source), instance, versions, layout, str(output)]
            result = subprocess.run(arguments, capture_output=True, text=True, timeout=20)
            count += 1
            if not success:
                assert result.returncode != 0 and not output.exists(), result.stdout
                return
            assert result.returncode == 0, result.stderr
            image = json.loads(output.read_text())
            assert image['kind'] == 'getdaq.cobo-calibration-image'
            assert not image['complete_prepare'] and not image['hardware_deployment_supported'] and not image['physical_delay_verified']
            before = output.read_bytes()
            again = subprocess.run(arguments, capture_output=True, timeout=20)
            assert again.returncode != 0 and output.read_bytes() == before
            return image

        for source, instance in zip(originals, ('0', 'Crate00_Slot00')):
            for layout in ('global', 'per-asad'):
                image = run(source, instance, '0=0x202,1=0x202,2=0x202,3=0x203', layout)
                assert image['asad_mask'] == 1 and image['aget_mask'] == 15 and image['compute_automatic_read_delay']
                assert image['sources'][0]['sha256'] == digests[source]

        included = folder / 'boards.xcfg'
        source = folder / 'experiment.xcfg'
        source.write_text('<Setup><Node id="CoBo"><Instance id="0" include="boards.xcfg"><isActive>true</isActive></Instance></Node></Setup>')

        def boards(control='', module='', active_chips=(2, 5, 15)):
            return '<Boards>' + module + ''.join(
                '<AsAd id="%d"><Control><isActive>%s</isActive>%s</Control>%s</AsAd>' % (
                    id_, 'false' if id_ == 2 else 'true', control if id_ == 0 else '',
                    ''.join('<Aget id="%d"><Control><isActive>%s</isActive></Control></Aget>' %
                            (chip, 'true' if 4*id_+chip in active_chips else 'false') for chip in range(4)))
                for id_ in range(4)) + '</Boards>'

        included.write_text(boards()); image = run(source)
        assert image['asad_mask'] == 11 and image['aget_mask'] == 0x8024 and image['calibration_asad_mask'] == 11
        assert image['first_calibration_global_chip'] == 2 and image['automatic_read_delay_mask'] == 0
        assert [b['calibration_aget_id'] for b in image['boards']] == [2, 1, 3]
        assert len(image['sources']) == 2 and any(p.get('source_file') == str(included) for p in image['parameters'])
        assert any(p.get('default_from_code') and p['query_path'].endswith('/calibrationAgetId') for p in image['parameters'])
        module = '<Module><calibrateAutoReadDataDelay>false</calibrateAutoReadDataDelay><enableAutoReadDataDelay>true</enableAutoReadDataDelay></Module>'
        included.write_text(boards(module=module)); image = run(source)
        assert not image['compute_automatic_read_delay'] and image['automatic_read_delay_mask'] == 11
        assert all(b['delay_policy_path'].endswith('/Module/enableAutoReadDataDelay') for b in image['boards'])
        override = '<enableAutoReadDataDelay>false</enableAutoReadDataDelay>'
        included.write_text(boards(override, module)); run(source, success=False)
        image = run(source, layout='per-asad'); assert image['automatic_read_delay_mask'] == 10
        assert image['boards'][0]['delay_policy_path'].endswith('/AsAd[0]/Control/enableAutoReadDataDelay')
        included.write_text(boards(active_chips=(2,))); image = run(source, versions='2=0x201')
        assert image['calibration_asad_mask'] == 1 and image['boards'][1]['calibration_aget_id'] is None
        included.write_text(boards('<calibrationAgetId>1</calibrationAgetId>', active_chips=(1, 2, 5, 15)))
        image = run(source, versions='1=0x201,2=0x201,5=0x202,15=0x203'); assert image['boards'][0]['calibration_aget_id'] == 1
        for control in (
            '<calibrationAgetId>1</calibrationAgetId>', '<calibrationAgetId>4</calibrationAgetId>',
            '<calibrationAgetId>-1</calibrationAgetId>', '<calibrationAgetId>02</calibrationAgetId>',
            '<calibrationAgetId unit="chips">2</calibrationAgetId>', '<calibrationAgetId range="[0,1]">2</calibrationAgetId>',
            '<calibrationAgetId class="unresolved"/>', '<enableAutoReadDataDelay>1</enableAutoReadDataDelay>',
        ):
            included.write_text(boards(control)); run(source, success=False)
        included.write_text(boards())
        for versions in ('', '2=0x201', '2=0x201,5=0x202,15=0x203,0=0x202',
                         '2=0x204,5=0x202,15=0x203', '2=0x201,2=0x201', '02=0x201', '16=0x202'):
            run(source, versions=versions, success=False)
        for layout in ('', 'GLOBAL', 'auto'):
            run(source, layout=layout, success=False)
        included.write_text(boards().replace('<AsAd id="3">', '<AsAd id="4">')); run(source, success=False)
        included.write_text(boards().replace('<Aget id="3">', '<Aget id="4">')); run(source, success=False)
        included.write_text(boards(module='<Module><calibrateAutoReadDataDelay>1</calibrateAutoReadDataDelay></Module>')); run(source, success=False)
        included.write_text(boards()); run(source, instance='*', success=False)
        run(source, instance='missing', success=False)
    assert all(hashlib.sha256(p.read_bytes()).hexdigest() == digest for p, digest in digests.items())
    print('PASS CoBo calibration CLI: %d contracts; original files unchanged, include/default provenance, coverage, delay policies and no output overwrite' % count)


if __name__ == '__main__':
    main()
