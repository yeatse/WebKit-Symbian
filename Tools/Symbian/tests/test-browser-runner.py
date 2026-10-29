#!/usr/bin/env python3
"""Host regression: the runner must collect buffered logs after normal shutdown.

Run with DISPLAY pointing to an otherwise idle Xvfb. No Symbian image is used.
"""

import ctypes
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

TOOLS = Path(__file__).resolve().parents[1]
MARKER = 'PASS: buffered final console line'


def fake_emulator():
    mode = os.environ['BROWSER_RUNNER_TEST_MODE']
    if mode == 'early-exit':
        return
    if mode == 'exit-during-screenshot':
        log = Path(os.environ['EKA2L1_DATA']) / 'drives/c/Data/WebKit/log.txt'
        log.write_text(MARKER + '\n')
        time.sleep(1.2)
        sys.exit(7)
    spec = importlib.util.spec_from_file_location('close_window', TOOLS / 'eka2l1/close-emulator-window.py')
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    x11 = ctypes.CDLL('libX11.so.6')
    pointer, ulong = ctypes.c_void_p, ctypes.c_ulong
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = pointer
    x11.XDefaultRootWindow.argtypes = [pointer]
    x11.XDefaultRootWindow.restype = ulong
    x11.XCreateSimpleWindow.argtypes = [pointer, ulong, ctypes.c_int, ctypes.c_int,
                                       ctypes.c_uint, ctypes.c_uint, ctypes.c_uint,
                                       ulong, ulong]
    x11.XCreateSimpleWindow.restype = ulong
    x11.XInternAtom.argtypes = [pointer, ctypes.c_char_p, ctypes.c_int]
    x11.XInternAtom.restype = ulong
    x11.XChangeProperty.argtypes = [pointer, ulong, ulong, ulong, ctypes.c_int,
                                   ctypes.c_int, pointer, ctypes.c_int]
    x11.XSetWMProtocols.argtypes = [pointer, ulong, ctypes.POINTER(ulong), ctypes.c_int]
    x11.XMapWindow.argtypes = [pointer, ulong]
    x11.XFlush.argtypes = [pointer]
    x11.XPending.argtypes = [pointer]
    x11.XNextEvent.argtypes = [pointer, ctypes.POINTER(helper.XEvent)]
    x11.XCloseDisplay.argtypes = [pointer]
    display = x11.XOpenDisplay(None)
    if not display:
        sys.exit('Fake emulator cannot open DISPLAY')
    window = x11.XCreateSimpleWindow(display, x11.XDefaultRootWindow(display),
                                     0, 0, 100, 100, 0, 0, 0)
    pid = ulong(os.getpid())
    x11.XChangeProperty(display, window, x11.XInternAtom(display, b'_NET_WM_PID', 0),
                       6, 32, 0, ctypes.byref(pid), 1)  # XA_CARDINAL
    delete = ulong(x11.XInternAtom(display, b'WM_DELETE_WINDOW', 0))
    x11.XSetWMProtocols(display, window, ctypes.byref(delete), 1)
    x11.XMapWindow(display, window)
    x11.XFlush(display)
    log = Path(os.environ['EKA2L1_DATA']) / 'drives/c/Data/WebKit/log.txt'
    with log.open('w') as output:
        # Like the EKA host VFS, retain the last guest write until normal close.
        output.write(MARKER + '\n')
        if mode == 'ignore-close':
            # Even an already visible PASS must not conceal forced shutdown.
            output.flush()
        while True:
            if x11.XPending(display):
                event = helper.XEvent()
                x11.XNextEvent(display, ctypes.byref(event))
                if event.client.type == 33 and event.client.data.l[0] == delete.value and mode != 'ignore-close':
                    break
            time.sleep(.01)
    x11.XCloseDisplay(display)


@unittest.skipUnless(os.environ.get('DISPLAY'), 'requires DISPLAY with Xvfb')
class BrowserRunnerTest(unittest.TestCase):
    def run_mode(self, mode):
        with tempfile.TemporaryDirectory(prefix='browser-runner-test-') as directory:
            work = Path(directory)
            image = work / 'image'
            image.mkdir()
            for name in ('browser.exe', 'browser_reg.rsc', 'browser_loc.rsc'):
                (image / name).write_bytes(b'host test placeholder')
            icu = work / 'icu/install/share/icu'
            icu.mkdir(parents=True)
            (icu / 'icudt70l.dat').write_bytes(b'host test placeholder')
            emulator = work / 'fake-emulator'
            emulator.write_text(f'#!{sys.executable}\nimport runpy\nrunpy.run_path({str(Path(__file__).resolve())!r}, run_name="__main__")\n')
            emulator.chmod(0o755)
            environment = os.environ.copy()
            for name in ('EKA2L1_BROWSER_LOCK_FD', 'SCREENSHOT'):
                environment.pop(name, None)
            environment.update(SYMBIAN_WEBKIT_WORK=str(work), ICU_DIR=str(work / 'icu'),
                               EKA2L1=str(emulator), EKA2L1_DATA=str(work / 'data'),
                               XDG_RUNTIME_DIR=str(work / 'runtime'), WAIT='1',
                               EKA2L1_EXPECT_PASS=MARKER, BROWSER_RUNNER_TEST_MODE=mode)
            if mode == 'exit-during-screenshot':
                screenshot = work / 'import'
                screenshot.write_text(f'#!{sys.executable}\nimport time\ntime.sleep(2)\n')
                screenshot.chmod(0o755)
                environment['PATH'] = str(work) + os.pathsep + environment['PATH']
            result = subprocess.run(['bash', str(TOOLS / 'eka2l1/run-browser.sh'), str(image / 'browser.exe')],
                                    env=environment, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=25)
            runs = list((work / 'runs').iterdir())
            self.assertEqual(len(runs), 1, result.stdout)
            return result, (runs[0] / 'log.txt').read_text() if (runs[0] / 'log.txt').exists() else ''

    def test_normal_close_flushes_last_console_message(self):
        result, log = self.run_mode('normal')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn(MARKER, log)

    def test_early_zero_exit_is_failure(self):
        result, _ = self.run_mode('early-exit')
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('exited before observation completed', result.stdout)

    def test_forced_shutdown_is_failure_even_with_pass(self):
        result, log = self.run_mode('ignore-close')
        self.assertIn(MARKER, log)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('emulator graceful shutdown failed', result.stdout)

    def test_exit_during_screenshot_is_failure_even_with_pass(self):
        result, log = self.run_mode('exit-during-screenshot')
        self.assertIn(MARKER, log)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('emulator exited before requested shutdown', result.stdout)


if __name__ == '__main__':
    if 'BROWSER_RUNNER_TEST_MODE' in os.environ:
        fake_emulator()
    else:
        unittest.main()
