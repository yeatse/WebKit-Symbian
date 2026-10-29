// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "common/system_utils.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <e32std.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <unistd.h>

namespace angle
{
namespace
{
std::string UTF8(const TDesC &value)
{
    std::string result;
    for (TInt i = 0; i < value.Length(); ++i)
    {
        uint32_t c = value[i];
        if (c >= 0xd800 && c <= 0xdbff && i + 1 < value.Length()
            && value[i + 1] >= 0xdc00 && value[i + 1] <= 0xdfff)
            c = 0x10000 + ((c - 0xd800) << 10) + (value[++i] - 0xdc00);
        else if (c >= 0xd800 && c <= 0xdfff)
            c = 0xfffd;
        if (c < 0x80)
            result += static_cast<char>(c);
        else
        {
            if (c < 0x800)
                result += static_cast<char>(0xc0 | (c >> 6));
            else
            {
                if (c < 0x10000)
                    result += static_cast<char>(0xe0 | (c >> 12));
                else
                {
                    result += static_cast<char>(0xf0 | (c >> 18));
                    result += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
                }
                result += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
            }
            result += static_cast<char>(0x80 | (c & 0x3f));
        }
    }
    return result;
}

struct NativeLibrary
{
    RLibrary handle;
    ~NativeLibrary() { handle.Close(); }
};

class UnsupportedPageFaultHandler final : public PageFaultHandler
{
  public:
    explicit UnsupportedPageFaultHandler(PageFaultCallback callback)
        : PageFaultHandler(std::move(callback)) {}
    bool enable() override { return false; }
    bool disable() override { return true; }
};
}  // namespace

std::string GetExecutablePath() { return UTF8(RProcess().FileName()); }
std::string GetExecutableDirectory() { return StripFilenameFromPath(GetExecutablePath()); }
std::string GetModuleDirectory() { return GetExecutableDirectory(); }
const char *GetSharedLibraryExtension() { return "dll"; }
const char *GetExecutableExtension() { return ".exe"; }
char GetPathSeparator() { return '\\'; }
const char *GetPathSeparatorForEnvironmentVar() { return ";"; }
std::string GetRootDirectory() { return "C:\\"; }

Optional<std::string> GetCWD()
{
    std::array<char, 1024> path;
    return getcwd(path.data(), path.size()) ? Optional<std::string>(path.data())
                                           : Optional<std::string>::Invalid();
}
bool SetCWD(const char *directory) { return !chdir(directory); }
bool SetEnvironmentVar(const char *name, const char *value) { return !setenv(name, value, 1); }
bool UnsetEnvironmentVar(const char *name)
{
    unsetenv(name);  // Open C's historical API returns void.
    return !getenv(name);
}
std::string GetEnvironmentVar(const char *name)
{
    const char *value = getenv(name);
    return value ? value : "";
}
bool IsDirectory(const char *path)
{
    struct stat status;
    return !stat(path, &status) && S_ISDIR(status.st_mode);
}
void MakeForwardSlashThePathSeparator(std::string &path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
}
bool CreateDirectories(const std::string &path)
{
    std::string normalized = path;
    MakeForwardSlashThePathSeparator(normalized);
    size_t start = normalized.size() > 2 && normalized[1] == ':' ? 3 : 0;
    do
    {
        size_t separator = normalized.find('/', start);
        std::string part = normalized.substr(0, separator);
        if (!part.empty() && !IsDirectory(part.c_str()) && mkdir(part.c_str(), 0700)
            && !(errno == EEXIST && IsDirectory(part.c_str())))
            return false;
        if (separator == std::string::npos)
            return true;
        start = separator + 1;
    } while (true);
}
bool IsSameFileDescriptor(int first, int second)
{
    if (first == second)
        return true;
    struct stat a, b;
    return !fstat(first, &a) && !fstat(second, &b) && a.st_ino && a.st_ino == b.st_ino
           && a.st_dev == b.st_dev;
}
Optional<std::string> GetTempDirectory()
{
    auto path = GetEnvironmentVar("TMPDIR");
    if (path.empty())
        path = "C:\\data\\temp";
    return CreateDirectories(path) ? Optional<std::string>(path) : Optional<std::string>::Invalid();
}
Optional<std::string> CreateTemporaryFileInDirectoryWithExtension(const std::string &directory,
                                                                  const std::string &extension)
{
    // mkstemp atomically creates the file. Renaming a live temporary file to a
    // requested suffix would lose that guarantee, so report that unsupported case.
    if (!extension.empty())
        return Optional<std::string>::Invalid();
    std::string path = directory + "\\angle-XXXXXX";
    int descriptor = mkstemp(path.data());
    if (descriptor < 0)
        return Optional<std::string>::Invalid();
    close(descriptor);
    return path;
}
Optional<std::string> CreateTemporaryFileInDirectory(const std::string &directory)
{
    return CreateTemporaryFileInDirectoryWithExtension(directory, "");
}
double GetCurrentSystemTime()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
double GetCurrentProcessCpuTime()
{
    struct tms usage;
    long frequency = sysconf(_SC_CLK_TCK);
    if (frequency <= 0 || times(&usage) == static_cast<clock_t>(-1))
        return 0;
    return (static_cast<double>(usage.tms_utime) + usage.tms_stime) / frequency;
}
void SetCurrentThreadName(const char *name)
{
    if (!name)
        return;
    TBuf<64> nativeName;
    for (size_t i = 0; name[i] && i < 64; ++i)
        nativeName.Append(static_cast<TUint8>(name[i]));
    User::RenameThread(nativeName);
}
bool IsDebuggerAttached() { return false; }
void BreakDebugger() { std::abort(); }

void *OpenSystemLibraryWithExtensionAndGetError(const char *name, SearchType search,
                                                std::string *errorOut)
{
    auto fail = [&](const char *message) -> void * {
        if (errorOut)
            *errorOut = message;
        return nullptr;
    };
    if (search == SearchType::AlreadyLoaded)
        return fail("Symbian does not expose a portable already-loaded DLL lookup");
    std::string path = search == SearchType::ModuleDir ? ConcatenatePath(GetModuleDirectory(), name)
                                                      : name;
    if (path.size() > KMaxFileName)
        return fail("Symbian DLL filename is too long");
    TFileName nativeName;
    for (unsigned char c : path)
    {
        if (c >= 0x80)
            return fail("Non-ASCII DLL names are not supported by ANGLE's optional library loader");
        nativeName.Append(c);
    }
    auto library = std::make_unique<NativeLibrary>();
    TInt error = library->handle.Load(nativeName);
    if (error != KErrNone)
    {
        if (errorOut)
            *errorOut = "RLibrary::Load failed: " + std::to_string(error);
        return nullptr;
    }
    return library.release();
}
void *GetLibrarySymbol(void *, const char *)
{
    // Symbian DLLs expose ordinals, not arbitrary exported names. EGL/GLES use
    // FunctionsEGLSymbian's verified firmware tables instead of this loader.
    return nullptr;
}
std::string GetLibraryPath(void *handle)
{
    return handle ? UTF8(static_cast<NativeLibrary *>(handle)->handle.FileName()) : "";
}
void CloseSystemLibrary(void *handle) { delete static_cast<NativeLibrary *>(handle); }

// Write-watch capture requires page-fault interception, which Symbian does not
// provide to applications. Do not claim to have installed memory protection.
bool ProtectMemory(uintptr_t, size_t) { return false; }
bool UnprotectMemory(uintptr_t, size_t) { return false; }
size_t GetPageSize() { return 4096; }
PageFaultHandler *CreatePageFaultHandler(PageFaultCallback callback)
{
    return new UnsupportedPageFaultHandler(std::move(callback));
}
uint64_t GetProcessMemoryUsageKB() { return 0; }  // No portable process-wide RSS query.
bool RunApp(const std::vector<const char *> &, std::string *, std::string *, int *)
{
    return false;  // Host tooling only; no subprocess-based shader compiler is used.
}
}  // namespace angle
