#!/usr/bin/env python3
"""Qt客户端与实际ECC服务边界；只提交非法Idle Start，不触发硬件/数据流程。"""
import argparse
import http.client
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("probe", type=Path)
    parser.add_argument("service", type=Path)
    parser.add_argument("template", type=Path)
    parser.add_argument("--output-parent", type=Path, required=True)
    args = parser.parse_args()
    parent = args.output_parent.resolve()
    parent.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="qt-ecc-probe-", dir=parent))
    token = os.urandom(32).hex()
    credential = root / "credential"
    credential.write_text(token + "\n")
    credential.chmod(0o600)
    with socket.socket() as candidate:
        candidate.bind(("127.0.0.1", 0))
        port = candidate.getsockname()[1]
    config = json.loads(args.template.read_text())
    config.update(port=port, bind="127.0.0.1", audit_root=str(root / "controller"), credential_file=str(credential))
    path = root / "service.json"
    path.write_text(json.dumps(config, indent=2) + "\n")
    with (root / "service.log").open("w") as log:
        process = subprocess.Popen([str(args.service.resolve()), "--config", str(path)], stdout=log, stderr=subprocess.STDOUT)
        try:
            end = time.monotonic() + 8
            while True:
                assert process.poll() is None, "Actual ECC service startup failed"
                try:
                    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=1)
                    connection.request("GET", "/v1/identity", headers={"Authorization": "Bearer " + token})
                    response = connection.getresponse()
                    body = response.read()
                    connection.close()
                    if response.status == 200:
                        identity = json.loads(body)
                        break
                except (OSError, http.client.HTTPException):
                    pass
                assert time.monotonic() < end, "Actual ECC identity deadline"
                time.sleep(.03)
            command = [str(args.probe.resolve()), "127.0.0.1", str(port), str(credential), config["service_id"], config["client_id"],
                       identity["snapshot_sha256"], identity["input_binding_sha256"], str(root / "intent.json"), "qt-illegal-start"]
            probe = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=15)
            (root / "probe.log").write_text(probe.stdout)
            assert probe.returncode == 0, "Actual Qt/ECC probe failed; inspect private evidence log"
            rows = [json.loads(row) for row in (root / "controller/network/operations.jsonl").read_text().splitlines()]
            accepted = [row for row in rows if row["event"] == "accepted"]
            assert len(accepted) == 1 and accepted[0]["request"] == (root / "intent.json").read_text(), "Qt request was not persisted/bound exactly once"
            assert not any(json.loads(row).get("phase") == "intent" for row in (root / "controller/ecc.jsonl").read_text().splitlines()), "Illegal Qt request began hardware workflow"
        finally:
            process.terminate()
            process.wait(timeout=10)
        assert process.returncode == 0, "Actual ECC service normal exit failed"
    for path in root.rglob("*.log"):
        assert token not in path.read_text(), "Credential leaked to log"
    (root / "scope.json").write_text(json.dumps({"passed": True, "real_qt_client": True, "real_ecc_service": True,
        "hardware_commands_sent": False, "raw_receivers_tested": False, "full_acquisition_tested": False}, indent=2) + "\n")
    print("PASS actual Qt client / ECC auth, scope, persisted request and failed transition; no hardware/data workflow")
    print("Evidence:", root)


if __name__ == "__main__":
    main()
