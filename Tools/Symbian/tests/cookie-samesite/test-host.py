#!/usr/bin/env python3
"""Exercise production policy functions and SQL, plus the local fixture protocol.

This is not a substitute for browser tests: request context creation, real WTF
String parsing, WebCore's SQLite wrapper and on-device persistence need the guest.
"""

import argparse
import copy
import importlib.util
import json
import re
import shlex
import sqlite3
import subprocess
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
SOURCE = REPO / "Source/WebCore/platform/network/curl"
spec = importlib.util.spec_from_file_location("samesite_fixture", HERE / "server.py")
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)
validator_spec = importlib.util.spec_from_file_location(
    "samesite_validator", HERE / "validate.py"
)
validator = importlib.util.module_from_spec(validator_spec)
validator_spec.loader.exec_module(validator)


def sql_macro(name):
    text = (SOURCE / "CookieJarDB.cpp").read_text()
    match = re.search(
        r"^#define " + name + r"\s+\\\n((?:.*\\\n)*.*_s)", text, re.MULTILINE
    )
    if not match:
        raise AssertionError("SQL macro missing: " + name)
    return "".join(
        json.loads(part) for part in re.findall(r'"(?:[^"\\]|\\.)*"', match.group(1))
    )


class ProductionPolicy(unittest.TestCase):
    def test_actual_inline_rules(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            # Only the two data declarations are shimmed. All decision functions
            # are compiled directly from the production CookieSameSite.h.
            (work / "Cookie.h").write_text(
                "#pragma once\nnamespace WebCore { struct Cookie { enum class SameSitePolicy { None, Lax, Strict }; }; }\n"
            )
            (work / "SameSiteInfo.h").write_text(
                "#pragma once\nnamespace WebCore { struct SameSiteInfo { bool isSameSite; bool isTopSite; bool isSafeHTTPMethod; }; }\n"
            )
            code = r"""
#include "CookieSameSite.h"
#include <cassert>
using namespace WebCore;
int main() {
    using Policy = Cookie::SameSitePolicy;
    // Explicit outcomes; especially GET subresources, unsafe navigations, and
    // DOM reads must not inherit Lax's top-level safe navigation exception.
    struct Case { SameSiteInfo context; bool header; bool none, lax, strict; };
    Case cases[] = {
        {{true, false, false}, true, true, true, true},
        {{true, true, true}, true, true, true, true},
        {{false, false, true}, true, true, false, false},
        {{false, false, false}, true, true, false, false},
        {{false, true, true}, true, true, true, false},
        {{false, true, false}, true, true, false, false},
        {{false, true, true}, false, true, false, false},
        {{false, false, true}, false, true, false, false},
        {{true, true, true}, false, true, true, true},
    };
    for (auto c : cases) {
        assert(sameSiteCookieMayBeSent(Policy::None, c.context, c.header) == c.none);
        assert(sameSiteCookieMayBeSent(Policy::Lax, c.context, c.header) == c.lax);
        assert(sameSiteCookieMayBeSent(Policy::Strict, c.context, c.header) == c.strict);
    }
    assert(sameSiteCookieMayBeSet(Policy::Strict, {false, true, false}, true));
    assert(!sameSiteCookieMayBeSet(Policy::Strict, {false, true, false}, false));
    assert(!sameSiteCookieMayBeSet(Policy::Lax, {false, false, true}, true));
    assert(!sameSiteCookieMayBeSet(Policy::Lax, {false, false, true}, false));
    assert(sameSiteCookieMayBeSet(Policy::None, {false, false, false}, false));
    assert(sameSiteCookieMayBeSet(Policy::Strict, {true, false, false}, false));
    assert(cookieSameSitePolicyFromDatabase(0) == Policy::None);
    assert(cookieSameSitePolicyFromDatabase(1) == Policy::Lax);
    assert(cookieSameSitePolicyFromDatabase(2) == Policy::Strict);
    assert(cookieSameSitePolicyFromDatabase(-1) == Policy::Lax);
    assert(cookieSameSitePolicyFromDatabase(999) == Policy::Lax);
}
"""
            (work / "test.cpp").write_text(code)
            subprocess.run(
                [
                    *shlex.split(CXX),
                    "-std=c++20",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-I" + str(work),
                    "-I" + str(SOURCE),
                    str(work / "test.cpp"),
                    "-o",
                    str(work / "test"),
                ],
                check=True,
            )
            subprocess.run([str(work / "test")], check=True)

    def old_database(self, connection):
        # The actual v2 schema with only the new field removed is the v1 schema.
        old_sql = sql_macro("CREATE_COOKIE_TABLE_SQL").replace(
            "  sameSite INTEGER NOT NULL DEFAULT 1,", ""
        )
        self.assertNotIn("sameSite", old_sql)
        connection.execute(old_sql)
        connection.execute("PRAGMA user_version=1")
        connection.execute(
            "INSERT INTO Cookie(name,value,domain,path,expires,size,session,httponly,secure) VALUES('existing','secret','localhost','/',4102444800000,6,0,1,1)"
        )
        connection.commit()

    def test_actual_migration_sql_preserves_rows_and_persists_policy(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "cookies.db"
            with sqlite3.connect(path) as db:
                self.old_database(db)
                db.execute("BEGIN IMMEDIATE")
                db.execute(sql_macro("MIGRATE_COOKIE_SAMESITE_SQL"))
                db.execute(sql_macro("UPDATE_COOKIE_SCHEMA_SQL"))
                db.commit()
                self.assertEqual(
                    db.execute(
                        "SELECT value,httponly,secure,sameSite FROM Cookie"
                    ).fetchone(),
                    ("secret", 1, 1, 1),
                )
                self.assertEqual(db.execute("PRAGMA user_version").fetchone()[0], 2)
                db.execute(
                    sql_macro("SET_COOKIE_SQL"),
                    ("strict", "v", "localhost", "/", 4102444800000, 1, 0, 0, 1, 2),
                )
            with sqlite3.connect(path) as db:
                self.assertEqual(
                    db.execute(
                        "SELECT sameSite FROM Cookie WHERE name='strict'"
                    ).fetchone()[0],
                    2,
                )
                self.assertEqual(
                    db.execute("SELECT count(*) FROM Cookie").fetchone()[0], 2
                )

    def test_failed_transaction_keeps_v1_schema_and_rows(self):
        with sqlite3.connect(":memory:") as db:
            self.old_database(db)
            db.execute("BEGIN IMMEDIATE")
            db.execute(sql_macro("MIGRATE_COOKIE_SAMESITE_SQL"))
            # Simulate the production rollback after a failed version update.
            db.rollback()
            self.assertEqual(db.execute("PRAGMA user_version").fetchone()[0], 1)
            self.assertNotIn(
                "sameSite", [row[1] for row in db.execute("PRAGMA table_info(Cookie)")]
            )
            self.assertEqual(
                db.execute("SELECT value FROM Cookie").fetchone()[0], "secret"
            )

    def test_v1_with_surviving_column_keeps_rows_and_uses_lax(self):
        with sqlite3.connect(":memory:") as db:
            db.execute(sql_macro("CREATE_COOKIE_TABLE_SQL"))
            db.execute("PRAGMA user_version=1")
            for name, policy in (("old-none", 0), ("old-strict", 2)):
                db.execute(
                    sql_macro("SET_COOKIE_SQL"),
                    (name, "unchanged", "localhost", "/", 4102444800000, 9, 0, 1, 1, policy),
                )
            db.commit()
            before = db.execute("SELECT * FROM Cookie ORDER BY name").fetchall()
            # This is the real failure of the former unconditional ALTER.
            with self.assertRaisesRegex(sqlite3.OperationalError, "duplicate column"):
                db.execute(sql_macro("MIGRATE_COOKIE_SAMESITE_SQL"))
            db.execute("BEGIN IMMEDIATE")
            db.execute(sql_macro("RESET_LEGACY_COOKIE_SAMESITE_SQL"))
            db.execute(sql_macro("UPDATE_COOKIE_SCHEMA_SQL"))
            db.commit()
            columns = [row[1] for row in db.execute("PRAGMA table_info(Cookie)")]
            policy_column = columns.index("sameSite")
            after = db.execute("SELECT * FROM Cookie ORDER BY name").fetchall()
            for old, new in zip(before, after, strict=True):
                self.assertEqual(new[policy_column], 1)
                self.assertEqual(old[:policy_column] + old[policy_column + 1:],
                                 new[:policy_column] + new[policy_column + 1:])
            self.assertEqual(db.execute("PRAGMA user_version").fetchone()[0], 2)

    def test_old_browser_live_version_query_prevents_schema_removal(self):
        with sqlite3.connect(":memory:") as db:
            db.execute(sql_macro("CREATE_COOKIE_TABLE_SQL"))
            db.execute(sql_macro("UPDATE_COOKIE_SCHEMA_SQL"))
            db.execute(sql_macro("SET_COOKIE_SQL"),
                       ("surviving", "v", "localhost", "/", 4102444800000, 1, 0, 0, 1, 2))
            db.commit()
            # The released v1 verifier keeps its PRAGMA statement at ROW while
            # clearAllTables attempts DROP TABLE. The failed DROP is logged,
            # but the verifier still changes user_version to 1.
            version = db.execute("PRAGMA user_version")
            with self.assertRaises(sqlite3.OperationalError) as failure:
                db.execute("DROP TABLE Cookie")
            self.assertEqual(failure.exception.sqlite_errorcode, sqlite3.SQLITE_LOCKED)
            db.execute("PRAGMA user_version=1")
            version.close()
            db.execute("INSERT INTO Cookie(name,value,domain,path,expires,size,session,httponly,secure) "
                       "VALUES('after-downgrade','keep','localhost','/',4102444800000,4,0,1,1)")
            db.commit()
            self.assertIn("sameSite", [row[1] for row in db.execute("PRAGMA table_info(Cookie)")])
            db.execute("BEGIN IMMEDIATE")
            db.execute(sql_macro("RESET_LEGACY_COOKIE_SAMESITE_SQL"))
            db.execute(sql_macro("UPDATE_COOKIE_SCHEMA_SQL"))
            db.commit()
            self.assertEqual(db.execute("SELECT name,value,httponly,secure,sameSite FROM Cookie ORDER BY name").fetchall(),
                             [("after-downgrade", "keep", 1, 1, 1), ("surviving", "v", 0, 1, 1)])

    def test_failed_version_write_rolls_back_policy_reset(self):
        with sqlite3.connect(":memory:") as db:
            db.execute(sql_macro("CREATE_COOKIE_TABLE_SQL"))
            db.execute("PRAGMA user_version=1")
            db.execute(sql_macro("SET_COOKIE_SQL"),
                       ("existing", "keep", "localhost", "/", 4102444800000, 4, 0, 1, 1, 2))
            db.commit()
            db.set_authorizer(lambda action, name, value, *_:
                              sqlite3.SQLITE_DENY if action == sqlite3.SQLITE_PRAGMA
                              and name == "user_version" and value is not None else sqlite3.SQLITE_OK)
            with self.assertRaises(sqlite3.DatabaseError):
                with db:
                    db.execute("BEGIN IMMEDIATE")
                    db.execute(sql_macro("RESET_LEGACY_COOKIE_SAMESITE_SQL"))
                    db.execute(sql_macro("UPDATE_COOKIE_SCHEMA_SQL"))
            db.set_authorizer(None)
            self.assertEqual(db.execute("PRAGMA user_version").fetchone()[0], 1)
            self.assertEqual(db.execute("SELECT value,sameSite FROM Cookie").fetchall(), [("keep", 2)])


class FixtureProtocol(unittest.TestCase):
    def setUp(self):
        self.work = tempfile.TemporaryDirectory()
        self.server = fixture.Server(("127.0.0.1", 0), fixture.Handler)
        self.server.state = fixture.State(
            self.work.name,
            self.server.server_port,
            self.server.server_port + 1,
            self.server.server_port + 2,
        )
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base = "http://127.0.0.1:" + str(self.server.server_port)

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.work.cleanup()

    def test_seed_has_distinct_policies_and_observes_wire_cookie(self):
        with urllib.request.urlopen(self.base + "/start?run=fixture") as response:
            headers = response.headers.get_all("Set-Cookie")
            self.assertEqual(len(headers), 10)
            self.assertTrue(
                any("SameSite=None" in h and "; Secure" not in h for h in headers)
            )
        request = urllib.request.Request(
            self.base + "/echo?run=fixture", headers={"Cookie": "raw=unchanged"}
        )
        with urllib.request.urlopen(request) as response:
            self.assertEqual(json.load(response)["cookie"], "raw=unchanged")
        data = json.loads((Path(self.work.name) / "fixture.json").read_text())
        self.assertEqual(len(data["requests"]), 2)
        self.assertNotEqual(
            data["config"]["a"].split(":")[1], data["config"]["b"].split(":")[1]
        )

    def test_report_roundtrip_and_bad_run_rejected(self):
        value = [
            {
                "name": "wire",
                "status": "FAIL",
                "detail": "must not be converted to PASS",
            }
        ]
        request = urllib.request.Request(
            self.base + "/record?run=fixture", data=json.dumps(value).encode()
        )
        with urllib.request.urlopen(request) as response:
            self.assertEqual(response.status, 200)
        with urllib.request.urlopen(self.base + "/summary?run=fixture") as response:
            self.assertEqual(json.load(response), value)
        with self.assertRaises(urllib.error.HTTPError) as error:
            urllib.request.urlopen(self.base + "/start?run=../bad")
        self.assertEqual(error.exception.code, 400)


class ReportValidation(unittest.TestCase):
    def synthetic(self, resumed=False):
        token = "synthetic"
        config = {
            "run": token,
            "a": "https://localhost:8791",
            "b": "https://127.0.0.1:8791",
            "sameSiteOtherPort": "https://localhost:8792",
            "httpA": "http://localhost:8790",
        }
        names = validator.INITIAL_NAMES + (validator.RESUME_NAMES if resumed else ())
        rows = [
            {
                "name": name,
                "phase": "main"
                if i < 12
                else "nav"
                if i < 23
                else "resume"
                if i < 26
                else "resume-b",
                "status": "PASS",
                "detail": "",
            }
            for i, name in enumerate(names)
        ]
        requests = []

        def wire(path, host, keys=(), method="GET", phase=None, step=None):
            query = {"run": token}
            if phase:
                query["phase"] = phase
            if step:
                query["step"] = step
            requests.append(
                {
                    "path": path,
                    "host": host,
                    "method": method,
                    "query": query,
                    "tls": not host.endswith("8790"),
                    "time_ns": 1000 + len(requests),
                    "cookie": "; ".join(
                        "ss_" + token + "_" + key + "=1" for key in sorted(keys)
                    ),
                }
            )

        a, b = "localhost:8791", "127.0.0.1:8791"
        wire("/start", a)
        wire("/seed-b", b)
        wire("/echo", "localhost:8792", validator.ALL, phase="main")
        wire("/ws", a, validator.ALL, phase="main")
        wire("/ws", b, {"none"}, phase="main")
        wire("/ws", a, {"none"}, phase="nested-child")
        wire("/nav", b, validator.LAX, step="get")
        wire("/nav", a, {"none"}, method="POST", step="post")
        wire("/nav", b, {"none"}, method="POST", step="post307")
        wire("/nav", a, validator.LAX, step="chain-end")
        wire("/nav", a, {"none"}, method="POST", step="scheme-post")
        wire("/nav", a, validator.LAX, step="scheme-get")
        wire("/nav", "localhost:8790", step="scheme-post-source")
        wire("/nav", "localhost:8790", step="scheme-get-source")
        if resumed:
            wire("/resume", a, validator.ALL - {"session"})
            wire("/resume-b", b, validator.LAX)
        return {
            "suite": "cookie-samesite",
            "config": config,
            "requests": requests,
            "results": rows,
        }

    def test_manifest_matches_browser_cases(self):
        names = re.findall(r"await test\('([^']+)'", (HERE / "case.js").read_text())
        self.assertEqual(names, list(validator.INITIAL_NAMES + validator.RESUME_NAMES))

    def test_complete_initial_and_resume(self):
        self.assertEqual(
            validator.validate_initial(self.synthetic(), "synthetic", 1000)["passed"],
            23,
        )
        self.assertEqual(
            validator.validate_resumed(self.synthetic(True), "synthetic", 1000)[
                "passed"
            ],
            28,
        )

    def test_incomplete_failed_stale_or_wrong_cookie_cannot_pass(self):
        original = self.synthetic()
        bad = copy.deepcopy(original)
        bad["results"].pop()
        with self.assertRaises(ValueError):
            validator.validate_initial(bad, "synthetic")
        bad = copy.deepcopy(original)
        bad["results"].append(bad["results"][0])
        with self.assertRaises(ValueError):
            validator.validate_initial(bad, "synthetic")
        bad = copy.deepcopy(original)
        bad["results"][0]["status"] = "FAIL"
        with self.assertRaises(ValueError):
            validator.validate_initial(bad, "synthetic")
        with self.assertRaises(ValueError):
            validator.validate_initial(original, "different")
        with self.assertRaises(ValueError):
            validator.validate_initial(original, "synthetic", 1001)
        bad = copy.deepcopy(original)
        next(
            row for row in bad["requests"] if row.get("query", {}).get("step") == "post"
        )["cookie"] += "; ss_synthetic_strict=1"
        with self.assertRaises(ValueError):
            validator.validate_initial(bad, "synthetic")
        with self.assertRaises(ValueError):
            validator.validate_resumed(original, "synthetic")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="c++")
    arguments, rest = parser.parse_known_args()
    CXX = arguments.cxx
    unittest.main(argv=[__file__, *rest])
