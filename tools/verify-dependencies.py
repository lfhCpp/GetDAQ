#!/usr/bin/env python3
"""Verify locally embedded build inputs without network access or extraction."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys

ROOT=Path(__file__).resolve().parents[1]
ARCHIVES=('ice-v3.5.1-40aac360.tar.gz','bzip2-1.0.8.tar.gz','mcpp-2.7.2-4ice-671886cb.tar.gz','gsoap-2.8.124.orig.tar.gz')
PROFILES=('control','data','data-host','analysis','full')


def sha(path):
    value=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):value.update(block)
    return value.hexdigest()


def regular(root,name):
    part=PurePosixPath(name)
    if not name or '\\' in name or ':' in name or part.is_absolute() or str(part)!=name or any(p in ('','..','.') for p in part.parts):
        raise ValueError('Invalid embedded dependency path: '+name)
    current=root
    for item in part.parts:
        current=current/item
        if current.is_symlink():raise ValueError('Dependency symlink is not a source copy: '+name)
    if not current.is_file():raise ValueError('Missing embedded dependency file: '+name)
    return current


def checksum_list(root,name):
    path=regular(root,name);values={}
    for line in path.read_text(encoding='utf-8').splitlines():
        match=re.fullmatch(r'([0-9a-f]{64})  (.+)',line)
        if not match or match[2] in values:raise ValueError('Invalid or duplicate checksum entry: '+name)
        values[match[2]]=match[1]
    if not values:raise ValueError('Empty checksum list: '+name)
    return path,values


def verify(root=ROOT,profile='full'):
    if profile not in PROFILES:raise ValueError('Unknown build profile')
    root=Path(root).resolve(strict=True)
    result=dict(schema='getdaq.embedded-dependencies:1',profile=profile,passed=True,
                checked_files={},network_used=False,archives_extracted=False,
                board_services_required=False,toolchain_verified=False)
    # Native data and optional analysis profiles use system libraries and do
    # not require the application's embedded Ice, yaml-cpp, or archives.
    if profile in ('data','analysis'):return result
    vendor=root/'third_party'
    if vendor.is_symlink():raise ValueError('Dependency symlink is not a source copy: third_party')
    def record(path):
        value=sha(path);result['checked_files'][path.relative_to(root).as_posix()]=value;return value
    manifest_file=regular(vendor,'dependencies.json');manifest=json.loads(manifest_file.read_text(encoding='utf-8'))
    if manifest['schema_version']!=1:raise ValueError('Unsupported dependency manifest schema')
    record(manifest_file)
    if profile in ('control','full'):
        sums,expected=checksum_list(vendor,'yaml-cpp/SHA256SUMS');record(sums)
        if not {'LICENSE','include/yaml-cpp/yaml.h','src/parse.cpp'}.issubset(expected):
            raise ValueError('Incomplete embedded yaml-cpp source inventory')
        for name,digest in expected.items():
            path=regular(vendor/'yaml-cpp',name)
            if record(path)!=digest:raise ValueError('Embedded yaml-cpp checksum mismatch: '+name)
        actual={p.relative_to(vendor/'yaml-cpp').as_posix() for p in (vendor/'yaml-cpp').rglob('*') if p.is_file() and p!=sums}
        if actual!=set(expected):raise ValueError('Embedded yaml-cpp inventory differs; missing or unreviewed source')
    if profile=='full':
        sums,expected=checksum_list(vendor,'archives/SHA256SUMS');record(sums)
        if set(expected)!=set(ARCHIVES):raise ValueError('Incomplete embedded archive inventory')
        records={}
        for entry in manifest['dependencies']:
            local=entry.get('local_path')
            if local and local.startswith('archives/'):
                if local in records:raise ValueError('Duplicate embedded archive manifest entry: '+local)
                records[local]=entry
        for name in ARCHIVES:
            local='archives/'+name;entry=records.get(local)
            if entry is None:raise ValueError('Missing archive manifest entry: '+local)
            path=regular(vendor,local)
            if path.stat().st_size!=entry['size_bytes'] or record(path)!=entry['sha256'] or entry['sha256']!=expected[name]:
                raise ValueError('Embedded archive size/checksum mismatch: '+local)
            licenses=entry.get('license_files',[])+([entry['license_file']] if 'license_file' in entry else [])
            if not licenses:raise ValueError('Missing archive license declaration: '+local)
            for license_file in licenses:record(regular(vendor,license_file))
        record(regular(vendor,'licenses/getbench/COPYING'))
        record(regular(vendor,'ATTPC-LICENSE'))
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',choices=PROFILES,default='full')
    parser.add_argument('--json',action='store_true',help='Print the complete verification receipt to stdout')
    args=parser.parse_args();value=verify(profile=args.profile)
    if args.json:print(json.dumps(value,ensure_ascii=False,indent=2))
    else:print('PASS embedded dependencies:',args.profile,';',len(value['checked_files']),'files; no download; toolchain and hardware not checked')


if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,TypeError) as error:
        print('Dependency verification failed: '+str(error),file=sys.stderr);sys.exit(1)
