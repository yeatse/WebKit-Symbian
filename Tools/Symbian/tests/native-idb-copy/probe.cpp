#include <e32base.h>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using String = std::string;
static String nativePath(const String& value) { return value; }
static int writesBeforeFailure = -1;
static ssize_t checkedWrite(int descriptor, const void* bytes, size_t length)
{
    if (!writesBeforeFailure) {
        errno = ENOSPC;
        return -1;
    }
    if (writesBeforeFailure > 0)
        --writesBeforeFailure;
    return write(descriptor, bytes, length);
}
#define write checkedWrite
#include "production-copy.inc"
#undef write

static FILE* output;
static unsigned passed, failed;
static std::string directory, sourcePath, targetPath, otherPath, partialPath;
static const char* source;
static const char* target;
static const char* other;
static const char* partial;
static unsigned char payload[40007];
static void check(bool value, const char* name)
{
    std::fprintf(output, "%s: %s\n", value ? "PASS" : "FAIL", name);
    std::fflush(output);
    value ? ++passed : ++failed;
}
static bool writeFile(const char* path, const void* data, size_t length)
{
    int descriptor = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (descriptor < 0)
        return false;
    bool result = write(descriptor, data, length) == static_cast<ssize_t>(length);
    return !close(descriptor) && result;
}
static bool equals(const char* path, const void* data, size_t length)
{
    int descriptor = open(path, O_RDONLY);
    if (descriptor < 0)
        return false;
    unsigned char buffer[1024];
    bool result = true;
    size_t offset = 0;
    while (offset < length) {
        size_t expected = length - offset;
        if (expected > sizeof(buffer))
            expected = sizeof(buffer);
        ssize_t got = read(descriptor, buffer, expected);
        if (got != static_cast<ssize_t>(expected) || memcmp(buffer, static_cast<const unsigned char*>(data) + offset, expected)) {
            result = false;
            break;
        }
        offset += expected;
    }
    if (result)
        result = !read(descriptor, buffer, 1);
    close(descriptor);
    return result;
}
int main()
{
    output = std::fopen("C:/Data/WebKit/idb-copy-probe.log", "wb");
    if (!output)
        return 2;
    std::fprintf(output, "BEGIN idb-copy-probe version=2\n");
    // A new root-level directory avoids pre-existing host-only case collisions.
    // The alias check still changes the case of the entire guest path.
    char unique[96];
    std::snprintf(unique, sizeof(unique), "C:/IDBCopy-%08lx-%08lx", static_cast<unsigned long>(getpid()), static_cast<unsigned long>(User::FastCounter()));
    directory = unique;
    if (mkdir(directory.c_str(), 0700)) {
        check(false, "create a new owned probe directory");
        std::fprintf(output, "RESULT {\"probe\":\"idb-copy-probe\",\"passed\":0,\"failed\":1}\nidb-copy-probe exiting 1\n");
        std::fclose(output);
        return 1;
    }
    sourcePath = directory + "/idb-copy-source.bin"; source = sourcePath.c_str();
    targetPath = directory + "/idb-copy-target.bin"; target = targetPath.c_str();
    otherPath = directory + "/idb-copy-existing.bin"; other = otherPath.c_str();
    partialPath = directory + "/idb-copy-partial.bin"; partial = partialPath.c_str();
    std::string alias = sourcePath;
    for (char& value : alias)
        value = static_cast<char>(std::islower(static_cast<unsigned char>(value)) ? std::toupper(static_cast<unsigned char>(value)) : std::tolower(static_cast<unsigned char>(value)));
    for (size_t i = 0; i < sizeof(payload); ++i)
        payload[i] = static_cast<unsigned char>(i * 37);
    const char sentinel[] = "unchanged";
    check(writeFile(source, payload, sizeof(payload)) && writeFile(other, sentinel, sizeof(sentinel)), "create two distinct same-drive source and sentinel files");
    struct stat sourceInfo { }, otherInfo { };
    bool stats = !stat(source, &sourceInfo) && !stat(other, &otherInfo);
    std::fprintf(output, "INFO sourceDev=%lu sourceIno=%lu otherDev=%lu otherIno=%lu\n", static_cast<unsigned long>(sourceInfo.st_dev), static_cast<unsigned long>(sourceInfo.st_ino), static_cast<unsigned long>(otherInfo.st_dev), static_cast<unsigned long>(otherInfo.st_ino));
    check(stats && sourceInfo.st_dev == otherInfo.st_dev && !sourceInfo.st_ino && !otherInfo.st_ino, "real OpenC reports zero inode for distinct same-drive files");
    check(copy(target, source, false), "production O_EXCL copy accepts a new target with zero inode");
    check(equals(target, payload, sizeof(payload)) && equals(source, payload, sizeof(payload)), "copied bytes and original binary payload match exactly");
    check(!copy(other, source, false) && equals(other, sentinel, sizeof(sentinel)), "existing target refuses overwrite and retains its contents");
    check(!copy(source, source, false) && equals(source, payload, sizeof(payload)), "same-path exclusive copy preserves source");
    check(!copy(alias, source, false) && equals(source, payload, sizeof(payload)), "case-insensitive alias refuses copy and preserves source");
    check(!copy(source, source, true) && equals(source, payload, sizeof(payload)), "overwrite same-source guard remains fail-closed");
    writesBeforeFailure = 1;
    bool rejected = !copy(partial, source, false);
    writesBeforeFailure = -1;
    check(rejected && access(partial, F_OK) && equals(source, payload, sizeof(payload)), "partial write failure removes owned output and preserves source");
    check(!unlink(target) && copy(target, source, false) && equals(target, payload, sizeof(payload)), "rollback cleanup permits a fresh successful exclusive copy");
    bool cleaned = true;
    for (auto path : { source, target, other })
        cleaned = !unlink(path) && cleaned;
    // An old case-cache failure may have created a second spelling. Both are
    // inside this invocation's new directory and belong to this probe.
    cleaned = (!unlink(alias.c_str()) || errno == ENOENT) && cleaned;
    cleaned = !rmdir(directory.c_str()) && cleaned;
    check(cleaned, "cleanup removes only owned test files");
    std::fprintf(output, "RESULT {\"probe\":\"idb-copy-probe\",\"passed\":%u,\"failed\":%u}\n", passed, failed);
    std::fprintf(output, "%s: idb-copy-probe %u/%u\nidb-copy-probe exiting %d\n", failed ? "FAIL" : "PASS", passed, passed + failed, failed ? 1 : 0);
    std::fclose(output);
    return failed ? 1 : 0;
}
