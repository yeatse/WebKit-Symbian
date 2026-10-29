// Copyright (c) 2026 The symbian-webkit authors.
// SPDX-License-Identifier: BSD-2-Clause
// Compile against the patched EKA2L1 services/fs/host_volume.h; no emulator.
#include <services/fs/host_volume.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using namespace eka2l1;

static unsigned passed;
static void check(bool condition, const char* name)
{
    if (!condition)
        throw std::runtime_error(name);
    ++passed;
    std::cout << "PASS: " << name << '\n';
}

struct TemporaryDirectory {
    fs::path path;
    explicit TemporaryDirectory(const fs::path& parent)
    {
        path = parent / ("eka-volume-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!fs::create_directory(path))
            throw std::runtime_error("create private host fixture");
    }
    ~TemporaryDirectory()
    {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

int main(int argc, char** argv)
{
    try {
        if (argc == 3 && std::string(argv[1]) == "--permission") {
            check(epoc::fs::query_host_volume(argv[2], false).error == epoc::error_access_denied,
                "actual inaccessible mapped root returns KErrAccessDenied");
            return 0;
        }
        TemporaryDirectory directory(fs::temp_directory_path());
        check(epoc::fs::volume_session_drive(u"E:\\Data\\") == 4,
            "default drive follows the RFs session path rather than system C");
        check(epoc::fs::volume_session_drive(u"z:/resource/") == 25,
            "lowercase session drive parses without truncation");
        check(!epoc::fs::volume_session_drive(u"") && !epoc::fs::volume_session_drive(u"3:\\")
                && !epoc::fs::volume_session_drive(u"C:relative") && !epoc::fs::volume_session_drive(u"\\C:\\"),
            "malformed default drive paths fail before drive-array lookup");
        const auto unicodePath = directory.path / fs::path(u"mapped-\u6d4b\u8bd5");
        fs::create_directory(unicodePath);
        std::error_code error;
        const auto actual = fs::space(unicodePath, error);
        check(!error, "independent filesystem space query succeeds");
        const auto queried = epoc::fs::query_host_volume(unicodePath, false);
        check(queried.error == epoc::error_none && static_cast<std::uintmax_t>(queried.capacity) == actual.capacity,
            "Unicode mapped root reports its actual filesystem capacity");
        check(queried.available >= 0 && queried.available <= queried.capacity,
            "writable available space is bounded by capacity");
        const auto readOnly = epoc::fs::query_host_volume(unicodePath, true);
        check(readOnly.error == epoc::error_none && readOnly.capacity == queried.capacity && !readOnly.available,
            "read-only or ROM mapping has no guest-writable space");
        check(epoc::fs::query_host_volume({}, false).error == epoc::error_not_ready,
            "empty mapping cannot fall back to cwd");
        check(epoc::fs::query_host_volume(directory.path / "missing", false).error == epoc::error_not_ready,
            "missing mapped root is not ready");
        const auto file = directory.path / "regular-file";
        std::ofstream(file) << "not a mounted directory";
        check(epoc::fs::query_host_volume(file, false).error == epoc::error_not_ready,
            "regular file cannot masquerade as a drive root");

        const std::uintmax_t capacity = (std::uintmax_t(1) << 44) + 12345;
        const std::uintmax_t available = (std::uintmax_t(1) << 42) + 6789;
        const auto wide = epoc::fs::checked_host_volume_space({ capacity, available + 4096, available }, false);
        check(wide.error == epoc::error_none && wide.capacity == capacity && wide.available == available,
            "capacity and user-available values above 4 GiB retain all bits");
        const auto reserved = epoc::fs::checked_host_volume_space({ capacity, available + 4096, available }, false);
        check(reserved.available == available, "reserved host blocks do not count as user-available");
        const std::uintmax_t limit = std::numeric_limits<std::int64_t>::max();
        check(epoc::fs::checked_host_volume_space({ limit, limit, limit }, false).capacity == std::numeric_limits<std::int64_t>::max(),
            "TInt64 maximum remains representable");
        check(epoc::fs::checked_host_volume_space({ limit + 1, 0, 0 }, false).error == epoc::error_overflow,
            "capacity larger than TInt64 fails without wrapping");
        check(epoc::fs::checked_host_volume_space({ std::numeric_limits<std::uintmax_t>::max(), 0, 0 }, false).error == epoc::error_overflow,
            "unknown all-ones capacity never becomes a successful volume");
        check(epoc::fs::checked_host_volume_space({ limit, limit + 1, limit + 1 }, false).error == epoc::error_overflow,
            "available larger than TInt64 fails without wrapping");
        check(epoc::fs::checked_host_volume_space({ 10, 11, 11 }, false).error == epoc::error_general,
            "inconsistent available greater than capacity fails");
        const auto readOnlyUnknown = epoc::fs::checked_host_volume_space({ capacity, 0, std::numeric_limits<std::uintmax_t>::max() }, true);
        check(readOnlyUnknown.error == epoc::error_none && !readOnlyUnknown.available,
            "read-only volume has known zero writable bytes even when host availability is unknown");
        check(epoc::fs::host_volume_error(std::make_error_code(std::errc::permission_denied)) == epoc::error_access_denied,
            "permission errors retain native access-denied semantics");
        check(epoc::fs::host_volume_error(std::make_error_code(std::errc::io_error)) == epoc::error_general,
            "unexpected host I/O failure cannot become success");

        // Linux CI has a small tmpfs distinct from the workspace filesystem.
        // Querying that mapped root must not report this process's cwd volume.
        if (fs::is_directory("/dev/shm")) {
            TemporaryDirectory other("/dev/shm");
            const auto expected = fs::space(other.path);
            const auto fromOther = epoc::fs::query_host_volume(other.path, false);
            check(fromOther.error == epoc::error_none && static_cast<std::uintmax_t>(fromOther.capacity) == expected.capacity,
                "a second mounted filesystem uses its own capacity");
            std::cout << "OBSERVATION: mapped_capacity=" << fromOther.capacity
                      << " cwd_capacity=" << fs::space(fs::current_path()).capacity << '\n';
        }
        std::cout << "RESULT: host-volume " << passed << '/' << passed << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
