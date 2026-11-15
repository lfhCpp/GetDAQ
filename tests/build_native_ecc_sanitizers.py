#!/usr/bin/env python3
"""严格C++11插桩构建新ECC/硬件/参数源码；旧本地依赖仍使用已构建静态库。"""
import argparse
import concurrent.futures
import hashlib
import json
import os
import shlex
from pathlib import Path
import subprocess


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=root / "build-ice-compat")
    parser.add_argument("--output-dir", type=Path, default=Path("/tmp/getdaq-native-ecc-asan"))
    parser.add_argument("--jobs", type=int, default=3)
    parser.add_argument("--incremental", action="store_true", help="Only reuse identical source/header/compiler-command objects")
    parser.add_argument("--ecc-service", action="store_true", help="Also instrument the low-frequency ECC Qt frontend; system Qt and old vendor libraries remain uninstrumented")
    parser.add_argument("--ice-source", action="store_true", help="Instrument source control and source-side lifecycle for actual ICE ECC integration")
    parser.add_argument("--ordinary", action="store_true", help="Build the same native sources without sanitizer instrumentation")
    programs=[("native-ecc-coordinator-tests", "native_ecc_coordinator_test.cpp"),
              ("broken-acquisition-stop-tests", "broken_acquisition_stop_test.cpp"),
              ("hardware-system-acquisition-tests", "hardware_system_acquisition_test.cpp"),
              ("native-ecc-receiver-tests", "native_ecc_receiver_test.cpp"),
              ("native-ecc-remote-receiver-tests", "native_ecc_remote_receiver_test.cpp"),
              ("fixed-ecc-configuration-tests", "fixed_ecc_configuration_test.cpp"),
              ("ecc-network-service-tests", "ecc_network_service_test.cpp"),
              ("ice-source-control-tests", "ice_source_control_test.cpp")]
    parser.add_argument("--tests", nargs="+", choices=[name for name,_ in programs],
                        help="Build selected test executables only; default preserves the full existing test suite")
    args = parser.parse_args()
    if args.tests and "ice-source-control-tests" in args.tests and not args.ice_source:
        parser.error("ice-source-control-tests requires --ice-source")
    programs=[pair for pair in programs if (pair[0]!="ice-source-control-tests" or args.ice_source)
              and (not args.tests or pair[0] in args.tests)]
    if not 1 <= args.jobs <= 8:
        parser.error("--jobs must be 1..8")
    build = args.build_dir.resolve()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    ice = sorted((build / "vendor").glob("ice-*/cpp/include"))
    if len(ice) != 1:
        raise RuntimeError("Need exactly one built embedded Ice include tree")
    core, hw, ecc = (root / p for p in ("common/experiment", "apps/hardware", "apps/ecc"))
    native = ecc / "native"
    supervisor = root / "apps/datasupervisor"
    protocol = root / "common/control"
    includes = [core, root / "common/hardware", hw, ecc, native, supervisor, protocol, root / "tests", build / "generated", build / "source-control-protocol/generated", ice[0]]
    flags = ["g++", "-std=c++11", "-pedantic-errors", "-O1", "-g", "-fPIC", "-fno-omit-frame-pointer",
             "-fsanitize=address,undefined", "-Wall", "-Wextra", "-Wno-deprecated-declarations",
             "-DICE_STATIC_LIBS", "-DGETDAQ_TEST_ECC_JOURNAL"]
    flags += ["-I" + str(p) for p in includes]
    if args.ordinary:
        flags=[flag for flag in flags if flag!="-fsanitize=address,undefined"]
    if args.ice_source:
        includes += [root/"apps/icecontrol",root/"apps/iceingress",build/"ice-ingress/generated",root/"apps/sourcereader"]
        flags += ["-I"+str(p) for p in includes[-4:]]+["-DGETDAQ_TEST_ICE_SOURCE"]
    mains = {"main.cpp", "alignmentmain.cpp", "systemconfigurationmain.cpp", "systemdescriptionmain.cpp"}
    sources = sorted(core.glob("*.cpp")) + [p for p in sorted(hw.glob("*.cpp")) if p.name not in mains]
    sources += [root / "common/config/configurationrange.cpp",
                root / "common/config/configurationrange_parse.cpp"]
    sources += [root / "common/hardware/hardwaredescription.cpp"]
    sources += [ecc / "eccstatemachine.cpp", ecc / "eccjournal.cpp",
                ecc / "eccjournal_replay.cpp"] + sorted(native.glob("*.cpp"))
    # The supervisor directory contains mutually exclusive POSIX and Win32
    # implementations. This direct-source test builder mirrors the native
    # CMake source selection and must not feed the Win32-only translation
    # units to Linux GCC (or vice versa).
    platform_only = {"controlhttp_win.cpp", "credential_win.cpp", "queryfile_win.cpp"}
    if os.name == "nt":
        platform_only = {"controlhttp.cpp", "credential.cpp", "queryfile.cpp"}
    sources += [p for p in sorted(supervisor.glob("*.cpp"))
                if p.name not in {"querymain.cpp", "catalogmain.cpp"} | platform_only]
    sources += sorted(protocol.glob("*.cpp"))
    sources += [build/"source-control-protocol/generated/SourceControl.cpp"]
    if args.ice_source:
        sources += [root/"apps/sourcereader/sourceringreader.cpp",root/"apps/sourcereader/sourceringreadout.cpp",root/"apps/sourcereader/sourcemfmring.cpp",root/"apps/icecontrol/controlledicesource.cpp",root/"apps/icecontrol/sourcecontrolservant.cpp",root/"apps/iceingress/icesourceclient.cpp",build/"ice-ingress/generated/IceIngress.cpp"]
    tests = [test for _,test in programs]
    sources += [root / "tests" / name for name in tests]
    def input_name(path):
        try:
            return str(path.relative_to(root))
        except ValueError:
            return str(path)

    def object_name(path):
        try:
            return "-".join(path.relative_to(root).with_suffix("").parts)
        except ValueError:
            return "external-" + hashlib.sha256(str(path).encode()).hexdigest()[:24]

    objects = {p: output / (object_name(p) + ".o") for p in sources}
    headers = sorted(core.glob("*.h")) + sorted(hw.glob("*.h")) + sorted(ecc.glob("*.h")) + sorted(native.glob("*.h"))
    headers += sorted((root / "common/hardware").glob("*.h"))
    headers += sorted(supervisor.glob("*.h")) + sorted((root / "common/text").glob("*.h")) + sorted((root / "common/storage").glob("*.h"))
    headers += sorted((root / "tests").glob("*.h"))
    headers += sorted(protocol.glob("*.h"))
    headers += [root / "common/config/configurationrange.h",build/"source-control-protocol/generated/SourceControl.h"]
    if args.ice_source:headers += [root/"apps/datareceiver/mfmheader.h"]+list((root/"apps/sourcereader").glob("*.h"))+list((root/"apps/icecontrol").glob("*.h"))+list((root/"apps/iceingress").glob("*.h"))+[build/"ice-ingress/generated/IceIngress.h"]
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    previous = json.loads((output / "source-sha256.json").read_text()) if args.incremental else {}
    recipes = json.loads((output / "commands.json").read_text()) if args.incremental else []
    recipes = [recipe if isinstance(recipe, list) else recipe["command"] for recipe in recipes]
    headers_same = all(previous.get(input_name(p)) == digest(p) for p in headers)

    def compile_source(path):
        command = flags + ["-c", str(path), "-o", str(objects[path])]
        if headers_same and objects[path].exists() and previous.get(input_name(path)) == digest(path) and command in recipes:
            return {"command": command, "reused_identical_source_and_headers": True}
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (output / (objects[path].stem + ".compile.txt")).write_text(result.stdout)
        if result.returncode:
            raise RuntimeError("Compile failed: " + str(path) + "\n" + result.stdout[-6000:])
        return {"command": command, "reused_identical_source_and_headers": False}

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        commands = list(executor.map(compile_source, sources))
    # Keep successfully compiled recipes if a later link fails; this is not a
    # completed-build receipt. Incremental reuse still checks every header,
    # source and exact compiler command before accepting an object.
    (output / "source-sha256.json").write_text(json.dumps({input_name(p): digest(p) for p in sources + headers}, indent=2) + "\n")
    (output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    libraries = [build / ("libcompat-" + n + ".a") for n in ("GET", "Ice", "IceUtil")]
    libraries += [build / "libcompat-bzip.a"]
    for name,test in programs:
        command = flags + [str(objects[p]) for p in sources if p.parent != root / "tests"]
        command += [str(objects[root / "tests" / test])] + list(map(str, libraries[:3]))
        command += ["-lpthread", "-lrt", str(libraries[3]), "-ldl", "-o", str(output / name)]
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (output / (name + ".link.txt")).write_text(result.stdout)
        if result.returncode:
            raise RuntimeError("Link failed: " + result.stdout[-6000:])
        commands.append({"command": command})
    service_tracked = []
    select_main = root / "apps/ecc/select/main.cpp"
    select_object = output / "ecc-select-main.o"
    command = flags + ["-c", str(select_main), "-o", str(select_object)]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (output / "ecc-select-main.compile.txt").write_text(result.stdout)
    if result.returncode:
        raise RuntimeError("ECC selection CLI compile failed: " + result.stdout[-6000:])
    commands.append({"command": command})
    command = flags + [str(objects[p]) for p in sources if p.parent != root / "tests"] + [str(select_object)]
    command += list(map(str, libraries[:3])) + ["-lpthread", "-lrt", str(libraries[3]), "-ldl", "-o", str(output / "getdaq-ecc-select")]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (output / "getdaq-ecc-select.link.txt").write_text(result.stdout)
    if result.returncode:
        raise RuntimeError("ECC selection CLI link failed: " + result.stdout[-6000:])
    commands.append({"command": command})
    if args.ecc_service:
        service = root / "apps/ecc/service"
        http = root / "common/http"
        qt_cflags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "Qt5Core", "Qt5Network", "Qt5Concurrent"], text=True))
        qt_libraries = shlex.split(subprocess.check_output(["pkg-config", "--libs", "Qt5Core", "Qt5Network", "Qt5Concurrent"], text=True))
        moc = Path(subprocess.check_output(["qmake", "-query", "QT_HOST_BINS"], text=True).strip()) / "moc"
        generated, frontend_objects = [], []
        for header in (service / "eccservice.h", http / "httpserver.h"):
            destination = output / ("moc-" + header.stem + ".cpp")
            command = [str(moc)] + qt_cflags + ["-I" + str(http)] + ["-I" + str(p) for p in includes] + [str(header), "-o", str(destination)]
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            (output / (destination.stem + ".generate.txt")).write_text(result.stdout)
            if result.returncode:
                raise RuntimeError("ECC frontend moc failed: " + result.stdout[-6000:])
            generated.append(destination)
            commands.append({"command": command})
        frontend = [service / "main.cpp", service / "eccservice.cpp", http / "httpserver.cpp"]
        for path in frontend + generated:
            destination = output / ("qt-" + path.stem + ".o")
            command = flags + qt_cflags + ["-I" + str(http), "-c", str(path), "-o", str(destination)]
            result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            (output / (destination.stem + ".compile.txt")).write_text(result.stdout)
            if result.returncode:
                raise RuntimeError("ECC frontend compile failed: " + result.stdout[-6000:])
            frontend_objects.append(destination)
            commands.append({"command": command})
        command = flags + [str(objects[p]) for p in sources if p.parent != root / "tests"] + list(map(str, frontend_objects))
        command += list(map(str, libraries[:3])) + ["-lpthread", "-lrt", str(libraries[3]), "-ldl"]
        command += qt_libraries + ["-o", str(output / "getdaq-ecc-server")]
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (output / "getdaq-ecc-server.link.txt").write_text(result.stdout)
        if result.returncode:
            raise RuntimeError("ECC frontend link failed: " + result.stdout[-6000:])
        commands.append({"command": command})
        service_tracked = frontend + [service / "eccservice.h", http / "httpserver.h"]
        (output / "generated-sha256.json").write_text(json.dumps({str(p): digest(p) for p in generated}, indent=2) + "\n")
    tracked = sources + headers + service_tracked + [select_main, Path(__file__).resolve(), root / "tests/system_acquisition_original_reference.py"]
    (output / "source-sha256.json").write_text(json.dumps({input_name(p): digest(p) for p in tracked}, indent=2) + "\n")
    (output / "dependency-sha256.json").write_text(json.dumps({input_name(p): digest(p) for p in libraries}, indent=2) + "\n")
    (output / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    print("PASS strict C++11 "+("ordinary" if args.ordinary else "ASan/UBSan")+
          " build of new experiment, hardware, ECC and coordinator sources; old libraries not instrumented")


if __name__ == "__main__":
    main()
