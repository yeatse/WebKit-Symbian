#!/usr/bin/env python3
"""Real XTest interaction with an already running, exclusively owned EKA session.

Never launches an emulator or edits guest/production files. The parent runner
retains the emulator lock, bounded lifetime, normal shutdown and log ownership.
Credentials are read privately and never emitted or captured in screenshots.
"""

import argparse
import ctypes as C
import importlib.util
import json
import os
from pathlib import Path
import stat
import time
import urllib.request

ROOT = Path(__file__).resolve().parent
DEFAULT_INPUT = Path("/workspace/WebKit-Symbian-graphics/Tools/Symbian/tests/belle-ui/native-interact.py")


def load_input(path):
    spec = importlib.util.spec_from_file_location("belle_native_input", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.XInput


def read_credentials(path):
    if stat.S_IMODE(path.stat().st_mode) & 0o077:
        raise ValueError("fixture credentials file must be private (mode 0600)")
    value = json.loads(path.read_text())
    for name in ("username", "password", "wrong_password"):
        if not isinstance(value.get(name), str) or not 1 <= len(value[name]) <= 32:
            raise ValueError("invalid synthetic fixture credential")
        if any(c not in "abcdefghijklmnopqrstuvwxyz0123456789" for c in value[name]):
            raise ValueError("fixture credentials must be lowercase ASCII letters/digits")
    return value


class Drive:
    def __init__(self, args, device):
        self.args, self.x = args, device
        self.ready = json.loads((args.fixture / "ready.json").read_text())
        self.credentials = read_credentials(args.fixture / "credentials.private.json")
        self.base = "http://127.0.0.1:" + str(int(self.ready["port"]))
        self.opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        self.records = []
        if (args.guest_log.parent / "curl-verbose").exists():
            raise RuntimeError("remove the guest curl-verbose fixture marker before authentication input")
        live_log = args.guest_log.read_text(errors="replace") if args.guest_log.exists() else ""
        if "curl trace on" in live_log or os.environ.get("DEBUG_CURL"):
            raise RuntimeError("disable curl tracing and start with a fresh guest log before authentication input")
        self.output = {"run": self.ready["run"], "scheme": self.ready["scheme"],
            "case": self.ready["case"], "pid": args.pid, "status": "RUNNING",
            "records": self.records, "screenshots_taken": False,
            "native_input_module": str(args.input_module),
            "calibration": {"main_size": self.x.initial_size, "start_point": args.start_point,
                "fep_edit": args.fep_edit, "fep_submit": args.fep_submit,
                "prompt_buttons": args.prompt_buttons, "native_editor": args.native_editor}}

    def record(self, event, **fields):
        self.records.append({"event": event, "monotonic": time.monotonic(), **fields})

    def request(self, endpoint, body=None):
        url = self.base + endpoint + "?run=" + self.ready["run"]
        data = body.encode("ascii") if body else None
        with self.opener.open(urllib.request.Request(url, data=data), timeout=5) as response:
            data = response.read()
        return json.loads(data) if data else None

    def state(self):
        value = self.request("/state")
        if any(value.get(name) != self.ready[name] for name in ("run", "scheme", "case")):
            raise RuntimeError("fixture identity changed")
        return value["events"]

    def wait(self, condition, description, check=True):
        deadline = time.monotonic() + self.args.timeout
        while time.monotonic() < deadline:
            if check:
                self.x.check()
            value = condition()
            if value:
                return value
            time.sleep(.15)
        raise RuntimeError("timed out waiting for " + description)

    def page_event(self, name):
        return any(item["event"] == "page-" + name for item in self.state())

    def key_window(self, window, name, modifier=None):
        self.x.check()
        self.x.check_focus(window)
        def code(key):
            result = self.x.x.XKeysymToKeycode(self.x.display, self.x.x.XStringToKeysym(key.encode("ascii")))
            if not result:
                raise RuntimeError("a required input key has no X11 mapping")
            return result
        key = code(name)
        shift = code(modifier) if modifier else None
        try:
            if shift:
                if not self.x.t.XTestFakeKeyEvent(self.x.display, shift, 1, 0):
                    raise RuntimeError("XTest modifier press failed")
            if not self.x.t.XTestFakeKeyEvent(self.x.display, key, 1, 0):
                raise RuntimeError("XTest input press failed")
            self.x.x.XSync(self.x.display, 0)
            time.sleep(.035)
        finally:
            self.x.t.XTestFakeKeyEvent(self.x.display, key, 0, 0)
            if shift:
                self.x.t.XTestFakeKeyEvent(self.x.display, shift, 0, 0)
            self.x.x.XSync(self.x.display, 0)

    def edit_fep(self, role, value):
        # No input value, individual key, screenshot, clipboard or URL is logged.
        if not self.x.fep_window() and self.args.native_editor:
            self.x.click(*self.args.native_editor)
        window = self.wait(self.x.fep_window, "same-PID Enter the text modal for " + role)
        geometry = self.x.geometry(window)
        if geometry[2:] != self.args.fep_submit[2:]:
            raise RuntimeError("FEP modal size changed; recalibration required")
        self.x.click_window(window, geometry, *self.args.fep_edit)
        self.key_window(window, "a", "Control_L")
        self.key_window(window, "BackSpace")
        for character in value:
            self.key_window(window, character)
        self.x.submit_fep(self.args.fep_submit)
        self.wait(lambda: not self.x.fep_window(), "FEP editor submission")
        time.sleep(self.args.settle)
        self.record("editor-submitted", role=role)

    def native_button(self, accept):
        if self.args.prompt_buttons:
            self.x.click(*(self.args.prompt_buttons[:2] if accept else self.args.prompt_buttons[2:]))
        else:
            self.x.key("F1" if accept else "F2")
        self.record("native-accept" if accept else "native-cancel")
        time.sleep(self.args.settle)

    def username(self, attempt):
        self.record("username-prompt", attempt=attempt)
        self.edit_fep("username", self.credentials["username"])
        self.native_button(True)

    def password(self, attempt, wrong=False):
        self.record("password-prompt", attempt=attempt)
        self.edit_fep("password", self.credentials["wrong_password" if wrong else "password"])
        self.native_button(True)
        self.record("credential-submitted", attempt=attempt, expected="reject" if wrong else "accept")

    def leave_pending(self, at_password):
        if at_password:
            self.username(1)
        # EKA's host FEP is separate from the native query. Submit its empty
        # editor to expose the still-pending native query before lifecycle input.
        self.edit_fep("password" if at_password else "username", "")
        self.record("native-query-pending", step="password" if at_password else "username")

    def launch_exit_helper(self):
        if not self.args.exit_calibration:
            raise RuntimeError("exit case needs calibrated normal EKA Launch process menu")
        calibration = json.loads(self.args.exit_calibration.read_text())
        expected = {"menu", "launch_item", "dialog_size", "path_field", "launch_button"}
        if set(calibration) != expected or any(not isinstance(v, list) or len(v) != 2
            or any(not isinstance(n, int) or n < 0 for n in v) for v in calibration.values()):
            raise ValueError("invalid exit menu calibration")
        for name in ("menu", "launch_item"):
            self.x.click(*calibration[name])
            time.sleep(self.args.settle)
        def find_launch():
            matches = []
            for window in self.x.owned_windows():
                name = C.c_void_p()
                try:
                    if self.x.x.XFetchName(self.x.display, window, C.byref(name)) and name:
                        if C.string_at(name) == b"Launch process":
                            matches.append(window)
                finally:
                    if name:
                        self.x.x.XFree(name)
            if len(matches) > 1:
                raise RuntimeError("multiple same-PID Launch process dialogs")
            return matches[0] if matches else None
        window = self.wait(find_launch, "same-PID Launch process dialog")
        geometry = self.x.geometry(window)
        if geometry[2:] != calibration["dialog_size"]:
            raise RuntimeError("Launch process dialog size changed")
        self.x.click_window(window, geometry, *calibration["path_field"])
        self.key_window(window, "a", "Control_L")
        self.key_window(window, "BackSpace")
        for character in "C:\\sys\\bin\\browser_exit_request.exe":
            shifted = character.isupper() or character in ":_"
            name = character.lower() if character.isalpha() else {":": "semicolon", "_": "minus",
                "\\": "backslash", ".": "period"}.get(character, character)
            self.key_window(window, name, "Shift_L" if shifted else None)
        self.record("normal-exit-helper-launch", helper="C:\\sys\\bin\\browser_exit_request.exe")
        self.x.click_window(window, geometry, *calibration["launch_button"])
        self.output["status"] = "EXIT_REQUEST_SENT_PENDING_VALIDATION"

    def execute(self):
        if any(e["event"] == "page-started" for e in self.state()):
            raise RuntimeError("this fixture already started; do not replay a run")
        self.wait(lambda: self.page_event("ready"), "fresh browser fixture page")
        self.x.click(*self.args.start_point)
        self.record("start-clicked")
        self.wait(lambda: self.page_event("started"), "real page click")
        self.wait(lambda: any(e["event"] == "auth-request" for e in self.state()), "first challenge")
        case = self.ready["case"]
        if case in ("success", "retry"):
            for attempt in range(1, 4 if case == "retry" else 2):
                self.username(attempt)
                self.password(attempt, case == "retry")
                if case == "retry":
                    self.wait(lambda: sum(e["event"] == "auth-request" and e["authorization_present"]
                        for e in self.state()) >= attempt, "submitted credential reaching fixture")
            self.wait(lambda: self.page_event("loaded" if case == "success" else "exhausted"), "final HTTP response page")
        elif case.startswith("cancel"):
            self.leave_pending(case.endswith("password"))
            self.native_button(False)
        elif case.startswith("navigate"):
            self.leave_pending(case.endswith("password"))
            self.request("/command", "navigate")
            self.record("navigation-requested")
            self.wait(lambda: self.page_event("navigated"), "browser navigation completing with query pending")
        elif case.startswith("exit"):
            self.leave_pending(case.endswith("password"))
            self.launch_exit_helper()
            return
        if not case.startswith("navigate"):
            self.request("/command", "ping")
            self.wait(lambda: self.page_event("responsive"), "parent page responding after native query")
        # A visible fourth FEP is failure, never silently accepted or cancelled.
        before = len([e for e in self.state() if e["event"] == "auth-request"])
        deadline = time.monotonic() + self.args.quiet
        while time.monotonic() < deadline:
            self.x.check()
            if self.x.fep_window():
                raise RuntimeError("unexpected native editor remained or reappeared")
            time.sleep(.2)
        after = len([e for e in self.state() if e["event"] == "auth-request"])
        if after != before:
            raise RuntimeError("authentication continued during quiet observation")
        self.record("quiet-observation", seconds=self.args.quiet)
        self.output["status"] = "INTERACTION_COMPLETE_PENDING_VALIDATION"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--guest-log", type=Path, required=True,
        help="live guest C/Data/WebKit/log.txt; used to refuse enabled curl tracing")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--start-point", nargs=2, type=int, required=True)
    parser.add_argument("--fep-edit", nargs=2, type=int, required=True)
    parser.add_argument("--fep-submit", nargs=4, type=int, required=True,
        metavar=("X", "Y", "WIDTH", "HEIGHT"))
    parser.add_argument("--prompt-buttons", nargs=4, type=int)
    parser.add_argument("--native-editor", nargs=2, type=int)
    parser.add_argument("--exit-calibration", type=Path)
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--settle", type=float, default=.7)
    parser.add_argument("--quiet", type=float, default=5)
    parser.add_argument("--input-module", type=Path, default=DEFAULT_INPUT)
    args = parser.parse_args()
    if not 0 < args.timeout <= 120 or not .1 <= args.settle <= 10 or not 3 <= args.quiet <= 30:
        parser.error("invalid bounded timing")
    if min(args.start_point + args.fep_edit + args.fep_submit + (args.prompt_buttons or []) + (args.native_editor or [])) < 0:
        parser.error("coordinates must be nonnegative")
    if not (0 <= args.fep_submit[0] < args.fep_submit[2] and 0 <= args.fep_submit[1] < args.fep_submit[3]):
        parser.error("FEP Submit must be inside the calibrated modal")
    if not (args.fep_edit[0] < args.fep_submit[2] and args.fep_edit[1] < args.fep_submit[3]):
        parser.error("FEP edit point must be inside the calibrated modal")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # Reserve the output before any GUI input and never overwrite earlier input.
    stream = os.fdopen(os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), "w")
    driver, device = None, None
    result = {"status": "FAIL", "error": "setup did not complete"}
    try:
        device = load_input(args.input_module)(args.pid)
        driver = Drive(args, device)
        driver.execute()
        result = driver.output
    except Exception as error:
        result = driver.output if driver else result
        result.update(status="FAIL", error=str(error))
    finally:
        if device:
            device.close()
        json.dump(result, stream, indent=2)
        stream.write("\n")
        stream.close()
    print(json.dumps({key: result.get(key) for key in ("status", "run", "scheme", "case", "error")}))
    return 1 if result["status"] == "FAIL" else 0


if __name__ == "__main__":
    raise SystemExit(main())
