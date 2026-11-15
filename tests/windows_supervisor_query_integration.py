#!/usr/bin/env python3
"""Exercise the native Windows supervisor client against bounded loopback replies."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socketserver
import subprocess
import sys
import tempfile
import threading


TOKEN = 'a' * 64


class OnceServer(socketserver.TCPServer):
    allow_reuse_address = True
    timeout = 6


class ScenarioHandler(socketserver.BaseRequestHandler):
    def handle(self):
        try:
            self.request.settimeout(3)
            received = bytearray()
            while b'\r\n\r\n' not in received:
                block = self.request.recv(2048)
                if not block:
                    raise RuntimeError('Client closed before completing the HTTP request')
                received.extend(block)
                if len(received) > 8192:
                    raise RuntimeError('HTTP request exceeded its metadata limit')
            header, body = bytes(received).split(b'\r\n\r\n', 1)
            lines = header.decode('ascii').split('\r\n')
            content_lengths = [line.split(':', 1)[1].strip() for line in lines[1:]
                               if line.lower().startswith('content-length:')]
            if len(content_lengths) != 1 or not content_lengths[0].isdigit():
                raise RuntimeError('Expected exactly one fixed Content-Length')
            expected_body_bytes = int(content_lengths[0])
            while len(body) < expected_body_bytes:
                block = self.request.recv(min(2048, expected_body_bytes - len(body)))
                if not block:
                    raise RuntimeError('Client closed before completing the HTTP body')
                body += block
            if len(body) != expected_body_bytes:
                raise RuntimeError('HTTP body length differs from Content-Length')
            authorization = [line for line in lines[1:] if line.lower().startswith('authorization:')]
            if authorization != ['Authorization: Bearer ' + TOKEN]:
                raise RuntimeError('Authorization header differs or was sent more than once')
            index = self.server.received_requests
            if index >= len(self.server.responses):
                raise RuntimeError('Client sent more requests than the scenario allows')
            expected, status, response_body = self.server.responses[index]
            if lines[0] != expected['request_line']:
                raise RuntimeError('Unexpected request line: ' + lines[0])
            if expected.get('body') is not None and json.loads(body.decode('utf-8')) != expected['body']:
                raise RuntimeError('POST body differs from the single validated intent')
            if expected.get('empty_body') and body:
                raise RuntimeError('Unexpected request body')
            self.server.request_lines.append(lines[0])
            self.server.received_requests += 1
            response_bytes = response_body.encode('utf-8')
            if status is None:
                return
            reason = {200: 'OK', 202: 'Accepted', 401: 'Unauthorized', 500: 'Internal Server Error'}[status]
            self.request.sendall(('HTTP/1.1 %d %s\r\nContent-Type: application/json\r\n'
                                  'Content-Length: %d\r\nConnection: close\r\n\r\n' %
                                  (status, reason, len(response_bytes))).encode('ascii') + response_bytes)
        except Exception as error:
            self.server.errors.append(str(error))
            try:
                self.request.sendall(b'HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n')
            except OSError:
                pass


def serve_requests(server, count):
    for _ in range(count):
        before = server.received_requests
        server.handle_request()
        if server.received_requests == before:
            server.errors.append('Loopback server timed out waiting for a request')
            return


def serve_requests_and_watch_retry(server, count):
    serve_requests(server, count)
    if server.errors:
        return
    # Keep listening beyond the client's two-second deadline so a delayed
    # automatic retry cannot disappear when the expected response is sent.
    server.timeout = 3
    server.handle_request()


def canonical_json(value):
    return json.dumps(value, ensure_ascii=True, separators=(',', ':'))


def positive_fixture(controller):
    instance = '1' * 64
    source = {'id': 'C0', 'receiver_address': '127.0.0.1:5000', 'bind': '127.0.0.1',
              'output_root': '/tmp/getdaq-loopback/output', 'data_port': 5000, 'status_port': 5001,
              'export_port': 0, 'queue_mib': 1, 'max_frame_mib': 1, 'rotate_mib': 1,
              'export_queue_mib': 1}
    options = {'schema': 1, 'kind': 'getdaq.data-supervisor-plan', 'deployment_id': 'loopback-deployment',
               'audit_root': '/tmp/getdaq-loopback/audit', 'executable': '/tmp/getdaq-receiver',
               'executable_sha256': '9' * 64, 'configuration_sha256': 'a' * 64, 'artifact_sha256': 'b' * 64,
               'startup_timeout_ms': 2000, 'finish_timeout_ms': 1200, 'status_timeout_ms': 500,
               'max_total_queue_mib': 8, 'max_total_frame_mib': 8, 'sources': [source]}
    option_bytes = (canonical_json(options) + '\n').encode('ascii')
    plan_sha = hashlib.sha256(option_bytes).hexdigest()
    identity = {'schema': 1, 'service': 'getdaq-supervisor', 'agent_id': 'loopback-agent',
                'controller_id': controller, 'instance_id': instance, 'plan_sha256': plan_sha, 'plan': options}
    request = {'schema': 1, 'operation_id': 'loopback-operation-1', 'command': 'validate',
               'plan_sha256': plan_sha, 'instance_id': instance, 'controller_id': controller,
               'controller_snapshot_sha256': 'c' * 64, 'controller_epoch': 'd' * 64,
               'configuration_sha256': 'a' * 64, 'artifact_sha256': 'b' * 64,
               'parent_audit_reference': 'loopback-parent'}
    request_bytes = (canonical_json(request) + '\n').encode('ascii')
    request_key = hashlib.sha256(b'getdaq.supervisor-request:1\n' + request_bytes).hexdigest()
    accepted = {'schema': 1, 'operation_id': request['operation_id'], 'command': request['command'],
                'request_key': request_key, 'phase': 'accepted', 'message': 'accepted', 'instance_id': instance,
                'plan_sha256': plan_sha, 'controller_id': controller,
                'controller_snapshot_sha256': request['controller_snapshot_sha256'],
                'controller_epoch': request['controller_epoch'], 'result': None}
    responses = [({'request_line': 'GET /v1/identity HTTP/1.1', 'empty_body': True}, 200,
                  canonical_json(identity)),
                 ({'request_line': 'POST /v1/operations HTTP/1.1', 'body': request}, 202,
                  canonical_json(accepted))]
    return plan_sha, request, responses


def protect_credential(path):
    icacls = shutil.which('icacls.exe') or shutil.which('icacls')
    if not icacls:
        raise RuntimeError('icacls is required to create the restricted test credential ACL')
    owner = subprocess.check_output(['whoami.exe'], text=True, stderr=subprocess.STDOUT).strip()
    subprocess.run([icacls, str(path), '/inheritance:r', '/grant:r', owner + ':(R)', 'SYSTEM:(R)'],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--program', required=True, type=Path)
    parser.add_argument('--evidence', required=True, type=Path)
    parser.add_argument('--temporary-root', type=Path,
                        help='Use this new directory for the credential fixture and remove it after the test')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This transport/ACL integration must run on native Windows')
    program = args.program.resolve(strict=True)
    if program.name.lower() != 'getdaq-supervisor-query.exe':
        parser.error('Select the exact native supervisor query helper')
    owned_root = args.temporary_root.resolve() if args.temporary_root else None
    if owned_root:
        owned_root.mkdir(parents=True, exist_ok=False)
    fixture_root = None
    try:
        with tempfile.TemporaryDirectory(prefix='getdaq-supervisor-query-', dir=str(owned_root) if owned_root else None) as temporary:
            root = Path(temporary)
            fixture_root = root
            credential = root / 'test-credential.txt'
            credential.write_text(TOKEN + '\n', encoding='ascii', newline='')
            protect_credential(credential)
            with OnceServer(('127.0.0.1', 0), ScenarioHandler) as server:
                server.received_requests = 0
                server.errors = []
                server.request_lines = []
                server.responses = [({'request_line': 'GET /v1/identity HTTP/1.1', 'empty_body': True},
                                     401, '')]
                worker = threading.Thread(target=serve_requests_and_watch_retry, args=(server, 1), daemon=True)
                worker.start()
                command = [str(program), 'identity', '--host', '127.0.0.1', '--port', str(server.server_address[1]),
                           '--credential-file', str(credential), '--expected-agent', 'loopback-agent',
                           '--controller', 'loopback-controller', '--expected-plan', 'b' * 64, '--timeout-ms', '2000']
                process = subprocess.run(command, capture_output=True, text=True, timeout=10)
                worker.join(timeout=8)
                if worker.is_alive():
                    raise RuntimeError('Loopback server did not receive the single identity request')
                if process.returncode != 2 or 'Supervisor returned HTTP 401' not in process.stderr:
                    raise RuntimeError('Expected the bounded 401 response to stop the query without retry')
                if TOKEN in process.stdout or TOKEN in process.stderr:
                    raise RuntimeError('Credential value leaked into helper output')
                if server.received_requests != 1 or server.errors or server.request_lines != ['GET /v1/identity HTTP/1.1']:
                    raise RuntimeError('Expected exactly one authenticated identity request and no retry; count='+
                                       str(server.received_requests)+' lines='+repr(server.request_lines)+' errors='+repr(server.errors))

            controller = 'loopback-controller'
            plan_sha, request, responses = positive_fixture(controller)
            request_file = root / 'submit-request.json'
            request_file.write_text(json.dumps(request, separators=(',', ':')) + '\n', encoding='ascii', newline='')
            with OnceServer(('127.0.0.1', 0), ScenarioHandler) as server:
                server.received_requests = 0
                server.errors = []
                server.request_lines = []
                server.responses = responses
                worker = threading.Thread(target=serve_requests, args=(server, len(responses)), daemon=True)
                worker.start()
                command = [str(program), 'submit', '--host', '127.0.0.1', '--port', str(server.server_address[1]),
                           '--credential-file', str(credential), '--expected-agent', 'loopback-agent',
                           '--controller', controller, '--expected-plan', plan_sha, '--request-file', str(request_file),
                           '--timeout-ms', '2000']
                accepted = subprocess.run(command, capture_output=True, text=True, timeout=10)
                worker.join(timeout=8)
                if worker.is_alive():
                    raise RuntimeError('Loopback server did not receive the one identity GET and one control POST')
                if accepted.returncode != 0 or '"phase":"accepted"' not in accepted.stdout:
                    raise RuntimeError('Expected the scope-checked single POST to receive an accepted response')
                if TOKEN in accepted.stdout or TOKEN in accepted.stderr:
                    raise RuntimeError('Credential value leaked during successful control submission')
                expected_lines = ['GET /v1/identity HTTP/1.1', 'POST /v1/operations HTTP/1.1']
                if server.received_requests != 2 or server.errors or server.request_lines != expected_lines:
                    raise RuntimeError('Expected exactly one identity GET followed by one control POST: ' + repr(server.errors))

            uncertain_results = {}
            for name, post_status in (('connection_closed_after_post', None), ('http_500_after_post', 500)):
                uncertain_responses = [responses[0], responses[1]]
                uncertain_responses[1] = (uncertain_responses[1][0], post_status,
                                          '' if post_status is None else '{}')
                with OnceServer(('127.0.0.1', 0), ScenarioHandler) as server:
                    server.received_requests = 0
                    server.errors = []
                    server.request_lines = []
                    server.responses = uncertain_responses
                    worker = threading.Thread(target=serve_requests_and_watch_retry, args=(server, 2), daemon=True)
                    worker.start()
                    command = [str(program), 'submit', '--host', '127.0.0.1', '--port', str(server.server_address[1]),
                               '--credential-file', str(credential), '--expected-agent', 'loopback-agent',
                               '--controller', controller, '--expected-plan', plan_sha,
                               '--request-file', str(request_file), '--timeout-ms', '2000']
                    uncertain = subprocess.run(command, capture_output=True, text=True, timeout=10)
                    worker.join(timeout=5)
                    if worker.is_alive():
                        raise RuntimeError('Loopback server did not finish the uncertain POST scenario: ' + name)
                    if uncertain.returncode != 2:
                        raise RuntimeError('Uncertain POST must return an error for ' + name)
                    if TOKEN in uncertain.stdout or TOKEN in uncertain.stderr:
                        raise RuntimeError('Credential value leaked during uncertain POST scenario')
                    if (server.received_requests != 2 or server.errors or
                            server.request_lines != expected_lines):
                        raise RuntimeError('Uncertain POST was retried or request sequence differed for ' + name +
                                           ': ' + repr(server.request_lines) + ' ' + repr(server.errors))
                    uncertain_results[name] = {'identity_status': 200,
                                                'post_response': 'connection_closed' if post_status is None else 500,
                                                'requests': 2, 'retry': False, 'reported_success': False}
            helper_sha = hashlib.sha256(program.read_bytes()).hexdigest()
            result = {'schema': 'getdaq.windows-supervisor-query-test:1', 'status': 'passed',
                      'program_sha256': helper_sha, 'http_status': 401, 'requests': 7,
                      'negative_case': {'http_status': 401, 'requests': 1, 'retry': False},
                      'positive_case': {'identity_status': 200, 'submit_status': 202,
                                        'requests': 2, 'methods': ['GET', 'POST'],
                                        'accepted_response_validated': True},
                      'uncertain_post_cases': uncertain_results,
                      'credential_value_recorded': False, 'temporary_root_removed': True}
        if fixture_root and fixture_root.exists():
            raise RuntimeError('Windows credential fixture directory was not removed')
    finally:
        if owned_root:
            owned_root.rmdir()
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('PASS Windows supervisor query: protected ACL; 401 no-retry; identity GET plus one accepted control POST; temporary root removed')


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        print('Windows supervisor query integration failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
