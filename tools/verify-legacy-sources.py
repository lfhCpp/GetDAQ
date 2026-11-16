#!/usr/bin/env python3
"""Verify the curated source bundle using its local manifest; never reads source/."""
import hashlib
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]

def verify(root=ROOT):
    directory=root/'third_party/legacy-get'
    manifest=json.loads((directory/'SOURCE_MANIFEST.json').read_text())
    if manifest.get('schema')!='getdaq.legacy-source:1':raise ValueError('Unknown legacy source manifest')
    for name,record in manifest['files'].items():
        relative=Path(name)
        if relative.is_absolute() or '..' in relative.parts:raise ValueError('Unsafe source path: '+name)
        path=directory/relative
        if path.is_symlink() or not path.is_file():raise ValueError('Missing regular source file: '+name)
        if hashlib.sha256(path.read_bytes()).hexdigest()!=record['sha256']:raise ValueError('Changed source file: '+name)
    print('Verified project-local legacy source bundle: %d files; no source/ access'%len(manifest['files']))

if __name__=='__main__':
    try:verify()
    except (OSError,ValueError,KeyError) as error:sys.exit(str(error))
