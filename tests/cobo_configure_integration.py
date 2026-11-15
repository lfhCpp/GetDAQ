#!/usr/bin/env python3
"""Compile the received configurations without connecting to any hardware."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    binary = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[1]
    source = root / 'source/get/GetBench-software-release-20210208/data/config/configure-pedestals.xcfg'
    original = source.read_text()
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    versions = '0=0x202,1=0x202,2=0x202,3=0x202'
    with tempfile.TemporaryDirectory(prefix='getdaq-configure-') as temp:
        directory = Path(temp)

        def run(path, output, expected=0, cobo='0'):
            result = subprocess.run([str(binary), 'compile-cobo-configure', str(path), cobo, versions, str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == expected, (result.returncode, result.stdout, result.stderr)
            if expected:
                assert not output.exists(), 'failure produced an artifact'
            return result

        output = directory / 'original.json'
        run(source, output)
        image = json.loads(output.read_text())
        assert image['kind'] == 'getdaq.cobo-configure-image' and image['hardware_deployment_supported'] is False
        assert image['asad_mask'] == 1 and len(image['asad_boards']) == 1
        assert image['asad_boards'][0]['inspection_value'] == '5'
        assert image['pulser']['voltage_mv'] == '-1000' and image['pulser']['dac'] == '15430'
        assert image['pulser']['default_dac'] == '8089' and image['pulser']['periodic'] is False
        assert image['initialize_2p'] is True and image['initialize_2p_delay_ms'] == 20
        assert image['readout']['full_readout'] is True and image['sources'][0]['sha256'] == digest
        existing = output.read_bytes()
        result = subprocess.run([str(binary), 'compile-cobo-configure', str(source), '0', versions, str(output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 1 and output.read_bytes() == existing

        def variant(name, declaration='', text=None, error=None):
            if text is None:
                text = original.replace('<Instance id="0" />', '<Instance id="0">' + declaration + '</Instance>')
            path = directory / (name + '.xcfg')
            path.write_text(text, encoding='utf-8')
            out = directory / (name + '.json')
            result = run(path, out, 1 if error else 0)
            if error:
                assert error in result.stderr, result.stderr
                return None
            return json.loads(out.read_text())

        custom = variant('custom', '<Generator><amplitudeStart type="real" unit="mV">-12.9</amplitudeStart>'
                         '<defaultVoltage>+12.9</defaultVoltage><externalTriggerMode>true</externalTriggerMode>'
                         '<periodicModeEnabled>true</periodicModeEnabled><period_ms unit="ms">10</period_ms>'
                         '<doublePulseEnabled>true</doublePulseEnabled><poissonModeEnabled>true</poissonModeEnabled>'
                         '<secondPulseAmplitude type="real">-500.0</secondPulseAmplitude></Generator>'
                         '<AsAd id="0"><Generator><isCalibMode>true</isCalibMode><injectValue>11pF</injectValue>'
                         '<triggerDelay unit="WCK periods">31</triggerDelay></Generator><InspectionLines>'
                         '<deviceToInspect>Multiplicity</deviceToInspect><Multiplicity><ISP1>CKO</ISP1>'
                         '<ISP2>DISCT3</ISP2></Multiplicity></InspectionLines></AsAd>')
        assert custom['pulser']['voltage_mv'] == '-12' and custom['pulser']['default_voltage_mv'] == '12'
        assert custom['pulser']['external'] and custom['pulser']['periodic'] and custom['pulser']['double_pulse'] and custom['pulser']['poisson']
        assert custom['asad_boards'][0]['inspection_value'] == '255'
        assert custom['asad_boards'][0]['capacitor_11pf'] and custom['asad_boards'][0]['trigger_delay_wck'] == 31
        entry = next(p for p in custom['parameters'] if p['query_path'].endswith('/Generator/amplitudeStart'))
        assert entry['value'] == '-12.9' and 'Instance[0]' in entry['source_path']

        # The old no-argument amplitude function explicitly fell back to AsAd0/Amplitude[0].
        legacy = re.sub(r'<amplitudeStart\b[^>]*>[^<]*</amplitudeStart>', '', original)
        legacy_image = variant('legacy-amplitude', text=legacy.replace('<Instance id="0" />',
                               '<Instance id="0"><AsAd id="0"><Generator><Amplitude id="0" unit="mV">-250</Amplitude></Generator></AsAd></Instance>'))
        assert legacy_image['pulser']['voltage_mv'] == '-250'
        assert any(p['query_path'].endswith('/AsAd[0]/Generator/Amplitude[0]') for p in legacy_image['parameters'])
        variant('global-wins', '<Generator><amplitudeStart type="real">-1000.0</amplitudeStart></Generator>'
                '<AsAd id="0"><Generator><Amplitude id="0" unit="bad">ignored</Amplitude></Generator></AsAd>')
        for name, declaration, error in [
            ('voltage', '<Generator><amplitudeStart type="real">1000.1</amplitudeStart></Generator>', 'amplitudeStart'),
            ('nan', '<Generator><defaultVoltage type="real">nan</defaultVoltage></Generator>', 'finite'),
            ('voltage-unit', '<Generator><defaultVoltage type="real" unit="V">1.0</defaultVoltage></Generator>', 'Unsupported unit'),
            ('period', '<Generator><period_ms>0</period_ms></Generator>', 'positive'),
            ('boolean', '<Generator><externalTriggerMode>yes</externalTriggerMode></Generator>', 'true or false'),
            ('delay', '<AsAd id="0"><Generator><triggerDelay>32</triggerDelay></Generator></AsAd>', 'triggerDelay'),
            ('capacitor', '<AsAd id="0"><Generator><injectValue>10nF</injectValue></Generator></AsAd>', 'injectValue'),
            ('isp', '<AsAd id="0"><InspectionLines><ADC><ISP1>CKO</ISP1></ADC></InspectionLines></AsAd>', 'ISP1'),
            ('class', '<Generator><amplitudeStart type="real" class="shared">-1000.0</amplitudeStart></Generator>', 'class'),
        ]:
            variant(name, declaration, error=error)
        external = source.parent / 'configure-ExternalTrigger-2CoBo_1Mutant.xcfg'
        external_digest = hashlib.sha256(external.read_bytes()).hexdigest()
        external_output = directory / 'external.json'
        run(external, external_output, cobo='Crate00_Slot00')
        assert json.loads(external_output.read_text())['sources'][0]['sha256'] == external_digest
        assert hashlib.sha256(source.read_bytes()).hexdigest() == digest
        assert hashlib.sha256(external.read_bytes()).hexdigest() == external_digest
    print('PASS: CoBo Configure CLI, original configurations, AsAd inspection/pulser, legacy amplitude, units/provenance and failure isolation')


if __name__ == '__main__':
    main()
