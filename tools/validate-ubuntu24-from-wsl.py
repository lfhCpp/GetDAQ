#!/usr/bin/env python3
"""Build/test in a project-owned Ubuntu24 userspace using the existing WSL distribution.

The official base image is authenticated before extraction. Bootstrap uses root
only in the dedicated filesystem; build commands run as uid 1000. This is a
userspace compatibility environment, not an independent Ubuntu24 kernel/machine.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import urllib.request
import uuid

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build-ubuntu24-validation'
IMAGE='ubuntu-base-24.04.5-base-amd64.tar.gz'
URL='https://cdimage.ubuntu.com/ubuntu-base/releases/24.04/release/'
FINGERPRINT='843938DF228D22F7B3742BC0D94AA3F0EFE21092'
PACKAGES=['build-essential','cmake','python3','qtbase5-dev','qt5-qmake','pkg-config','fonts-dejavu-core','ca-certificates']


def sha(path):
    digest=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()


def write_json(path,value):
    temporary=path.with_name(path.name+'.tmp')
    temporary.write_text(json.dumps(value,indent=2)+'\n');temporary.replace(path)


def require(okay,message):
    if not okay:raise RuntimeError(message)


def authenticate(download=False):
    directory=BASE/'downloads';directory.mkdir(parents=True,exist_ok=True)
    names=['SHA256SUMS','SHA256SUMS.gpg',IMAGE]
    for name in names:
        path=directory/name
        if not path.exists():
            require(download,'Missing official base input; run bootstrap to download it: '+str(path))
            maximum=40*1024*1024 if name==IMAGE else 32*1024
            temporary=path.with_name(path.name+'.partial')
            require(not temporary.exists(),'A partial download exists; inspect it before retrying: '+str(temporary))
            with urllib.request.urlopen(URL+name,timeout=60) as source,temporary.open('xb') as output:
                total=0
                for block in iter(lambda:source.read(1024*1024),b''):
                    total+=len(block);require(total<=maximum,'Official base download exceeds budget');output.write(block)
            temporary.replace(path)
        require(path.is_file() and not path.is_symlink(),'Invalid official base input')
    keyrings=[Path('/usr/share/keyrings/ubuntu-archive-keyring.gpg'),Path('/usr/share/keyrings/ubuntu-cloudimage-keyring.gpg')]
    command=['gpgv','--status-fd','1']
    for ring in keyrings:
        require(ring.is_file(),'Trusted installed Ubuntu keyring is missing: '+str(ring));command+=['--keyring',str(ring)]
    result=subprocess.run(command+[str(directory/'SHA256SUMS.gpg'),str(directory/'SHA256SUMS')],
                          stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    (directory/'signature-verification.log').write_text(result.stdout)
    require(result.returncode==0 and '[GNUPG:] VALIDSIG '+FINGERPRINT+' ' in result.stdout,'Ubuntu image signature verification failed')
    expected=[line.split()[0] for line in (directory/'SHA256SUMS').read_text().splitlines()
              if line.split() and line.split()[-1].lstrip('*')==IMAGE]
    require(expected==[sha(directory/IMAGE)],'Ubuntu image SHA256 differs from authenticated checksum')
    receipt={'schema':'getdaq.ubuntu24-base:1','status':'verified','image':IMAGE,'image_sha256':expected[0],
             'fingerprint':FINGERPRINT,'trusted_keyrings':{str(p):sha(p) for p in keyrings},
             'files':{name:{'url':URL+name,'sha256':sha(directory/name),'bytes':(directory/name).stat().st_size} for name in names}}
    write_json(directory/'download-receipt.json',receipt)
    return receipt


def mount_environment(rootfs):
    # Give the chroot a visible root mount. Without this, /proc/self/mountinfo
    # lists only child mounts and QStorageInfo cannot resolve /build even
    # though statvfs succeeds. This bind lives in the private mount namespace.
    subprocess.run(['mount','--bind',str(rootfs),str(rootfs)],check=True)
    for name in ['proc','dev','dev/pts','dev/shm','code']:(rootfs/name).mkdir(parents=True,exist_ok=True)
    for name,major,minor in [('null',1,3),('zero',1,5),('random',1,8),('urandom',1,9)]:
        path=rootfs/'dev'/name
        if path.is_symlink():path.unlink()
        if not path.exists():os.mknod(str(path),stat.S_IFCHR|0o666,os.makedev(major,minor))
        os.chmod(str(path),0o666)
    subprocess.run(['mount','-t','proc','proc',str(rootfs/'proc')],check=True)
    subprocess.run(['mount','-t','devpts','-o','newinstance,ptmxmode=0666,mode=0620','devpts',str(rootfs/'dev/pts')],check=True)
    # Several filesystem regressions require a genuinely distinct st_dev.
    # Keep their private synthetic copies on a bounded tmpfs and never use the
    # host's shared /dev/shm from inside the target userspace.
    subprocess.run(['mount','-t','tmpfs','-o','size=256m,nosuid,nodev,mode=1777','tmpfs',str(rootfs/'dev/shm')],check=True)
    ptmx=rootfs/'dev/ptmx'
    if ptmx.is_symlink() or ptmx.exists():ptmx.unlink()
    ptmx.symlink_to('pts/ptmx')
    subprocess.run(['mount','--bind',str(ROOT),str(rootfs/'code')],check=True)
    subprocess.run(['mount','-o','remount,bind,ro',str(rootfs/'code')],check=True)


def internal_root(action,plan=None):
    require(os.getuid()==0,'Internal bootstrap must use the explicit WSL root launcher')
    require(str(BASE.resolve())==str(BASE) and not BASE.is_symlink(),'Validation base must be canonical')
    rootfs=BASE/'rootfs';marker=rootfs/'.getdaq-ubuntu24-validation.json'
    receipt=json.loads((BASE/'downloads/download-receipt.json').read_text())
    require(receipt['status']=='verified' and receipt['fingerprint']==FINGERPRINT,'Authenticated image receipt required')
    if not rootfs.exists():
        require(action=='bootstrap','Bootstrap the Ubuntu24 filesystem first')
        image=BASE/'downloads'/IMAGE;require(sha(image)==receipt['image_sha256'],'Base image changed before extraction')
        with tarfile.open(str(image),'r:gz') as archive:
            for member in archive.getmembers():
                path=Path(member.name)
                require(not path.is_absolute() and '..' not in path.parts,'Unexpected base image member path')
        rootfs.mkdir(mode=0o755)
        subprocess.run(['tar','--extract','--preserve-permissions','--numeric-owner','--file',str(image),'--directory',str(rootfs)],check=True)
        write_json(marker,{'schema':'getdaq.ubuntu24-filesystem:1','image_sha256':receipt['image_sha256'],'status':'extracted'})
    require(rootfs.is_dir() and not rootfs.is_symlink() and marker.is_file(),'Existing filesystem lacks the dedicated ownership marker')
    owned=json.loads(marker.read_text());require(owned['schema']=='getdaq.ubuntu24-filesystem:1' and owned['image_sha256']==receipt['image_sha256'],'Filesystem image binding differs')
    if action=='bootstrap' and owned.get('status')=='ready':
        print('Ubuntu24 validation filesystem already ready; existing toolchain left unchanged',flush=True)
        return 0
    mount_environment(rootfs)
    clean_env=['/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin','HOME=/root','LC_ALL=C.UTF-8','DEBIAN_FRONTEND=noninteractive']
    def chroot(arguments):return subprocess.run(['chroot',str(rootfs),*clean_env,*arguments],check=True)
    if action=='bootstrap':
        # Replace only files in this newly provisioned base, never the host's apt settings.
        resolver=rootfs/'etc/resolv.conf'
        if resolver.is_symlink():resolver.unlink()
        resolver.write_bytes(Path('/etc/resolv.conf').read_bytes())
        certificates=rootfs/'etc/ssl/certs/ca-certificates.crt';certificates.parent.mkdir(parents=True,exist_ok=True)
        if certificates.is_symlink():certificates.unlink()
        certificates.write_bytes(Path('/etc/ssl/certs/ca-certificates.crt').read_bytes())
        (rootfs/'usr/sbin/policy-rc.d').write_text('#!/bin/sh\nexit 101\n');os.chmod(str(rootfs/'usr/sbin/policy-rc.d'),0o755)
        sources=rootfs/'etc/apt/sources.list.d';sources.mkdir(exist_ok=True)
        for path in list(sources.glob('*.list'))+list(sources.glob('*.sources')):path.unlink()
        legacy=rootfs/'etc/apt/sources.list'
        if legacy.exists():legacy.write_text('# Dedicated validation root: see sources.list.d/ubuntu.sources\n')
        (sources/'ubuntu.sources').write_text('Types: deb\nURIs: https://archive.ubuntu.com/ubuntu\nSuites: noble noble-updates noble-backports\nComponents: main universe\nSigned-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n\nTypes: deb\nURIs: https://security.ubuntu.com/ubuntu\nSuites: noble-security\nComponents: main universe\nSigned-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n')
        chroot(['/usr/bin/apt-get','-o','Dpkg::Use-Pty=0','update'])
        chroot(['/usr/bin/apt-get','-o','Dpkg::Use-Pty=0','--yes','--no-install-recommends','install',*PACKAGES])
        if 'getdaq-build:' not in (rootfs/'etc/passwd').read_text():
            chroot(['/usr/sbin/useradd','--uid','1000','--create-home','--shell','/bin/bash','getdaq-build'])
        for name in ['build','build/tmp','build/runtime','build/cache']:
            path=rootfs/name;path.mkdir(exist_ok=True);os.chown(str(path),1000,1000);os.chmod(str(path),0o700)
        os.chmod(str(rootfs/'tmp'),0o1777)
        versions=subprocess.check_output(['chroot',str(rootfs),'/usr/bin/dpkg-query','-W','-f=${Package}\t${Version}\n'],text=True)
        owned.update(status='ready',packages_requested=PACKAGES,installed_packages=versions.splitlines(),
                     kernel_is_host_wsl=True,independent_machine_accepted=False)
        write_json(marker,owned)
        write_json(BASE/'environment-receipt.json',owned)
        print('PASS official Ubuntu24 userspace provisioned; host WSL kernel; no independent-machine acceptance',flush=True)
        return 0
    require(action=='run' and owned['status']=='ready','A ready Ubuntu24 filesystem is required')
    require(plan is not None and plan.parent==BASE/'tasks' and plan.is_file() and not plan.is_symlink(),'Invalid validation command plan')
    inside='/code/'+str(plan.relative_to(ROOT))
    command=['chroot','--userspec=1000:1000','--groups=1000',str(rootfs),'/usr/bin/env','-i',
             'PATH=/usr/sbin:/usr/bin:/sbin:/bin','HOME=/home/getdaq-build','LANG=C.UTF-8','LC_ALL=C.UTF-8',
             'TMPDIR=/build/tmp','XDG_RUNTIME_DIR=/build/runtime','XDG_CACHE_HOME=/build/cache',
             'QT_QPA_PLATFORM=offscreen','/usr/bin/python3','/code/tools/validate-ubuntu24-from-wsl.py','--inside-run',inside]
    return subprocess.run(command).returncode


def main():
    if len(sys.argv)>1 and sys.argv[1]=='--inside-run':
        require(os.getuid()==1000,'Validation builds must run as uid1000')
        plan=json.loads(Path(sys.argv[2]).read_text());os.chdir('/code')
        environment=dict(os.environ);environment.update(plan.get('environment',{}))
        return subprocess.run(plan['arguments'],env=environment).returncode
    if len(sys.argv)>1 and sys.argv[1]=='--internal-root':
        return internal_root(sys.argv[2],Path(sys.argv[3]) if len(sys.argv)>3 else None)
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--distribution',default='Ubuntu',help='Existing WSL distribution used only as kernel/bootstrap host')
    parser.add_argument('action',choices=['bootstrap','verify-image','run'])
    parser.add_argument('arguments',nargs=argparse.REMAINDER)
    args=parser.parse_args();BASE.mkdir(exist_ok=True)
    if args.action in ['bootstrap','verify-image']:
        authenticate(download=args.action=='bootstrap')
        if args.action=='verify-image':print('PASS Ubuntu Base official signature and SHA256');return 0
    plan=None
    if args.action=='run':
        arguments=args.arguments[1:] if args.arguments[:1]==['--'] else args.arguments
        require(arguments,'Supply a validation command after run --')
        tasks=BASE/'tasks';tasks.mkdir(exist_ok=True);plan=tasks/('command-'+uuid.uuid4().hex+'.json')
        write_json(plan,{'arguments':arguments,'environment':{}})
    with (BASE/'.runner.lock').open('a') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise RuntimeError('Another Ubuntu24 validation process is active; observe that process before retrying')
        command=['/mnt/c/Windows/System32/wsl.exe','--distribution',args.distribution,'--user','root','--exec',
                 '/usr/bin/unshare','--mount','--pid','--fork','--propagation','private','/usr/bin/python3',
                 str(Path(__file__).resolve()),'--internal-root',args.action]
        if plan is not None:command.append(str(plan))
        result=subprocess.run(command)
        if plan is not None:write_json(plan.with_name(plan.stem+'-result.json'),{'exit_code':result.returncode,'plan_sha256':sha(plan)})
        return result.returncode


if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,RuntimeError,ValueError,subprocess.SubprocessError) as error:
        print('Ubuntu24 validation failed: '+str(error),file=sys.stderr);sys.exit(1)
