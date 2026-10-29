"""Shared evidence checks for QEMU Linux and real Symbian ARM11 JSC runs."""
import json
import math
import re

COMMON_OPTIONS = ["--useDollarVM=true", "--useSharedArrayBuffer=true", "--useFTLJIT=false",
                  "--useConcurrentJIT=false", "--jitPolicyScale=0.05"]
PROFILE_OPTIONS = {
    "llint": COMMON_OPTIONS + ["--useJIT=false", "--useDFGJIT=false"],
    "baseline": COMMON_OPTIONS + ["--useJIT=true", "--useBaselineJIT=true", "--useDFGJIT=false"],
    "dfg": COMMON_OPTIONS + ["--useJIT=true", "--useBaselineJIT=true", "--useDFGJIT=true"],
}
CASES = ("execution-tier", "osr-entry", "osr-exit", "inline-caches", "call-arity", "softfp-math",
         "number-guards", "bitwise-unsigned", "double-comparisons", "gc-live-values", "rab-gsab", "exceptions")
PANIC = re.compile(r"access violation|panicked|KERN-EXEC|USER-EXEC|segmentation fault|illegal instruction", re.I)


def validate_output(text, mode):
    if mode not in PROFILE_OPTIONS:
        raise ValueError("Unknown execution profile")
    if PANIC.search(text) or re.search(r"\bFAIL:", text):
        raise ValueError("Guest reported a failure or crash")
    records = [line.removeprefix("RESULT ") for line in text.splitlines() if line.startswith("RESULT ")]
    if len(records) != 1:
        raise ValueError("Expected exactly one current RESULT record")
    try:
        result = json.loads(records[0])
    except ValueError as error:
        raise ValueError("Malformed RESULT record") from error
    if not isinstance(result, dict):
        raise ValueError("RESULT must be an object")
    if result.get("mode") != mode or result.get("completed") != list(CASES):
        raise ValueError("Wrong mode, missing cases, or unexpected case order")
    for marker in [*("PASS: " + name for name in CASES), f"PASS: arm11-jit {len(CASES)}/{len(CASES)}"]:
        if text.splitlines().count(marker) != 1:
            raise ValueError("Missing or repeated console marker: " + marker)
    digest = result.get("digest")
    if not isinstance(digest, list) or len(digest) != len(CASES) or any(
            isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) for value in digest):
        raise ValueError("Invalid semantic result digest")
    tiers = result.get("tiers")
    if not isinstance(tiers, list) or len(tiers) != 3 or any(type(value) is not bool for value in tiers):
        raise ValueError("Missing observed execution tiers")
    if not tiers[{"llint": 0, "baseline": 1, "dfg": 2}[mode]]:
        raise ValueError("Requested execution tier never ran")
    if (mode == "llint" and any(tiers[1:])) or (mode == "baseline" and tiers[2]):
        raise ValueError("An explicitly disabled tier ran")
    compiled = result.get("compiled")
    if not isinstance(compiled, dict) or set(compiled) != set(CASES[1:]) or any(
            type(value) is not int or value < 0 for value in compiled.values()):
        raise ValueError("Missing per-function DFG compilation counts")
    if mode == "dfg":
        if any(value == 0 or value == 1000000 for value in compiled.values()):
            raise ValueError("At least one kernel lacks a genuine DFG compilation count")
        bodies = result.get("dfgBodies", {})
        if not isinstance(bodies, dict) or not all(bodies.get(name) is True for name in CASES[2:]):
            raise ValueError("DFG compilation occurred without executing every kernel body")
        entry, exit_state = result.get("osrEntry", {}), result.get("osrExit", {})
        if not isinstance(entry, dict) or entry.get("startedInDFG") is not False or entry.get("enteredDFG") is not True:
            raise ValueError("No observed OSR entry during the first call")
        if not isinstance(exit_state, dict) or exit_state.get("before") is not True or exit_state.get("after") is not False:
            raise ValueError("No observed DFG OSR exit with recovered live values")
        rab = result.get("rabTransition", {})
        if (not isinstance(rab, dict) or rab.get("before") is not True or rab.get("after") is not False or rab.get("value") != 17
                or type(rab.get("attempts")) is not int or not 1 <= rab["attempts"] <= 4000):
            raise ValueError("No observed 32-bit RAB guard OSR exit with the correct value")
    else:
        # TestRunnerUtils deliberately pretends to have many compiles when any
        # required JIT tier is disabled. This sentinel is not compilation evidence.
        if any(value not in (0, 1000000) for value in compiled.values()):
            raise ValueError("Unexpected compilation count in a profile that disabled DFG")
        bodies = result.get("dfgBodies", {})
        if not isinstance(bodies, dict) or any(bodies.values()):
            raise ValueError("DFG code ran in a profile that disabled DFG")
        rab = result.get("rabTransition", {})
        if (not isinstance(rab, dict) or rab.get("before") is not False or rab.get("after") is not False or rab.get("value") != 17
                or type(rab.get("attempts")) is not int or rab["attempts"] != 1):
            raise ValueError("Invalid RAB result in a profile that disabled DFG")
    return result


def compare_results(results):
    if not results:
        raise ValueError("No successful profiles to compare")
    if len({result["mode"] for result in results}) != len(results):
        raise ValueError("Repeated execution profile")
    if any(result["digest"] != results[0]["digest"] for result in results[1:]):
        raise ValueError("Semantic results differ between execution tiers")
