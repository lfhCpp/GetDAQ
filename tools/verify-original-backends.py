#!/usr/bin/env python3
"""Check offline original backend inputs without visiting source/ or the network."""
from pathlib import Path
import hashlib,json,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
def verify():
 subprocess.run([sys.executable,str(ROOT/'tools/verify-legacy-sources.py')],check=True)
 vendor=ROOT/'third_party/DataExporter';manifest=json.loads((vendor/'SOURCE_MANIFEST.json').read_text())
 for name,record in manifest['files'].items():
  rel=Path(name)
  if rel.is_absolute() or '..' in rel.parts:raise ValueError('Unsafe DataExporter source path')
  p=vendor/rel
  if p.is_symlink() or not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=record['bundled_sha256']:raise ValueError('DataExporter source differs: '+name)
 print('Verified original DataExporter: %d source files; offline'%len(manifest['files']))
if __name__=='__main__':verify()
