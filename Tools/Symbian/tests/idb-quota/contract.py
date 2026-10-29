"""Fixed page evidence contract; quotas must be demonstrated by operations."""
CASES = {
    "write": (
        "blob replacement and repeated put are atomic",
        "Blob clear abort preserves old committed payload",
        "Blob clear commit and put-clear release references",
        "Blob payload quota rejection preserves source and old record",
        "Blob clear commit returns quota to another database",
        "one transaction cannot exceed origin quota",
        "parallel databases share pending origin reservations",
        "durable writes stop below 32 MiB plus SQLite metadata",
        "another origin remains writable at origin quota",
        "deleteDatabase returns space for subsequent writes",
        "multiple origins share 128 MiB port quota",
        "stage committed data for independent process restart",
    ),
    "read": (
        "restart recovers committed array and Blob payloads",
        "restart reloads committed origin usage before granting space",
        "delete committed data before second restart",
    ),
    "erased": ("second restart observes deletion and allows fresh write",),
    "cleanup": ("remove only this run token across all test origins",),
}


def validate(report, run, phase, origin, require_success=True):
    if report.get("schema") != 1 or report.get("run") != run or report.get("phase") != phase or report.get("origin") != origin:
        raise ValueError("report identity mismatch")
    rows = report.get("results")
    if not isinstance(rows, list) or [row.get("name") for row in rows if isinstance(row, dict)] != list(CASES[phase]):
        raise ValueError("missing, extra or reordered cases")
    if any(type(row.get("pass")) is not bool for row in rows):
        raise ValueError("non-boolean pass state")
    passed = sum(row["pass"] for row in rows)
    if report.get("total") != len(rows) or report.get("passed") != passed:
        raise ValueError("incorrect totals")
    if require_success and passed != len(rows):
        raise ValueError("page reported failure")
    elapsed = report.get("elapsed_ms")
    if type(elapsed) not in (int, float) or not 0 <= elapsed <= (240000 if phase == "write" else 60000):
        raise ValueError("page exceeded bounded deadline")
