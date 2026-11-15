#!/usr/bin/env python3
"""Launch the installed GUI without leaking its startup lock into services."""
from pathlib import Path
import ctypes
import re
import fcntl
import os
import subprocess
import sys
import time


def focus_existing(pid):
    try:
        if subprocess.run(['wmctrl', '-xa', 'GetDAQ'], stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL, check=False).returncode == 0:
            return
    except OSError:
        pass
    display = None
    try:
        clients = subprocess.run(['xprop', '-root', '_NET_CLIENT_LIST'], capture_output=True,
                                 text=True, timeout=3).stdout
        x = ctypes.CDLL('libX11.so.6')
        x.XOpenDisplay.restype = ctypes.c_void_p
        display = x.XOpenDisplay(None)
        if not display:
            return
        x.XMapRaised.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        x.XSetInputFocus.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
        x.XFlush.argtypes = [ctypes.c_void_p]
        x.XCloseDisplay.argtypes = [ctypes.c_void_p]
        for window in re.findall(r'0x[0-9a-fA-F]+', clients):
            prop = subprocess.run(['xprop', '-id', window, '_NET_WM_PID'], capture_output=True,
                                  text=True, timeout=3).stdout
            match = re.search(r'=\s*(\d+)', prop)
            if match and int(match.group(1)) == int(pid):
                x.XMapRaised(display, int(window, 16))
                x.XSetInputFocus(display, int(window, 16), 2, 0)
                x.XFlush(display)
                break
    except (OSError, subprocess.SubprocessError):
        pass
    finally:
        if display:
            x.XCloseDisplay(display)


def launch(root):
    logs = root / 'logs'
    logs.mkdir(parents=True, exist_ok=True)
    runtime_file = root / 'site/gui-runtime.txt'
    runtime = runtime_file.read_text().strip() if runtime_file.exists() else 'runtime-linux'
    program = (root / runtime / 'bin/GetDAQ').resolve()
    if root.resolve() not in program.parents or not program.is_file():
        raise RuntimeError('GUI executable is missing: ' + str(program))
    if not os.environ.get('DISPLAY') and not os.environ.get('WAYLAND_DISPLAY'):
        raise RuntimeError('No desktop display. Run start-getdaq.sh from the Ubuntu desktop terminal.')
    # This lock only serializes launches. The GUI and detached services never own it.
    with (logs / 'gui-start-v2.lock').open('a+') as guard:
        fcntl.flock(guard, fcntl.LOCK_EX)
        for proc in Path('/proc').iterdir():
            if not proc.name.isdigit():
                continue
            try:
                exe = (proc / 'exe').resolve(strict=True)
                if proc.stat().st_uid == os.getuid() and exe.name == 'GetDAQ' and root.resolve() in exe.parents:
                    print('GetDAQ already running, PID ' + proc.name, flush=True)
                    focus_existing(proc.name)
                    return 0
            except (OSError, RuntimeError):
                continue
        args = sys.argv[1:]
        if '--config' not in args and not any(a.startswith('--config=') for a in args):
            deployment = Path(os.environ.get('GETDAQ_DEPLOYMENT_DIR', str(root / 'site/acquisition-pedestals-cobo64')))
            args = ['--config', str(deployment / 'envoy.yml')] + args
        log_path = logs / 'GetDAQ-native.log'
        with log_path.open('ab', buffering=0) as log:
            process = subprocess.Popen([str(program)] + args, cwd=str(root), stdin=subprocess.DEVNULL,
                                       stdout=log, stderr=log, close_fds=True, start_new_session=True)
        time.sleep(0.5)
        result = process.poll()
        if result is not None and result != 0:
            raise RuntimeError('GetDAQ exited with code %s. See %s\n%s' %
                               (result, log_path, log_path.read_text(errors='replace')[-4000:]))
        print('GetDAQ started, PID %s. Log: %s' % (process.pid, log_path), flush=True)
        return 0


if __name__ == '__main__':
    try:
        sys.exit(launch(Path(__file__).resolve().parent.parent))
    except (OSError, RuntimeError) as error:
        print('GetDAQ launch failed: ' + str(error), file=sys.stderr, flush=True)
        sys.exit(1)
