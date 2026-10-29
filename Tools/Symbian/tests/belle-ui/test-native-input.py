#!/usr/bin/env python3
"""Exercise XTest against a private host window; never connect to the EKA display."""

import ctypes as C
import importlib.util
import os
from pathlib import Path
import select
import shutil
import subprocess
import time
import unittest

spec = importlib.util.spec_from_file_location("native_interact", Path(__file__).with_name("native-interact.py"))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


@unittest.skipUnless(shutil.which("Xvfb"), "private Xvfb required")
class NativeInputTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        read_fd, write_fd = os.pipe()
        cls.host = subprocess.Popen(["Xvfb", "-displayfd", str(write_fd), "-screen", "0", "800x600x24", "-nolisten", "tcp"],
                                   pass_fds=(write_fd,), stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        os.close(write_fd)
        try:
            if not select.select([read_fd], [], [], 5)[0]:
                raise RuntimeError("private Xvfb did not become ready")
            display_number = os.read(read_fd, 32).decode().strip()
            if not display_number.isdecimal():
                raise RuntimeError("invalid private display number")
        except BaseException:
            cls.host.terminate()
            cls.host.wait(timeout=5)
            raise
        finally:
            os.close(read_fd)
        cls.old_display = os.environ.get("DISPLAY")
        os.environ["DISPLAY"] = ":" + display_number
        cls.x = C.CDLL("libX11.so.6")
        p, u, i = C.c_void_p, C.c_ulong, C.c_int
        for name, args, result in [
            ("XOpenDisplay", [C.c_char_p], p), ("XDefaultRootWindow", [p], u),
            ("XCreateSimpleWindow", [p, u, i, i, C.c_uint, C.c_uint, C.c_uint, u, u], u),
            ("XInternAtom", [p, C.c_char_p, i], u),
            ("XChangeProperty", [p, u, u, u, i, i, p, i], i),
            ("XSelectInput", [p, u, C.c_long], i), ("XMapWindow", [p, u], i),
            ("XSync", [p, i], i), ("XPending", [p], i), ("XNextEvent", [p, p], i),
            ("XCloseDisplay", [p], i), ("XDestroyWindow", [p, u], i),
            ("XSetInputFocus", [p, u, i, u], i),
            ("XSetTransientForHint", [p, u, u], i),
            ("XStoreName", [p, u, C.c_char_p], i),
        ]:
            function = getattr(cls.x, name)
            function.argtypes, function.restype = args, result
        cls.display = cls.x.XOpenDisplay(None)
        cls.window = cls.x.XCreateSimpleWindow(cls.display, cls.x.XDefaultRootWindow(cls.display), 20, 30, 400, 300, 0, 0, 0)
        atom = cls.x.XInternAtom(cls.display, b"_NET_WM_PID", 0)
        value = C.c_ulong(os.getpid())
        cls.x.XChangeProperty(cls.display, cls.window, atom, 6, 32, 0, C.byref(value), 1)
        cls.x.XSelectInput(cls.display, cls.window, 1 | 2 | 4 | 8 | 64)
        cls.x.XMapWindow(cls.display, cls.window)
        cls.x.XSync(cls.display, 0)
        cls.device = module.XInput(os.getpid())

    @classmethod
    def tearDownClass(cls):
        cls.device.close()
        cls.x.XDestroyWindow(cls.display, cls.window)
        cls.x.XCloseDisplay(cls.display)
        cls.host.terminate()
        cls.host.wait(timeout=5)
        cls.host.stderr.close()
        if cls.old_display is None:
            os.environ.pop("DISPLAY", None)
        else:
            os.environ["DISPLAY"] = cls.old_display

    def drain(self):
        events = []
        event = (C.c_long * 24)()
        self.x.XSync(self.display, 0)
        while self.x.XPending(self.display):
            self.x.XNextEvent(self.display, C.byref(event))
            events.append(C.cast(C.byref(event), C.POINTER(C.c_int))[0])
        return events

    def test_real_click_and_softkey_events(self):
        self.drain()
        self.device.click(100, 100)
        self.device.key("F1")
        time.sleep(.05)
        types = self.drain()
        for expected in (4, 5, 2, 3):
            self.assertIn(expected, types, "missing actual button/key press/release event")

    def test_click_outside_window_rejected(self):
        with self.assertRaisesRegex(ValueError, "outside"):
            self.device.click(9999, 100)

    def test_keyboard_focus_loss_rejected(self):
        self.x.XSetInputFocus(self.display, 1, 2, 0)
        self.x.XSync(self.display, 0)
        with self.assertRaisesRegex(RuntimeError, "focus"):
            self.device.key("F1")

    def test_wrong_pid_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "PID"):
            module.XInput(2147483647)

    def test_fep_requires_pid_title_and_transient_parent(self):
        modal = self.x.XCreateSimpleWindow(self.display, self.x.XDefaultRootWindow(self.display), 40, 50, 200, 150, 0, 0, 0)
        try:
            atom = self.x.XInternAtom(self.display, b"_NET_WM_PID", 0)
            value = C.c_ulong(os.getpid())
            self.x.XChangeProperty(self.display, modal, atom, 6, 32, 0, C.byref(value), 1)
            self.x.XStoreName(self.display, modal, b"Enter the text")
            self.x.XMapWindow(self.display, modal)
            self.x.XSync(self.display, 0)
            self.assertIsNone(self.device.fep_window(), "same PID/title alone must not bypass focus guard")
            self.x.XSetTransientForHint(self.display, modal, self.window)
            self.x.XSync(self.display, 0)
            self.assertEqual(self.device.fep_window(), modal)
            self.x.XStoreName(self.display, modal, b"Other emulator dialog")
            self.x.XSync(self.display, 0)
            self.assertIsNone(self.device.fep_window())
            self.x.XStoreName(self.display, modal, b"Enter the text")
            value.value += 1
            self.x.XChangeProperty(self.display, modal, atom, 6, 32, 0, C.byref(value), 1)
            self.x.XSync(self.display, 0)
            self.assertIsNone(self.device.fep_window())
        finally:
            self.x.XDestroyWindow(self.display, modal)
            self.x.XSync(self.display, 0)

    def test_real_fep_submit_targets_modal_and_checks_size(self):
        modal = self.x.XCreateSimpleWindow(self.display, self.x.XDefaultRootWindow(self.display), 40, 50, 200, 150, 0, 0, 0)
        try:
            atom = self.x.XInternAtom(self.display, b"_NET_WM_PID", 0)
            value = C.c_ulong(os.getpid())
            self.x.XChangeProperty(self.display, modal, atom, 6, 32, 0, C.byref(value), 1)
            self.x.XStoreName(self.display, modal, b"Enter the text")
            self.x.XSetTransientForHint(self.display, modal, self.window)
            self.x.XSelectInput(self.display, modal, 4 | 8)
            self.x.XMapWindow(self.display, modal)
            self.x.XSetInputFocus(self.display, modal, 2, 0)
            self.x.XSync(self.display, 0)
            with self.assertRaisesRegex(RuntimeError, "focus"):
                self.device.key("F1")
            with self.assertRaisesRegex(RuntimeError, "resized"):
                self.device.submit_fep([100, 130, 400, 300])
            self.drain()
            record = self.device.submit_fep([100, 130, 200, 150])
            self.assertEqual(record["window"], hex(modal))
            types = self.drain()
            self.assertIn(4, types)
            self.assertIn(5, types)
        finally:
            self.x.XDestroyWindow(self.display, modal)
            self.x.XSync(self.display, 0)


if __name__ == "__main__":
    unittest.main()
