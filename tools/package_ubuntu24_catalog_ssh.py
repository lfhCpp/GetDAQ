#!/usr/bin/env python3
"""Create and verify Ubuntu 24 candidates with native catalog transfer tools."""

import argparse
import datetime
import hashlib
import importlib.util
import json
import shutil
import subprocess
from pathlib import Path
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[1]
ROOTFS = ROOT / "build-ubuntu24-validation/rootfs"
DEFAULT_BASE = ROOT / "build-packages/getdaq-ubuntu24-20261020-failed-retirement-candidate"
DEFAULT_DESTINATION = ROOT / "build-packages/getdaq-ubuntu24-20261020-ssh-archive-candidate"
DEFAULT_RESULT = ROOT / "build-catalog-ssh/package-result.json"
PROGRAM = "getdaq-catalog"
RECEIVER_PROGRAM = "getdaq-receiver"
MERGE_PROGRAM = "getdaq-merge"
MATRIX_PROGRAM = "getdaq-matrix"
CONTROL_MCP_PROGRAM = "getdaq-control-mcp"


def expected_full_programs():
    suite_path = ROOT / "tools/build-suite.py"
    spec = importlib.util.spec_from_file_location("getdaq_build_suite", suite_path)
    suite = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(suite)
    args = SimpleNamespace(profile="full", cmake="cmake", qmake="qmake", make="make", jobs=1)
    steps = suite.plan(args, Path("/tmp/getdaq-expected-program-inventory"))
    return {name for step in steps for name in step["programs"]}


def accepted_full_program_inventories(expected=None):
    """Current full inventory plus the explicitly supported historical sets."""
    expected = expected_full_programs() if expected is None else set(expected)
    variants = {frozenset(expected)}
    # Matrix and merge were added to the suite before the MCP adapter. Keep
    # their documented historical candidates verifiable after MCP was added.
    for omitted in (
        {MATRIX_PROGRAM},
        {MERGE_PROGRAM, MATRIX_PROGRAM},
        {CONTROL_MCP_PROGRAM},
        {CONTROL_MCP_PROGRAM, MATRIX_PROGRAM},
        {CONTROL_MCP_PROGRAM, MERGE_PROGRAM, MATRIX_PROGRAM},
    ):
        variants.add(frozenset(expected - omitted))
    return variants


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def loader_context(rootfs=ROOTFS):
    rootfs = Path(rootfs).resolve()
    loader = rootfs / "usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2"
    if not loader.is_file():
        raise RuntimeError("Ubuntu 24 dynamic loader is missing: " + str(loader))
    libraries = [rootfs / part for part in (
        "lib/x86_64-linux-gnu", "lib", "usr/lib/x86_64-linux-gnu", "usr/lib", "usr/local/lib"
    ) if (rootfs / part).is_dir()]
    return loader, ":".join(str(path) for path in libraries), rootfs


def run_target(loader, library_path, *args):
    return subprocess.run(
        [str(loader), "--library-path", library_path, *map(str, args)],
        capture_output=True, text=True, timeout=45, check=False,
    )


def require_rootfs_library_resolution(output, rootfs, program):
    root_prefix = str(Path(rootfs).resolve()) + "/"
    for line in output.splitlines():
        if "=>" not in line:
            continue
        resolved = line.split("=>", 1)[1].strip().split()[0]
        if not resolved.startswith(root_prefix):
            raise RuntimeError("runtime dependency escaped the Ubuntu 24 rootfs for " + program + ": " + line)


def verify_validation_receipts(candidate, manifest):
    candidate = Path(candidate).resolve()
    provenance = manifest.get("rebuild_provenance", {})
    validation = manifest.get("validation", {})
    for key, program, digest_field in (
        ("catalog_ssh_fake_endpoint_test", PROGRAM, "remote_receiver_binary_sha256"),
        ("catalog_authenticated_tcp", PROGRAM, "catalog_delivery_binary_sha256"),
        ("catalog_plain_tcp", PROGRAM, "catalog_delivery_binary_sha256"),
        ("catalog_threshold_analysis", PROGRAM, "catalog_delivery_binary_sha256"),
        ("receiver_tcp_integration", RECEIVER_PROGRAM, "receiver_delivery_binary_sha256"),
    ):
        record = validation.get(key)
        if not record:
            continue
        if record.get("status") != "passed" or record.get("raw_files_remaining", record.get("raw_remaining")) != 0:
            raise RuntimeError("candidate has an incomplete validation receipt: " + key)
        evidence_relative = Path(record.get("evidence_in_package", ""))
        if evidence_relative.is_absolute() or ".." in evidence_relative.parts or not evidence_relative.parts:
            raise RuntimeError("candidate validation receipt has an unsafe evidence path: " + key)
        evidence = candidate / evidence_relative
        if not evidence.is_file() or evidence.is_symlink():
            raise RuntimeError("candidate validation evidence is missing: " + key)
        if sha256(evidence) != record.get("result_sha256"):
            raise RuntimeError("candidate validation evidence digest mismatch: " + key)
        receipt = read_json(evidence)
        allowed_candidates = {str(candidate.relative_to(ROOT))}
        if provenance.get("base_candidate"):
            base_candidate = provenance["base_candidate"]
            allowed_candidates.add(base_candidate)
            if "/" not in base_candidate:
                allowed_candidates.add(str(Path("build-packages") / base_candidate))
        if receipt.get("candidate") not in allowed_candidates:
            raise RuntimeError("candidate validation evidence names a different package: " + key)
        expected_program_sha = manifest["programs"].get(program, {}).get("sha256")
        if not expected_program_sha or receipt.get(digest_field) != expected_program_sha:
            raise RuntimeError("candidate validation evidence is not bound to " + program)
        raw_limit = receipt.get("synthetic_payload_max_bytes", receipt.get("largest_ssh_fixture_file_bytes", 0))
        if raw_limit > 20 * 1024 ** 3:
            raise RuntimeError("candidate validation receipt exceeds the 20 GiB synthetic-data limit")


def verify_candidate_inventory(candidate):
    candidate = Path(candidate)
    if candidate.is_symlink():
        raise RuntimeError("candidate directory is a symlink: " + str(candidate))
    candidate = candidate.resolve()
    if not candidate.is_dir():
        raise RuntimeError("candidate directory is missing: " + str(candidate))
    manifest_path = candidate / "manifest.json"
    if manifest_path.is_symlink() or not manifest_path.is_file():
        raise RuntimeError("candidate manifest is missing or is a symlink: " + str(manifest_path))
    manifest = read_json(manifest_path)
    declared_files = set(manifest["files"]) | {"manifest.json"}
    actual_files = set()
    for path in candidate.rglob("*"):
        if path.is_symlink():
            raise RuntimeError("candidate contains a symlink: " + str(path.relative_to(candidate)))
        if path.is_file():
            actual_files.add(str(path.relative_to(candidate)))
        elif not path.is_dir():
            raise RuntimeError("candidate contains a non-regular entry: " + str(path.relative_to(candidate)))
    if actual_files != declared_files:
        missing = sorted(declared_files - actual_files)
        extra = sorted(actual_files - declared_files)
        raise RuntimeError("candidate inventory differs from manifest; missing=" + repr(missing) + ", extra=" + repr(extra))
    for relative, expected in manifest["files"].items():
        relative_path = Path(relative)
        if relative_path.is_absolute() or ".." in relative_path.parts or "." in relative_path.parts:
            raise RuntimeError("candidate manifest contains an unsafe path: " + relative)
        actual_path = candidate / relative
        if not actual_path.is_file() or sha256(actual_path) != expected:
            raise RuntimeError("candidate file checksum mismatch: " + relative)
    return candidate, manifest


def verify_files(candidate):
    candidate, manifest = verify_candidate_inventory(candidate)
    expected = expected_full_programs()
    actual = set(manifest["programs"])
    historical_inventories = accepted_full_program_inventories(expected)
    if actual not in historical_inventories:
        raise RuntimeError("candidate program inventory differs from the full suite; missing=" +
                           repr(sorted(expected - actual)) + ", extra=" + repr(sorted(actual - expected)))
    verify_validation_receipts(candidate, manifest)
    loader, library_path, rootfs = loader_context()
    rootfs_prefix = str(rootfs)
    runtimes = {}
    for name, entry in manifest["programs"].items():
        binary = candidate / entry["path"]
        if sha256(binary) != entry["sha256"]:
            raise RuntimeError("program checksum mismatch: " + name)
        result = run_target(loader, library_path, "--list", binary)
        output = (result.stdout + result.stderr).replace(rootfs_prefix, "<ubuntu24-rootfs>")
        if result.returncode or "not found" in output:
            raise RuntimeError("Ubuntu 24 loader rejected " + name + ":\n" + output)
        require_rootfs_library_resolution(result.stdout + result.stderr, rootfs, name)
        runtimes[name] = output.splitlines()
    help_results = {}
    for name in (PROGRAM, RECEIVER_PROGRAM, MERGE_PROGRAM, MATRIX_PROGRAM):
        if name not in manifest["programs"]:
            continue
        help_results[name] = run_target(loader, library_path, candidate / "bin" / name, "--help")
        if help_results[name].returncode:
            raise RuntimeError("Ubuntu 24 " + name + " --help failed: " + help_results[name].stderr)
    receiver_runtime = "\n".join(runtimes[RECEIVER_PROGRAM])
    if "libQt5" in receiver_runtime:
        raise RuntimeError("native receiver unexpectedly depends on Qt")
    if MERGE_PROGRAM in runtimes:
        merge_runtime = "\n".join(runtimes[MERGE_PROGRAM])
        if any(library in merge_runtime for library in ("libQt", "libIce", "libboost", "libyaml", "libcurl")):
            raise RuntimeError("native merger unexpectedly depends on a GUI/legacy runtime")
    if MATRIX_PROGRAM in runtimes:
        matrix_runtime = "\n".join(runtimes[MATRIX_PROGRAM])
        if any(library in matrix_runtime for library in ("libQt", "libIce", "libboost", "libyaml", "libcurl", "libCore.so")):
            raise RuntimeError("native matrix converter unexpectedly depends on a GUI/legacy runtime")
    return manifest, runtimes, help_results


def verify_base(base):
    _, manifest = verify_candidate_inventory(base)
    if PROGRAM not in manifest["programs"]:
        raise RuntimeError("base candidate does not contain getdaq-catalog")
    return manifest


def invalidate_rebuilt_validation(manifest, staging, overrides):
    """Drop package test receipts whose tested executable is being replaced."""
    validation = manifest.setdefault("validation", {})
    for key, program in (
        ("catalog_ssh_fake_endpoint_test", PROGRAM),
        ("catalog_authenticated_tcp", PROGRAM),
        ("catalog_plain_tcp", PROGRAM),
        ("catalog_threshold_analysis", PROGRAM),
        ("receiver_tcp_integration", RECEIVER_PROGRAM),
    ):
        if program not in overrides:
            continue
        record = validation.pop(key, None)
        if not record:
            continue
        relative = Path(record.get("evidence_in_package", ""))
        if relative.is_absolute() or ".." in relative.parts or "." in relative.parts or not relative.parts:
            raise RuntimeError("rebuilt program has an unsafe inherited validation path: " + key)
        evidence = Path(staging) / relative
        current = Path(staging)
        for part in relative.parts:
            current = current / part
            if current.is_symlink():
                raise RuntimeError("rebuilt program validation evidence traverses a symlink: " + key)
        if evidence.exists():
            if not evidence.is_file():
                raise RuntimeError("rebuilt program validation evidence is not a regular file: " + key)
            evidence.unlink()


def assemble(binary, receiver_binary, base, destination, result_path):
    base, destination = (Path(base).resolve(), Path(destination).resolve())
    result_path = Path(result_path)
    overrides = {}
    if binary:
        overrides[PROGRAM] = Path(binary).resolve()
    if receiver_binary:
        overrides[RECEIVER_PROGRAM] = Path(receiver_binary).resolve()
    if not overrides:
        raise RuntimeError("at least one program binary must be supplied")
    if destination.exists():
        raise RuntimeError("refusing to overwrite existing candidate: " + str(destination))
    staging = destination.with_name(destination.name + ".staging")
    if staging.exists():
        raise RuntimeError("refusing to overwrite existing staging directory: " + str(staging))
    for name, path in overrides.items():
        if not path.is_file():
            raise RuntimeError(name + " build is missing: " + str(path))
    verify_base(base)
    loader, library_path, _ = loader_context()
    source_shas = {}
    delivery_shas = {}
    source_dependencies = {}
    source_help = {}
    for name, path in overrides.items():
        source_shas[name] = sha256(path)
        dependencies = run_target(loader, library_path, "--list", path)
        dependency_text = dependencies.stdout + dependencies.stderr
        if dependencies.returncode or "not found" in dependency_text:
            raise RuntimeError("Ubuntu 24 runtime cannot load the built " + name + ":\n" + dependency_text)
        require_rootfs_library_resolution(dependency_text, ROOTFS, name)
        source_dependencies[name] = dependency_text
        source_help[name] = run_target(loader, library_path, path, "--help")
        if source_help[name].returncode:
            raise RuntimeError("built " + name + " --help failed: " + source_help[name].stderr)

    base_sha = sha256(base / "manifest.json")
    try:
        shutil.copytree(base, staging)
        for name, path in overrides.items():
            output_binary = staging / "bin" / name
            shutil.copy2(path, output_binary)
            stripped = subprocess.run(["strip", "--strip-debug", str(output_binary)], capture_output=True, text=True)
            if stripped.returncode:
                raise RuntimeError("cannot strip debug information from " + name + " delivery copy: " + stripped.stderr)
            delivery_shas[name] = sha256(output_binary)

        shutil.rmtree(staging / "docs")
        shutil.copytree(ROOT / "docs", staging / "docs")
        shutil.copy2(ROOT / "README.md", staging / "README.md")

        manifest = read_json(staging / "manifest.json")
        invalidate_rebuilt_validation(manifest, staging, overrides)
        previous_provenance = dict(manifest.get("rebuild_provenance", {}))
        manifest["created_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        for name in overrides:
            program_entry = manifest["programs"][name]
            program_entry["sha256"] = delivery_shas[name]
            program_entry["runtime"] = {
                "checked": True,
                "loader": "Ubuntu 24 rootfs ld-linux-x86-64.so.2",
                "resolved_dependencies": source_dependencies[name].replace(str(ROOTFS.resolve()), "<ubuntu24-rootfs>").splitlines(),
            }

        all_names = set(manifest["programs"])
        manifest["rebuild_provenance"] = {
            "base_candidate": base.name,
            "base_manifest_sha256": base_sha,
            "inherited_rebuild_provenance": previous_provenance,
            "rebuilt": sorted(overrides),
            "rebuilt_sha256": source_shas,
            "delivery_debug_symbols_stripped": True,
            "stripped_programs": {name: {"source_build_sha256": source_shas[name], "delivery_sha256": delivery_shas[name]} for name in overrides},
            "reused_unchanged": sorted(all_names - set(overrides)),
            "full_suite_recompiled": False,
            "build_standard": "C++11",
            "build_compiler": "GNU C++ 9.4.0",
            "build_host": "Ubuntu 20.04.6 WSL2 host; runtime checked in Ubuntu 24 rootfs",
            "target_hardware_accepted": False,
            "source_hashes": {},
        }
        source_paths = {}
        if PROGRAM in overrides:
            source_paths.update({
                "apps/datasupervisor/catalogtransfer.cpp": ROOT / "apps/datasupervisor/catalogtransfer.cpp",
                "apps/datasupervisor/catalogtransfer.h": ROOT / "apps/datasupervisor/catalogtransfer.h",
                "apps/datasupervisor/catalogmain.cpp": ROOT / "apps/datasupervisor/catalogmain.cpp",
                "tests/catalog_transfer_test.cpp": ROOT / "tests/catalog_transfer_test.cpp",
            })
        if RECEIVER_PROGRAM in overrides:
            source_paths.update({
                "apps/datareceiver/exporter.cpp": ROOT / "apps/datareceiver/exporter.cpp",
                "apps/datareceiver/exporter.h": ROOT / "apps/datareceiver/exporter.h",
                "apps/datareceiver/CMakeLists.txt": ROOT / "apps/datareceiver/CMakeLists.txt",
                "tests/exporter_lock_test.cpp": ROOT / "tests/exporter_lock_test.cpp",
                "tests/build_receiver_validation.py": ROOT / "tests/build_receiver_validation.py",
            })
        manifest["rebuild_provenance"]["source_hashes"] = {name: sha256(path) for name, path in source_paths.items()}
        checks = dict(manifest.get("validation", {}))
        checks.pop("all_25_programs_resolved_against_ubuntu24_rootfs", None)
        checks["all_candidate_programs_resolved_against_ubuntu24_rootfs"] = True
        help_checks = dict(checks.get("rebuilt_cli_help_checks", {}))
        for name in overrides:
            help_checks[name] = {
                "exit_code": 0,
                "output_sha256": hashlib.sha256(source_help[name].stdout.encode()).hexdigest(),
                "source_build_binary_sha256": source_shas[name],
            }
        checks["rebuilt_cli_help_checks"] = help_checks
        checks["receiver_has_no_qt_dependency"] = True
        checks["target_platform_and_hardware_accepted"] = False
        manifest["validation"] = checks
        manifest["files"] = {
            str(path.relative_to(staging)): sha256(path)
            for path in sorted(staging.rglob("*"))
            if path.is_file() and path.name != "manifest.json"
        }
        write_json(staging / "manifest.json", manifest)
        verified, _, final_help = verify_files(staging)
        staging.rename(destination)
    finally:
        if staging.exists():
            shutil.rmtree(staging)
    result = {
        "status": "complete",
        "candidate": str(destination.relative_to(ROOT)),
        "base_candidate": str(base.relative_to(ROOT)),
        "base_manifest_sha256": base_sha,
        "program_count": len(verified["programs"]),
        "rebuilt_programs": sorted(overrides),
        "reused_programs": sorted(all_names - set(overrides)),
        "source_build_binary_sha256": source_shas.get(PROGRAM),
        "delivery_binary_sha256": delivery_shas.get(PROGRAM),
        "source_build_binary_sha256_by_program": source_shas,
        "delivery_binary_sha256_by_program": delivery_shas,
        "manifest_sha256": sha256(destination / "manifest.json"),
        "target_loader": "Ubuntu 24 rootfs ld-linux-x86-64.so.2",
        "target_loader_dependency_check": "passed",
        "catalog_help_exit_code": final_help[PROGRAM].returncode,
        "receiver_help_exit_code": final_help[RECEIVER_PROGRAM].returncode,
        "raw_remaining": 0,
        "target_platform_and_hardware_accepted": False,
    }
    write_json(result_path, result)
    print("PASS candidate package, all runtime dependencies, and catalog help:", destination)


def record_fake_ssh_test(candidate, test_result_path, result_path):
    candidate = Path(candidate).resolve()
    test_result_path = Path(test_result_path).resolve()
    result_path = Path(result_path)
    test_result = read_json(test_result_path)
    if test_result.get("status") != "passed" or test_result.get("passed") is not True or test_result.get("raw_remaining") != 0:
        raise RuntimeError("fake-SSH test result is missing or incomplete")
    manifest = read_json(candidate / "manifest.json")
    candidate_binary_sha = manifest["programs"][PROGRAM]["sha256"]
    if test_result.get("remote_receiver_binary_sha256") != candidate_binary_sha:
        raise RuntimeError("fake-SSH test did not use this candidate's catalog receiver binary")
    if test_result.get("candidate") != str(candidate.relative_to(ROOT)):
        raise RuntimeError("fake-SSH test result names a different candidate")
    evidence = candidate / "validation" / "catalog-ssh-test.json"
    evidence.parent.mkdir(exist_ok=True)
    shutil.copy2(test_result_path, evidence)
    test_record = dict(test_result)
    test_record["result_sha256"] = sha256(test_result_path)
    test_record["evidence_in_package"] = str(evidence.relative_to(candidate))
    manifest.setdefault("validation", {})["catalog_ssh_fake_endpoint_test"] = test_record
    manifest["files"] = {
        str(path.relative_to(candidate)): sha256(path)
        for path in sorted(candidate.rglob("*"))
        if path.is_file() and path.name != "manifest.json"
    }
    write_json(candidate / "manifest.json", manifest)
    verify_files(candidate)
    result = read_json(result_path)
    result["manifest_sha256"] = sha256(candidate / "manifest.json")
    result["fake_ssh_test_result_sha256"] = sha256(test_result_path)
    result["fake_ssh_receiver_binary_sha256"] = test_result.get("remote_receiver_binary_sha256")
    write_json(result_path, result)
    print("PASS recorded fake-SSH package validation:", candidate)


def record_receiver_integration_test(candidate, test_result_path, result_path):
    candidate = Path(candidate).resolve()
    test_result_path = Path(test_result_path).resolve()
    result_path = Path(result_path)
    test_result = read_json(test_result_path)
    if test_result.get("status") != "passed" or test_result.get("raw_files_remaining") != 0:
        raise RuntimeError("receiver integration result is missing or incomplete")
    if test_result.get("synthetic_payload_max_bytes", 0) > 20 * 1024 ** 3:
        raise RuntimeError("receiver integration payload exceeds the user's 20 GiB limit")
    manifest = read_json(candidate / "manifest.json")
    candidate_binary_sha = manifest["programs"][RECEIVER_PROGRAM]["sha256"]
    if test_result.get("receiver_delivery_binary_sha256") != candidate_binary_sha:
        raise RuntimeError("receiver integration did not test this candidate's delivery binary")
    if test_result.get("candidate") != str(candidate.relative_to(ROOT)):
        raise RuntimeError("receiver integration result names a different candidate")
    evidence = candidate / "validation" / "receiver-integration-test.json"
    evidence.parent.mkdir(exist_ok=True)
    shutil.copy2(test_result_path, evidence)
    test_record = dict(test_result)
    test_record["result_sha256"] = sha256(test_result_path)
    test_record["evidence_in_package"] = str(evidence.relative_to(candidate))
    manifest.setdefault("validation", {})["receiver_tcp_integration"] = test_record
    manifest["files"] = {
        str(path.relative_to(candidate)): sha256(path)
        for path in sorted(candidate.rglob("*"))
        if path.is_file() and path.name != "manifest.json"
    }
    write_json(candidate / "manifest.json", manifest)
    verify_files(candidate)
    if result_path.is_file():
        package_result = read_json(result_path)
        package_result["manifest_sha256"] = sha256(candidate / "manifest.json")
        package_result["receiver_integration_result_sha256"] = sha256(test_result_path)
        write_json(result_path, package_result)
    print("PASS recorded receiver integration validation:", candidate)


def record_catalog_plain_tcp_test(candidate, test_result_path):
    candidate_input = Path(candidate)
    if candidate_input.is_symlink():
        raise RuntimeError("candidate directory must not be a symlink")
    candidate = candidate_input.resolve()
    test_result_path = Path(test_result_path).resolve()
    try:
        candidate.relative_to((ROOT / "build-packages").resolve())
    except ValueError:
        raise RuntimeError("candidate must be inside build-packages")
    test_result = read_json(test_result_path)
    if test_result.get("status") != "passed" or test_result.get("raw_remaining") != 0:
        raise RuntimeError("plain-TCP test result is missing or incomplete")
    payload_limit = test_result.get("synthetic_payload_max_bytes", 0)
    if type(payload_limit) is not int or payload_limit <= 0 or payload_limit > 20 * 1024 ** 3:
        raise RuntimeError("plain-TCP test payload limit is invalid or exceeds 20 GiB")
    generated = test_result.get("synthetic_raw_bytes_generated")
    if type(generated) is not int or generated < 0 or generated > payload_limit or test_result.get("synthetic_raw_bytes_deleted") != generated:
        raise RuntimeError("plain-TCP synthetic payload accounting is incomplete")
    required_scenarios = (
        "thread_local_sigpipe_isolation", "oversized_metadata_fields_rejected_before_staging",
        "plain_tcp_no_authentication", "interrupted_partial_resumed",
        "receiver_readback_verified", "completed_resume_no_payload", "separate_cli_listener_send_readback",
        "cli_connection_refusal_bounded", "native_catalog_regressions",
    )
    scenarios = test_result.get("scenario", {})
    if any(scenarios.get(name) is not True for name in required_scenarios) or test_result.get("owned_test_roots_empty") is not True:
        raise RuntimeError("plain-TCP test did not prove every required scenario and cleanup")
    manifest = read_json(candidate / "manifest.json")
    if test_result.get("candidate") != str(candidate.relative_to(ROOT)):
        raise RuntimeError("plain-TCP test result names a different candidate")
    catalog_sha = manifest["programs"][PROGRAM]["sha256"]
    if test_result.get("catalog_delivery_binary_sha256") != catalog_sha:
        raise RuntimeError("plain-TCP test did not use this candidate's catalog binary")
    evidence = candidate / "validation" / "catalog-plain-tcp-71-95.json"
    evidence.parent.mkdir(exist_ok=True)
    if evidence.exists() or evidence.is_symlink():
        raise RuntimeError("refusing to overwrite existing plain-TCP evidence: " + str(evidence))
    shutil.copy2(test_result_path, evidence)
    record = dict(test_result)
    record["result_sha256"] = sha256(test_result_path)
    record["evidence_in_package"] = str(evidence.relative_to(candidate))
    manifest.setdefault("validation", {})["catalog_plain_tcp"] = record
    manifest["files"] = {
        str(path.relative_to(candidate)): sha256(path)
        for path in sorted(candidate.rglob("*"))
        if path.is_file() and path.name != "manifest.json"
    }
    write_json(candidate / "manifest.json", manifest)
    verify_files(candidate)
    print("PASS recorded plain-TCP package validation:", candidate)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", help="Optional C++11 getdaq-catalog binary to package")
    parser.add_argument("--receiver-binary", help="Optional C++11 getdaq-receiver binary to package")
    parser.add_argument("--base", default=str(DEFAULT_BASE), help="Previously verified Ubuntu 24 candidate")
    parser.add_argument("--destination", default=str(DEFAULT_DESTINATION), help="New candidate path; must not exist")
    parser.add_argument("--result", default=str(DEFAULT_RESULT), help="Package receipt JSON path")
    parser.add_argument("--verify", action="store_true", help="Verify an existing candidate")
    parser.add_argument("--candidate", help="Candidate directory for --verify or --record-fake-ssh-test")
    parser.add_argument("--record-fake-ssh-test", help="Test result JSON to bind into an existing candidate")
    parser.add_argument("--record-receiver-integration-test", help="Receiver integration result JSON to bind into an existing candidate")
    parser.add_argument("--record-catalog-plain-tcp-test", help="Plain-TCP result JSON to bind into an existing candidate")
    args = parser.parse_args()
    if args.verify:
        candidate = Path(args.candidate or args.destination)
        verify_files(candidate)
        print("PASS candidate package:", candidate)
    elif args.record_fake_ssh_test:
        record_fake_ssh_test(args.candidate or args.destination, args.record_fake_ssh_test, args.result)
    elif args.record_receiver_integration_test:
        record_receiver_integration_test(args.candidate or args.destination, args.record_receiver_integration_test, args.result)
    elif args.record_catalog_plain_tcp_test:
        record_catalog_plain_tcp_test(args.candidate or args.destination, args.record_catalog_plain_tcp_test)
    elif args.binary or args.receiver_binary:
        assemble(args.binary, args.receiver_binary, args.base, args.destination, args.result)
    else:
        parser.error("provide --binary, --verify, or --record-fake-ssh-test")


if __name__ == "__main__":
    main()
