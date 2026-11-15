#!/usr/bin/env python3
"""Keep shipped Sentry examples aligned with the migrated receiver lifecycle."""
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def load(name):
    return json.loads((ROOT / 'config/sentry' / name).read_text(encoding='utf-8'))


def main():
    node = load('node.example.json')
    assert node['process_name'] == 'getdaq-receiver'
    assert len(node['process_name'].encode('utf-8')) <= 15  # Linux process comm has a 15-byte name limit.
    assert node['check_process'] is True

    native = load('native-receiver.example.json')
    assert native['process_name'] == 'getdaq-receiver'
    assert native['check_process'] is False
    assert node['data_dir'] != native['data_dir']
    data_service = (ROOT / 'docs/DATA_SERVICE.md').read_text(encoding='utf-8')
    assert '传统 dataRouter 示例仍保持原来的进程检查逻辑' not in data_service
    assert '部署旧版 `dataRouter` 或 Windows `DataExporter.exe` 时' in data_service
    print('PASS Sentry examples distinguish live process monitoring from sealed native receiver archiving')


if __name__ == '__main__':
    main()
