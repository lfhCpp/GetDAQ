#!/usr/bin/env python3
"""Offline CoBo AGET/Module/buffer compilation against the supplied GET configuration."""
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
    with tempfile.TemporaryDirectory(prefix='getdaq-readout-') as directory:
        directory = Path(directory)

        def run(path, output, expected=0, cobo='0', version_map=versions):
            result = subprocess.run([str(binary), 'compile-cobo-readout', str(path), cobo, version_map, str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == expected, (result.args, result.returncode, result.stdout, result.stderr)
            if expected:
                assert not output.exists(), 'failed compilation created an artifact'
            return result

        output = directory / 'original.json'
        run(source, output)
        image = json.loads(output.read_text())
        assert image['kind'] == 'getdaq.cobo-readout-image' and image['hardware_deployment_supported'] is False
        assert image['full_readout'] is True and image['suppress_multiplicity'] == 0
        assert image['trigger_mask'] == '4' and image['buffer_layout'] == 'per-asad'
        assert image['sources'][0]['sha256'] == digest
        writes = {(w['device'], w['register'], w['field']): int(w['value']) for w in image['writes']}
        assert writes['ctrl', 'triggerPeriod_10ns', ''] == 100000000
        assert writes['ctrl', 'ScwMultDelay', ''] == 50 and writes['ctrl', 'ScwScrDelay', ''] == 0
        assert writes['ctrl', 'memStart_asad0', ''] == 0x8000000 and writes['ctrl', 'memEnd_asad0', ''] == 0xa000000
        assert writes['ctrl', 'readPtr_asad1', ''] == 0xa000000
        assert writes['ctrl', 'pipeCtrl', 'partialReadOut'] == 0
        external = source.parent / 'configure-ExternalTrigger-2CoBo_1Mutant.xcfg'
        external_digest = hashlib.sha256(external.read_bytes()).hexdigest()
        external_output = directory / 'external.json'
        run(external, external_output, cobo='Crate00_Slot00')
        external_image = json.loads(external_output.read_text())
        assert external_image['trigger_mask'] == '0' and external_image['full_readout'] is True
        assert external_image['sources'][0]['sha256'] == external_digest
        thresholds = [w for w in image['writes'] if w['field'] == 'threshold']
        assert len(thresholds) == 272 and all(int(w['value']) == 600 for w in thresholds)
        assert [w['register'] for w in thresholds[:68]] == [f'aget0ch{id}' for id in range(68)]
        saved = output.read_bytes()
        result = subprocess.run([str(binary), 'compile-cobo-readout', str(source), '0', versions, str(output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 1 and output.read_bytes() == saved

        def compile_variant(name, module='', asad='', circular='', text=None, error=None):
            if text is None:
                selected = '<Instance id="0">' + module + asad + circular + '</Instance>'
                text = original.replace('<Instance id="0" />', selected)
            path = directory / f'{name}.xcfg'
            path.write_text(text, encoding='utf-8')
            out = directory / f'{name}.json'
            result = run(path, out, 1 if error else 0)
            if error:
                assert error in result.stderr, result.stderr
                return None
            return json.loads(out.read_text())

        def value(image, reg, field=''):
            return next(int(w['value']) for w in reversed(image['writes']) if w['register'] == reg and w['field'] == field)

        maximum = compile_variant('high', module='<Module><triggerPeriod_10ns unit="10ns">4294967295</triggerPeriod_10ns>'
                                  '<triggerPeriod unit="bad">ignored</triggerPeriod><coboId>255</coboId>'
                                  '<scwScrDelay unit="10ns">125</scwScrDelay><writeReadDelay>ignored</writeReadDelay></Module>')
        assert value(maximum, 'triggerPeriod_10ns') == 0xffffffff and value(maximum, 'COBO_index', 'd') == 255
        assert value(maximum, 'ScwScrDelay') == 125
        typed_hex = compile_variant('typed-hex', circular='<CircularBuffer><interruptRate type="hex">10</interruptRate></CircularBuffer>')
        assert value(typed_hex, 'interruptRate') == 16
        hex_source = next(p for p in typed_hex['parameters'] if p['query_path'].endswith('/CircularBuffer/interruptRate'))
        assert hex_source['value'] == '10' and hex_source['compiled_value'] == '0x10'
        period = next(p for p in maximum['parameters'] if p['query_path'].endswith('/Module/triggerPeriod_10ns'))
        assert 'Instance[0]' in period['source_path']
        disabled = compile_variant('disabled-channel', asad='<AsAd id="0"><Aget id="0"><channel id="11">'
                                   '<isActive>false</isActive><zeroSuppressionThreshold>4095</zeroSuppressionThreshold></channel></Aget></AsAd>')
        assert value(disabled, 'aget0ch11', 'threshold') == 4095
        for name, module, asad, circular, error in [
            ('mode', '<Module><triggerMode>External</triggerMode></Module>', '', '', 'triggerMode'),
            ('period-overflow', '<Module><triggerPeriod>42950</triggerPeriod></Module>', '', '', 'triggerPeriod'),
            ('period-unit', '<Module><triggerPeriod unit="s">1</triggerPeriod></Module>', '', '', 'Unsupported unit'),
            ('secondary-2p', '<Module><secondaryTriggerMode>external_2p</secondaryTriggerMode></Module>', '', '', 'secondaryTriggerMode'),
            ('threshold', '', '<AsAd id="0"><Aget id="0"><channel id="67"><zeroSuppressionThreshold>4096</zeroSuppressionThreshold></channel></Aget></AsAd>', '', 'zeroSuppressionThreshold'),
            ('mixed-readout', '', '<AsAd id="0"><Aget id="1"><Global><Reg1><isAllChannelRead>false</isAllChannelRead></Reg1></Global></Aget></AsAd>', '', 'isAllChannelRead'),
            ('range', '', '', '<CircularBuffer><endOfMemory>0x7000000</endOfMemory></CircularBuffer>', 'less than'),
            ('outside-range', '', '<AsAd id="3"><CircularBuffer><startOfMemory>0x0</startOfMemory></CircularBuffer></AsAd>', '', 'range'),
            ('levels', '', '', '<CircularBuffer><levelAlmostEmptyMemory>1</levelAlmostEmptyMemory></CircularBuffer>', 'levels'),
            ('rate', '', '', '<CircularBuffer><interruptRate>0</interruptRate></CircularBuffer>', 'positive'),
            ('unknown-class', '<Module><triggerDelay class="shared">800</triggerDelay></Module>', '', '', 'class'),
        ]:
            compile_variant(name, module, asad, circular, error=error)

        without_explicit = re.sub(r'<CircularBuffer>.*?</CircularBuffer>', '', original, flags=re.S)
        # Retain the global circular-buffer declaration while removing the four AsAd declarations.
        global_begin = original.index('<CircularBuffer>')
        global_end = original.index('</CircularBuffer>', global_begin) + len('</CircularBuffer>')
        without_explicit = without_explicit.replace('<AsAd id="*">', original[global_begin:global_end] + '<AsAd id="*">', 1)
        automatic = compile_variant('automatic', text=without_explicit)
        assert value(automatic, 'memEnd_asad0') == 0x10000000 and value(automatic, 'memStart_asad1') == 0x10000000
        # An unambiguous partial layout: remove all AsAd ranges, then explicitly specify only one board.
        partial = without_explicit.replace('<Instance id="0" />', '<Instance id="0"><AsAd id="0"><CircularBuffer><startOfMemory>0x8000000</startOfMemory><endOfMemory>0x10000000</endOfMemory></CircularBuffer></AsAd></Instance>')
        compile_variant('partial', text=partial, error='all four')
        missing_threshold = re.sub(r'<zeroSuppressionThreshold\b[^>]*>[^<]*</zeroSuppressionThreshold>', '', original)
        compile_variant('missing-threshold', text=missing_threshold, error='zeroSuppressionThreshold')
        assert hashlib.sha256(source.read_bytes()).hexdigest() == digest, 'provided source was changed'
        assert hashlib.sha256(external.read_bytes()).hexdigest() == external_digest, 'provided external config was changed'
    print('PASS: CoBo readout CLI, real GET mapping, aliases/provenance, buffer/trigger/zero validation and no output on failure')


if __name__ == '__main__':
    main()
