#!/usr/bin/env python3
"""Exercise the catalog sender against a temporary, key-only localhost sshd."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import pwd
import re
import shutil
import signal
import socket
import subprocess
import tempfile
import time


def run(command, **kwargs):
    return subprocess.run(command, check=True, text=True, capture_output=True, **kwargs)


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def stop(process):
    if process is None or process.poll() is not None:
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test', type=Path, required=True)
    parser.add_argument('--catalog', type=Path, required=True,
                        help='same-build getdaq-catalog passed to the test binary')
    parser.add_argument('--remote-catalog', type=Path, required=True,
                        help='Ubuntu 24 candidate getdaq-catalog used by the SSH server side')
    parser.add_argument('--loader', type=Path, required=True)
    parser.add_argument('--library-path', required=True)
    parser.add_argument('--evidence-dir', type=Path, required=True)
    parser.add_argument('--large-file-bytes', type=int, default=16 * 1024,
                        help='bytes in each source data0.graw; data1.graw remains 4096 bytes')
    parser.add_argument('--payload-budget-bytes', type=int, default=1024 * 1024,
                        help='hard upper bound for the SSH fixture payload, checked before writing')
    args = parser.parse_args()
    if os.name != 'posix' or not Path('/proc').exists():
        raise SystemExit('This loopback sshd integration runner requires Linux/POSIX.')
    paths = {name: getattr(args, name).resolve(strict=True) for name in
             ('test', 'catalog', 'remote_catalog', 'loader')}
    openssh = {name: shutil.which(name) for name in ('sshd', 'ssh', 'ssh-keygen', 'ssh-keyscan')}
    for name, executable in openssh.items():
        if not executable:
            raise SystemExit('Required OpenSSH program is missing: ' + name)
    evidence = args.evidence_dir.resolve()
    aggregate_limit = 20 * 1024**3
    expected_payload = 2 * (args.large_file_bytes + 4096)
    if args.large_file_bytes <= 0 or args.payload_budget_bytes <= 0:
        raise SystemExit('Synthetic file size and payload budget must be positive.')
    if args.payload_budget_bytes > aggregate_limit or expected_payload > args.payload_budget_bytes:
        raise SystemExit('SSH fixture exceeds its payload budget or the 20-GiB aggregate limit.')
    evidence.mkdir(parents=True, exist_ok=False)
    user = pwd.getpwuid(os.getuid()).pw_name
    server = None
    test_result = None
    test_log = ''
    deleted = []
    source_payload_bytes = 0
    source_payload_files = 0
    with tempfile.TemporaryDirectory(prefix='getdaq-ssh-loopback-') as temporary, \
            tempfile.TemporaryDirectory(prefix='getdaq-ssh-shm-', dir='/dev/shm') as shared_temporary:
        work = Path(temporary)
        shared_root = Path(shared_temporary)
        ssh_home = work / 'ssh-home'
        ssh_home.mkdir(mode=0o700)
        host_key = work / 'host_key'
        client_key = ssh_home / 'id_ed25519'
        run([openssh['ssh-keygen'], '-q', '-t', 'ed25519', '-N', '', '-f', str(host_key)])
        run([openssh['ssh-keygen'], '-q', '-t', 'ed25519', '-N', '', '-f', str(client_key)])
        authorized = ssh_home / 'authorized_keys'
        authorized.write_bytes(Path(str(client_key) + '.pub').read_bytes())
        authorized.chmod(0o600)

        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        server_config = work / 'sshd_config'
        server_log = work / 'sshd.log'
        server_config.write_text('\n'.join([
            'Port ' + str(port), 'ListenAddress 127.0.0.1',
            'HostKey ' + str(host_key), 'PidFile ' + str(work / 'sshd.pid'),
            'AuthorizedKeysFile ' + str(authorized), 'StrictModes no',
            'PubkeyAuthentication yes', 'AuthenticationMethods publickey',
            'PasswordAuthentication no', 'KbdInteractiveAuthentication no',
            'PermitRootLogin no', 'UsePAM no', 'UsePrivilegeSeparation no',
            'AllowUsers ' + user,
            'Subsystem sftp internal-sftp', 'LogLevel VERBOSE', '']), encoding='utf-8')
        run([openssh['sshd'], '-t', '-f', str(server_config)])
        sshd_log = server_log.open('w')
        server = subprocess.Popen([openssh['sshd'], '-D', '-e', '-f', str(server_config)],
                                  stdout=sshd_log, stderr=subprocess.STDOUT,
                                  start_new_session=True)
        try:
            config = ssh_home / 'config'
            known_hosts = ssh_home / 'known_hosts'
            config.write_text('\n'.join([
                'Host getdaq-catalog-loopback', '  HostName 127.0.0.1',
                '  User ' + user, '  Port ' + str(port),
                '  IdentityFile ' + str(client_key), '  IdentitiesOnly yes',
                '  UserKnownHostsFile ' + str(known_hosts),
                '  StrictHostKeyChecking accept-new', '  BatchMode yes', '']),
                encoding='utf-8')
            config.chmod(0o600)
            # Install a command shim only to pass the isolated config file. The
            # transport itself is the system OpenSSH client and temporary sshd.
            shim_dir = work / 'bin'
            shim_dir.mkdir()
            shim = shim_dir / 'ssh'
            real_ssh = openssh['ssh']
            shim.write_text('#!/bin/sh\nexec ' + real_ssh +
                            ' -F "$GETDAQ_TEST_SSH_CONFIG" "$@"\n', encoding='utf-8')
            shim.chmod(0o700)

            ready = False
            for _ in range(100):
                if server.poll() is not None:
                    break
                try:
                    with socket.create_connection(('127.0.0.1', port), timeout=0.1):
                        ready = True
                        break
                except OSError:
                    time.sleep(0.05)
            if not ready:
                raise RuntimeError('Temporary sshd did not listen on localhost: ' +
                                   server_log.read_text(errors='replace')[-3000:])
            known = run([openssh['ssh-keyscan'], '-T', '2', '-p', str(port), '127.0.0.1'])
            known_hosts.write_text(known.stdout, encoding='utf-8')
            known_hosts.chmod(0o600)
            run([real_ssh, '-F', str(config), 'getdaq-catalog-loopback', 'true'], timeout=10)

            remote_root = work / 'remote-root'
            remote_root.mkdir(mode=0o700)
            remote_program = work / 'remote-getdaq-catalog'
            library_dirs = [Path(item).resolve(strict=True) for item in args.library_path.split(':') if item]
            if not library_dirs:
                raise RuntimeError('At least one absolute Ubuntu 24 library directory is required.')
            loader_libs = ':'.join(str(item) for item in library_dirs)
            remote_program.write_text(
                '#!/bin/sh\nexec ' + str(paths['loader']) +
                ' --library-path ' + loader_libs + ' ' + str(paths['remote_catalog']) + ' "$@"\n',
                encoding='utf-8')
            remote_program.chmod(0o700)
            env = dict(os.environ)
            env.update({
                'PATH': str(shim_dir) + os.pathsep + env.get('PATH', ''),
                'GETDAQ_TEST_SSH_CONFIG': str(config),
                'GETDAQ_CATALOG_TEST_REAL_SSH': '1',
                'GETDAQ_CATALOG_TEST_REAL_SSH_FILE_BYTES': str(args.large_file_bytes),
                'GETDAQ_CATALOG_TEST_REAL_SSH_PAYLOAD_BUDGET_BYTES': str(args.payload_budget_bytes),
                'GETDAQ_CATALOG_TEST_SSH_TARGET': user + '@getdaq-catalog-loopback',
                'GETDAQ_CATALOG_TEST_REMOTE_PROGRAM': str(remote_program),
                'GETDAQ_CATALOG_TEST_REMOTE_ROOT': str(remote_root),
                'GETDAQ_CATALOG_TEST_ROOT': str(work / 'source-fixtures'),
                'GETDAQ_TRANSFER_TEST_SHM_ROOT': str(shared_root),
            })
            (work / 'source-fixtures').mkdir(mode=0o700)
            completed = subprocess.run([str(paths['test']), str(paths['catalog'])],
                                       text=True, capture_output=True, timeout=180, env=env)
            test_log = completed.stdout + completed.stderr
            (evidence / 'catalog-ssh-loopback.log').write_text(test_log, encoding='utf-8')
            if completed.returncode:
                raise RuntimeError('Catalog transfer test failed: ' + test_log[-5000:] +
                                   '\nsshd: ' + server_log.read_text(errors='replace')[-3000:])
            if 'PASS real OpenSSH loopback transfer;' not in test_log:
                raise RuntimeError('Test binary did not run the real OpenSSH scenario.')
            match = re.search(r'PASS real OpenSSH loopback transfer; payload=(\d+) bytes;', test_log)
            if not match or int(match.group(1)) != expected_payload:
                raise RuntimeError('Real OpenSSH transfer size differs from the preflight budget estimate.')
            source_cleanup = re.search(
                r'PASS real OpenSSH loopback transfer; payload=\d+ bytes; source_fixture_removed=true; '
                r'source_raw_bytes=(\d+); source_raw_files=(\d+);', test_log)
            if not source_cleanup:
                raise RuntimeError('C++ test did not confirm removal of its source fixture and raw files.')
            source_payload_bytes, source_payload_files = map(int, source_cleanup.groups())
            fixture_root = work / 'source-fixtures'
            if source_payload_bytes != expected_payload or source_payload_files != 4 or list(fixture_root.glob('catalog-*')):
                raise RuntimeError('Source fixture cleanup receipt differs from the generated raw files.')
            test_result = {
                'status': 'passed', 'transport': 'system OpenSSH client and temporary localhost sshd',
                'runner_sha256': sha(Path(__file__).resolve()),
                'test_source_sha256': sha(Path(__file__).resolve().parents[1] / 'tests/catalog_transfer_test.cpp'),
                'test_binary_sha256': sha(paths['test']),
                'client_catalog_sha256': sha(paths['catalog']),
                'remote_candidate_catalog_sha256': sha(paths['remote_catalog']),
                'remote_root_scope': 'private temporary localhost root; removed after test',
                'target': '127.0.0.1 only', 'temporary_sshd_stopped': True,
                'temporary_keys_removed': True,
                'payload_budget_bytes': args.payload_budget_bytes,
                'actual_ssh_payload_bytes': expected_payload,
                'synthetic_large_file_bytes_per_source': args.large_file_bytes,
                'aggregate_test_limit_bytes': aggregate_limit,
                'source_fixture_removed': True,
                'source_raw_deleted_bytes': source_payload_bytes,
                'source_raw_deleted_files': source_payload_files,
                'raw_remaining': 0,
            }
        finally:
            stop(server)
            sshd_log.close()
        for parent in (work, shared_root):
            for raw in parent.rglob('*.graw'):
                if raw.is_file() and not raw.is_symlink():
                    size = raw.stat().st_size
                    deleted.append({'relative_path': str(raw.relative_to(parent)), 'bytes': size})
                    raw.unlink()
            if list(parent.rglob('*.graw')):
                raise RuntimeError('Raw test payload remains after explicit cleanup.')
    deleted_total = sum(item['bytes'] for item in deleted)
    if deleted_total > aggregate_limit:
        raise RuntimeError("Test cleanup exceeded the user's aggregate 20-GiB limit.")
    test_result.update({
        'target_raw_deleted_bytes': deleted_total,
        'target_raw_deleted_files': len(deleted),
        'raw_deleted_bytes': deleted_total + source_payload_bytes,
        'raw_deleted_files': len(deleted) + source_payload_files,
        'raw_remaining': 0,
    })
    (evidence / 'result.json').write_text(json.dumps(test_result, indent=2) + '\n', encoding='utf-8')
    print('PASS real OpenSSH localhost catalog transfer; transferred=' + str(test_result['actual_ssh_payload_bytes']) +
          ' bytes; source and target raw deleted=' + str(test_result['raw_deleted_bytes']) + ' bytes; evidence=' + str(evidence))


if __name__ == '__main__':
    main()
