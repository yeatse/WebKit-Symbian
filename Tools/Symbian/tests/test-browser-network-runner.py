#!/usr/bin/env python3
"""Host-only regression checks; never starts EKA2L1 or changes an installed CA bundle."""
import copy
from contextlib import contextmanager
import http.server
import importlib.util
import json
import os
from pathlib import Path
import select
import shutil
import signal
import socket
import ssl
import stat
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("network_runner", Path(__file__).with_name("run-browser-network-tests.py"))
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class RunnerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.certificates = tempfile.TemporaryDirectory()
        cls.ca = Path(cls.certificates.name) / "ca.pem"
        subprocess.run(["openssl", "req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256",
                        "-nodes", "-days", "1", "-subj", "/CN=Runner restoration test",
                        "-keyout", str(cls.ca.with_suffix(".key")), "-out", str(cls.ca)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        cls.tls = Path(cls.certificates.name) / "tls"
        subprocess.run([str(Path(__file__).with_name("make-test-certificates.sh")), str(cls.tls)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    @classmethod
    def tearDownClass(cls):
        cls.certificates.cleanup()

    @contextmanager
    def private_x_display(self):
        if not shutil.which("Xvfb"):
            self.skipTest("Xvfb is required for the real driver's graceful-close fixture")
        read_fd, write_fd = os.pipe()
        server = subprocess.Popen(["Xvfb", "-displayfd", str(write_fd), "-screen", "0", "320x240x24", "-nolisten", "tcp"],
                                  pass_fds=(write_fd,), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        os.close(write_fd)
        try:
            if not select.select([read_fd], [], [], 5)[0]:
                self.fail("Private Xvfb did not become ready")
            number = os.read(read_fd, 32).decode().strip()
            self.assertTrue(number.isdecimal(), "Private Xvfb did not return a display number")
            yield ":" + number
        finally:
            os.close(read_fd)
            server.terminate()
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.kill()
                server.wait(timeout=5)

    @contextmanager
    def tls_servers(self):
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                self.send_response(200)
                self.send_header("Content-Length", "2")
                self.end_headers()
                self.wfile.write(b"ok")

            def log_message(self, *args):
                pass

        servers = []
        try:
            for name in ("good", "wrong", "untrusted"):
                server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                context.load_cert_chain(self.tls / f"{name}.crt", self.tls / f"{name}.key")
                server.socket = context.wrap_socket(server.socket, server_side=True)
                thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": 0.01}, daemon=True)
                thread.start()
                servers.append((name, server, thread))
            yield {name: server.server_port for name, server, _ in servers}
        finally:
            for _, server, thread in servers:
                server.shutdown()
                server.server_close()
                thread.join(timeout=5)

    def test_tls_preflight_verifies_endpoint_identity_and_negative_causes(self):
        with self.tls_servers() as ports:
            evidence = []
            runner.check_tls_fixtures(self.tls / "ca.crt", evidence, ports)
            self.assertEqual(len(evidence), 5)
            self.assertTrue(all(item["status"] == "PASS" for item in evidence))
            self.assertEqual(evidence[3]["verify_code"], 62)
            self.assertEqual(evidence[4]["verify_code"], 18)
            for item in evidence[:3]:
                self.assertEqual(len(item["certificate_sha256"]), 64)
                self.assertEqual(item["http_status"], 200)

    def test_tls_preflight_rejects_unreachable_and_swapped_listeners(self):
        with self.tls_servers() as ports, socket.socket() as unavailable:
            unavailable.bind(("127.0.0.1", 0))
            for replacement in (unavailable.getsockname()[1], ports["good"]):
                evidence = []
                with self.assertRaisesRegex(RuntimeError, "TLS preflight"):
                    runner.check_tls_fixtures(self.tls / "ca.crt", evidence, {**ports, "wrong": replacement})
                self.assertEqual(evidence[-1]["status"], "FAIL")
                self.assertNotEqual(evidence[-1].get("error_type"), None)

    def test_existing_ca_is_restored_after_exception(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "cacert.pem"
            original = b"original bytes\r\n\x00\xff\n"
            target.write_bytes(original)
            target.chmod(0o640)
            restoration = {}
            with self.assertRaisesRegex(RuntimeError, "fixture failure"):
                with runner.temporary_test_ca(target, self.ca, restoration):
                    self.assertTrue(target.read_bytes().startswith(original))
                    self.assertIn(self.ca.read_bytes(), target.read_bytes())
                    raise RuntimeError("fixture failure")
            self.assertEqual(target.read_bytes(), original)
            self.assertEqual(stat.S_IMODE(target.stat().st_mode), 0o640)
            self.assertTrue(restoration["restored"])

    def test_absent_ca_and_created_directories_are_restored(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "resource/webkit/cacert.pem"
            with self.assertRaises(KeyboardInterrupt):
                with runner.temporary_test_ca(target, self.ca):
                    self.assertTrue(target.is_file())
                    raise KeyboardInterrupt()
            self.assertFalse(target.exists())
            self.assertFalse(target.parent.exists())

    def test_ca_symlink_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            original = Path(directory) / "original.pem"
            original.write_bytes(b"unchanged")
            target = Path(directory) / "cacert.pem"
            target.symlink_to(original)
            with self.assertRaises(RuntimeError):
                with runner.temporary_test_ca(target, self.ca):
                    self.fail("Symlink was accepted")
            self.assertEqual(original.read_bytes(), b"unchanged")
            self.assertTrue(target.is_symlink())

    def test_hardlinked_ca_is_rejected_without_changing_either_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "cacert.pem"
            alias = Path(directory) / "shared.pem"
            target.write_bytes(b"shared original")
            os.link(target, alias)
            with self.assertRaises(RuntimeError):
                with runner.temporary_test_ca(target, self.ca):
                    self.fail("Hardlinked bundle was accepted")
            self.assertEqual(target.read_bytes(), b"shared original")
            self.assertEqual(alias.read_bytes(), b"shared original")
            self.assertEqual(target.stat().st_ino, alias.stat().st_ino)

    def test_reports_require_token_count_unique_names_and_success(self):
        report = {"suite": "tls", "run": "current", "results": [
            {"name": name, "status": "PASS", "detail": ""} for name in runner.EXPECTED["tls"]]}
        runner.validate_report(report, "tls", "current")
        variants = []
        wrong_token = copy.deepcopy(report); wrong_token["run"] = "old"; variants.append(wrong_token)
        short = copy.deepcopy(report); short["results"].pop(); variants.append(short)
        duplicate = copy.deepcopy(report); duplicate["results"][1] = duplicate["results"][0]; variants.append(duplicate)
        failed = copy.deepcopy(report); failed["results"][0]["status"] = "FAIL"; variants.append(failed)
        for value in variants:
            with self.subTest(value=value), self.assertRaises(ValueError):
                runner.validate_report(value, "tls", "current")

    def test_old_and_duplicate_report_files_are_rejected(self):
        report = {"suite": "tls", "run": "current", "results": [
            {"name": name, "status": "PASS"} for name in runner.EXPECTED["tls"]]}
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            (directory / "old.json").write_text(json.dumps(report))
            with self.assertRaises(RuntimeError):
                runner.find_report(directory, {"old.json"}, "tls", "current")
            (directory / "new.json").write_text(json.dumps(report))
            path, _ = runner.find_report(directory, {"old.json"}, "tls", "current")
            self.assertEqual(path.name, "new.json")
            with self.assertRaises(RuntimeError):
                runner.find_report(directory, set(), "tls", "current")

    def test_inherited_lock_reuses_lock_and_blocks_other_open_descriptions(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / ".browser-run.lock"
            with runner.exclusive_lock(path) as lock:
                program = "import fcntl,os; fcntl.flock(int(os.environ['LOCK_FD']),fcntl.LOCK_EX|fcntl.LOCK_NB)"
                subprocess.run(["bash", "-c", 'python3 -c "$CHECK_LOCK"'], check=True,
                               pass_fds=(lock.fileno(),), env={**os.environ, "LOCK_FD": str(lock.fileno()), "CHECK_LOCK": program})
                with self.assertRaises(RuntimeError):
                    with runner.exclusive_lock(path):
                        self.fail("A separate lock should fail")

    def test_real_driver_with_fake_emulator_correlates_reports_and_restores_ca(self):
        with self.private_x_display() as display, tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            work = root / "work"; work.mkdir()
            data = root / "data"; data.mkdir()
            reports = root / "reports"; reports.mkdir()
            image = root / "browser.exe"; image.write_bytes(b"host-only image fixture")
            for name in ("browser_reg.rsc", "browser_loc.rsc"):
                (root / name).write_bytes(b"resource fixture")
            icu = root / "icu/install/share/icu"; icu.mkdir(parents=True)
            (icu / "icudt70l.dat").write_bytes(b"data fixture")
            bundle = data / "drives/c/resource/webkit/cacert.pem"
            bundle.parent.mkdir(parents=True); bundle.write_bytes(b"original bundle bytes")
            binaries = root / "bin"; binaries.mkdir()
            fake_import = binaries / "import"
            fake_import.write_text("#!/usr/bin/env python3\nimport pathlib,sys; pathlib.Path(sys.argv[-1]).write_bytes(b'fixture screenshot')\n")
            fake_import.chmod(0o755)
            emulator = binaries / "fake-emulator"
            emulator.write_text("#!/usr/bin/env python3\n" +
                "import importlib.util,json,os,pathlib,sys,urllib.parse\n" +
                "expected=" + repr(runner.EXPECTED) + "\n" +
                "url=urllib.parse.urlsplit(sys.argv[-1].strip()); suite=pathlib.Path(url.path).stem\n" +
                "token=urllib.parse.parse_qs(url.query)['run'][0]\n" +
                "data=pathlib.Path(os.environ['EKA2L1_DATA'])\n" +
                "if suite=='tls': assert b'BEGIN CERTIFICATE' in (data/'drives/c/resource/webkit/cacert.pem').read_bytes()\n" +
                "names=expected[suite]; summary=f'PASS: {suite} {len(names)}/{len(names)}'\n" +
                "log_names=[name for name in names if name!=os.environ.get('FIXTURE_DROP_LINE')]\n" +
                "report={'suite':suite,'run':token,'results':[{'name':name,'status':'PASS'} for name in names]}\n" +
                "(pathlib.Path(os.environ['FIXTURE_REPORTS'])/(token+'.json')).write_text(json.dumps(report))\n" +
                "spec=importlib.util.spec_from_file_location('browser_fixture'," + repr(str(Path(__file__).with_name("test-browser-runner.py"))) + ")\n" +
                "fixture=importlib.util.module_from_spec(spec); spec.loader.exec_module(fixture)\n" +
                "fixture.MARKER=''.join('PASS: '+name+'\\n' for name in log_names)+summary\n" +
                "os.environ['BROWSER_RUNNER_TEST_MODE']='normal'\n" +
                "print('host-only fake emulator started',flush=True); fixture.fake_emulator()\n")
            emulator.chmod(0o755)
            environment = {"EKA2L1": str(emulator), "DISPLAY": display, "FIXTURE_REPORTS": str(reports),
                           "SYMBIAN_WEBKIT_WORK": str(work), "ICU_DIR": str(root / "icu"),
                           "PATH": str(binaries) + os.pathsep + os.environ["PATH"],
                           "XDG_DATA_HOME": str(root / "xdg-data"), "XDG_CONFIG_HOME": str(root / "xdg-config"),
                           "XDG_CACHE_HOME": str(root / "xdg-cache"), "XDG_RUNTIME_DIR": str(root / "xdg-runtime")}
            output = root / "results"
            with patch.dict(os.environ, environment), patch.object(runner, "check_tls_fixtures") as preflight:
                status = runner.main([str(image), "--data", str(data), "--reports", str(reports),
                                      "--ca", str(self.ca), "--timeout", "1", "--output", str(output)])
                preflight.assert_called_once()
            self.assertEqual(status, 0, (output / "result.json").read_text())
            result = json.loads((output / "result.json").read_text())
            self.assertEqual([suite["passed"] for suite in result["suites"]], [23, 6])
            self.assertTrue(result["test_ca_restored"])
            self.assertEqual(bundle.read_bytes(), b"original bundle bytes")
            with patch.dict(os.environ, {**environment, "FIXTURE_DROP_LINE": "fetch"}):
                incomplete = root / "incomplete-console"
                status = runner.main([str(image), "--suite", "network", "--data", str(data),
                                      "--reports", str(reports), "--timeout", "1", "--output", str(incomplete)])
            self.assertEqual(status, 1)
            self.assertIn("not corroborated", (incomplete / "result.json").read_text())

    def test_cleanup_collects_session_created_by_a_term_handler(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ready = root / "ready"
            child = root / "child"
            escaped = root / "escaped.py"
            escaped.write_text("import os,pathlib,signal,subprocess,sys,time\n" +
                "def stop(signum,frame):\n" +
                " p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(60)'],start_new_session=True)\n" +
                " pathlib.Path(" + repr(str(child)) + ").write_text(str(p.pid))\n" +
                " raise SystemExit(0)\n" +
                "signal.signal(signal.SIGTERM,stop)\n" +
                "pathlib.Path(" + repr(str(ready)) + ").write_text(str(os.getpid()))\n" +
                "time.sleep(60)\n")
            with runner.adopt_orphaned_children() as excluded:
                program = "import subprocess,sys; subprocess.Popen([sys.executable,sys.argv[1]],start_new_session=True)"
                driver = subprocess.Popen([sys.executable, "-c", program, str(escaped)], start_new_session=True)
                driver.wait(timeout=5)
                deadline = time.monotonic() + 5
                while not ready.exists() and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(ready.exists())
                runner.stop_process_tree(driver, {}, excluded)
                self.assertTrue(child.exists(), "TERM handler did not exercise the late-fork path")
                for path in (ready, child):
                    with self.assertRaises(ProcessLookupError):
                        os.kill(int(path.read_text()), 0)

    def test_escaped_session_is_reaped_even_if_driver_already_exited(self):
        unrelated = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"], start_new_session=True)
        try:
            with runner.adopt_orphaned_children() as excluded:
                program = "import subprocess,sys; p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(60)'],start_new_session=True); print(p.pid,flush=True)"
                driver = subprocess.Popen([sys.executable, "-c", program], stdout=subprocess.PIPE, text=True, start_new_session=True)
                escaped = int(driver.stdout.readline())
                driver.stdout.close()
                driver.wait(timeout=5)
                # No snapshot was taken while the driver still existed.
                runner.stop_process_tree(driver, {}, excluded)
                with self.assertRaises(ProcessLookupError):
                    os.kill(escaped, 0)
                self.assertIsNone(unrelated.poll())
        finally:
            unrelated.terminate()
            unrelated.wait(timeout=5)


if __name__ == "__main__":
    unittest.main()
