/*
 * Copyright (C) 2026 The symbian-webkit authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/FileSystem.h>
#include "FileSystemSymbianNative.h"

#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/HexNumber.h>
#include <wtf/FileHandle.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WTF::FileSystemImpl {

static std::string nativePath(StringView path)
{
    auto utf8 = path.utf8();
    if (!utf8.length())
        return { };
    std::string result(utf8.data(), utf8.length());
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

static size_t rootLength(const std::string& path)
{
    if (path.size() >= 2 && isASCIIAlpha(path[0]) && path[1] == ':')
        return path.size() >= 3 && path[2] == '/' ? 3 : 2;
    return !path.empty() && path[0] == '/' ? 1 : 0;
}

String lexicallyNormal(const String& input)
{
    auto path = nativePath(input);
    if (path.empty())
        return input;
    size_t root = rootLength(path);
    bool absolute = root && path[root - 1] == '/';
    Vector<std::string> components;
    size_t position = root;
    while (position < path.size()) {
        size_t end = path.find('/', position);
        if (end == std::string::npos)
            end = path.size();
        auto component = path.substr(position, end - position);
        if (component == "..") {
            if (!components.isEmpty() && components.last() != "..")
                components.removeLast();
            else if (!absolute)
                components.append(component);
        } else if (!component.empty() && component != ".")
            components.append(component);
        position = end + 1;
    }
    std::string result = path.substr(0, root);
    for (auto& component : components) {
        if (!result.empty() && result.back() != '/' && result.back() != ':')
            result += '/';
        result += component;
    }
    auto lastSlash = path.find_last_of('/');
    auto finalComponent = path.substr(lastSlash == std::string::npos ? root : lastSlash + 1);
    bool endsInDirectory = path.back() == '/' || finalComponent == "." || finalComponent == "..";
    if (endsInDirectory && !components.isEmpty() && components.last() != ".." && result.back() != '/')
        result += '/';
    return String::fromUTF8(result.empty() ? "." : result.c_str());
}

String pathFileName(const String& input)
{
    auto path = nativePath(input);
    auto slash = path.find_last_of('/');
    size_t start = slash == std::string::npos ? rootLength(path) : slash + 1;
    return String::fromUTF8(path.substr(start).c_str());
}

String parentPath(const String& input)
{
    auto path = nativePath(input);
    size_t root = rootLength(path);
    bool hadTrailingSeparator = path.size() > root && path.back() == '/';
    while (path.size() > root && path.back() == '/')
        path.pop_back();
    if (hadTrailingSeparator || path.size() == root)
        return String::fromUTF8(path.c_str());
    auto slash = path.find_last_of('/');
    size_t length = slash == std::string::npos ? root : std::max(root, slash);
    while (length > root && path[length - 1] == '/')
        --length;
    return String::fromUTF8(path.substr(0, length).c_str());
}

String pathByAppendingComponent(StringView base, StringView component)
{
    auto path = nativePath(base);
    auto next = nativePath(component);
    if (rootLength(next)) {
        if (next[0] == '/' && path.size() >= 2 && path[1] == ':')
            next = path.substr(0, 2) + next;
        return String::fromUTF8(next.c_str());
    }
    if (!path.empty() && path.back() != '/' && path.back() != ':')
        path += '/';
    path += next;
    return String::fromUTF8(path.c_str());
}

String pathByAppendingComponents(StringView base, std::span<const StringView> components)
{
    String result = base.toString();
    for (auto component : components)
        result = pathByAppendingComponent(result, component);
    return result;
}

bool isAncestor(const String& ancestor, const String& child)
{
    auto a = nativePath(lexicallyNormal(ancestor));
    auto c = nativePath(lexicallyNormal(child));
    while (a.size() > rootLength(a) && a.back() == '/')
        a.pop_back();
    while (c.size() > rootLength(c) && c.back() == '/')
        c.pop_back();
    if (a.empty() || a == "." || c.size() <= a.size())
        return false;
    if (a.back() != '/')
        a += '/';
    return c.size() >= a.size() && equalIgnoringASCIICase(String::fromUTF8(c.substr(0, a.size()).c_str()), String::fromUTF8(a.c_str()));
}

bool fileExists(const String& path) { return !access(nativePath(path).c_str(), F_OK); }
bool deleteFile(const String& path) { return !unlink(nativePath(path).c_str()); }
bool deleteEmptyDirectory(const String& path) { return !rmdir(nativePath(path).c_str()); }

static bool ensureDirectory(const std::string& path)
{
    if (!mkdir(path.c_str(), 0700))
        return true;
    struct stat info;
    return errno == EEXIST && !stat(path.c_str(), &info) && S_ISDIR(info.st_mode);
}

bool makeAllDirectories(const String& input)
{
    auto path = nativePath(input);
    if (path.empty())
        return false;
    for (size_t i = rootLength(path); i < path.size(); ++i) {
        if (path[i] == '/' && !ensureDirectory(path.substr(0, i)))
            return false;
    }
    return ensureDirectory(path);
}

static std::optional<FileType> getType(const String& path, bool follow)
{
    struct stat info;
    auto name = nativePath(path);
    if ((follow ? stat(name.c_str(), &info) : lstat(name.c_str(), &info)))
        return std::nullopt;
    if (S_ISDIR(info.st_mode))
        return FileType::Directory;
    if (S_ISLNK(info.st_mode))
        return FileType::SymbolicLink;
    if (S_ISREG(info.st_mode))
        return FileType::Regular;
    return std::nullopt;
}
std::optional<FileType> fileType(const String& path) { return getType(path, false); }
std::optional<FileType> fileTypeFollowingSymlinks(const String& path) { return getType(path, true); }

std::optional<uint64_t> fileSize(const String& path)
{
    struct stat info;
    if (stat(nativePath(path).c_str(), &info) || !S_ISREG(info.st_mode) || info.st_size < 0)
        return std::nullopt;
    return info.st_size;
}

static std::optional<Vector<String>> entries(const String& path)
{
    DIR* directory = opendir(nativePath(path).c_str());
    if (!directory)
        return std::nullopt;
    Vector<String> result;
    errno = 0;
    while (auto* entry = readdir(directory)) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
            result.append(String::fromUTF8(entry->d_name));
        errno = 0;
    }
    int error = errno;
    closedir(directory);
    if (error)
        return std::nullopt;
    return result;
}
Vector<String> listDirectory(const String& path) { return entries(path).value_or(Vector<String> { }); }
std::optional<Vector<String>> tryListDirectory(const String& path) { return entries(path); }

std::optional<uint64_t> directorySize(const String& path)
{
    auto children = entries(path);
    if (!children)
        return std::nullopt;
    CheckedUint64 size = 0;
    for (auto& child : *children) {
        auto fullPath = pathByAppendingComponent(path, child);
        auto type = fileType(fullPath);
        if (!type)
            return std::nullopt;
        if (*type == FileType::SymbolicLink)
            continue;
        auto childSize = *type == FileType::Directory ? directorySize(fullPath) : fileSize(fullPath);
        if (!childSize)
            return std::nullopt;
        size += *childSize;
        if (size.hasOverflowed())
            return std::nullopt;
    }
    return size.value();
}

std::optional<uint64_t> directorySizeIfExists(const String& path)
{
    struct stat info;
    if (lstat(nativePath(path).c_str(), &info)) {
        if (errno == ENOENT)
            return 0;
        return std::nullopt;
    }
    if (!S_ISDIR(info.st_mode))
        return std::nullopt;
    return directorySize(path);
}

bool deleteNonEmptyDirectory(const String& path)
{
    struct stat info;
    if (lstat(nativePath(path).c_str(), &info))
        return errno == ENOENT;
    if (!S_ISDIR(info.st_mode))
        return deleteFile(path);
    auto children = entries(path);
    if (!children)
        return false;
    for (auto& child : *children) {
        if (!deleteNonEmptyDirectory(pathByAppendingComponent(path, child)))
            return false;
    }
    return deleteEmptyDirectory(path);
}

static bool volumeInformation(const String& input, uint64_t& capacity, uint64_t& freeSpace)
{
    auto path = nativePath(input);
    if (path.empty() || path.find('\0') != std::string::npos)
        return false;
    if (path.find(':') == std::string::npos) {
        // Open C maintains its own current directory. An RFs session's default
        // drive is not necessarily the drive of a relative Open C path.
        // A native path has at most 256 UTF-16 units (at most 768 UTF-8 bytes).
        char currentDirectory[1024];
        if (!getcwd(currentDirectory, sizeof(currentDirectory)))
            return false;
        path = currentDirectory;
    }
    if (path.size() < 2 || !isASCIIAlpha(path[0]) || path[1] != ':' || path.find(':', 2) != std::string::npos)
        return false;
    unsigned drive = (path[0] >= 'a' && path[0] <= 'z') ? path[0] - 'a' : path[0] - 'A';
    return SymbianSystem::volumeInformation(drive, capacity, freeSpace);
}

std::optional<uint64_t> volumeFreeSpace(const String& path)
{
    uint64_t capacity = 0;
    uint64_t freeSpace = 0;
    if (!volumeInformation(path, capacity, freeSpace))
        return std::nullopt;
    return freeSpace;
}

std::optional<uint64_t> volumeCapacity(const String& path)
{
    uint64_t capacity = 0;
    uint64_t freeSpace = 0;
    if (!volumeInformation(path, capacity, freeSpace))
        return std::nullopt;
    return capacity;
}
bool hardLink(const String&, const String&) { return false; }
std::optional<uint64_t> hardLinkCount(const String&) { return std::nullopt; }
bool createSymbolicLink(const String& target, const String& link) { return !symlink(nativePath(target).c_str(), nativePath(link).c_str()); }

static bool copy(const String& target, const String& source, bool overwrite)
{
    int input = open(nativePath(source).c_str(), O_RDONLY | O_NONBLOCK);
    if (input < 0)
        return false;
    struct stat sourceInfo;
    if (fstat(input, &sourceInfo) || !S_ISREG(sourceInfo.st_mode)) {
        close(input);
        return false;
    }
    int output = open(nativePath(target).c_str(), O_WRONLY | O_CREAT | O_NONBLOCK | (overwrite ? 0 : O_EXCL), 0600);
    if (output < 0) {
        close(input);
        return false;
    }
    struct stat targetInfo;
    bool success = !fstat(output, &targetInfo) && S_ISREG(targetInfo.st_mode)
        && !(sourceInfo.st_dev == targetInfo.st_dev && sourceInfo.st_ino == targetInfo.st_ino);
    // Compare descriptors before truncation: symlinks and alternate names may alias.
    success = success && !ftruncate(output, 0);
    uint8_t buffer[16384];
    while (success) {
        ssize_t count = read(input, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            success = !count;
            break;
        }
        ssize_t offset = 0;
        while (offset < count) {
            ssize_t written = write(output, buffer + offset, count - offset);
            if (written < 0 && errno == EINTR)
                continue;
            if (written <= 0) {
                success = false;
                break;
            }
            offset += written;
        }
    }
    if (close(output))
        success = false;
    close(input);
    if (!success && !overwrite)
        unlink(nativePath(target).c_str());
    return success;
}

bool copyFile(const String& target, const String& source) { return copy(target, source, true); }
bool hardLinkOrCopyFile(const String& target, const String& link) { return copy(link, target, false); }

bool moveFile(const String& source, const String& target)
{
    if (!rename(nativePath(source).c_str(), nativePath(target).c_str()))
        return true;
    if (errno != EXDEV || fileType(source) != FileType::Regular)
        return false;
    return copyFile(target, source) && deleteFile(source);
}

std::optional<WallTime> fileModificationTime(const String& path)
{
    struct stat info;
    if (stat(nativePath(path).c_str(), &info))
        return std::nullopt;
    return WallTime::fromRawSeconds(info.st_mtime);
}
bool updateFileModificationTime(const String& path) { return !utime(nativePath(path).c_str(), nullptr); }
bool isHiddenFile(const String& path) { return pathFileName(path).startsWith('.'); }

String realPath(const String& path)
{
    char buffer[4096];
    return realpath(nativePath(path).c_str(), buffer) ? String::fromUTF8(buffer) : path;
}

String createTemporaryFile(StringView prefix, StringView suffix)
{
    auto [path, handle] = openTemporaryFile(prefix, suffix);
    return path;
}

FileHandle createDumpFile(StringView filename, StringView extension, StringView path)
{
    if (path.isEmpty()) {
        auto [name, handle] = openTemporaryFile(filename, extension);
        return WTF::move(handle);
    }
    return openFile(pathByAppendingComponent(path, makeString(filename, extension)), FileOpenMode::ReadWrite);
}

String createTemporaryDirectory()
{
    const String root = "C:/Data/WebKit/tmp"_s;
    if (!makeAllDirectories(root))
        return { };
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        std::array<uint8_t, 16> random;
        cryptographicallyRandomValues(random);
        StringBuilder name;
        name.append(root, "/directory-"_s);
        for (auto byte : random)
            name.append(hex(byte, 2));
        auto path = name.toString();
        if (!mkdir(nativePath(path).c_str(), 0700))
            return path;
        if (errno != EEXIST)
            return { };
    }
    return { };
}

} // namespace WTF::FileSystemImpl
