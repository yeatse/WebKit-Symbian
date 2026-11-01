#!/usr/bin/env python3
"""Send real XTest input to an already-running, exclusively owned EKA session.

This helper never launches EKA, changes guest files, assigns input.files, or
executes JavaScript. The normal browser runner retains lock/cleanup ownership.
"""

import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time


NAMES = ["Unicode prompt default", "Empty prompt accepted", "Prompt cancellation",
         "Unicode single-file upload", "Single file on multiple input",
         "Cancel preserves selected file", "Detached-frame chooser lifetime",
         "Navigated-frame chooser lifetime"]


class XInput:
    def __init__(self, pid):
        self.pid = pid
        self.x = C.CDLL("libX11.so.6")
        self.t = C.CDLL("libXtst.so.6")
        p, u, i = C.c_void_p, C.c_ulong, C.c_int
        signatures = {
            "XOpenDisplay": ([C.c_char_p], p), "XCloseDisplay": ([p], i),
            "XDefaultRootWindow": ([p], u), "XDefaultScreen": ([p], i),
            "XInternAtom": ([p, C.c_char_p, i], u),
            "XQueryTree": ([p, u, C.POINTER(u), C.POINTER(u), C.POINTER(C.POINTER(u)), C.POINTER(C.c_uint)], i),
            "XGetWindowProperty": ([p, u, u, C.c_long, C.c_long, i, u, C.POINTER(u), C.POINTER(i),
                                    C.POINTER(u), C.POINTER(u), C.POINTER(p)], i),
            "XGetGeometry": ([p, u, C.POINTER(u), C.POINTER(i), C.POINTER(i), C.POINTER(C.c_uint),
                              C.POINTER(C.c_uint), C.POINTER(C.c_uint), C.POINTER(C.c_uint)], i),
            "XTranslateCoordinates": ([p, u, u, i, i, C.POINTER(i), C.POINTER(i), C.POINTER(u)], i),
            "XSetInputFocus": ([p, u, i, u], i), "XRaiseWindow": ([p, u], i),
            "XGetInputFocus": ([p, C.POINTER(u), C.POINTER(i)], i),
            "XGetTransientForHint": ([p, u, C.POINTER(u)], i),
            "XFetchName": ([p, u, C.POINTER(p)], i),
            "XStringToKeysym": ([C.c_char_p], u), "XKeysymToKeycode": ([p, u], C.c_uint),
            "XSync": ([p, i], i), "XFree": ([p], i),
        }
        for name, (args, result) in signatures.items():
            function = getattr(self.x, name)
            function.argtypes, function.restype = args, result
        for name, args in {
                "XTestFakeMotionEvent": [p, i, i, i, u],
                "XTestFakeButtonEvent": [p, C.c_uint, i, u],
                "XTestFakeKeyEvent": [p, C.c_uint, i, u]}.items():
            function = getattr(self.t, name)
            function.argtypes, function.restype = args, i
        self.display = self.x.XOpenDisplay(None)
        if not self.display:
            raise RuntimeError("cannot open DISPLAY")
        self.root = self.x.XDefaultRootWindow(self.display)
        self.screen = self.x.XDefaultScreen(self.display)
        self.pid_atom = self.x.XInternAtom(self.display, b"_NET_WM_PID", 1)
        matches = [(self.geometry(window)[2] * self.geometry(window)[3], window)
                   for window in self.owned_windows()]
        if not matches:
            self.close()
            raise RuntimeError("no X11 window belongs to the supplied PID")
        self.window = max(matches)[1]
        self.initial_size = self.geometry(self.window)[2:]

    def owned_windows(self):
        matches = []
        pending = [self.root]
        while pending:
            window = pending.pop()
            if self.property(window, self.pid_atom) == [self.pid]:
                matches.append(window)
            root, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
            children = C.POINTER(C.c_ulong)()
            if self.x.XQueryTree(self.display, window, C.byref(root), C.byref(parent), C.byref(children), C.byref(count)):
                pending.extend(children[index] for index in range(count.value))
            if children:
                self.x.XFree(children)
        return matches

    def fep_window(self):
        matches = []
        for window in self.owned_windows():
            parent, name = C.c_ulong(), C.c_void_p()
            if not self.x.XGetTransientForHint(self.display, window, C.byref(parent)) or parent.value != self.window:
                continue
            try:
                if self.x.XFetchName(self.display, window, C.byref(name)) and name:
                    if C.string_at(name) == b"Enter the text":
                        matches.append(window)
            finally:
                if name:
                    self.x.XFree(name)
        if len(matches) > 1:
            raise RuntimeError("more than one matching FEP modal; do not guess a target")
        return matches[0] if matches else None

    def property(self, window, atom):
        if not atom:
            return []
        kind, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
        form, data = C.c_int(), C.c_void_p()
        status = self.x.XGetWindowProperty(self.display, window, atom, 0, 16, 0, 0,
            C.byref(kind), C.byref(form), C.byref(count), C.byref(remaining), C.byref(data))
        try:
            if status or form.value != 32 or not data:
                return []
            values = C.cast(data, C.POINTER(C.c_ulong))
            return [values[index] for index in range(count.value)]
        finally:
            if data:
                self.x.XFree(data)

    def geometry(self, window):
        root, child = C.c_ulong(), C.c_ulong()
        x, y = C.c_int(), C.c_int()
        width, height, border, depth = [C.c_uint() for _ in range(4)]
        if not self.x.XGetGeometry(self.display, window, C.byref(root), C.byref(x), C.byref(y),
                C.byref(width), C.byref(height), C.byref(border), C.byref(depth)):
            raise RuntimeError("cannot obtain window geometry")
        if not self.x.XTranslateCoordinates(self.display, window, self.root, 0, 0,
                C.byref(x), C.byref(y), C.byref(child)):
            raise RuntimeError("cannot translate window coordinates")
        return [x.value, y.value, width.value, height.value]

    def check(self):
        os.kill(self.pid, 0)
        if self.property(self.window, self.pid_atom) != [self.pid]:
            raise RuntimeError("target window PID changed")
        geometry = self.geometry(self.window)
        if geometry[2:] != self.initial_size:
            raise RuntimeError("window resized; recalibrate content rectangle before more input")
        return geometry

    def click(self, x, y):
        geometry = self.check()
        self.click_window(self.window, geometry, x, y)

    def click_window(self, window, geometry, x, y):
        if not (0 <= x < geometry[2] and 0 <= y < geometry[3]):
            raise ValueError("click outside target window")
        self.x.XRaiseWindow(self.display, window)
        self.x.XSetInputFocus(self.display, window, 2, 0)
        if not self.t.XTestFakeMotionEvent(self.display, self.screen, geometry[0] + x, geometry[1] + y, 0):
            raise RuntimeError("XTest mouse motion failed")
        self.x.XSync(self.display, 0)
        time.sleep(.08)
        try:
            if not self.t.XTestFakeButtonEvent(self.display, 1, 1, 0):
                raise RuntimeError("XTest button press failed")
            self.x.XSync(self.display, 0)
            time.sleep(.12)
        finally:
            self.t.XTestFakeButtonEvent(self.display, 1, 0, 0)
            self.x.XSync(self.display, 0)

    def key(self, name):
        self.check()
        self.check_focus(self.window)
        symbol = self.x.XStringToKeysym(name.encode("ascii"))
        code = self.x.XKeysymToKeycode(self.display, symbol) if symbol else 0
        if not code:
            raise ValueError("no X11 keycode for " + name)
        # The preceding real pointer click focuses the display widget. Do not
        # force focus onto Qt's parent window instead of its input widget.
        try:
            if not self.t.XTestFakeKeyEvent(self.display, code, 1, 0):
                raise RuntimeError("XTest key press failed")
            self.x.XSync(self.display, 0)
            time.sleep(.08)
        finally:
            self.t.XTestFakeKeyEvent(self.display, code, 0, 0)
            self.x.XSync(self.display, 0)

    def check_focus(self, expected):
        focus, revert = C.c_ulong(), C.c_int()
        self.x.XGetInputFocus(self.display, C.byref(focus), C.byref(revert))
        window = focus.value
        while window not in (0, 1, self.root, expected):
            root, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
            children = C.POINTER(C.c_ulong)()
            status = self.x.XQueryTree(self.display, window, C.byref(root), C.byref(parent), C.byref(children), C.byref(count))
            if children:
                self.x.XFree(children)
            if not status:
                break
            window = parent.value
        if window != expected:
            raise RuntimeError("keyboard focus left the selected emulator window")

    def submit_fep(self, calibration):
        self.check()
        window = self.fep_window()
        if not window:
            raise RuntimeError("expected same-PID FEP modal is missing")
        self.check_focus(window)
        x, y, width, height = calibration
        geometry = self.geometry(window)
        if geometry[2:] != [width, height]:
            raise RuntimeError("FEP modal resized; recalibrate Submit before more input")
        self.click_window(window, geometry, x, y)
        return {"window": hex(window), "geometry": geometry, "submit": [x, y]}

    def close(self):
        if self.display:
            self.x.XCloseDisplay(self.display)
            self.display = None


def load_picker(path):
    actions = json.loads(path.read_text())
    if not isinstance(actions, list) or not 1 <= len(actions) <= 40:
        raise ValueError("picker plan must contain 1–40 calibrated native actions")
    for action in actions:
        if not isinstance(action, dict) or set(action) - {"key", "point", "count", "wait", "note"}:
            raise ValueError("picker actions accept only key/point/count/wait/note")
        if ("key" in action) == ("point" in action):
            raise ValueError("picker action must specify exactly one key or window-relative point")
        if "key" in action and not isinstance(action["key"], str):
            raise ValueError("picker key must be an X11 key name")
        if "point" in action and (not isinstance(action["point"], list) or len(action["point"]) != 2
                                  or any(not isinstance(value, int) or value < 0 for value in action["point"])):
            raise ValueError("picker point must contain two nonnegative integer coordinates")
        if not isinstance(action.get("count", 1), int) or not 1 <= action.get("count", 1) <= 40:
            raise ValueError("picker key count outside 1–40")
        if not 0 <= action.get("wait", .5) <= 10:
            raise ValueError("picker settling delay outside 0–10 seconds")
    return actions


class Interaction:
    def __init__(self, args, input_device):
        self.args, self.input = args, input_device
        self.records = []
        args.output.mkdir(parents=True, exist_ok=True)
        if (args.output / "interaction.json").exists():
            raise ValueError("use a fresh output directory; retain the earlier interaction record")
        self.ready = 'READY: belle-native-ui ' + json.dumps({"run": args.run_token}, separators=(",", ":"))

    def log(self):
        text = self.args.guest_log.read_text(errors="replace") if self.args.guest_log.exists() else ""
        if self.ready not in text:
            return ""
        text = text[text.rindex(self.ready):]
        if re.search(r"(?:FAIL:|access violation|panicked|KERN-EXEC|USER-EXEC)", text, re.I):
            raise RuntimeError("guest reported failure; inspect raw log")
        return text

    def wait(self, condition, description):
        deadline = time.monotonic() + self.args.timeout
        while time.monotonic() < deadline:
            self.input.check()
            value = condition(self.log())
            if value:
                return value
            time.sleep(.2)
        raise RuntimeError("timed out waiting for " + description)

    def screenshot(self, name):
        path = self.args.output / (name + ".png")
        subprocess.run(["import", "-window", "root", str(path)], check=True, timeout=15)
        self.records.append({"screenshot": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                             "monotonic": time.monotonic()})

    def click_target(self, step, target):
        def locate(text):
            matches = []
            for line in text.splitlines():
                if "ACTION: belle-native-ui " not in line:
                    continue
                value = json.loads(line.split("ACTION: belle-native-ui ", 1)[1])
                if value.get("run") == self.args.run_token and value.get("step") == step and value.get("target") == target:
                    matches.append(value)
            return matches[-1] if matches else None
        action = self.wait(locate, f"step {step} {target} coordinates")
        left, top, width, height = self.args.content_rect
        if not (0 < action["x"] < action["width"] and 0 < action["y"] < action["height"]):
            raise RuntimeError("DOM target is outside viewport; do not click a guessed position")
        scale = width / action["width"]
        if action["y"] * scale >= height:
            raise RuntimeError("target falls outside the calibrated visible content rectangle")
        x = round(left + action["x"] * scale)
        y = round(top + action["y"] * scale)
        self.records.append({"click": [x, y], "action": action, "monotonic": time.monotonic()})
        self.input.click(x, y)

    def key(self, name, wait=.5):
        self.records.append({"key": name, "monotonic": time.monotonic()})
        self.input.key(name)
        time.sleep(wait)

    def native_point(self, point, wait=.5):
        self.records.append({"native_click": point, "monotonic": time.monotonic()})
        self.input.click(*point)
        time.sleep(wait)

    def execute(self, picker):
        self.wait(lambda text: bool(text), "fresh fixture READY token")
        for step in range(self.args.from_step, self.args.through_step + 1):
            if "BEGIN: " + NAMES[step - 1] in self.log():
                raise RuntimeError("step already started; do not replay stale coordinates/results")
            self.screenshot(f"{step:02d}-page")
            self.click_target(step, "next")
            self.wait(lambda text: "BEGIN: " + NAMES[step - 1] in text, "native interaction start")
            if step >= 7:
                self.click_target(step, "child")
            time.sleep(self.args.settle)
            self.screenshot(f"{step:02d}-native-open")
            if step <= 3:
                if self.args.fep_submit:
                    self.wait(lambda text: self.input.fep_window(), "same-PID FEP input modal")
                    self.records.append({"fep": self.input.submit_fep(self.args.fep_submit), "monotonic": time.monotonic()})
                    self.wait(lambda text: not self.input.fep_window(), "FEP Submit completion")
                    time.sleep(self.args.settle)
                    self.screenshot(f"{step:02d}-native-after-fep")
                if self.args.prompt_buttons:
                    self.native_point(self.args.prompt_buttons[2:] if step == 3 else self.args.prompt_buttons[:2])
                else:
                    self.key("F2" if step == 3 else "F1")
            elif step == 6:
                if self.args.picker_cancel:
                    self.native_point(self.args.picker_cancel)
                else:
                    self.key("F2")
            else:
                if step >= 7:
                    marker = "LIFECYCLE: iframe " + ("detached while native picker is open" if step == 7
                        else "navigation completed while picker is open")
                    self.wait(lambda text: marker in text, "actual page invalidation before choosing")
                    self.screenshot(f"{step:02d}-native-after-invalidation")
                for number, action in enumerate(picker):
                    for _ in range(action.get("count", 1)):
                        if "point" in action:
                            self.native_point(action["point"], action.get("wait", .5))
                        else:
                            self.key(action["key"], action.get("wait", .5))
                    self.screenshot(f"{step:02d}-picker-{number + 1:02d}")
            self.wait(lambda text: "PASS: " + NAMES[step - 1] in text, "page assertion " + NAMES[step - 1])
            self.screenshot(f"{step:02d}-passed")
        if self.args.through_step == 8:
            self.wait(lambda text: "RESULT: belle-native-ui passed=8 failed=0 total=8" in text, "eight page assertions")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--guest-log", type=Path, required=True)
    parser.add_argument("--run-token", required=True)
    parser.add_argument("--content-rect", type=int, nargs=4, metavar=("X", "Y", "WIDTH", "HEIGHT"), required=True,
                        help="HTML content rectangle relative to the selected Qt window, excluding URL bar/softkeys")
    parser.add_argument("--picker-actions", type=Path, help="calibrated native key sequence; required for steps 4/5/7/8")
    parser.add_argument("--fep-submit", type=int, nargs=4, metavar=("X", "Y", "WIDTH", "HEIGHT"),
                        help="calibrated Submit point and expected size of EKA's separate 'Enter the text' modal")
    parser.add_argument("--prompt-buttons", type=int, nargs=4, metavar=("OK_X", "OK_Y", "CANCEL_X", "CANCEL_Y"),
                        help="calibrated native query button points relative to the main window; default F1/F2")
    parser.add_argument("--picker-cancel", type=int, nargs=2, metavar=("X", "Y"),
                        help="calibrated native memory-picker Cancel point relative to the main window; default F2")
    parser.add_argument("--from-step", type=int, default=1, choices=range(1, 9))
    parser.add_argument("--through-step", type=int, default=8, choices=range(1, 9))
    parser.add_argument("--timeout", type=float, default=90)
    parser.add_argument("--settle", type=float, default=1.5)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.from_step > args.through_step or args.timeout <= 0 or not 0 <= args.settle <= 10:
        parser.error("invalid step range or timing")
    if min(args.content_rect[:2]) < 0 or min(args.content_rect[2:]) <= 0:
        parser.error("invalid content rectangle")
    if args.fep_submit and not (0 <= args.fep_submit[0] < args.fep_submit[2]
                               and 0 <= args.fep_submit[1] < args.fep_submit[3]):
        parser.error("invalid FEP Submit calibration")
    if any(value < 0 for value in (args.prompt_buttons or []) + (args.picker_cancel or [])):
        parser.error("native button coordinates must be nonnegative")
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,80}", args.run_token):
        parser.error("run token must contain 1–80 ASCII letters/digits/underscores/hyphens")
    needs_picker = any(step in (4, 5, 7, 8) for step in range(args.from_step, args.through_step + 1))
    if needs_picker and not args.picker_actions:
        parser.error("supply a calibrated --picker-actions plan; native file choices are never guessed")
    picker = load_picker(args.picker_actions) if args.picker_actions else []
    device = XInput(args.pid)
    session = Interaction(args, device)
    result = {"status": "FAIL", "pid": args.pid, "window": hex(device.window), "run": args.run_token,
              "window_geometry": device.geometry(device.window), "content_rect": args.content_rect,
              "from_step": args.from_step, "through_step": args.through_step,
              "scope": "Real XTest interaction only; normal runner shutdown/panic checks and upload records remain required."}
    try:
        session.execute(picker)
        result["status"] = "INTERACTION_COMPLETE" if args.through_step == 8 else "PARTIAL"
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        result["error"] = str(error)
        session.screenshot("failure")
    finally:
        result["actions"] = session.records
        (args.output / "interaction.json").write_text(json.dumps(result, indent=2) + "\n")
        device.close()
    print(json.dumps(result, indent=2))
    return int(result["status"] == "FAIL")


if __name__ == "__main__":
    raise SystemExit(main())
