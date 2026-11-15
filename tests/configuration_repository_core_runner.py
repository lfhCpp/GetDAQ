#!/usr/bin/env python3
"""运行仓库文件系统测试并记录实际程序/冻结输入摘要，不保存认证文件。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import uuid


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('program', type=Path)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, help='Existing validated publication; defaults to prior frozen phase evidence')
    args = parser.parse_args()
    program = args.program.resolve()
    repo = Path(__file__).resolve().parents[1]
    frozen = sorted((repo/'build-suite-tests/validation-configuration-publication/cli').glob('*/v1/publication.json'))
    assert args.manifest is not None or frozen, 'Supply --manifest PACKAGE/publication.json or prior frozen evidence'
    manifest = args.manifest.resolve() if args.manifest else frozen[0]
    args.evidence.mkdir(parents=True, exist_ok=True)
    output = args.evidence.resolve()/('repository-core-'+uuid.uuid4().hex)
    receipt = json.loads((program.parent/'build-receipt.json').read_text())
    assert receipt['program_sha256'][program.name] == sha(program), 'Build program changed'
    result = subprocess.run([str(program),str(manifest),str(output)],capture_output=True,text=True,timeout=180)
    assert output.is_dir(), 'Test did not establish its evidence directory'
    (output/'result.txt').write_text(result.stdout+result.stderr)
    (output/'scope.json').write_text(json.dumps({'schema':1,'returncode':result.returncode,
        'program':str(program),'program_sha256':sha(program),'manifest':str(manifest),'manifest_sha256':sha(manifest),
        'test_script_sha256':sha(Path(__file__)),
        'sanitized':receipt['sanitized'],'hardware_connected':False,'network_executed':False,'receiver_executed':False,
        'excluded_evidence_files':['credential-private']},indent=2)+'\n')
    assert result.returncode == 0, 'Inspect '+str(output/'result.txt')
    print('PASS repository core; evidence',output)


if __name__ == '__main__':
    main()
