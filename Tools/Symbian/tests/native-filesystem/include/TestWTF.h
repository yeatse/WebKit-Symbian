#pragma once

#define NOESCAPE

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iconv.h>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <utility>
#include <vector>

// These test-only wide entry points consume 16-bit code units. They never call
// the host libc's 32-bit wchar_t interfaces; run.sh explicitly uses -fshort-wchar.
extern "C" int wstat(const wchar_t*, struct stat*);
extern "C" int waccess(const wchar_t*, int);

namespace WTF {
template<typename Signature> using Function = std::function<Signature>;
// Use the host's independent codec solely at the String/native test boundary.
inline std::string convertTestString(std::string_view input, const char* from, const char* to)
{
    static_assert(std::endian::native == std::endian::little);
    iconv_t codec = iconv_open(to, from);
    assert(codec != reinterpret_cast<iconv_t>(-1));
    std::string output(input.size() * 4 + 4, 0);
    char* cursor = const_cast<char*>(input.data());
    size_t remaining = input.size();
    char* target = output.data();
    size_t available = output.size();
    auto result = iconv(codec, &cursor, &remaining, &target, &available);
    assert(result != static_cast<size_t>(-1) && !remaining);
    assert(!iconv_close(codec));
    output.resize(output.size() - available);
    return output;
}

class CString {
public:
    explicit CString(std::string value) : m_value(std::move(value)) { }
    const char* data() const { return m_value.c_str(); }
    size_t length() const { return m_value.size(); }
private:
    std::string m_value;
};
class String {
public:
    String() = default;
    String(const char* value) : String(std::string(value)) { }
    explicit String(std::string value) : m_value(std::move(value))
    {
        auto units = convertTestString(m_value, "UTF-8", "UTF-16LE");
        m_units.resize(units.size() / sizeof(char16_t));
        std::memcpy(m_units.data(), units.data(), units.size());
    }
    static String fromUTF16(std::u16string_view value)
    {
        String result;
        result.m_units = value;
        result.m_value = convertTestString({ reinterpret_cast<const char*>(value.data()), value.size() * sizeof(char16_t) }, "UTF-16LE", "UTF-8");
        return result;
    }
    size_t length() const { return m_units.size(); }
    char16_t operator[](size_t i) const { return m_units[i]; }
    static String fromUTF8(const char* value) { return String(value); }
    CString utf8() const { return CString(m_value); }
    String toString() const { return *this; }
    bool isEmpty() const { return m_value.empty(); }
    bool startsWith(char value) const { return !m_value.empty() && m_value.front() == value; }
    const std::string& bytes() const { return m_value; }
    bool operator==(const String&) const = default;
private:
    std::string m_value;
    std::u16string m_units;
};
using StringView = String;
inline String operator""_s(const char* value, size_t length) { return String(std::string(value, length)); }
inline bool isASCIIAlpha(char value) { return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z'); }
inline bool equalIgnoringASCIICase(const String& a, const String& b)
{
    if (a.bytes().size() != b.bytes().size())
        return false;
    return std::equal(a.bytes().begin(), a.bytes().end(), b.bytes().begin(), [](unsigned char x, unsigned char y) {
        auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
        return lower(x) == lower(y);
    });
}
template<class T> class Vector : public std::vector<T> {
public:
    using std::vector<T>::vector;
    bool isEmpty() const { return this->empty(); }
    T& last() { return this->back(); }
    void removeLast() { this->pop_back(); }
    void append(const T& value) { this->push_back(value); }
};
class CheckedUint64 {
public:
    CheckedUint64(uint64_t value) : m_value(value) { }
    CheckedUint64& operator+=(uint64_t value) { m_overflow |= UINT64_MAX - m_value < value; m_value += value; return *this; }
    bool hasOverflowed() const { return m_overflow; }
    uint64_t value() const { return m_value; }
private:
    uint64_t m_value;
    bool m_overflow = false;
};
struct WallTime { double seconds; static WallTime fromRawSeconds(double value) { return { value }; } };
using std::move;
inline String hex(uint8_t value, unsigned) { char buffer[3]; snprintf(buffer, sizeof(buffer), "%02X", value); return String(buffer); }
class StringBuilder {
public:
    template<class... T> void append(const T&... values) { (m_value.append(values.bytes()), ...); }
    String toString() const { return String(m_value); }
private:
    std::string m_value;
};
template<class... T> String makeString(const T&... values) { StringBuilder builder; builder.append(values...); return builder.toString(); }
// Deterministic fixture entropy exercises exclusive mkdir retries, not RNG quality.
inline void cryptographicallyRandomValues(std::span<uint8_t> output) { static uint8_t counter = 0; std::fill(output.begin(), output.end(), ++counter); }

namespace FileSystemImpl {
class FileHandle { };
enum class FileType { Regular, Directory, SymbolicLink };
enum class FileOpenMode { Read, Truncate, ReadWrite };
String lexicallyNormal(const String&);
String pathFileName(const String&);
String parentPath(const String&);
String pathByAppendingComponent(StringView, StringView);
String pathByAppendingComponents(StringView, std::span<const StringView>);
bool isAncestor(const String&, const String&);
bool fileExists(const String&);
bool deleteFile(const String&);
bool deleteEmptyDirectory(const String&);
bool makeAllDirectories(const String&);
std::optional<FileType> fileType(const String&);
std::optional<FileType> fileTypeFollowingSymlinks(const String&);
std::optional<uint64_t> fileSize(const String&);
std::optional<WallTime> fileModificationTime(const String&);
std::optional<uint64_t> directorySize(const String&);
Vector<String> listDirectory(const String&);
void traverseDirectory(const String&, const Function<void(const String&, FileType)>&);
std::optional<Vector<String>> tryListDirectory(const String&);
std::optional<uint64_t> directorySizeIfExists(const String&);
bool deleteNonEmptyDirectory(const String&);
std::optional<uint64_t> volumeFreeSpace(const String&);
std::optional<uint64_t> volumeCapacity(const String&);
bool hardLink(const String&, const String&);
std::optional<uint64_t> hardLinkCount(const String&);
bool createSymbolicLink(const String&, const String&);
bool copyFile(const String&, const String&);
bool hardLinkOrCopyFile(const String&, const String&);
bool moveFile(const String&, const String&);
String createTemporaryDirectory();
std::pair<String, FileHandle> openTemporaryFile(StringView, StringView);
FileHandle openFile(const String&, FileOpenMode);
}
}
