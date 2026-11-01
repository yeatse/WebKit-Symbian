// Normal public-file map_file API check. Never accepts ROM/profile/DB paths.
#include <common/virtualmem.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef CANARY_TASK_ROOT
#error CANARY_TASK_ROOT must identify the external task directory
#endif

namespace {
constexpr std::size_t fixture_size = 4096;
bool capture_open = false;
bool synthetic_open_failure = false;
const char* captured_path = nullptr;
unsigned open_calls = 0;
int observed_flags = -1;
int owned_fd = -1;
bool unexpected_open = false;

struct Result {
    bool fixture_valid = false;
    bool private_open_readonly = false;
    bool map_succeeded = false;
    bool cow_byte_changed = false;
    bool mapping_released = false;
    bool original_bytes_unchanged = false;
    bool original_mode_unchanged = false;
    bool read_flags_unchanged = true;
    bool write_flags_unchanged = true;
    bool other_rw_flags_unchanged = true;
    bool unsupported_flags_unchanged = true;
};

const char* boolean(bool value) { return value ? "true" : "false"; }

void begin_capture(const std::string& path, bool synthetic) {
    captured_path = path.c_str();
    synthetic_open_failure = synthetic;
    open_calls = 0;
    observed_flags = -1;
    owned_fd = -1;
    unexpected_open = false;
    capture_open = true;
}

void end_capture() {
    capture_open = false;
    if (owned_fd >= 0) {
        close(owned_fd); // map_file currently retains its opened descriptor.
        owned_fd = -1;
    }
}

bool read_expected_fixture(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    std::array<unsigned char, fixture_size> bytes{};
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = read(fd, bytes.data() + offset, bytes.size() - offset);
        if (count <= 0) { close(fd); return false; }
        offset += static_cast<std::size_t>(count);
    }
    unsigned char extra = 0;
    const bool exact_size = read(fd, &extra, 1) == 0;
    close(fd);
    if (!exact_size) return false;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        if (bytes[i] != static_cast<unsigned char>('A' + (i % 23))) return false;
    return true;
}

bool flag_case(const std::string& path, prot permission, std::size_t size,
    bool is_private, int expected, bool should_open = true) {
    begin_capture(path, true);
    void* result = eka2l1::common::map_file(path, permission, size, is_private);
    end_capture();
    return result == nullptr && !unexpected_open
        && open_calls == (should_open ? 1u : 0u)
        && (!should_open || observed_flags == expected);
}

bool run(const std::filesystem::path& supplied, Result& out) {
    const auto root = std::filesystem::path(CANARY_TASK_ROOT) / "canary-runs";
    if (!supplied.is_absolute() || supplied != std::filesystem::canonical(supplied)
        || supplied.filename() != "fixture.bin"
        || supplied.parent_path().parent_path() != root) return false;
    const std::string path = supplied.string();
    struct stat before{};
    if (lstat(path.c_str(), &before) != 0 || !S_ISREG(before.st_mode)
        || before.st_nlink != 1 || before.st_uid != geteuid()
        || before.st_size != static_cast<off_t>(fixture_size)
        || (before.st_mode & 07777) != 0444 || !read_expected_fixture(path)) return false;
    out.fixture_valid = true;

    begin_capture(path, false);
    void* mapping = eka2l1::common::map_file(path, prot_read_write, 0, true);
    end_capture();
    out.private_open_readonly = !unexpected_open && open_calls == 1 && observed_flags == O_RDONLY;
    out.map_succeeded = mapping != nullptr && mapping != MAP_FAILED;
    if (out.map_succeeded) {
        volatile auto* bytes = static_cast<unsigned char*>(mapping);
        const unsigned char changed = static_cast<unsigned char>(bytes[7] ^ 0x20);
        bytes[7] = changed;
        out.cow_byte_changed = bytes[7] == changed;
        // Normal public API cleanup using the known task-file length.
        out.mapping_released = eka2l1::common::unmap_memory(mapping, fixture_size);
    }

    for (bool is_private : {false, true}) {
        for (std::size_t size : {std::size_t{0}, fixture_size}) {
            out.read_flags_unchanged &= flag_case(path, prot_read, size, is_private, O_RDONLY);
            out.write_flags_unchanged &= flag_case(path, prot_write, size, is_private, O_WRONLY | O_CREAT);
            if (!(is_private && size == 0))
                out.other_rw_flags_unchanged &= flag_case(path, prot_read_write, size, is_private, O_RDWR | O_CREAT);
        }
    }
    for (prot permission : {prot_none, prot_exec, prot_read_exec, prot_read_write_exec})
        out.unsupported_flags_unchanged &= flag_case(path, permission, 0, true, 0, false);

    struct stat after{};
    out.original_mode_unchanged = lstat(path.c_str(), &after) == 0
        && S_ISREG(after.st_mode) && after.st_dev == before.st_dev && after.st_ino == before.st_ino
        && after.st_size == before.st_size && (after.st_mode & 07777) == (before.st_mode & 07777);
    out.original_bytes_unchanged = read_expected_fixture(path);
    return out.fixture_valid && out.private_open_readonly && out.map_succeeded
        && out.cow_byte_changed && out.mapping_released && out.original_bytes_unchanged
        && out.original_mode_unchanged && out.read_flags_unchanged && out.write_flags_unchanged
        && out.other_rw_flags_unchanged && out.unsupported_flags_unchanged;
}
}

extern "C" int __real_open(const char*, int, ...);
extern "C" int __wrap_open(const char* path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = static_cast<mode_t>(va_arg(args, int));
        va_end(args);
    }
    if (capture_open) {
        ++open_calls;
        observed_flags = flags;
        if (!captured_path || std::strcmp(path, captured_path)) {
            unexpected_open = true;
            return -1;
        }
        if (synthetic_open_failure) return -1;
        owned_fd = __real_open(path, flags, mode);
        return owned_fd;
    }
    return __real_open(path, flags, mode);
}

int main(int argc, char** argv) {
    Result out;
    bool passed = false;
    try { if (argc == 2) passed = run(argv[1], out); }
    catch (...) { passed = false; }
    std::printf("{\"status\":\"%s\",\"fixture_valid\":%s,\"private_open_readonly\":%s,"
        "\"map_succeeded\":%s,\"cow_byte_changed\":%s,\"mapping_released\":%s,"
        "\"original_bytes_unchanged\":%s,\"original_mode_unchanged\":%s,"
        "\"read_flags_unchanged\":%s,\"write_flags_unchanged\":%s,"
        "\"other_rw_flags_unchanged\":%s,\"unsupported_flags_unchanged\":%s}\n",
        passed ? "PASS" : "FAIL", boolean(out.fixture_valid), boolean(out.private_open_readonly),
        boolean(out.map_succeeded), boolean(out.cow_byte_changed), boolean(out.mapping_released),
        boolean(out.original_bytes_unchanged), boolean(out.original_mode_unchanged),
        boolean(out.read_flags_unchanged), boolean(out.write_flags_unchanged),
        boolean(out.other_rw_flags_unchanged), boolean(out.unsupported_flags_unchanged));
    return passed ? 0 : 1;
}
