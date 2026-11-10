#include <e32base.h>
#include <mbedtls/sha256.h>
#include <f32file.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static FILE* output;
static unsigned passed, failed;
static constexpr size_t MiB = 1024 * 1024;
static constexpr size_t capacity = 37 * MiB;
static const char* path = "C:/Data/WebKit/large-file-owned.bin";
static unsigned char pattern(size_t index) { return static_cast<unsigned char>((index * 37 + (index >> 16) * 11) & 255); }
static void check(bool value, const char* name)
{
    std::fprintf(output, "%s: %s\n", value ? "PASS" : "FAIL", name);
    std::fflush(output);
    value ? ++passed : ++failed;
}
static bool matches(const unsigned char* data, size_t offset, size_t length, unsigned& hash)
{
    bool valid = true;
    for (size_t index = 0; index < length; ++index) {
        valid = (data[index] == pattern(offset + index)) && valid;
        hash = (hash ^ data[index]) * 16777619u;
    }
    return valid;
}
static bool chunkedReadback(size_t length)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    unsigned char buffer[4096];
    size_t offset = 0;
    bool valid = true;
    unsigned hash = 2166136261u;
    while (offset < length) {
        size_t count = length - offset;
        if (count > sizeof(buffer))
            count = sizeof(buffer);
        ssize_t got = read(fd, buffer, count);
        if (got != static_cast<ssize_t>(count) || !matches(buffer, offset, count, hash)) {
            std::fprintf(output, "INFO mismatch offset=%lu got=%ld\n", static_cast<unsigned long>(offset), static_cast<long>(got));
            valid = false;
            break;
        }
        offset += count;
    }
    if (valid)
        valid = read(fd, buffer, 1) == 0;
    std::fprintf(output, "INFO chunkedReadBytes=%lu fnv1a32=%08x\n", static_cast<unsigned long>(offset), hash);
    close(fd);
    return valid;
}
static void run(unsigned char* data, size_t length, size_t chunk, bool bulkRead)
{
    for (size_t index = 0; index < length; ++index)
        data[index] = pattern(index);
    unlink(path);
    int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
    bool wrote = fd >= 0;
    size_t offset = 0;
    unsigned calls = 0;
    int error = 0;
    while (wrote && offset < length) {
        size_t count = length - offset;
        if (count > chunk)
            count = chunk;
        ssize_t returned = write(fd, data + offset, count);
        ++calls;
        if (returned != static_cast<ssize_t>(count)) {
            error = errno;
            wrote = false;
            break;
        }
        offset += count;
    }
    if (fd >= 0)
        wrote = !close(fd) && wrote;
    std::fprintf(output, "INFO length=%lu chunk=%lu calls=%u reportedBytes=%lu errno=%d\n", static_cast<unsigned long>(length), static_cast<unsigned long>(chunk), calls, static_cast<unsigned long>(offset), error);
    check(wrote, "POSIX write and close report the complete requested bytes");
    struct stat info { };
    bool stated = !stat(path, &info);
    std::fprintf(output, "INFO physicalBytes=%ld\n", stated ? static_cast<long>(info.st_size) : -1L);
    check(stated && info.st_size == static_cast<off_t>(length), "physical length equals requested length");
    check(chunkedReadback(length), "complete chunked readback matches every source byte and EOF");
    if (bulkRead) {
        std::memset(data, 0, length);
        fd = open(path, O_RDONLY);
        ssize_t got = fd < 0 ? -1 : read(fd, data, length);
        unsigned hash = 2166136261u;
        bool valid = got == static_cast<ssize_t>(length) && matches(data, 0, length, hash);
        std::fprintf(output, "INFO singleReadBytes=%ld fnv1a32=%08x\n", static_cast<long>(got), hash);
        unsigned char digest[32];
        if (got == static_cast<ssize_t>(length) && !mbedtls_sha256(data, length, digest, 0)) {
            std::fprintf(output, "INFO singleReadSHA256=");
            for (auto byte : digest)
                std::fprintf(output, "%02x", byte);
            std::fprintf(output, "\n");
        } else
            valid = false;
        check(valid, "single POSIX read returns exact full-length binary payload");
        check(fd >= 0 && read(fd, data, 1) == 0, "read after complete payload returns EOF");
        if (fd >= 0)
            close(fd);
    }
}
class RawFile : public RFile {
public:
    TInt writeLength(const TDesC8& bytes, TInt length)
    {
        // EFsFileWrite is 35 in Belle. Send through the inherited subsession
        // entry to exercise server validation, bypassing RFile client checks.
        return RSubSessionBase::SendReceive(35, TIpcArgs(&bytes, length, 0));
    }
};
static void ipcBounds()
{
    RFs session;
    TInt connected = session.Connect();
    RawFile file;
    TInt opened = connected == KErrNone ? file.Replace(session, _L("C:\\Data\\WebKit\\large-file-ipc.bin"), EFileWrite | EFileShareExclusive) : connected;
    check(opened == KErrNone, "open a distinct owned file for raw IPC bounds");
    if (opened == KErrNone) {
        unsigned char bytes[32] { 17, 29, 43, 59 };
        TPtrC8 descriptor(bytes, 4);
        check(file.writeLength(descriptor, 0) == KErrNone, "zero-length write is valid");
        check(file.writeLength(descriptor, 5) == KErrArgument, "write length greater than descriptor is rejected");
        check(file.writeLength(descriptor, -1) == KErrArgument, "negative write length is rejected");
        TInt length = -1;
        check(file.Size(length) == KErrNone && !length, "invalid writes leave file empty");
        file.Close();
    }
    if (connected == KErrNone) {
        session.Delete(_L("C:\\Data\\WebKit\\large-file-ipc.bin"));
        session.Close();
    }
}
int main()
{
    // SDK startup currently calls main(1, argv); read the native command line.
    TBuf<128> arguments;
    if (User::CommandLineLength() > arguments.MaxLength())
        return 2;
    User::CommandLine(arguments);
    arguments.Trim();
    const bool negative = arguments == _L("--legacy-negative");
    if (arguments.Length() && !negative)
        return 2;
    output = std::fopen("C:/Data/WebKit/large-file-probe.log", "wb");
    if (!output)
        return 2;
    std::fprintf(output, "BEGIN large-file-probe version=2 legacy-negative=%d\n", negative);
    auto* data = static_cast<unsigned char*>(std::malloc(capacity));
    check(!!data, "allocate 37 MiB source without changing allocator budget");
    if (data) {
        run(data, capacity, MiB, !negative);
        if (negative)
            run(data, 34 * MiB, 34 * MiB, false);
        else {
            run(data, 16 * MiB - 1, 16 * MiB - 1, true);
            run(data, 16 * MiB, 16 * MiB, true);
            run(data, 16 * MiB + 1, 16 * MiB + 1, true);
            run(data, 34 * MiB, 34 * MiB, true);
            run(data, capacity, capacity, true);
            ipcBounds();
        }
        std::free(data);
    }
    check(!unlink(path) || errno == ENOENT, "remove only the owned binary file");
    std::fprintf(output, "RESULT {\"probe\":\"large-file-probe\",\"legacyNegative\":%s,\"passed\":%u,\"failed\":%u}\n", negative ? "true" : "false", passed, failed);
    std::fprintf(output, "%s: large-file-probe %u/%u\nlarge-file-probe exiting %d\n", failed ? "FAIL" : "PASS", passed, passed + failed, failed ? 1 : 0);
    std::fclose(output);
    return failed ? 1 : 0;
}
