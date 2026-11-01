#!/usr/bin/env python3
"""Drive the existing nine-case Belle clipboard page through genuine XTest input.

Connects only to an already running EKA process; never starts a guest, reads the
clipboard, injects DOM values, or substitutes clipboard events. The outer runner
must preserve/restore the old guest clipboard and own launch/cleanup locks.
"""

import argparse
import ctypes as C
import datetime
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import time

REPO = Path("/workspace/WebKit-Symbian-graphics")
INPUT = REPO / "Tools/Symbian/tests/belle-ui/native-interact.py"
CONTRACT = REPO / "Tools/Symbian/tests/belle-clipboard/contract.py"
SEQUENCE = {
    1: [("start", "start"), ("paste", "target")],
    2: [("start", "start"), ("paste", "target")],
    3: [("start", "start"), ("paste", "richTarget")],
    4: [("start", "start")],
    5: [("start", "start"), ("paste", "target")],
    6: [("start", "start"), ("paste", "target")],
    7: [("start", "start"), ("copy", "source"), ("paste", "target")],
    8: [("start", "start"), ("cut", "source"), ("paste", "target")],
    9: [("start", "start"), ("select-all", "source"), ("copy", "source"), ("paste", "target")],
}
SHORTCUT = {"copy": "c", "cut": "x", "paste": "v", "select-all": "a"}
MENU_KEYS = {"F1", "F2", "Return", "Escape", "Up", "Down", "Left", "Right", "Home", "End"}
CRASH = re.compile(r"FAIL:|KERN-EXEC|USER-EXEC|panicked|access violation|SIGSEGV|segmentation fault", re.I)


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fresh_log(text, run, mode):
    """Anchor to exactly one completed START line; never consume stale actions."""
    lines = text.splitlines(keepends=True)
    lines = [line for line in lines if line.endswith("\n")]
    starts = []
    for index, line in enumerate(lines):
        _, marker, data = line.partition("START: belle-clipboard ")
        if not marker:
            continue
        value = json.loads(data)
        if value.get("run") == run:
            if value != {"run": run, "mode": mode, "total": 9}:
                raise ValueError("fresh START has a different mode/contract")
            starts.append(index)
    if len(starts) > 1:
        raise ValueError("run token was restarted/reused")
    if not starts:
        return "", []
    fresh = "".join(lines[starts[0]:])
    if CRASH.search(fresh):
        raise RuntimeError("fresh guest log reported a fixture failure or crash; inspect retained log")
    actions = []
    for line in lines[starts[0] + 1:]:
        _, marker, data = line.partition("START: belle-clipboard ")
        if marker:
            raise ValueError("a different page run replaced this fixture")
        _, marker, data = line.partition("ACTION: belle-clipboard ")
        if not marker:
            continue
        value = json.loads(data)
        required = {"run", "step", "operation", "target", "mode", "x", "y", "width", "height"}
        if set(value) != required or value["run"] != run or value["mode"] != mode:
            raise ValueError("ACTION identity or shape is inconsistent")
        if type(value["step"]) is not int or value["step"] not in SEQUENCE:
            raise ValueError("ACTION step is outside the nine cases")
        if (value["operation"], value["target"]) not in SEQUENCE[value["step"]]:
            raise ValueError("unexpected operation/target for this case")
        if any(type(value[name]) not in (int, float) or not math.isfinite(value[name]) for name in ("x", "y", "width", "height")):
            raise ValueError("ACTION coordinates must be finite numbers")
        if not 0 < value["x"] < value["width"] or not 0 < value["y"] < value["height"]:
            raise ValueError("ACTION target is outside its browser viewport")
        actions.append(value)
    return fresh, actions


def start_point(action, content_rect):
    if action["operation"] != "start":
        raise ValueError("only Start may be clicked; keep the selected field/caret unchanged")
    left, top, width, height = content_rect
    scale = width / action["width"]
    if action["y"] * scale >= height:
        raise ValueError("Start is outside calibrated visible content")
    return [round(left + action["x"] * scale), round(top + action["y"] * scale)]


def menu_plan(path):
    value = json.loads(path.read_text())
    if not isinstance(value, dict) or set(value) != set(SHORTCUT):
        raise ValueError("menu plan must define copy/cut/paste/select-all independently")
    for operation, actions in value.items():
        if not isinstance(actions, list) or not 1 <= len(actions) <= 16:
            raise ValueError("each menu operation needs 1–16 calibrated actions")
        for action in actions:
            if not isinstance(action, dict) or set(action) - {"key", "point", "wait"}:
                raise ValueError("menu actions accept only key/point/wait")
            if ("key" in action) == ("point" in action):
                raise ValueError("menu actions need exactly one native key or point")
            if "key" in action and action["key"] not in MENU_KEYS:
                raise ValueError("menu plan accepts navigation keys only; clipboard shortcuts belong to keyboard mode")
            if "point" in action and (not isinstance(action["point"], list) or len(action["point"]) != 2
                or any(type(n) is not int or n < 0 for n in action["point"])):
                raise ValueError("menu points must be two nonnegative integers")
            if type(action.get("wait", .25)) not in (int, float) or not 0 <= action.get("wait", .25) <= 2:
                raise ValueError("menu settling wait must be between zero and two seconds")
    return value


class Driver:
    def __init__(self, args, device):
        self.args, self.x = args, device
        self.started = time.monotonic()
        self.deadline = self.started + args.deadline
        self.previous_size = 0
        self.records = []
        self.actions_consumed = 0
        self.menu = menu_plan(args.menu_plan) if args.menu_plan else None
        self.contract = load_module("clipboard_contract", args.contract)
        self.report = args.reports / (args.run_token + "-" + args.mode + ".json")
        if self.report.exists():
            raise RuntimeError("report already exists; start a fresh run token")
        self.result = {"status": "RUNNING", "run": args.run_token, "mode": args.mode,
            "pid": args.pid, "origin": args.origin, "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "through_step": args.through_step, "content_rect": args.content_rect, "main_size": self.x.initial_size,
            "input_module": str(args.input_module), "input_module_sha256": sha(args.input_module),
            "driver_sha256": sha(Path(__file__)), "contract_sha256": sha(args.contract),
            "menu_plan_sha256": sha(args.menu_plan) if args.menu_plan else None,
            "clipboard_read_or_written_directly": False, "dom_injection": False, "screenshots_taken": False,
            "clipboard_restoration": "outer runner responsibility; not observed by this driver",
            "records": self.records}
        self.ledger = (args.output / "actions.jsonl").open("x")

    def record(self, event, **fields):
        entry = {"event": event, "elapsed": time.monotonic() - self.started, **fields}
        self.records.append(entry)
        self.ledger.write(json.dumps(entry, sort_keys=True) + "\n")
        self.ledger.flush()

    def log(self):
        if not self.args.guest_log.exists():
            return "", []
        size = self.args.guest_log.stat().st_size
        if size < self.previous_size:
            raise RuntimeError("live guest log was truncated/replaced")
        if size > 8 * 1024 * 1024:
            raise RuntimeError("unexpectedly large live guest log")
        self.previous_size = size
        return fresh_log(self.args.guest_log.read_text(errors="replace"), self.args.run_token, self.args.mode)

    def input_key(self, window, key_name, control=False):
        self.x.check()
        self.x.check_focus(window)
        def keycode(name):
            code = self.x.x.XKeysymToKeycode(self.x.display, self.x.x.XStringToKeysym(name.encode("ascii")))
            if not code:
                raise RuntimeError("required X11 input mapping is unavailable")
            return code
        key = keycode(key_name)
        modifier = keycode("Control_L") if control else None
        try:
            if modifier and not self.x.t.XTestFakeKeyEvent(self.x.display, modifier, 1, 0):
                raise RuntimeError("XTest Control press failed")
            if not self.x.t.XTestFakeKeyEvent(self.x.display, key, 1, 0):
                raise RuntimeError("XTest key press failed")
            self.x.x.XSync(self.x.display, 0)
            time.sleep(.08)
        finally:
            self.x.t.XTestFakeKeyEvent(self.x.display, key, 0, 0)
            if modifier:
                self.x.t.XTestFakeKeyEvent(self.x.display, modifier, 0, 0)
            self.x.x.XSync(self.x.display, 0)

    def check_fep(self):
        window = self.x.fep_window()
        if not window:
            return
        if not self.args.fep_escape:
            raise RuntimeError("same-PID FEP opened; review its cancel behavior and calibrate --fep-escape before a new run")
        geometry = self.x.geometry(window)
        if geometry[2:] != self.args.fep_escape:
            raise RuntimeError("FEP size changed; do not dismiss an uncalibrated dialog")
        # Escape is QDialog's genuine Reject path. Do not read/submit editor text,
        # click a page field, or restore DOM selection after this cancellation.
        self.input_key(window, "Escape")
        self.record("fep-escape", window=hex(window), geometry=geometry)
        deadline = min(self.deadline, time.monotonic() + 5)
        while time.monotonic() < deadline:
            self.x.check()
            if not self.x.fep_window():
                self.restore_main_focus_after_fep()
                return
            time.sleep(.1)
        raise RuntimeError("FEP did not close after genuine Escape")

    def restore_main_focus_after_fep(self):
        # Called only after the calibrated same-PID modal accepted real Escape
        # and disappeared. Bare Xvfb has no window manager to return focus to
        # its parent; restore only server/root focus, never another application.
        self.x.check()
        focused, revert = C.c_ulong(), C.c_int()
        self.x.x.XGetInputFocus(self.x.display, C.byref(focused), C.byref(revert))
        if focused.value in (0, 1, self.x.root):  # None, PointerRoot, root.
            previous = focused.value
            self.x.x.XRaiseWindow(self.x.display, self.x.window)
            self.x.x.XSetInputFocus(self.x.display, self.x.window, 2, 0)
            self.x.x.XSync(self.x.display, 0)
            self.x.check_focus(self.x.window)
            self.record("main-focus-restored-after-fep", previous=hex(previous), window=hex(self.x.window))
        else:
            # Existing check accepts main/descendant focus and rejects any
            # unrelated window. Do not focus over another window or click DOM.
            self.x.check_focus(self.x.window)

    def wait(self, check, description):
        deadline = min(self.deadline, time.monotonic() + self.args.action_timeout)
        while time.monotonic() < deadline:
            self.x.check()
            self.check_fep()
            value = check()
            if value:
                return value
            time.sleep(.1)
        raise RuntimeError("bounded deadline waiting for " + description)

    def next_action(self, step, operation, target):
        def find():
            text, actions = self.log()
            if len(actions) <= self.actions_consumed:
                return None
            action = actions[self.actions_consumed]
            if (action["step"], action["operation"], action["target"]) != (step, operation, target):
                raise RuntimeError("page ACTION sequence changed; do not guess/replay an operation")
            return action
        action = self.wait(find, "case " + str(step) + " " + operation + " ACTION")
        self.actions_consumed += 1
        return action

    def command(self, action):
        operation, step = action["operation"], action["step"]
        self.check_fep()
        self.log()  # Stop on any page failure before further native input.
        if operation == "start":
            point = start_point(action, self.args.content_rect)
            self.x.click(*point)
            self.record("start-pointer-click", step=step, point=point, action=action)
        elif self.args.mode == "keyboard":
            self.input_key(self.x.window, SHORTCUT[operation], control=True)
            self.record("native-keyboard-operation", step=step, operation=operation,
                        shortcut="Ctrl+" + SHORTCUT[operation].upper(), action=action)
        else:
            for number, item in enumerate(self.menu[operation]):
                if time.monotonic() >= self.deadline:
                    raise RuntimeError("overall input deadline expired")
                if self.x.fep_window():
                    raise RuntimeError("FEP appeared while a menu was open")
                if "point" in item:
                    self.x.click(*item["point"])
                else:
                    self.x.key(item["key"])
                self.record("native-menu-action", step=step, operation=operation, number=number + 1, action=item)
                time.sleep(item.get("wait", .25))
            self.record("native-menu-operation", step=step, operation=operation, action=action)
        time.sleep(self.args.settle)

    def execute(self):
        self.wait(lambda: self.log()[0], "fresh nine-case START")
        text, _ = self.log()
        if "BEGIN: belle-clipboard " in text:
            raise RuntimeError("a case already began; use a new run instead of replaying it")
        for step in range(1, self.args.through_step + 1):
            for operation, target in SEQUENCE[step]:
                action = self.next_action(step, operation, target)
                self.command(action)
            marker = "PASS: " + self.contract.CASES[step - 1]
            self.wait(lambda: marker in self.log()[0], "case " + str(step) + " page PASS")
            self.record("case-passed", step=step, name=self.contract.CASES[step - 1])
        if self.args.through_step < 9:
            self.result["status"] = "PARTIAL"
            return
        self.wait(lambda: "STORED: belle-clipboard " + self.args.run_token in self.log()[0]
            and self.report.exists(), "immutable HTTP report storage")
        report = json.loads(self.report.read_text())
        self.contract.validate(report, self.args.run_token, self.args.mode, self.args.origin)
        text, actions = self.log()
        results = [json.loads(line.partition("RESULT: belle-clipboard ")[2]) for line in text.splitlines()
                   if "RESULT: belle-clipboard " in line]
        if results != [report] or len(actions) != self.actions_consumed:
            raise RuntimeError("console/HTTP result differs or extra ACTION appeared")
        self.result.update(status="INPUT_AND_PAGE_CONTRACT_PASS_PENDING_RUNTIME", cases=9,
            report=str(self.report), report_sha256=sha(self.report))
        self.record("report-contract-passed", cases=9)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--guest-log", type=Path, required=True)
    parser.add_argument("--run-token", required=True)
    parser.add_argument("--mode", choices=("keyboard", "menu"), default="keyboard")
    parser.add_argument("--origin", default="http://localhost:8802")
    parser.add_argument("--content-rect", nargs=4, type=int, required=True, metavar=("X", "Y", "WIDTH", "HEIGHT"))
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--menu-plan", type=Path)
    parser.add_argument("--fep-escape", nargs=2, type=int, metavar=("WIDTH", "HEIGHT"),
        help="opt in to real Escape on the observed same-PID FEP, without editing or submitting its text")
    parser.add_argument("--through-step", type=int, choices=range(1, 10), default=9)
    parser.add_argument("--deadline", type=float, default=450)
    parser.add_argument("--action-timeout", type=float, default=40)
    parser.add_argument("--settle", type=float, default=.2)
    parser.add_argument("--input-module", type=Path, default=INPUT)
    parser.add_argument("--contract", type=Path, default=CONTRACT)
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,80}", args.run_token):
        parser.error("invalid fresh run token")
    if not re.fullmatch(r"http://(?:localhost|127\.0\.0\.1):[0-9]{1,5}", args.origin):
        parser.error("origin must be the controlled localhost HTTP fixture")
    if args.mode == "menu" and not args.menu_plan or args.mode == "keyboard" and args.menu_plan:
        parser.error("a calibrated menu plan belongs only to menu mode and is required there")
    if not 10 <= args.deadline <= 460 or not 1 <= args.action_timeout <= 40 or not 0 <= args.settle <= 2:
        parser.error("bounded timing must remain below the page's 45s/480s deadlines")
    if min(args.content_rect[:2]) < 0 or min(args.content_rect[2:]) <= 0 or args.fep_escape and min(args.fep_escape) <= 0:
        parser.error("invalid content/FEP calibration")
    args.output.mkdir(parents=True, exist_ok=False, mode=0o700)
    os.chmod(args.output, 0o700)
    device, driver = None, None
    result = {"status": "FAIL", "run": args.run_token, "mode": args.mode, "error": "setup did not finish"}
    try:
        device = load_module("native_input", args.input_module).XInput(args.pid)
        driver = Driver(args, device)
        driver.execute()
        result = driver.result
    except Exception as error:
        result = driver.result if driver else result
        result.update(status="FAIL", error=str(error))
    finally:
        if device:
            device.close()
        if driver:
            driver.ledger.close()
        result["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        with (args.output / "interaction.json").open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
    print(json.dumps({key: result.get(key) for key in ("status", "run", "mode", "cases", "error")}))
    return 1 if result["status"] == "FAIL" else 0


if __name__ == "__main__":
    raise SystemExit(main())
