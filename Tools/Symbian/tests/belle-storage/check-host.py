#!/usr/bin/env python3
"""Validate the fixture in Chromium; this is not a Symbian capability result."""
import argparse
import asyncio
import json
from pathlib import Path
import tempfile
import uuid

from contract import SUITES, validate


async def check(args):
    from playwright.async_api import async_playwright
    token = "host-" + uuid.uuid4().hex
    with tempfile.TemporaryDirectory(prefix="belle-storage-chromium-") as profile:
        async with async_playwright() as playwright:
            for phase in SUITES[args.suite]:
                context = await playwright.chromium.launch_persistent_context(profile,
                    headless=True, executable_path=str(args.chromium), args=["--no-sandbox"])
                try:
                    # Never leave a test URL for Chromium's session restore to
                    # reopen. A restored write page could invalidate a restart test.
                    for existing in context.pages:
                        await existing.goto("about:blank")
                    page = await context.new_page()
                    page.on("console", lambda message: print(message.text, flush=True))
                    await page.goto(f"{args.origin}/index.html?run={token}&phase={phase}")
                    await page.wait_for_function("document.getElementById('output').textContent.includes('BELLE_' + '" + args.suite.upper() + "_')", timeout=60000)
                    report = json.loads((args.reports / f"{token}-{phase}.json").read_text())
                    validate(report, token, phase, args.origin, args.suite)
                finally:
                    for page in context.pages:
                        await page.goto("about:blank")
                    await context.close()
    total = sum(len(cases) for cases in SUITES[args.suite].values())
    print(f"HOST FIXTURE PASS {total}/{total}: {token}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reports", type=Path, required=True)
    parser.add_argument("--suite", choices=tuple(SUITES), default="storage")
    parser.add_argument("--origin")
    parser.add_argument("--chromium", type=Path, default=Path("/usr/bin/chromium"))
    args = parser.parse_args()
    args.origin = args.origin or ("http://localhost:8780" if args.suite == "storage" else "http://localhost:8784")
    asyncio.run(check(args))


if __name__ == "__main__":
    main()
