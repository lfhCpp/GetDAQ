#!/usr/bin/env python3
"""Offline CLI contract against received files and minimal include configurations."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    root = Path(__file__).resolve().parents[1]
    executable = Path(sys.argv[1]).resolve()
    originals = [root / 'source/get/GetBench-software-release-20210208/data/config' / name for name in
                 ('configure-pedestals.xcfg', 'configure-ExternalTrigger-2CoBo_1Mutant.xcfg')]
    digests = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in originals}
    cases = 0
    with tempfile.TemporaryDirectory(prefix='getdaq-monitoring-cli-') as temporary:
        folder = Path(temporary)

        def run(source, instance='0', mapping='declared', success=True):
            nonlocal cases
            output = folder / ('image-' + str(cases) + '.json')
            result = subprocess.run([str(executable), 'compile-cobo-monitoring', str(source), instance, mapping, str(output)],
                                    capture_output=True, text=True, timeout=20)
            cases += 1
            if success:
                assert result.returncode == 0, result.stderr
                image = json.loads(output.read_text())
                assert image['kind'] == 'getdaq.cobo-monitoring-image'
                assert not image['complete_prepare'] and not image['hardware_deployment_supported']
                assert not image['host_alarm_subscription_supported'] and not image['led_manager_supported']
                assert image['native_led_manager_available'] and image['native_alarm_subscription_available']
                before = output.read_bytes()
                again = subprocess.run([str(executable), 'compile-cobo-monitoring', str(source), instance, mapping, str(output)], capture_output=True, timeout=20)
                assert again.returncode != 0 and output.read_bytes() == before
                return image
            assert result.returncode != 0 and not output.exists(), result.stdout

        for source, instance in zip(originals, ('0', 'Crate00_Slot00')):
            for mode in ('declared', 'legacy'):
                image = run(source, instance, mode)
                assert image['mode_mapping'] == mode
                assert image['boards'][0]['limits']['VDD']['lower_code'] == '204'
                assert image['boards'][0]['vdd_settle_minimum_raw'] == '205'
                assert image['sources'][0]['sha256'] == digests[source]

        monitor = '''<Monitoring><isAlarmSet>true</isAlarmSet><singleChannelItem>IDD</singleChannelItem>
<fastConversionEnabled>true</fastConversionEnabled><singleChannelModeEnabled>true</singleChannelModeEnabled>
<LimitsText><lowerValue type="int" unit="C">-10</lowerValue><upperValue unit="C">50</upperValue></LimitsText>
<LimitsVDD><lowerValue type="real" unit="V">3</lowerValue><upperValue unit="V">3.5</upperValue></LimitsVDD>
<LimitsIDD><lowerValue unit="A">0</lowerValue><upperValue unit="A">2</upperValue></LimitsIDD></Monitoring>'''
        included = folder / 'boards.xcfg'
        def boards(fragment):
            return '<Boards>' + ''.join('<AsAd id="%d"><Control><isActive>%s</isActive></Control>%s</AsAd>' %
                                       (id_, 'true' if id_ == 0 else 'false', fragment if id_ == 0 else '') for id_ in range(4)) + '</Boards>'
        included.write_text(boards(monitor))
        source = folder / 'experiment.xcfg'
        source.write_text('<Setup><Node id="CoBo"><Instance id="0" include="boards.xcfg"><isActive>true</isActive></Instance></Node></Setup>')
        declared, legacy = run(source), run(source, mapping='legacy')
        assert len(declared['sources']) == 2
        assert declared['boards'][0]['configuration2'] == '21' and declared['boards'][0]['configuration3'] == '1'
        assert legacy['boards'][0]['configuration2'] == '49' and legacy['boards'][0]['configuration3'] == '0'
        assert declared['boards'][0]['limits']['Text']['lower_code'] == '246'
        assert any(p.get('default_from_code') and p['query_path'].endswith('/checkPowerSupply') for p in declared['parameters'])
        assert any(p.get('source_file') == str(included) and p['line'] > 0 for p in declared['parameters'])
        assert declared['alarm_policy'] == {'check_asad': True, 'subscribe': True}
        assert any(p.get('default_from_code') and p['query_path'].endswith('/Alarms/subscribe') for p in declared['parameters'])
        original_text = source.read_text()
        source.write_text(original_text.replace('<isActive>true</isActive>', '<isActive>true</isActive><Alarms><checkAsAd type="bool">false</checkAsAd><subscribe>false</subscribe></Alarms>'))
        assert run(source)['alarm_policy'] == {'check_asad': False, 'subscribe': False}
        for fragment in ('<checkAsAd>1</checkAsAd>', '<subscribe>yes</subscribe>', '<subscribe type="int">1</subscribe>', '<subscribe unit="V">true</subscribe>'):
            source.write_text(original_text.replace('<isActive>true</isActive>', '<isActive>true</isActive><Alarms>'+fragment+'</Alarms>'))
            run(source, success=False)
        source.write_text(original_text)
        # Old flat fields use the old wait function's independent default of 3.2V.
        flat = monitor.replace('<LimitsVDD><lowerValue type="real" unit="V">3</lowerValue><upperValue unit="V">3.5</upperValue></LimitsVDD>',
                               '<VddLowLimit unit="V">3</VddLowLimit><VddHighLimit unit="V">3.5</VddHighLimit>')
        included.write_text(boards(flat)); image = run(source)
        assert image['boards'][0]['vdd_settle_lower_volts'] == 3.2 and image['boards'][0]['vdd_settle_minimum_raw'] == '218'
        disabled = monitor.replace('<isAlarmSet>true</isAlarmSet>', '<isAlarmSet>false</isAlarmSet>').replace('</LimitsIDD>', '<disableAlerts>not-a-bool</disableAlerts></LimitsIDD>')
        included.write_text(boards(disabled)); image = run(source)
        assert image['boards'][0]['protection_mask1'] == '255' and image['boards'][0]['protection_configuration1'] == '45'
        assert not any(p['query_path'].endswith('/disableAlerts') for p in image['parameters'])
        for bad in (
            monitor.replace('unit="V">3.5', 'unit="mV">3.5'),
            monitor.replace('type="real" unit="V">3', 'type="real" unit="V">3e999'),
            monitor.replace('unit="V">3.5', 'unit="V">2.5'),
            monitor.replace('unit="A">2', 'unit="A">4'),
            monitor.replace('unit="C">50', 'unit="C">128'),
            monitor.replace('type="int" unit="C">-10', 'type="int" unit="C">-129'),
            monitor.replace('<singleChannelItem>IDD', '<singleChannelItem>unknown'),
            monitor.replace('<singleChannelModeEnabled>true', '<singleChannelModeEnabled>1'),
            monitor.replace('<lowerValue type="real" unit="V">3</lowerValue>', ''),
            monitor.replace('<upperValue unit="V">3.5', '<upperValue unit="V" range="[0,3]">3.5'),
            monitor.replace('<singleChannelItem>IDD</singleChannelItem>', '<singleChannelItem class="unresolved"/>'),
        ):
            included.write_text(boards(bad)); run(source, success=False)
        included.write_text(boards(monitor))
        for mode in ('', 'DECLARED', 'automatic'):
            run(source, mapping=mode, success=False)
        for instance in ('*', 'missing', 'a/b'):
            run(source, instance=instance, success=False)
        included.write_text(boards(monitor).replace('<isActive>true</isActive>', '<isActive>false</isActive>')); run(source, success=False)
    assert all(hashlib.sha256(p.read_bytes()).hexdigest() == h for p, h in digests.items())
    print('PASS monitoring CLI: %d contracts, original hashes unchanged, includes/defaults/mapping/limits and no-overwrite failures' % cases)


if __name__ == '__main__':
    main()
