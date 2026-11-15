#!/usr/bin/env python3
"""Regression tests for package-bound Ubuntu 24 validation receipts."""

import hashlib
import importlib.util
import json
import os
import tempfile
import unittest
from unittest import mock
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "package_ubuntu24_catalog_ssh", ROOT / "tools/package_ubuntu24_catalog_ssh.py"
)
PACKAGER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGER)


class PackageValidationReceiptTest(unittest.TestCase):
    def setUp(self):
        self.package_root = ROOT / "build-packages"
        self.created_package_root = not self.package_root.exists()
        self.package_root.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="getdaq-package-receipt-", dir=self.package_root)
        self.candidate = Path(self.temporary.name)
        (self.candidate / "validation").mkdir()

    def tearDown(self):
        self.temporary.cleanup()
        if self.created_package_root and not any(self.package_root.iterdir()):
            self.package_root.rmdir()

    def test_full_inventory_accepts_current_merge_tool_and_previous_candidate(self):
        expected = PACKAGER.expected_full_programs()
        self.assertIn("getdaq-merge", expected)
        self.assertIn("getdaq-matrix", expected)
        self.assertIn("getdaq-control-mcp", expected)
        self.assertEqual(len(expected), 29)
        self.assertEqual(len(expected - {"getdaq-matrix"}), 28)
        self.assertEqual(len(expected - {"getdaq-merge", "getdaq-matrix"}), 27)

    def test_full_inventory_accepts_documented_pre_mcp_candidates(self):
        expected = PACKAGER.expected_full_programs()
        accepted = PACKAGER.accepted_full_program_inventories(expected)
        self.assertIn(frozenset(expected - {"getdaq-control-mcp"}), accepted)
        self.assertIn(frozenset(expected - {"getdaq-control-mcp", "getdaq-matrix"}), accepted)
        self.assertIn(frozenset(expected - {"getdaq-control-mcp", "getdaq-matrix", "getdaq-merge"}), accepted)
        self.assertEqual(sorted(map(len, accepted)), [26, 27, 27, 28, 28, 29])

    def make_manifest(self, receipt, program=PACKAGER.PROGRAM, candidate=None, evidence="validation/result.json", validation_key=None):
        receipt_path = self.candidate / evidence
        receipt_path.parent.mkdir(parents=True, exist_ok=True)
        receipt_path.write_text(json.dumps(receipt, sort_keys=True) + "\n", encoding="utf-8")
        result_digest = hashlib.sha256(receipt_path.read_bytes()).hexdigest()
        key = validation_key or ("catalog_ssh_fake_endpoint_test" if program == PACKAGER.PROGRAM else "receiver_tcp_integration")
        record = dict(receipt)
        record["evidence_in_package"] = evidence
        record["result_sha256"] = result_digest
        record.pop("candidate", None)
        record.pop("remote_receiver_binary_sha256", None)
        record.pop("receiver_delivery_binary_sha256", None)
        manifest = {
            "programs": {program: {"sha256": "a" * 64}},
            "validation": {key: record},
        }
        if candidate:
            manifest["rebuild_provenance"] = {"base_candidate": candidate}
        return manifest, receipt_path

    def test_accepts_inherited_fake_ssh_receipt_bound_to_reused_catalog(self):
        base = "getdaq-ubuntu24-20261020-ssh-archive-candidate"
        receipt = {
            "candidate": "build-packages/" + base,
            "status": "passed",
            "raw_remaining": 0,
            "largest_ssh_fixture_file_bytes": 65536,
            "remote_receiver_binary_sha256": "a" * 64,
        }
        manifest, _ = self.make_manifest(receipt, candidate=base)
        PACKAGER.verify_validation_receipts(self.candidate, manifest)

    def test_accepts_plain_tcp_receipt_bound_to_refreshed_catalog(self):
        receipt = {
            "candidate": str(self.candidate.relative_to(ROOT)),
            "status": "passed",
            "passed": True,
            "raw_remaining": 0,
            "synthetic_payload_max_bytes": 64 * 1024 * 1024,
            "catalog_delivery_binary_sha256": "a" * 64,
        }
        manifest, _ = self.make_manifest(
            receipt, program=PACKAGER.PROGRAM,
            validation_key="catalog_plain_tcp",
        )
        PACKAGER.verify_validation_receipts(self.candidate, manifest)

    def test_accepts_threshold_analysis_receipt_without_misclassifying_it_as_tcp(self):
        receipt = {
            "candidate": str(self.candidate.relative_to(ROOT)),
            "status": "passed",
            "raw_remaining": 0,
            "synthetic_payload_max_bytes": 320,
            "catalog_delivery_binary_sha256": "a" * 64,
        }
        manifest, evidence = self.make_manifest(
            receipt, program=PACKAGER.PROGRAM,
            validation_key="catalog_threshold_analysis",
        )
        # make_manifest removes candidate fields for legacy receipts; threshold
        # analysis evidence must retain its own exact package identity.
        receipt["candidate"] = str(self.candidate.relative_to(ROOT))
        evidence.write_text(json.dumps(receipt, sort_keys=True) + "\n", encoding="utf-8")
        manifest["validation"]["catalog_threshold_analysis"]["result_sha256"] = hashlib.sha256(
            evidence.read_bytes()).hexdigest()
        manifest["validation"]["catalog_threshold_analysis"].pop("candidate", None)
        PACKAGER.verify_validation_receipts(self.candidate, manifest)

    def test_records_plain_tcp_result_without_replacing_candidate_binary(self):
        catalog_sha = "a" * 64
        readme = self.candidate / "README.md"
        readme.write_text("candidate\n", encoding="utf-8")
        manifest_path = self.candidate / "manifest.json"
        manifest_path.write_text(json.dumps({
            "programs": {PACKAGER.PROGRAM: {"sha256": catalog_sha}},
            "files": {"README.md": hashlib.sha256(readme.read_bytes()).hexdigest()},
        }) + "\n", encoding="utf-8")
        receipt = {
            "candidate": str(self.candidate.relative_to(ROOT)),
            "status": "passed",
            "passed": True,
            "raw_remaining": 0,
            "synthetic_payload_max_bytes": 64 * 1024 * 1024,
            "synthetic_raw_bytes_generated": 1024,
            "synthetic_raw_bytes_deleted": 1024,
            "owned_test_roots_empty": True,
            "scenario": {name: True for name in (
                "thread_local_sigpipe_isolation", "oversized_metadata_fields_rejected_before_staging",
                "plain_tcp_no_authentication", "interrupted_partial_resumed",
                "receiver_readback_verified", "completed_resume_no_payload", "separate_cli_listener_send_readback",
                "cli_connection_refusal_bounded", "native_catalog_regressions",
            )},
            "catalog_delivery_binary_sha256": catalog_sha,
        }
        result = self.candidate.parent / (self.candidate.name + "-plain-tcp.json")
        result.write_text(json.dumps(receipt) + "\n", encoding="utf-8")
        try:
            with mock.patch.object(PACKAGER, "verify_files"):
                PACKAGER.record_catalog_plain_tcp_test(self.candidate, result)
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            evidence = self.candidate / "validation/catalog-plain-tcp-71-95.json"
            self.assertEqual(json.loads(evidence.read_text(encoding="utf-8")), receipt)
            self.assertEqual(manifest["validation"]["catalog_plain_tcp"]["catalog_delivery_binary_sha256"], catalog_sha)
            self.assertEqual(manifest["files"]["validation/catalog-plain-tcp-71-95.json"],
                             hashlib.sha256(evidence.read_bytes()).hexdigest())
            PACKAGER.verify_validation_receipts(self.candidate, manifest)
        finally:
            result.unlink(missing_ok=True)

    def test_plain_tcp_recorder_rejects_binary_mismatch_and_limit_over_20_gib(self):
        candidate_sha = "a" * 64
        (self.candidate / "manifest.json").write_text(json.dumps({
            "programs": {PACKAGER.PROGRAM: {"sha256": candidate_sha}},
            "files": {},
        }) + "\n", encoding="utf-8")
        result = self.candidate.parent / (self.candidate.name + "-invalid-plain-tcp.json")
        receipt = {
            "candidate": str(self.candidate.relative_to(ROOT)),
            "status": "passed",
            "passed": True,
            "raw_remaining": 0,
            "synthetic_payload_max_bytes": 1024,
            "synthetic_raw_bytes_generated": 0,
            "synthetic_raw_bytes_deleted": 0,
            "owned_test_roots_empty": True,
            "scenario": {name: True for name in (
                "thread_local_sigpipe_isolation", "oversized_metadata_fields_rejected_before_staging",
                "plain_tcp_no_authentication", "interrupted_partial_resumed",
                "receiver_readback_verified", "completed_resume_no_payload", "separate_cli_listener_send_readback",
                "cli_connection_refusal_bounded", "native_catalog_regressions",
            )},
            "catalog_delivery_binary_sha256": "b" * 64,
        }
        result.write_text(json.dumps(receipt) + "\n", encoding="utf-8")
        try:
            with self.assertRaisesRegex(RuntimeError, "did not use this candidate"):
                PACKAGER.record_catalog_plain_tcp_test(self.candidate, result)
            receipt["catalog_delivery_binary_sha256"] = candidate_sha
            receipt["synthetic_payload_max_bytes"] = 20 * 1024 ** 3 + 1
            result.write_text(json.dumps(receipt) + "\n", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "exceeds 20 GiB"):
                PACKAGER.record_catalog_plain_tcp_test(self.candidate, result)
        finally:
            result.unlink(missing_ok=True)

    def test_rejects_plain_tcp_receipt_bound_to_other_catalog(self):
        receipt = {
            "candidate": str(self.candidate.relative_to(ROOT)),
            "status": "passed",
            "raw_remaining": 0,
            "synthetic_payload_max_bytes": 1024,
            "catalog_delivery_binary_sha256": "b" * 64,
        }
        manifest, _ = self.make_manifest(
            receipt, program=PACKAGER.PROGRAM,
            validation_key="catalog_plain_tcp",
        )
        with self.assertRaisesRegex(RuntimeError, "not bound to getdaq-catalog"):
            PACKAGER.verify_validation_receipts(self.candidate, manifest)

    def test_rejects_receipt_file_changed_after_binding(self):
        receipt = {
            "candidate": str(self.candidate.relative_to(ROOT)),
            "status": "passed",
            "raw_files_remaining": 0,
            "synthetic_payload_max_bytes": 1024,
            "receiver_delivery_binary_sha256": "a" * 64,
        }
        manifest, path = self.make_manifest(receipt, program=PACKAGER.RECEIVER_PROGRAM)
        path.write_text("{}\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "digest mismatch"):
            PACKAGER.verify_validation_receipts(self.candidate, manifest)

    def test_rejects_receiver_receipt_bound_to_different_binary(self):
        receipt = {
            "candidate": str(self.candidate.relative_to(ROOT)),
            "status": "passed",
            "raw_files_remaining": 0,
            "synthetic_payload_max_bytes": 1024,
            "receiver_delivery_binary_sha256": "b" * 64,
        }
        manifest, _ = self.make_manifest(receipt, program=PACKAGER.RECEIVER_PROGRAM)
        with self.assertRaisesRegex(RuntimeError, "not bound to getdaq-receiver"):
            PACKAGER.verify_validation_receipts(self.candidate, manifest)

    def test_rejects_synthetic_payload_above_user_limit(self):
        receipt = {
            "candidate": str(self.candidate.relative_to(ROOT)),
            "status": "passed",
            "raw_files_remaining": 0,
            "synthetic_payload_max_bytes": 20 * 1024 ** 3 + 1,
            "receiver_delivery_binary_sha256": "a" * 64,
        }
        manifest, _ = self.make_manifest(receipt, program=PACKAGER.RECEIVER_PROGRAM)
        with self.assertRaisesRegex(RuntimeError, "20 GiB"):
            PACKAGER.verify_validation_receipts(self.candidate, manifest)

    def test_rebuild_invalidates_only_the_replaced_program_receipt(self):
        catalog_evidence = self.candidate / "validation/catalog-ssh-test.json"
        receiver_evidence = self.candidate / "validation/receiver-integration-test.json"
        catalog_evidence.write_text("{}\n", encoding="utf-8")
        receiver_evidence.write_text("{}\n", encoding="utf-8")
        manifest = {"validation": {
            "catalog_ssh_fake_endpoint_test": {"evidence_in_package": "validation/catalog-ssh-test.json"},
            "receiver_tcp_integration": {"evidence_in_package": "validation/receiver-integration-test.json"},
        }}
        PACKAGER.invalidate_rebuilt_validation(manifest, self.candidate, {PACKAGER.PROGRAM: Path("catalog")})
        self.assertNotIn("catalog_ssh_fake_endpoint_test", manifest["validation"])
        self.assertFalse(catalog_evidence.exists())
        self.assertIn("receiver_tcp_integration", manifest["validation"])
        self.assertTrue(receiver_evidence.is_file())

    def test_rebuild_rejects_unsafe_inherited_evidence_path(self):
        manifest = {"validation": {"catalog_ssh_fake_endpoint_test": {"evidence_in_package": "../outside.json"}}}
        with self.assertRaisesRegex(RuntimeError, "unsafe inherited validation path"):
            PACKAGER.invalidate_rebuilt_validation(manifest, self.candidate, {PACKAGER.PROGRAM: Path("catalog")})

    def test_rebuild_rejects_symlinked_evidence_parent_without_touching_target(self):
        with tempfile.TemporaryDirectory(prefix="getdaq-package-receipt-outside-") as outside:
            target = Path(outside) / "evidence.json"
            target.write_text("keep\n", encoding="utf-8")
            link = self.candidate / "validation" / "linked"
            os.symlink(outside, link)
            manifest = {"validation": {"catalog_ssh_fake_endpoint_test": {
                "evidence_in_package": "validation/linked/evidence.json"
            }}}
            with self.assertRaisesRegex(RuntimeError, "traverses a symlink"):
                PACKAGER.invalidate_rebuilt_validation(
                    manifest, self.candidate, {PACKAGER.PROGRAM: Path("catalog")}
                )
            self.assertEqual(target.read_text(encoding="utf-8"), "keep\n")

    def test_verify_rejects_symlinked_candidate_root_before_resolving(self):
        with tempfile.TemporaryDirectory(prefix="getdaq-package-candidate-link-") as temporary:
            alias = Path(temporary) / "candidate"
            os.symlink(self.candidate, alias)
            with self.assertRaisesRegex(RuntimeError, "candidate directory is a symlink"):
                PACKAGER.verify_files(alias)

    def test_base_candidate_rejects_unlisted_nested_symlink(self):
        with tempfile.TemporaryDirectory(prefix="getdaq-package-base-") as temporary:
            base = Path(temporary) / "candidate"
            (base / "validation").mkdir(parents=True)
            payload = base / "payload"
            payload.write_text("candidate payload\n", encoding="utf-8")
            manifest = {
                "files": {"payload": hashlib.sha256(payload.read_bytes()).hexdigest()},
                "programs": {PACKAGER.PROGRAM: {"sha256": "a" * 64}},
            }
            (base / "manifest.json").write_text(json.dumps(manifest) + "\n", encoding="utf-8")
            with tempfile.TemporaryDirectory(prefix="getdaq-package-base-outside-") as outside:
                target = Path(outside) / "outside.txt"
                target.write_text("outside\n", encoding="utf-8")
                os.symlink(outside, base / "validation" / "linked")
                with self.assertRaisesRegex(RuntimeError, "candidate contains a symlink"):
                    PACKAGER.verify_base(base)
                self.assertEqual(target.read_text(encoding="utf-8"), "outside\n")


if __name__ == "__main__":
    unittest.main()
