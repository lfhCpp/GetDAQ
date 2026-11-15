#!/usr/bin/env python3
"""Small stdio + authenticated HTTP integration test for getdaq-control-mcp."""
import http.server
import json
import os
from pathlib import Path
import socketserver
import subprocess
import sys
import tempfile
import threading


TOKEN = 'a' * 64
DIGEST = 'b' * 64


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *_args):
        pass

    def respond(self, status, value):
        body = json.dumps(value, separators=(',', ':')).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        self.server.requests.append(('GET', self.path))
        if self.headers.get('Authorization') != 'Bearer ' + TOKEN:
            return self.respond(401, {'error': 'unauthorized'})
        if self.path == '/v1/identity':
            return self.respond(200, {'client_id': 'mcp-test', 'instance_id': DIGEST, 'snapshot_sha256': DIGEST})
        if self.path == '/v1/status':
            return self.respond(200, {'kind': 'status', 'raw_data_read': False,
                                     'client_epoch': self.server.owner_epoch,
                                     'instance_id': DIGEST, 'snapshot_sha256': DIGEST,
                                     'operation_active': self.server.operation_active,
                                     'controller': {'state': self.server.controller_state, 'transition': 0, 'error': 0,
                                                    'outcome_unknown': False, 'needs_reconciliation': False}})
        if self.path == '/v1/receivers':
            return self.respond(200, {'kind': 'receivers', 'items': []})
        if self.path == '/v1/hardware-state':
            return self.respond(200, {'kind': 'getdaq.hardware-state-observation', 'consistent': True,
                                     'cobo_lifecycle_is_hardware_readback': False, 'cobo_lifecycle': []})
        if self.path == '/v1/cobo-maintenance-targets':
            target = {'id': 'C0', 'node_name': 'CoBo0',
                      'lifecycle_state': self.server.lifecycle_state,
                      'operation_id': '', 'operation': '', 'operation_phase': 'idle'}
            if self.server.lifecycle_state == 'prepared':
                target.update({'operation_id': 'prepare-C0-0', 'operation': 'prepare',
                               'operation_phase': 'complete'})
            if self.server.lifecycle_state == 'configured':
                target.update({'operation_id': 'configure-C0-previous',
                               'operation': 'configure-cobo', 'operation_phase': 'complete'})
            cobos = [target, dict(target)] if self.server.duplicate_targets else [target]
            return self.respond(200, {'schema': 2, 'kind': 'getdaq.ecc-cobo-maintenance-targets',
                                     'instance_id': DIGEST, 'snapshot_sha256': self.server.target_snapshot,
                                     'configuration_sha256': DIGEST, 'artifact_sha256': DIGEST,
                                     'cobos': cobos})
        if self.path == '/v1/operations/op-1':
            return self.respond(200, {'operation_id': 'op-1', 'phase': 'complete'})
        if self.path == '/v1/operations/prepare-C0-0':
            return self.respond(200, {'operation_id': 'prepare-C0-0', 'phase': 'complete',
                                      'request': {'command': 'prepare', 'cobo_id': 'C0'}})
        if self.path == '/v1/operations/failed-catalog':
            return self.respond(200, {'operation_id': 'failed-catalog', 'phase': 'failed', 'request': {
                'operation_id': 'failed-catalog', 'command': 'catalog', 'catalog_replace_existing': True,
                'instance_id': DIGEST, 'snapshot_sha256': DIGEST, 'client_id': 'mcp-test',
                'client_epoch': self.server.owner_epoch}})
        return self.respond(404, {'error': 'unknown path'})

    def do_POST(self):
        self.server.requests.append(('POST', self.path))
        if self.headers.get('Authorization') != 'Bearer ' + TOKEN:
            return self.respond(401, {'error': 'unauthorized'})
        if self.path != '/v1/operations' or self.headers.get('Content-Type') != 'application/json':
            return self.respond(400, {'error': 'bad request'})
        size = int(self.headers.get('Content-Length', '0'))
        body = json.loads(self.rfile.read(size))
        if body.get('command') == 'pause':
            return self.respond(409, {'schema': 1, 'error': 'ECC submission busy; not queued'})
        self.server.operations.append(body)
        if not self.server.owner_epoch:
            self.server.owner_epoch = body['client_epoch']
        return self.respond(202, {'operation_id': body['operation_id'], 'phase': 'accepted'})


def send(binary, config, messages):
    payload = ''.join(json.dumps(message, separators=(',', ':')) + '\n' for message in messages)
    run = subprocess.run([binary, '--config', str(config)], input=payload, text=True,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
    if run.returncode != 0:
        raise AssertionError('MCP process failed: ' + run.stderr)
    if run.stderr:
        raise AssertionError('Unexpected MCP stderr: ' + run.stderr)
    if TOKEN in run.stdout or TOKEN in run.stderr:
        raise AssertionError('Credential leaked to MCP output')
    replies = [json.loads(line) for line in run.stdout.splitlines()]
    if len(replies) != sum('id' in message for message in messages):
        raise AssertionError('Unexpected stdout framing or missing JSON-RPC response')
    return replies


def content_text(reply):
    return reply['result']['content'][0]['text']


def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: ecc_mcp_test.py PATH_TO_BINARY')
    binary = str(Path(sys.argv[1]).resolve())
    server = socketserver.ThreadingTCPServer(('127.0.0.1', 0), Handler)
    server.operations = []
    server.requests = []
    server.owner_epoch = ''
    server.lifecycle_state = 'prepared'
    server.controller_state = 3
    server.operation_active = False
    server.target_snapshot = DIGEST
    server.duplicate_targets = False
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        with tempfile.TemporaryDirectory(prefix='getdaq-mcp-test-') as directory:
            root = Path(directory)
            credential = root / 'ecc.token'
            credential.write_text(TOKEN + '\n', encoding='ascii')
            os.chmod(str(credential), 0o600)
            config = root / 'mcp.json'
            base = {'schema': 1, 'base_url': 'http://127.0.0.1:%d' % server.server_address[1],
                    'credential_file': str(credential), 'client_epoch_file': str(root / 'client.epoch'),
                    'client_id': 'mcp-test', 'allow_control': True}
            config.write_text(json.dumps(base), encoding='utf-8')
            requests = [
                {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
                 'params': {'protocolVersion': '2025-11-25', 'capabilities': {},
                            'clientInfo': {'name': 'test', 'version': '1'}}},
                {'jsonrpc': '2.0', 'method': 'notifications/initialized'},
                {'jsonrpc': '2.0', 'id': 2, 'method': 'tools/list'},
                {'jsonrpc': '2.0', 'id': 3, 'method': 'tools/call',
                 'params': {'name': 'ecc_status', 'arguments': {}}},
                {'jsonrpc': '2.0', 'id': 4, 'method': 'tools/call',
                 'params': {'name': 'ecc_operation', 'arguments': {'operation_id': 'op-1'}}},
                {'jsonrpc': '2.0', 'id': 5, 'method': 'tools/call',
                 'params': {'name': 'ecc_submit_operation', 'arguments': {
                     'command': 'start', 'operation_id': 'run-bad', 'confirmation': 'no'}}},
                {'jsonrpc': '2.0', 'id': 6, 'method': 'tools/call',
                 'params': {'name': 'ecc_submit_operation', 'arguments': {
                     'command': 'start', 'operation_id': 'run-1-start',
                     'confirmation': 'I_CONFIRM_start_run-1-start'}}},
                {'jsonrpc': '2.0', 'id': 7, 'method': 'tools/call',
                 'params': {'name': 'ecc_catalog_run', 'arguments': {
                     'experiment': 'Exp', 'run_number': 12, 'replace_completed_run': True,
                     'operation_id': 'run-12-catalog', 'confirmation': 'wrong'}}},
                {'jsonrpc': '2.0', 'id': 8, 'method': 'tools/call',
                 'params': {'name': 'ecc_catalog_run', 'arguments': {
                     'experiment': 'Exp', 'run_number': 12, 'replace_completed_run': True,
                     'operation_id': 'run-12-catalog', 'confirmation': 'I_CONFIRM_catalog_run-12-catalog'}}},
                {'jsonrpc': '2.0', 'id': 9, 'method': 'tools/call',
                 'params': {'name': 'ecc_resume_catalog_replacement', 'arguments': {
                     'failed_operation_id': 'failed-catalog', 'operation_id': 'resume-12-catalog',
                     'confirmation': 'wrong'}}},
                {'jsonrpc': '2.0', 'id': 10, 'method': 'tools/call',
                 'params': {'name': 'ecc_resume_catalog_replacement', 'arguments': {
                     'failed_operation_id': 'failed-catalog', 'operation_id': 'resume-12-catalog',
                     'confirmation': 'I_CONFIRM_catalog_recovery_failed-catalog_resume-12-catalog'}}},
                {'jsonrpc': '2.0', 'id': 11, 'method': 'tools/call',
                 'params': {'name': 'ecc_configure_cobo', 'arguments': {
                     'cobo_id': 'C0', 'operation_id': 'configure-C0-1', 'confirmation': 'wrong'}}},
                {'jsonrpc': '2.0', 'id': 12, 'method': 'tools/call',
                 'params': {'name': 'ecc_configure_cobo', 'arguments': {
                     'cobo_id': 'C0', 'operation_id': 'configure-C0-1',
                     'confirmation': 'I_CONFIRM_configure-cobo_C0_configure-C0-1'}}},
                {'jsonrpc': '2.0', 'id': 13, 'method': 'tools/call',
                 'params': {'name': 'ecc_hardware_state', 'arguments': {}}},
                {'jsonrpc': '2.0', 'id': 14, 'method': 'tools/call',
                 'params': {'name': 'ecc_reprepare_cobo', 'arguments': {
                     'cobo_id': 'C0', 'operation_id': 'reprepare-C0-1', 'confirmation': 'wrong'}}},
                {'jsonrpc': '2.0', 'id': 15, 'method': 'tools/call',
                 'params': {'name': 'ecc_reprepare_cobo', 'arguments': {
                     'cobo_id': 'C0', 'operation_id': 'reprepare-C0-1',
                     'confirmation': 'I_CONFIRM_reprepare-cobo_C0_reprepare-C0-1'}}},
                {'jsonrpc': '2.0', 'id': 16, 'method': 'tools/call',
                 'params': {'name': 'ecc_cobo_lifecycles', 'arguments': {}}},
                {'jsonrpc': '2.0', 'id': 17, 'method': 'tools/call',
                 'params': {'name': 'ecc_operation', 'arguments': {'operation_id': 'prepare-C0-0'}}},
            ]
            replies = send(binary, config, requests)
            if replies[0]['result']['protocolVersion'] != '2025-11-25':
                raise AssertionError('Wrong MCP protocol negotiation')
            tools = replies[1]['result']['tools']
            by_name = {tool['name']: tool for tool in tools}
            if ('ecc_submit_operation' not in by_name or 'ecc_catalog_run' not in by_name or
                    'ecc_resume_catalog_replacement' not in by_name or 'ecc_configure_cobo' not in by_name or
                    'ecc_reprepare_cobo' not in by_name or
                    'ecc_cobo_lifecycles' not in by_name or
                    'ecc_hardware_state' not in by_name or
                    by_name['ecc_operation'].get('annotations', {}).get('readOnlyHint') is not True or
                    'run_number' not in by_name['ecc_catalog_run']['inputSchema']['required'] or
                    'failed_operation_id' not in by_name['ecc_resume_catalog_replacement']['inputSchema']['required'] or
                    'operation_id' not in by_name['ecc_operation']['inputSchema']['required']):
                raise AssertionError('Tool schemas are incomplete')
            if json.loads(content_text(replies[2]))['kind'] != 'status':
                raise AssertionError('Read-only status call failed')
            if json.loads(content_text(replies[3]))['phase'] != 'complete':
                raise AssertionError('Operation observation failed')
            if replies[4]['result'].get('isError') is not True:
                raise AssertionError('Invalid lifecycle confirmation was not rejected')
            submitted = json.loads(content_text(replies[5]))
            if submitted.get('phase') != 'accepted':
                raise AssertionError('Lifecycle submit did not reach the authenticated service')
            if replies[6]['result'].get('isError') is not True:
                raise AssertionError('Invalid catalog confirmation was not rejected')
            cataloged = json.loads(content_text(replies[7]))
            if cataloged.get('phase') != 'accepted' or len(server.operations) != 5:
                raise AssertionError('Explicit catalog request did not reach the authenticated service')
            if replies[8]['result'].get('isError') is not True:
                raise AssertionError('Invalid catalog recovery confirmation was not rejected')
            resumed = json.loads(content_text(replies[9]))
            if resumed.get('phase') != 'accepted' or len(server.operations) != 5:
                raise AssertionError('Explicit catalog recovery did not reach the authenticated service')
            if replies[10]['result'].get('isError') is not True:
                raise AssertionError('Configure CoBo accepted an invalid confirmation')
            configured = json.loads(content_text(replies[11]))
            if configured.get('phase') != 'accepted' or len(server.operations) != 5:
                raise AssertionError('Explicit selected-CoBo Configure did not reach the authenticated service')
            observation = json.loads(content_text(replies[12]))
            if observation.get('kind') != 'getdaq.hardware-state-observation' or observation.get('cobo_lifecycle_is_hardware_readback') is not False:
                raise AssertionError('Read-only fresh hardware-state observation failed')
            if replies[13]['result'].get('isError') is not True:
                raise AssertionError('Reprepare CoBo accepted an invalid confirmation')
            reprepared = json.loads(content_text(replies[14]))
            if reprepared.get('phase') != 'accepted' or len(server.operations) != 5:
                raise AssertionError('Explicit selected-CoBo reprepare did not reach the authenticated service')
            lifecycles = json.loads(content_text(replies[15]))
            if (lifecycles.get('kind') != 'getdaq.ecc-cobo-maintenance-targets' or
                    lifecycles.get('cobos', [{}])[0].get('lifecycle_state') != 'prepared' or
                    lifecycles.get('cobos', [{}])[0].get('operation_id') != 'prepare-C0-0'):
                raise AssertionError('Read-only per-CoBo lifecycle projection failed')
            operation_lookup = json.loads(content_text(replies[16]))
            if operation_lookup.get('operation_id') != 'prepare-C0-0' or operation_lookup.get('phase') != 'complete':
                raise AssertionError('Per-CoBo operation evidence could not be followed to its exact operation')
            for field, value in (('target_snapshot', 'c' * 64), ('duplicate_targets', True)):
                before_operations = len(server.operations)
                setattr(server, field, value)
                invalid_projection_call = {'jsonrpc': '2.0', 'id': 17, 'method': 'tools/call',
                                           'params': {'name': 'ecc_cobo_lifecycles', 'arguments': {}}}
                invalid_projection = send(binary, config, requests[:2] + [invalid_projection_call])[1]
                setattr(server, field, {'target_snapshot': DIGEST, 'duplicate_targets': False}[field])
                if invalid_projection['result'].get('isError') is not True or len(server.operations) != before_operations:
                    raise AssertionError('Lifecycle projection accepted invalid identity/target structure: ' + field)
            operation = server.operations[0]
            if (operation.get('schema') != 1 or operation.get('operation_id') != 'run-1-start' or
                    operation.get('command') != 'start' or operation.get('instance_id') != DIGEST or
                    operation.get('snapshot_sha256') != DIGEST or operation.get('client_id') != 'mcp-test' or
                    len(operation.get('client_epoch', '')) != 64):
                raise AssertionError('Lifecycle request binding is incomplete')
            if Path(base['client_epoch_file']).read_text(encoding='ascii').strip() != operation['client_epoch']:
                raise AssertionError('Client epoch was not persisted')
            catalog_operation = server.operations[1]
            if (catalog_operation.get('command') != 'catalog' or
                    catalog_operation.get('catalog_experiment') != 'Exp' or
                    catalog_operation.get('catalog_run_number') != 12 or
                    catalog_operation.get('catalog_replace_existing') is not True or
                    catalog_operation.get('client_epoch') != operation['client_epoch']):
                raise AssertionError('Catalog authorization or replacement binding is incomplete')
            recovery_operation = server.operations[2]
            if (recovery_operation.get('command') != 'resume-catalog-replacement' or
                    recovery_operation.get('failed_operation_id') != 'failed-catalog' or
                    recovery_operation.get('operation_id') != 'resume-12-catalog' or
                    recovery_operation.get('client_epoch') != operation['client_epoch']):
                raise AssertionError('Catalog recovery did not bind the original failure and owner epoch')
            configure_operation = server.operations[3]
            if (configure_operation.get('command') != 'configure-cobo' or configure_operation.get('cobo_id') != 'C0' or
                    configure_operation.get('operation_id') != 'configure-C0-1' or configure_operation.get('client_epoch') != operation['client_epoch']):
                raise AssertionError('Configure CoBo did not bind the exact selected target and owner epoch')
            reprepare_operation = server.operations[4]
            if (reprepare_operation.get('command') != 'reprepare-cobo' or reprepare_operation.get('cobo_id') != 'C0' or
                    reprepare_operation.get('operation_id') != 'reprepare-C0-1' or reprepare_operation.get('client_epoch') != operation['client_epoch']):
                raise AssertionError('Reprepare CoBo did not bind the exact selected target and owner epoch')
            if len(server.requests) != 32:
                raise AssertionError('Invalid confirmation unexpectedly caused an HTTP request')

            server.lifecycle_state = 'configured'
            configured_reprepare = {'jsonrpc': '2.0', 'id': 16, 'method': 'tools/call',
                                    'params': {'name': 'ecc_reprepare_cobo', 'arguments': {
                                        'cobo_id': 'C0', 'operation_id': 'reprepare-configured-C0',
                                        'confirmation': 'I_CONFIRM_reprepare-cobo_C0_reprepare-configured-C0'}}}
            configured_reply = send(binary, config, requests[:2] + [configured_reprepare])[1]
            server.lifecycle_state = 'prepared'
            if (configured_reply['result'].get('isError') or len(server.operations) != 6 or
                    server.operations[-1].get('command') != 'reprepare-cobo' or
                    server.operations[-1].get('operation_id') != 'reprepare-configured-C0'):
                raise AssertionError('Known Configured CoBo could not be reprepared')
            if len(server.requests) != 38:
                raise AssertionError('Unexpected request count for Configured CoBo reprepare: %d' % len(server.requests))

            # A second adapter instance without a local epoch must inherit the server's
            # already-bound owner epoch instead of accidentally attempting takeover.
            second_epoch = root / 'second-client.epoch'
            base['client_epoch_file'] = str(second_epoch)
            config.write_text(json.dumps(base), encoding='utf-8')
            second_request = requests[5]
            second_request['id'] = 7
            second_request['params']['arguments'] = {
                'command': 'stop', 'operation_id': 'run-2-stop',
                'confirmation': 'I_CONFIRM_stop_run-2-stop'}
            second = send(binary, config, requests[:2] + [second_request])
            if second[1]['result'].get('isError'):
                raise AssertionError('MCP did not reuse the established ECC owner epoch')
            if (Path(second_epoch).read_text(encoding='ascii').strip() != operation['client_epoch'] or
                    server.operations[-1]['client_epoch'] != operation['client_epoch']):
                raise AssertionError('A second adapter instance attempted to take over ECC ownership')
            if len(server.requests) != 41:
                raise AssertionError('Unexpected request count during owner epoch reuse')

            busy_request = {'jsonrpc': '2.0', 'id': 8, 'method': 'tools/call',
                            'params': {'name': 'ecc_submit_operation', 'arguments': {
                                'command': 'pause', 'operation_id': 'run-3-pause',
                                'confirmation': 'I_CONFIRM_pause_run-3-pause'}}}
            busy = send(binary, config, requests[:2] + [busy_request])[1]
            busy_text = content_text(busy)
            if (busy['result'].get('isError') is not True or 'status 409' not in busy_text or
                    'ECC submission busy; not queued' not in busy_text):
                raise AssertionError('MCP did not preserve the ECC busy/not-queued reason: ' + repr(busy))
            if len(server.operations) != 7 or len(server.requests) != 44:
                raise AssertionError('Busy rejection was queued or caused an unexpected HTTP request')

            # Selected-board Reprepare must fail closed on unsafe lifecycle states.
            reprepare_args = {'cobo_id': 'C0', 'operation_id': 'reprepare-gated',
                              'confirmation': 'I_CONFIRM_reprepare-cobo_C0_reprepare-gated'}
            for field, value in (('lifecycle_state', 'unknown'),
                                 ('controller_state', 2), ('operation_active', True)):
                before_operations = len(server.operations)
                before_requests = len(server.requests)
                setattr(server, field, value)
                gated_call = {'jsonrpc': '2.0', 'id': 9, 'method': 'tools/call',
                               'params': {'name': 'ecc_reprepare_cobo', 'arguments': reprepare_args}}
                gated_reply = send(binary, config, requests[:2] + [gated_call])[1]
                setattr(server, field, {'lifecycle_state': 'prepared',
                                        'controller_state': 3, 'operation_active': False}[field])
                if (gated_reply['result'].get('isError') is not True or
                        len(server.operations) != before_operations or
                        any(method == 'POST' for method, _path in server.requests[before_requests:])):
                    raise AssertionError('Reprepare safety preflight did not fail closed for ' + field)
            if len(server.requests) != 52:
                raise AssertionError('Unexpected request count during Reprepare safety preflight')

            base['allow_control'] = False
            config.write_text(json.dumps(base), encoding='utf-8')
            readonly = send(binary, config, requests[:3] + [requests[5], requests[7], requests[9], requests[15], requests[16], requests[17]])
            readonly_names = {tool['name'] for tool in readonly[1]['result']['tools']}
            if ('ecc_submit_operation' in readonly_names or 'ecc_catalog_run' in readonly_names or
                    'ecc_resume_catalog_replacement' in readonly_names or 'ecc_configure_cobo' in readonly_names or
                    'ecc_reprepare_cobo' in readonly_names):
                raise AssertionError('Control tool leaked into read-only mode')
            if 'ecc_cobo_lifecycles' not in readonly_names:
                raise AssertionError('Read-only lifecycle projection was hidden in read-only mode')
            if (readonly[2]['result'].get('isError') is not True or
                    readonly[3]['result'].get('isError') is not True or
                    readonly[4]['result'].get('isError') is not True or
                    readonly[5]['result'].get('isError') is not True or len(server.requests) != 55):
                raise AssertionError('Read-only MCP mode allowed a hidden control call: errors=%r count=%d' %
                                     ([reply.get('result', {}).get('isError') for reply in readonly[2:6]],
                                      len(server.requests)))
            if json.loads(content_text(readonly[6])).get('kind') != 'getdaq.ecc-cobo-maintenance-targets':
                raise AssertionError('Read-only per-CoBo lifecycle call failed in read-only mode')
            if json.loads(content_text(readonly[7])).get('operation_id') != 'prepare-C0-0':
                raise AssertionError('Read-only operation query failed in read-only mode')

            oversized = subprocess.run([binary, '--config', str(config)],
                                       input=('x' * (1024 * 1024 + 1)) + '\n', text=True,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       timeout=10, check=True)
            error = json.loads(oversized.stdout)
            if error.get('error', {}).get('code') != -32700:
                raise AssertionError('Oversized MCP message was not rejected')
        print('MCP stdio, tool schema, read-only calls, lifecycle/CoBo/catalog/recovery confirmation, owner reuse and busy/not-queued error reporting passed')
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


if __name__ == '__main__':
    main()
