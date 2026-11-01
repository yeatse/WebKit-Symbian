// Fault injection for the real DownloadFileNative.cpp adapter. This is not a
// file-server emulator and does not establish RFs atomicity or disk behavior.
#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

using TInt = int;
using TUint8 = uint8_t;
constexpr int KErrNone = 0, KErrNotFound = -1, KErrGeneral = -2,
    KErrArgument = -6, KErrAlreadyExists = -11, KErrPathNotFound = -12,
    KErrNotReady = -18, KErrBadName = -28, KErrTooBig = -40;
constexpr int EFileWrite = 1, EFileShareExclusive = 2;
struct TChar { uint16_t value; explicit TChar(uint16_t c) : value(c) { } };
struct TFileName {
    std::u16string value;
    void Append(TChar c) { value.push_back(c.value); }
    TInt LocateReverse(char c) const { auto p = value.rfind(c); return p == value.npos ? -1 : p; }
    std::u16string Left(TInt n) const { return value.substr(0, n); }
    unsigned Length() const { return value.size(); }
    void Zero() { value.clear(); }
};
struct TEntry { };
struct TPtrC8 {
    const uint8_t* data;
    unsigned size;
    TPtrC8(const uint8_t* p, unsigned n) : data(p), size(n) { }
};
namespace NativeFileTest {
inline std::map<std::u16string, std::vector<uint8_t>> files;
inline std::vector<std::u16string> deleted;
inline unsigned nextTemporary = 0;
inline int connectError = 0, entryError = 0, tempError = 0, writeError = 0,
    flushError = 0, renameError = 0, deleteError = 0;
inline void reset()
{
    files.clear(); deleted.clear(); nextTemporary = 0;
    connectError = entryError = tempError = writeError = flushError = renameError = deleteError = 0;
}
}
class RFs {
public:
    TInt Connect() { if (!NativeFileTest::connectError) live = true; return NativeFileTest::connectError; }
    void Close() { live = false; }
    bool Handle() const { return live; }
    bool IsValidName(const TFileName&) const { return true; }
    TInt Entry(const TFileName& name, TEntry&)
    {
        if (NativeFileTest::entryError) return NativeFileTest::entryError;
        return NativeFileTest::files.contains(name.value) ? 0 : KErrNotFound;
    }
    TInt Rename(const TFileName& from, const TFileName& to)
    {
        if (NativeFileTest::renameError) return NativeFileTest::renameError;
        if (NativeFileTest::files.contains(to.value)) return KErrAlreadyExists;
        auto entry = NativeFileTest::files.extract(from.value);
        if (entry.empty()) return KErrNotFound;
        entry.key() = to.value; NativeFileTest::files.insert(std::move(entry)); return 0;
    }
    TInt Delete(const TFileName& name)
    {
        NativeFileTest::deleted.push_back(name.value);
        if (NativeFileTest::deleteError) return NativeFileTest::deleteError;
        return NativeFileTest::files.erase(name.value) ? 0 : KErrNotFound;
    }
private:
    bool live = false;
};
class RFile {
public:
    TInt Temp(RFs&, const std::u16string& directory, TFileName& result, int)
    {
        if (NativeFileTest::tempError) {
            result.value = directory + u"existing.dat";
            return NativeFileTest::tempError;
        }
        result.value = directory + u"temporary-" + char16_t(u'0' + NativeFileTest::nextTemporary++);
        name = result.value; NativeFileTest::files[name] = { }; live = true; return 0;
    }
    bool SubSessionHandle() const { return live; }
    void Close() { live = false; }
    TInt Write(TPtrC8 input)
    {
        auto& bytes = NativeFileTest::files.at(name);
        // A failed native write may already have modified its temporary file.
        auto count = NativeFileTest::writeError ? std::min(1u, input.size) : input.size;
        bytes.insert(bytes.end(), input.data, input.data + count);
        return NativeFileTest::writeError;
    }
    TInt Flush() { return NativeFileTest::flushError; }
private:
    std::u16string name;
    bool live = false;
};
