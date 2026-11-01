// Fixed public-file contract through the actual WTF file API. No UI or enumeration.
#include "config.h"

#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

#include <array>
#include <cmath>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <e32base.h>
#include <mbedtls/sha256.h>

static_assert(sizeof(wchar_t) == 2 && alignof(wchar_t) == 2);
static_assert(sizeof(char16_t) == 2);

namespace {

constexpr char16_t unicodeName[] = u"字\U0001F600.dat";
static_assert(unicodeName[0] == 0x5b57 && unicodeName[1] == 0xd83d && unicodeName[2] == 0xde00);
constexpr const char16_t* names[] = { u"alpha.bin", u"zero.txt", unicodeName };
constexpr unsigned sizes[] = { 1024, 0, 24 };
constexpr const char* hashes[] = {
    "785b0751fc2c53dc14a4ce3d800e69ef9ce1009eb327ccf458afe09c242c26c9",
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "8a4a90e74c3d9c754d0f3ab6b391646f7de56516d70a5ad20c05e49030a481da"
};
constexpr uint8_t unicodeBytes[] = {
    0x42, 0x65, 0x6c, 0x6c, 0x65, 0x20, 0xe6, 0x96, 0x87, 0xe4, 0xbb, 0xb6,
    0xe4, 0xb8, 0x8a, 0xe4, 0xbc, 0xa0, 0x20, 0xf0, 0x9f, 0x98, 0x80, 0x0a
};
constexpr unsigned maximumBytes = 2048;
constexpr double expectedModificationTime = 1700000000;
unsigned passed = 0;
unsigned failed = 0;

const char* boolean(bool value) { return value ? "true" : "false"; }

struct Outcome {
    bool exists = false;
    int existsErrno = 0;
    bool sizeAvailable = false;
    uint64_t size = 0;
    int sizeErrno = 0;
    bool modificationTimeAvailable = false;
    double modificationTime = -1;
    bool modificationTimeOK = false;
    int modificationTimeErrno = 0;
    bool opened = false;
    int openErrno = 0;
    bool readOK = false;
    int readErrno = 0;
    bool eof = false;
    bool bounded = true;
    unsigned bytes = 0;
    bool exact = false;
    bool hashOK = false;
    char sha256[65] = { };
};

bool readToken(char* token)
{
    TBuf<128> line;
    User::CommandLine(line);
    line.Trim();
    if (line.Length() < 6 || line.Left(6) != _L("--run "))
        return false;
    TPtrC value = line.Mid(6);
    if (value.Length() < 8 || value.Length() > 64)
        return false;
    for (TInt i = 0; i < value.Length(); ++i) {
        TUint character = value[i];
        if (!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9') || character == '-' || character == '_'))
            return false;
        token[i] = static_cast<char>(character);
    }
    token[value.Length()] = 0;
    return true;
}

WTF::String filePath(const char* token, unsigned index)
{
    // The validated token is at most 64 ASCII units. This fixed path is below 256 units.
    char16_t units[256] = { };
    unsigned length = 0;
    constexpr char16_t prefix[] = u"C:\\Data\\WebKit\\wtf-file-owner-";
    for (char16_t character : prefix) {
        if (!character)
            break;
        units[length++] = character;
    }
    for (const char* character = token; *character; ++character)
        units[length++] = static_cast<char16_t>(*character);
    units[length++] = u'\\';
    for (const char16_t* character = names[index]; *character; ++character)
        units[length++] = *character;
    return WTF::String(std::span<const char16_t>(units, length));
}

void checkBytes(Outcome& outcome, unsigned index, std::span<const uint8_t> bytes)
{
    outcome.exact = bytes.size() == sizes[index];
    for (unsigned i = 0; outcome.exact && i < bytes.size(); ++i)
        outcome.exact = bytes[i] == (index == 0 ? static_cast<uint8_t>(i) : unicodeBytes[i]);

    unsigned char hash[32];
    if (mbedtls_sha256(bytes.data(), bytes.size(), hash, 0))
        return;
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < 32; ++i) {
        outcome.sha256[2 * i] = hex[hash[i] >> 4];
        outcome.sha256[2 * i + 1] = hex[hash[i] & 15];
    }
    outcome.sha256[64] = 0;
    outcome.hashOK = !strcmp(outcome.sha256, hashes[index]);
}

void report(unsigned index, const Outcome& outcome)
{
    bool pass = outcome.exists && outcome.sizeAvailable && outcome.size == sizes[index]
        && outcome.modificationTimeAvailable && outcome.modificationTimeOK && outcome.opened
        && outcome.readOK && outcome.eof && outcome.bounded && outcome.exact && outcome.hashOK;
    printf("CASE {\"api\":\"WTF::FileSystem\",\"file\":%u,\"pass\":%s,"
        "\"exists\":%s,\"exists_errno\":%d,\"size_available\":%s,\"size\":%llu,\"size_errno\":%d,"
        "\"mtime_available\":%s,\"mtime\":%.6f,\"mtime_ok\":%s,\"mtime_errno\":%d,"
        "\"opened\":%s,\"open_errno\":%d,\"read_ok\":%s,\"read_errno\":%d,"
        "\"eof\":%s,\"bounded\":%s,\"bytes\":%u,\"exact\":%s,\"hash_ok\":%s,\"sha256\":\"%s\"}\n",
        index, boolean(pass), boolean(outcome.exists), outcome.existsErrno,
        boolean(outcome.sizeAvailable), static_cast<unsigned long long>(outcome.size), outcome.sizeErrno,
        boolean(outcome.modificationTimeAvailable), outcome.modificationTime,
        boolean(outcome.modificationTimeOK), outcome.modificationTimeErrno,
        boolean(outcome.opened), outcome.openErrno, boolean(outcome.readOK), outcome.readErrno,
        boolean(outcome.eof), boolean(outcome.bounded), outcome.bytes, boolean(outcome.exact),
        boolean(outcome.hashOK), outcome.sha256);
    fflush(stdout);
    pass ? ++passed : ++failed;
}

void checkFile(const WTF::String& path, unsigned index)
{
    Outcome outcome;
    errno = 0;
    outcome.exists = FileSystem::fileExists(path);
    outcome.existsErrno = errno;
    errno = 0;
    auto size = FileSystem::fileSize(path);
    outcome.sizeErrno = errno;
    outcome.sizeAvailable = size.has_value();
    if (size)
        outcome.size = *size;
    errno = 0;
    auto modificationTime = FileSystem::fileModificationTime(path);
    outcome.modificationTimeErrno = errno;
    outcome.modificationTimeAvailable = modificationTime.has_value();
    if (modificationTime) {
        double seconds = modificationTime->secondsSinceEpoch().seconds();
        if (std::isfinite(seconds)) {
            outcome.modificationTime = seconds;
            outcome.modificationTimeOK = std::fabs(seconds - expectedModificationTime) <= 2;
        }
    }

    {
        errno = 0;
        auto file = FileSystem::openFile(path, FileSystem::FileOpenMode::Read);
        outcome.openErrno = errno;
        outcome.opened = file.isValid();
        if (file) {
            std::array<uint8_t, maximumBytes> bytes { };
            outcome.readOK = true;
            for (;;) {
                std::array<uint8_t, 256> block;
                errno = 0;
                auto count = file.read(std::span<uint8_t>(block));
                int readErrno = errno;
                if (!count) {
                    outcome.readOK = false;
                    outcome.readErrno = readErrno;
                    break;
                }
                if (!*count) {
                    outcome.eof = true;
                    break;
                }
                if (*count > block.size() || *count > maximumBytes - outcome.bytes) {
                    outcome.bounded = false;
                    outcome.readOK = false;
                    break;
                }
                memcpy(bytes.data() + outcome.bytes, block.data(), static_cast<size_t>(*count));
                outcome.bytes += static_cast<unsigned>(*count);
            }
            if (outcome.readOK && outcome.eof)
                checkBytes(outcome, index, std::span<const uint8_t>(bytes.data(), outcome.bytes));
        }
    } // FileHandle closes through its actual production destructor before report.
    report(index, outcome);
}

} // namespace

int main()
{
    char token[65] = { };
    if (!readToken(token))
        return 2;
    if (!freopen("C:/Data/WebKit/wtf-file-owner-contract.log", "w", stdout))
        return 2;
    printf("BEGIN {\"suite\":\"wtf-file-owner-contract\",\"version\":1,\"run\":\"%s\"}\n", token);
    fflush(stdout);
    WTF::initializeMainThread();
    printf("ABI {\"wchar\":%u,\"wchar_align\":%u,\"char16\":%u,\"unicode_units\":[23383,55357,56832]}\n",
        unsigned(sizeof(wchar_t)), unsigned(alignof(wchar_t)), unsigned(sizeof(char16_t)));
    for (unsigned index = 0; index < 3; ++index)
        checkFile(filePath(token, index), index);
    printf("RESULT {\"suite\":\"wtf-file-owner-contract\",\"version\":1,\"run\":\"%s\",\"passed\":%u,\"failed\":%u,\"total\":3}\n",
        token, passed, failed);
    printf("wtf-file-owner-contract exiting %d\n", failed ? 1 : 0);
    fflush(stdout);
    fclose(stdout);
    return failed ? 1 : 0;
}
