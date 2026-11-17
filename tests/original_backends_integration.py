#!/usr/bin/env python3
"""Real HTTPS distribution + original SOAP ECC + DataExporter; no physical hardware."""
import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import ssl
import subprocess
import tempfile
import time


def require(value, message):
    if not value:
        raise AssertionError(message)


def sha(value):
    return hashlib.sha256(value).hexdigest()


def port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def request(endpoint, method, path, body=None, token=None, repository=None, binary=False):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    connection = http.client.HTTPSConnection('127.0.0.1', endpoint['port'], context=context, timeout=25)
    connection.connect()
    require(sha(connection.sock.getpeercert(binary_form=True)) == endpoint['pin'], 'Paired server certificate differs')
    payload = body if binary else (json.dumps(body).encode() if body is not None else None)
    headers = {'Content-Type': 'application/octet-stream' if binary else 'application/json'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    if repository:
        headers['X-Repository-ID'] = repository
    connection.request(method, path, body=payload, headers=headers)
    response = connection.getresponse()
    result = response.read()
    status = response.status
    connection.close()
    return status, result


def wait(action, timeout=30):
    deadline = time.monotonic() + timeout
    while True:
        value = action()
        if value:
            return value
        require(time.monotonic() < deadline, 'Observation timed out')
        time.sleep(.05)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bin', type=Path)
    parser.add_argument('fixture', type=Path)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    programs = args.bin.resolve()
    args.evidence.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='distributed-', dir=args.evidence.resolve()))
    source = Path(__file__).resolve().parents[1]
    children = []
    opened = []
    checks = []
    selected_ports = []
    while len(selected_ports) < 8:
        value = port()
        if value not in selected_ports:
            selected_ports.append(value)

    def run(*command):
        result = subprocess.run(list(map(str, command)), capture_output=True, timeout=30)
        require(result.returncode == 0, result.stderr.decode(errors='replace')[:2000])
        return result.stdout

    def start(name, *command):
        log = (root / (name + '.log')).open('wb')
        opened.append(log)
        child = subprocess.Popen(list(map(str, command)), stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        children.append(child)
        return child

    def terminal(node, body):
        code, result = request(node, 'POST', '/v1/operations', body, node['token'])
        require(code == 202, result.decode(errors='replace'))
        def query():
            status, value = request(node, 'GET', '/v1/operations/' + body['operation_id'], token=node['token'])
            require(status == 200, value.decode(errors='replace'))
            record = json.loads(value)
            return record if record['phase'] in ['complete', 'failed', 'unknown'] else None
        return wait(query, 90)

    try:
        nodes = []
        for index in range(2):
            directory = root / ('node-' + str(index))
            run(programs / 'getdaq-node-agent', '--initialize', directory, '--programs', programs,
                '--bind', '127.0.0.1', '--port', selected_ports[index], '--data', directory / 'raw')
            invitation = json.loads((directory / 'pairing.json').read_text())
            node = {'id': invitation['node_id'], 'port': selected_ports[index], 'pin': invitation['certificate_sha256'],
                    'invitation': invitation, 'root': directory}
            nodes.append(node)
            start('node-' + str(index), programs / 'getdaq-node-agent', '--config', directory / 'node.json')
        repository_root = root / 'repository'
        run(programs / 'getdaq-config-repository', 'initialize', repository_root)
        run(programs / 'getdaq-config-service', '--generate-token', root / 'repository-admin.token')
        run(programs / 'getdaq-config-service', '--generate-token', root / 'repository-read.token')
        admin = (root / 'repository-admin.token').read_text().strip()
        read_token = (root / 'repository-read.token').read_text().strip()
        repository = {'port': selected_ports[2], 'pin': nodes[0]['pin']}
        start('repository', programs / 'getdaq-config-service', '--root', repository_root,
              '--credential', root / 'repository-admin.token', '--read-credential', root / 'repository-read.token',
              '--bind', '127.0.0.1', '--port', repository['port'], '--tls-certificate', nodes[0]['root'] / 'tls.crt',
              '--tls-key', nodes[0]['root'] / 'tls.key')
        def repository_ready():
            try:
                code, body = request(repository, 'GET', '/v1/identity', token=admin)
                return json.loads(body)['repository_id'] if code == 200 else None
            except (ConnectionError, OSError):
                return None
        repository_id = wait(repository_ready)
        for node in nodes:
            wait(lambda: request(node, 'GET', '/v1/identity')[0] == 200)
            pair = {'pairing_code': node['invitation']['pairing_code'], 'repository_id': repository_id,
                    'repository_url': 'https://127.0.0.1:' + str(repository['port']),
                    'repository_certificate_sha256': repository['pin'], 'repository_token': read_token}
            code, response = request(node, 'POST', '/v1/pair', pair)
            require(code == 200, response.decode(errors='replace'))
            node['token'] = json.loads(response)['credential']
            repeat_code, repeated_pair = request(node, 'POST', '/v1/pair', pair)
            require(repeat_code == 200 and json.loads(repeated_pair)['credential'] == node['token'], 'Pairing acknowledgement loss cannot recover the original pairing')
            require(request(node, 'POST', '/v1/pair', dict(pair, pairing_code='wrong'))[0] != 200, 'Consumed invitation enrolled another pairing request')
        checks.append('two pinned HTTPS node agents paired; one-time invitations consumed')
        forbidden = request(repository, 'POST', '/v1/objects/' + sha(b'forbidden'), b'forbidden', read_token, repository_id, True)
        require(forbidden[0] == 401, 'Read-only node credential allowed repository modification')
        checks.append('read-only repository credential cannot upload')
        fixture_root = root / 'fixture'
        run(args.fixture.resolve(), source, fixture_root, nodes[0]['id'], nodes[1]['id'], *selected_ports[3:])
        manifest_bytes = (fixture_root / 'manifest.json').read_bytes()
        manifest = json.loads(manifest_bytes)
        release = sha(manifest_bytes)
        for path in (fixture_root / 'objects').iterdir():
            content = path.read_bytes()
            code, response = request(repository, 'POST', '/v1/objects/' + path.name, content, admin, repository_id, True)
            require(code == 200, response.decode(errors='replace'))
            code, downloaded = request(repository, 'GET', '/v1/content/' + path.name, token=read_token)
            require(code == 200 and downloaded == content, 'Binary content download differs')
            code, present = request(repository, 'GET', '/v1/objects/' + path.name, token=read_token)
            require(code == 200 and json.loads(present)['present'], 'Legacy presence query changed behavior')
        chunks = []
        for offset in range(0, len(manifest_bytes), 32768):
            content = manifest_bytes[offset:offset + 32768]
            digest = sha(content)
            require(request(repository, 'POST', '/v1/objects/' + digest, content, admin, repository_id, True)[0] == 200, 'Manifest upload failed')
            chunks.append(digest)
        code, response = request(repository, 'POST', '/v1/deployments', {'manifest_objects': chunks}, admin, repository_id)
        require(code == 200 and json.loads(response)['release_id'] == release, response.decode(errors='replace'))
        checks.append('portable release registered; exact binary download; legacy presence endpoint preserved')
        receiver_token = os.urandom(32).hex()
        ecc_token = os.urandom(32).hex()
        prepare_receiver = {'schema': 1, 'operation_id': 'prepare-receiver', 'action': 'prepare', 'release_id': release,
                            'group_id': 'group1', 'role': 'receiver', 'receiver_id': 'data', 'secrets': {'receiver': receiver_token}}
        prepared = terminal(nodes[1], prepare_receiver)
        require(prepared['phase'] == 'complete', json.dumps(prepared))
        receiver_receipt = prepared['result']
        prepare_ecc = {'schema': 1, 'operation_id': 'prepare-ecc', 'action': 'prepare', 'release_id': release,
                       'group_id': 'group1', 'role': 'ecc', 'receiver_receipts': [receiver_receipt],
                       'secrets': {'ecc': ecc_token, 'receivers': {'data': receiver_token}}}
        prepared_ecc = terminal(nodes[0], prepare_ecc)
        require(prepared_ecc['phase'] == 'complete', json.dumps(prepared_ecc))
        checks.append('receiver and original ECC prepared on different roots with exact XML/XCFG copies')
        activate_receiver = {key: value for key, value in prepare_receiver.items() if key != 'secrets'}
        activate_receiver.update(operation_id='activate-receiver', action='activate', expected_previous='')
        active_receiver = terminal(nodes[1], activate_receiver)
        require(active_receiver['phase'] == 'complete', json.dumps(active_receiver))
        activate_ecc = {key: value for key, value in prepare_ecc.items() if key not in ['secrets', 'receiver_receipts']}
        activate_ecc.update(operation_id='activate-ecc', action='activate', expected_previous='')
        active_ecc = terminal(nodes[0], activate_ecc)
        require(active_ecc['phase'] == 'complete', json.dumps(active_ecc))
        repeated = terminal(nodes[0], activate_ecc)
        require(repeated == active_ecc, 'Repeated operation ID did not return the original result')
        changed = dict(activate_ecc, expected_previous='0' * 64)
        require(request(nodes[0], 'POST', '/v1/operations', changed, nodes[0]['token'])[0] == 409, 'Same ID accepted different content')
        checks.append('original SOAP ECC and DataExporter started; repeated activation retains original result; conflicting ID rejected')
        import urllib.request, xml.etree.ElementTree as ET
        config=json.loads(Path(active_ecc['result']['config_file']).read_text())
        soap=b'<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" xmlns:ecc="urn:http://localhost:8083/"><SOAP-ENV:Body><ecc:GetState/></SOAP-ENV:Body></SOAP-ENV:Envelope>'
        response=urllib.request.urlopen(urllib.request.Request('http://127.0.0.1:'+str(config['port'])+'/',soap,{'Content-Type':'text/xml','SOAPAction':'""'}),timeout=3).read()
        state=ET.fromstring(response).find('.//State')
        require(state is not None and state.text=='1','Original SOAP ECC must actually be Idle')
        require(not list((nodes[0]['root']).rglob('prepare.jsonl')), 'Deployment unexpectedly prepared hardware')
        checks.append('actual ECC status Idle; no board preparation or acquisition')
        for node, original in [(nodes[0], activate_ecc), (nodes[1], activate_receiver)]:
            stop_request = dict(original, operation_id='stop-' + original['role'], action='stop', ecc_stopped_confirmed=True)
            stopped = terminal(node, stop_request)
            require(stopped['phase'] == 'complete', json.dumps(stopped))
        checks.append('both backends normally stopped without affecting node agents or repository')
        (root / 'result.json').write_text(json.dumps({'passed': True, 'checks': checks, 'real_hardware': False,
                                                    'physical_multi_host': False, 'release_id': release,
                                                    'program_sha256': {path.name: sha(path.read_bytes()) for path in programs.iterdir() if path.is_file()}}, indent=2) + '\n')
        print('PASS', len(checks), 'distributed deployment checks;', root)
    finally:
        # Detached services are located only by this private test bin and config root.
        for entry in Path('/proc').iterdir():
            if not entry.name.isdigit():
                continue
            try:
                executable = (entry / 'exe').resolve(strict=True)
                command = (entry / 'cmdline').read_bytes()
                if executable.parent == programs and str(root).encode() in command and executable.name in ['getEccSoapServer', 'DataExporter', 'getdaq-sentry']:
                    os.kill(int(entry.name), signal.SIGTERM)
            except (OSError, RuntimeError):
                pass
        for child in reversed(children):
            if child.poll() is None:
                child.terminate()
            try:
                child.wait(timeout=10)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=3)
        for log in opened:
            log.close()
        require(not list(root.rglob('*.graw')), 'Unexpected acquisition payload generated')


if __name__ == '__main__':
    main()
