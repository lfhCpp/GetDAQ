#!/usr/bin/env python3
"""Offline ClockInit: source provenance, complete parameters and nominal model boundaries."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[1]
    source = root / 'source/get/GetBench-software-release-20210208/data/config/configure-pedestals.xcfg'
    original = source.read_text()
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix='getdaq-clock-') as directory:
        directory = Path(directory)

        def run(path, output, expected=0, version='2', cobo='0'):
            result = subprocess.run([str(binary), 'compile-cobo-clock', str(path), cobo, version, str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == expected, (result.args, result.returncode, result.stdout, result.stderr)
            if expected:
                assert not output.exists(), 'failed compilation created artifact'
            return result

        output = directory / 'original.json'
        run(source, output)
        image = json.loads(output.read_text())
        assert image['kind'] == 'getdaq.cobo-clock-image' and image['hardware_deployment_supported'] is False
        assert image['complete_prepare'] is False and image['asad_mask'] == 1 and image['nominal_targets_matched']
        assert image['nominal_upstream_known'] and not image['nominal_known_difference']
        assert len(image['pll_packets']) == 26 and image['pll_packets'][0]['hex'] == '0x20000'
        assert image['boards'][0]['lcm1']['hex'] == '0x48900001020000cf'
        assert int(image['boards'][0]['lcm1']['value']) == 0x48900001020000cf
        assert image['boards'][0]['effective_nominal_sampling_mhz'] == 100
        assert image['sources'][0]['sha256'] == digest
        assert all('/Aget[' not in p['query_path'] for p in image['parameters'])
        saved = output.read_bytes()
        result = subprocess.run([str(binary), 'compile-cobo-clock', str(source), '0', '2', str(output)], capture_output=True)
        assert result.returncode == 1 and output.read_bytes() == saved
        external = source.parent / 'configure-ExternalTrigger-2CoBo_1Mutant.xcfg'
        external_digest = hashlib.sha256(external.read_bytes()).hexdigest()
        external_output = directory / 'external.json'
        run(external, external_output, cobo='Crate00_Slot00')
        assert json.loads(external_output.read_text())['sources'][0]['sha256'] == external_digest
        counter = 0

        def variant(name, module='', clock='', control='', text=None, error=None, version='2'):
            nonlocal counter
            counter += 1
            if text is None:
                replacement = '<Instance id="0">' + (f'<Module>{module}</Module>' if module else '')
                if clock or control:
                    replacement += '<AsAd id="0">' + (f'<Control>{control}</Control>' if control else '')
                    replacement += f'<Clocking>{clock}</Clocking></AsAd>'
                replacement += '</Instance>'
                text = original.replace('<Instance id="0" />', replacement)
            path = directory / f'{counter}-{name}.xcfg'
            path.write_text(text)
            out = directory / f'{counter}-{name}.json'
            result = run(path, out, 1 if error else 0, version=version)
            if error:
                assert error in result.stderr, (name, result.stderr)
                return None
            return json.loads(out.read_text())

        reduced = variant('reduced', version='1')
        assert not reduced['program_pll'] and reduced['program_reading_divider'] and reduced['reading_divider'] == 2
        quantized = variant('quantization', module='<writingClockFrequency type="real">123.0</writingClockFrequency>')
        assert quantized['nominal_cobo_writing_mhz'] == 125 and quantized['nominal_known_difference']
        assert quantized['boards'][0]['effective_nominal_sampling_mhz'] == 125 and not quantized['nominal_targets_matched']
        low = variant('legacy-clamp', module='<writingClockFrequency type="real">1.0</writingClockFrequency>')
        assert low['nominal_outputs_within_range'] is False
        ckr = variant('r-ckr12', module='<readingClockFrequency type="real" range="{12.5, 25.0}">12.5</readingClockFrequency>', version='1')
        assert ckr['reading_divider'] == 2 and ckr['nominal_cobo_reading_mhz'] == 25
        assert ckr['boards'][0]['effective_nominal_adc_mhz'] == 50 and ckr['nominal_known_difference']
        unknown = variant('skip', module='<skipPLLConfig>true</skipPLLConfig>')
        assert unknown['pll_packets'] == [] and unknown['nominal_upstream_known'] is False
        assert unknown['nominal_cobo_writing_mhz'] is None and unknown['nominal_cobo_reading_mhz'] is None
        assert unknown['boards'][0]['effective_nominal_sampling_mhz'] is None and unknown['nominal_outputs_within_range'] is None
        mode = variant('expert-mode', module='<PLL><MODE>6</MODE></PLL>')
        assert mode['nominal_upstream_known'] is False and mode['nominal_cobo_reading_mhz'] is None
        sync = variant('sync', module='<PLL><EN_CLKin1>true</EN_CLKin1><SYNC_POL_INV>true</SYNC_POL_INV></PLL>')
        assert len(sync['pll_packets']) == 27
        assert sync['pll_packets'][12]['hex'] == '0x401000b' and sync['pll_packets'][13]['hex'] == '0x400000b'
        mux = variant('select', module='<PLL><CLKin_SELECT_MODE>1</CLKin_SELECT_MODE><CLKin_Select_MODE>bad-unused</CLKin_Select_MODE></PLL>')
        assert (int(mux['pll_packets'][14]['value']) >> 9) & 7 == 1
        alias = variant('legacy-alias', text=original.replace('CLKin_SELECT_MODE','CLKin_Select_MODE'))
        assert alias['pll_packets'] == image['pll_packets']
        assert any(p['query_path'].endswith('/CLKin_Select_MODE') for p in alias['parameters'])
        hex_value = variant('hex', module='<PLL><CLKin_SELECT_MODE type="hex" range="{0x1}">1</CLKin_SELECT_MODE></PLL>')
        assert any(p.get('compiled_value') == '0x1' for p in hex_value['parameters'])
        expert = variant('lcm-expert', clock='<Debug><LCM1><debugModeEnabled>true</debugModeEnabled><OAMUX>1</OAMUX></LCM1></Debug>')
        assert expert['boards'][0]['lcm1']['nominal_output_known'] is False
        assert expert['boards'][0]['effective_nominal_sampling_mhz'] is None
        disabled = variant('ignored-expert', clock='<Debug><LCM1><debugModeEnabled>false</debugModeEnabled><FINDIV>invalid-unused</FINDIV></LCM1></Debug>')
        assert disabled['boards'][0]['lcm1']['hex'] == image['boards'][0]['lcm1']['hex']
        unused = variant('ignored-r14', module='<PLL><Status_CLKin1_TYPE>invalid-unused</Status_CLKin1_TYPE></PLL>')
        assert unused['pll_packets'] == image['pll_packets']
        delay = variant('delay', clock='<delayAdcClock range="{376}">376</delayAdcClock>')
        assert delay['boards'][0]['lcm2']['quantized_adc_delay_ps'] == 535
        for text in ['NaN', 'inf', '1e999', '200.000001', '0.999999999', '0x64', '100MHz']:
            variant('bad-real', module=f'<writingClockFrequency type="real">{text}</writingClockFrequency>', error='writingClockFrequency')
        for tag, value, error in [('writingClockFrequency', '<writingClockFrequency type="real" unit="Hz">100.0</writingClockFrequency>', 'Unsupported unit'),
                                  ('readingClockFrequency', '<readingClockFrequency type="real" range="{6.25, 25.0}">6.25</readingClockFrequency>', 'Unsupported CoBo reading')]:
            variant(tag, module=value, error=error, version='1')
        variant('scalar-class', module='<writingClockFrequency type="real" class="shared">100.0</writingClockFrequency>', error='class')
        variant('bad-pll', module='<PLL><MODE>32</MODE></PLL>', error='MODE')
        variant('bad-bool', module='<PLL><EN_SYNC>1</EN_SYNC></PLL>', error='PLL boolean')
        variant('bad-delay', clock='<delayAdcClock range="{5336}">5336</delayAdcClock>', error='out of range')
        variant('bad-phase', clock='<phaseShiftAdcClock range="{45}">45</phaseShiftAdcClock>', error='ADC phase')
        variant('bad-divider', clock='<samplingClockDivider type="real" range="[0.0, 200.0]">0.0</samplingClockDivider>', error='positive and finite')
        variant('bad-expert', clock='<Debug><LCM1><debugModeEnabled>true</debugModeEnabled><FINDIV>128</FINDIV></LCM1></Debug>', error='FINDIV')
        for version in ['0','02','-1','4294967295','4294967296','1.0','2x']:
            run(source,directory / f'invalid-version-{version}.json',2,version=version)
        run(source,directory / 'invalid-id.json',1,cobo='*')
        include = directory / 'clock-defaults.xcfg'
        include.write_text('<AsAd><Control><isActive>true</isActive></Control><Clocking><phaseShiftAdcClock unit="degrees">270</phaseShiftAdcClock><delayAdcClock unit="ps">5335</delayAdcClock></Clocking></AsAd>')
        minimal = ('<Setup><Node id="CoBo"><Instance id="0"><isActive>true</isActive><Module>'
                   '<writingClockFrequency unit="MHz" type="real">100.0</writingClockFrequency>'
                   '<readingClockFrequency unit="MHz">25</readingClockFrequency></Module>'
                   '<AsAd id="*" include="clock-defaults.xcfg" />' + ''.join(f'<AsAd id="{i}" />' for i in range(4)) + '</Instance></Node></Setup>')
        path = directory / 'minimal.xcfg'; path.write_text(minimal); out = directory / 'minimal.json'; run(path,out)
        included = json.loads(out.read_text())
        assert included['asad_mask'] == 15 and len(included['sources']) == 2
        assert any(p['default_from_code'] and p['query_path'].endswith('/samplingClockDivider') for p in included['parameters'])
        assert any(p.get('source_file') == str(include) and p['query_path'].endswith('/phaseShiftAdcClock') for p in included['parameters'])
        variant('missing-asad', text=minimal.replace('<AsAd id="3" />',''),error='exactly four')
        variant('incomplete-expert', text=minimal.replace('<AsAd id="0" />','<AsAd id="0"><Clocking><Debug><LCM1><debugModeEnabled>true</debugModeEnabled></LCM1></Debug></Clocking></AsAd>'),error='No declared')
        assert hashlib.sha256(source.read_bytes()).hexdigest() == digest
        assert hashlib.sha256(external.read_bytes()).hexdigest() == external_digest
    print('PASS offline whole-CoBo clock CLI: original configs, include/default provenance, PLL/expert fields, nominal/unknown model, rejection and no overwrite')


if __name__ == '__main__':
    main()
