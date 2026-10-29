#!/usr/bin/env python3
"""Host-only checks that the evidence gate rejects unexecuted JIT/DFG tiers."""
import copy
import json
import unittest

from arm11_jit_common import CASES, compare_results, validate_output


def result_for(mode):
    return {"mode": mode, "completed": list(CASES), "digest": list(range(len(CASES))),
            "tiers": [mode == "llint", mode == "baseline", mode == "dfg"],
            "compiled": {name: int(mode == "dfg") for name in CASES[1:]},
            "dfgBodies": {name: mode == "dfg" for name in CASES[2:]},
            "osrEntry": {"startedInDFG": False, "enteredDFG": True},
            "osrExit": {"before": True, "after": False},
            "rabTransition": {"before": mode == "dfg", "after": False, "value": 17, "attempts": 1}}


def output_for(result):
    return "\n".join([*("PASS: " + name for name in CASES), "RESULT " + json.dumps(result), f"PASS: arm11-jit {len(CASES)}/{len(CASES)}"]) + "\n"


class EvidenceTests(unittest.TestCase):
    def test_profile_records_and_equal_semantics(self):
        values = [validate_output(output_for(result_for(mode)), mode) for mode in ("llint", "baseline", "dfg")]
        compare_results(values)

    def test_macros_or_success_marker_do_not_prove_tier_execution(self):
        with self.assertRaises(ValueError):
            validate_output(f"ENABLE_JIT=1 ENABLE_DFG_JIT=1\nPASS: arm11-jit {len(CASES)}/{len(CASES)}\n", "dfg")
        value = result_for("dfg")
        value["tiers"] = [True, True, False]
        with self.assertRaises(ValueError):
            validate_output(output_for(value), "dfg")

    def test_compilation_without_executing_bodies_is_rejected(self):
        value = result_for("dfg")
        value["dfgBodies"] = {}
        with self.assertRaises(ValueError):
            validate_output(output_for(value), "dfg")
        value = result_for("dfg")
        value["compiled"]["softfp-math"] = 0
        with self.assertRaises(ValueError):
            validate_output(output_for(value), "dfg")

    def test_osr_must_be_observed_in_both_directions(self):
        for key, replacement in (("osrEntry", {"startedInDFG": True, "enteredDFG": True}),
                                 ("osrExit", {"before": True, "after": True})):
            value = result_for("dfg")
            value[key] = replacement
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate_output(output_for(value), "dfg")

    def test_malformed_or_duplicate_records_are_rejected(self):
        for replacement in ([], None, 1):
            with self.subTest(replacement=replacement), self.assertRaises(ValueError):
                validate_output(output_for(replacement), "dfg")
        value = result_for("dfg")
        value["dfgBodies"] = []
        with self.assertRaises(ValueError):
            validate_output(output_for(value), "dfg")
        text = output_for(result_for("dfg"))
        with self.assertRaises(ValueError):
            validate_output(text + text, "dfg")

    def test_rab_requires_observed_guard_exit_and_recovered_value(self):
        for replacement in ({}, {"before": False, "after": False, "value": 17},
                            {"before": True, "after": True, "value": 17},
                            {"before": True, "after": False, "value": 0},
                            {"before": True, "after": False, "value": 17, "attempts": 4001}):
            value = result_for("dfg")
            value["rabTransition"] = replacement
            with self.subTest(replacement=replacement), self.assertRaises(ValueError):
                validate_output(output_for(value), "dfg")

    def test_tier_result_differences_are_failures(self):
        baseline = result_for("baseline")
        dfg = copy.deepcopy(result_for("dfg"))
        dfg["digest"][5] += 0.125
        with self.assertRaises(ValueError):
            compare_results([baseline, dfg])

    def test_disabled_dfg_sentinel_is_never_compilation_evidence(self):
        for mode in ("llint", "baseline"):
            value = result_for(mode)
            value["compiled"] = {name: 1000000 for name in CASES[1:]}
            validate_output(output_for(value), mode)
        value = result_for("dfg")
        value["compiled"]["softfp-math"] = 1000000
        with self.assertRaises(ValueError):
            validate_output(output_for(value), "dfg")


if __name__ == "__main__":
    unittest.main()
