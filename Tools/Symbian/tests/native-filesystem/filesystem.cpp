#include <TestWTF.h>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

using namespace WTF;
using namespace WTF::FileSystemImpl;

static bool injectIO;
static bool readInterrupted;
static bool writeInterrupted;
static bool failWrite;
static bool crossDevice;
static bool denyStat;
static unsigned shortWrites;

extern "C" ssize_t __real_read(int, void*, size_t);
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" int __real_rename(const char*, const char*);
extern "C" int __real_lstat(const char*, struct stat*);
extern "C" int __real_access(const char*, int);
extern "C" ssize_t __wrap_read(int fd, void* buffer, size_t size)
{
    if (injectIO && !readInterrupted) {
        readInterrupted = true;
        errno = EINTR;
        return -1;
    }
    return __real_read(fd, buffer, size);
}
extern "C" ssize_t __wrap_write(int fd, const void* buffer, size_t size)
{
    if (injectIO && !writeInterrupted) {
        writeInterrupted = true;
        errno = EINTR;
        return -1;
    }
    if (failWrite) {
        errno = ENOSPC;
        return -1;
    }
    if (injectIO && size > 3) {
        size = 3;
        ++shortWrites;
    }
    return __real_write(fd, buffer, size);
}
extern "C" int __wrap_rename(const char* from, const char* to)
{
    if (crossDevice) {
        errno = EXDEV;
        return -1;
    }
    return __real_rename(from, to);
}
extern "C" int __wrap_lstat(const char* path, struct stat* info)
{
    if (denyStat) {
        errno = EACCES;
        return -1;
    }
    return __real_lstat(path, info);
}
extern "C" int __wrap_access(const char* path, int mode)
{
    if (denyStat) {
        errno = EACCES;
        return -1;
    }
    return __real_access(path, mode);
}
static void expect(const String& value, const char* expected)
{
    if (value.bytes() != expected) {
        std::fprintf(stderr, "expected '%s', got '%s'\n", expected, value.bytes().c_str());
        std::abort();
    }
}
static std::string contents(const char* path)
{
    std::ifstream input(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
}
static void put(const char* path, const std::string& data) { std::ofstream(path, std::ios::binary) << data; }

int main()
{
    expect(lexicallyNormal("C:\\one\\.\\two\\..\\three"), "C:/one/three");
    expect(lexicallyNormal("C:/../../one"), "C:/one");
    expect(lexicallyNormal("../one/../../two"), "../../two");
    expect(lexicallyNormal("."), ".");
    expect(lexicallyNormal("one/two/.."), "one/");
    expect(lexicallyNormal("one/."), "one/");
    expect(lexicallyNormal("../"), "..");
    expect(lexicallyNormal(""), "");
    expect(parentPath("C:/"), "C:/");
    expect(parentPath("/"), "/");
    expect(parentPath("C:/one/two"), "C:/one");
    expect(parentPath("C:/one///"), "C:/one");
    expect(parentPath("one/"), "one");
    expect(parentPath("C:one"), "C:");
    expect(parentPath("one"), "");
    expect(parentPath("one//two"), "one");
    expect(pathFileName("C:/one/two"), "two");
    expect(pathFileName("C:/"), "");
    expect(pathByAppendingComponent("C:/one", "/two"), "C:/two");
    expect(pathByAppendingComponent("C:/one", "E:/two"), "E:/two");
    assert(isAncestor("C:/one", "c:/ONE/two"));
    assert(!isAncestor("C:/one", "C:/one-extra/two"));
    assert(!isAncestor("C:/one", "C:/one"));
    assert(!isAncestor("C:/one", "C:/one/"));
    assert(isAncestor("C:/one/", "C:/one/two"));
    std::puts("PASS drive, root, dot, parent, append and ancestor paths");

    assert(makeAllDirectories("C:/Data/WebKit/tmp"));
    assert(makeAllDirectories("E:/other"));
    assert(makeAllDirectories("C:/Target/nested"));
    assert(!symlink("../Target/nested", "C:/Data/directory-link"));
    assert(makeAllDirectories("C:/Data/directory-link/../new"));
    assert(fileType("C:/Target/new") == FileType::Directory);
    assert(!fileExists("C:/Data/new"));
    put("C:/Data/source", std::string(40000, 'x'));
    put("C:/Data/target", "unchanged");
    assert(!hardLinkOrCopyFile("C:/Data/source", "C:/Data/target"));
    assert(contents("C:/Data/target") == "unchanged");
    assert(!copyFile("C:/Data/source", "C:/Data/source"));
    assert(!link("C:/Data/source", "C:/Data/alias"));
    assert(!copyFile("C:/Data/alias", "C:/Data/source"));
    assert(!symlink("source", "C:/Data/symlink"));
    assert(!copyFile("C:/Data/symlink", "C:/Data/source"));
    assert(contents("C:/Data/source") == std::string(40000, 'x'));
    assert(!mkfifo("C:/Data/fifo", 0600));
    alarm(3);
    assert(!copyFile("C:/Data/fifo-output", "C:/Data/fifo"));
    assert(!copyFile("C:/Data/fifo", "C:/Data/source"));
    alarm(0);
    std::puts("PASS no-overwrite, aliases preserve data, non-regular files cannot block");

    injectIO = true;
    assert(copyFile("C:/Data/target", "C:/Data/source"));
    injectIO = false;
    assert(readInterrupted && writeInterrupted && shortWrites);
    assert(contents("C:/Data/target") == contents("C:/Data/source"));
    failWrite = true;
    assert(!hardLinkOrCopyFile("C:/Data/source", "C:/Data/failed-copy"));
    failWrite = false;
    assert(!fileExists("C:/Data/failed-copy"));
    std::puts("PASS short writes, read/write EINTR, failed exclusive-copy cleanup");

    crossDevice = true;
    assert(moveFile("C:/Data/target", "E:/other/moved"));
    assert(!fileExists("C:/Data/target"));
    assert(contents("E:/other/moved") == contents("C:/Data/source"));
    assert(!moveFile("C:/Data/WebKit", "E:/other/directory"));
    assert(!moveFile("C:/Data/symlink", "E:/other/symlink"));
    crossDevice = false;
    assert(!hardLink("C:/Data/source", "C:/Data/unsupported-link"));
    assert(!fileExists("C:/Data/unsupported-link"));
    assert(!volumeFreeSpace("C:/") && !volumeCapacity("C:/") && !hardLinkCount("C:/Data/source"));
    std::puts("PASS cross-device ordinary-file fallback and unsupported hardlink/capacity");

    auto first = createTemporaryDirectory();
    auto second = createTemporaryDirectory();
    assert(!first.isEmpty() && first != second && fileType(first) == FileType::Directory);
    assert(deleteNonEmptyDirectory(first) && deleteNonEmptyDirectory(second));
    denyStat = true;
    assert(!deleteNonEmptyDirectory("C:/Data"));
    denyStat = false;
    assert(deleteNonEmptyDirectory("missing"));
    assert(deleteNonEmptyDirectory("C:/Data/WebKit"));
    assert(fileExists("C:/Data/source"));
    std::puts("PASS unique temporary directories, recursive deletion and permission errors");
}
