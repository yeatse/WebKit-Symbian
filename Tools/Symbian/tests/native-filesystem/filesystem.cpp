#include <TestWTF.h>
#include <dirent.h>
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
static const char* nativeWorkingDirectory;
static bool failWorkingDirectory;
static bool failDirectoryOpen;
static bool failDirectoryRead;
static bool readDirectoryEntry;

extern "C" DIR* __real_opendir(const char*);
extern "C" struct dirent* __real_readdir(DIR*);
extern "C" DIR* __wrap_opendir(const char* path)
{
    if (failDirectoryOpen) {
        errno = EACCES;
        return nullptr;
    }
    return __real_opendir(path);
}
extern "C" struct dirent* __wrap_readdir(DIR* directory)
{
    if (failDirectoryRead && readDirectoryEntry) {
        errno = EIO;
        return nullptr;
    }
    auto* result = __real_readdir(directory);
    if (result && strcmp(result->d_name, ".") && strcmp(result->d_name, ".."))
        readDirectoryEntry = true;
    return result;
}

// Only native volume queries are injected here; the path/64-bit result logic
// below is the real FileSystemSymbian.cpp. RFs is checked by native-volume.
namespace WTF::SymbianSystem {
bool volumeInformation(unsigned drive, uint64_t& capacity, uint64_t& freeSpace)
{
    if (drive == 2) {
        capacity = 8ULL << 30;
        freeSpace = 6ULL << 30;
    } else if (drive == 4) {
        capacity = 16ULL << 30;
        freeSpace = 9ULL << 30;
    } else if (drive == 25) {
        capacity = 4ULL << 20;
        freeSpace = 0;
    } else
        return false;
    return true;
}
}

extern "C" char* __real_getcwd(char*, size_t);
extern "C" char* __wrap_getcwd(char* buffer, size_t capacity)
{
    if (failWorkingDirectory) {
        errno = ENOENT;
        return nullptr;
    }
    if (!nativeWorkingDirectory)
        return __real_getcwd(buffer, capacity);
    assert(strlen(nativeWorkingDirectory) < capacity);
    strcpy(buffer, nativeWorkingDirectory);
    return buffer;
}
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
    assert(!hardLinkCount("C:/Data/source"));
    std::puts("PASS cross-device ordinary-file fallback and unsupported hardlink");

    assert(volumeFreeSpace("C:\\Data\\file") == (6ULL << 30));
    assert(volumeCapacity("e:/Data") == (16ULL << 30));
    assert(volumeFreeSpace("Z:/resource") == 0);
    assert(!volumeFreeSpace("Q:/") && !volumeCapacity("Q:/"));
    assert(!volumeFreeSpace("") && !volumeCapacity("http://example.test"));
    assert(!volumeFreeSpace("C:/bad:name"));
    assert(!volumeFreeSpace(String(std::string("C:/a\0b", 6))));
    nativeWorkingDirectory = "E:/Data";
    assert(volumeFreeSpace("relative/file") == (9ULL << 30));
    assert(volumeCapacity("/rooted/file") == (16ULL << 30));
    failWorkingDirectory = true;
    assert(!volumeFreeSpace("relative/file"));
    failWorkingDirectory = false;
    nativeWorkingDirectory = nullptr;
    std::puts("PASS native volume drive selection, zero space, 64-bit values and errors");

    assert(makeAllDirectories("C:/empty-directory"));
    auto empty = tryListDirectory("C:/empty-directory");
    assert(empty && empty->isEmpty());
    assert(directorySizeIfExists("C:/empty-directory") == 0);
    assert(!tryListDirectory("C:/missing-directory"));
    assert(directorySizeIfExists("C:/missing-directory") == 0);
    assert(!directorySizeIfExists("C:/Data/source"));
    assert(!directorySizeIfExists("C:/Data/source/child"));
    failDirectoryOpen = true;
    assert(!tryListDirectory("C:/empty-directory"));
    assert(!directorySizeIfExists("C:/empty-directory"));
    failDirectoryOpen = false;
    readDirectoryEntry = false;
    failDirectoryRead = true;
    assert(!tryListDirectory("C:/Data") && readDirectoryEntry);
    failDirectoryRead = false;
    denyStat = true;
    assert(!directorySizeIfExists("C:/empty-directory"));
    assert(!directorySizeIfExists("C:/missing-directory"));
    denyStat = false;
    std::puts("PASS quota enumeration distinguishes empty, missing, partial and failed I/O");

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
