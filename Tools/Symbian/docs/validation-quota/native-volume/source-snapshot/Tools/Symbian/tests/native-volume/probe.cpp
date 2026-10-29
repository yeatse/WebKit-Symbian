#include "FileSystemSymbianNative.h"
#include <f32file.h>
#include <e32debug.h>
#include <stdarg.h>
#include <stdio.h>

static FILE* logFile;

static void report(const char* format, ...)
{
    char line[512];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    TUint16 units[512];
    unsigned length = 0;
    while (line[length] && length < 511) {
        units[length] = static_cast<unsigned char>(line[length]);
        ++length;
    }
    RDebug::RawPrint(TPtrC16(units, length));
    if (logFile) {
        fprintf(logFile, "%s", line);
        fflush(logFile);
    }
}

static unsigned passed;
static unsigned failed;

static void check(bool ok, const char* name)
{
    report("%s %s\n", ok ? "PASS" : "FAIL", name);
    ok ? ++passed : ++failed;
}

int main()
{
    logFile = fopen("C:/Data/WebKit/volume-probe.log", "w");
    if (!logFile)
        return 2;
    report("BEGIN: native-volume version=1\n");
    RFs session;
    if (session.Connect() != KErrNone)
        return 1;
    const unsigned checkedDrives[] = { EDriveC, EDriveE, EDriveZ };
    for (unsigned drive : checkedDrives) {
        uint64_t capacity = 0;
        uint64_t freeSpace = 0;
        TVolumeInfo reference;
        bool ok = WTF::SymbianSystem::volumeInformation(drive, capacity, freeSpace);
        TInt error = session.Volume(reference, drive);
        report("VOLUME %c capacity=%llu available=%llu native-error=%d\n", 'A' + drive,
            static_cast<unsigned long long>(capacity), static_cast<unsigned long long>(freeSpace), error);
        // Both queries must observe a real mounted volume; no fallback sizes.
        check(ok && !error && capacity == static_cast<uint64_t>(reference.iSize)
            && freeSpace <= capacity, "mounted volume and 64-bit native capacity");
        if (drive == EDriveZ)
            check(ok && !freeSpace, "read-only ROM has no writable space");
    }
    uint64_t capacity = 123;
    uint64_t freeSpace = 456;
    check(!WTF::SymbianSystem::volumeInformation(26, capacity, freeSpace)
        && capacity == 123 && freeSpace == 456, "invalid drive leaves outputs untouched");
    TDriveList drives;
    TInt error = session.DriveList(drives);
    bool missingChecked = false;
    if (!error) {
        for (unsigned drive = EDriveA; drive <= EDriveZ; ++drive) {
            if (drives[drive])
                continue;
            capacity = 123;
            freeSpace = 456;
            check(!WTF::SymbianSystem::volumeInformation(drive, capacity, freeSpace)
                && capacity == 123 && freeSpace == 456, "unmounted drive returns failure");
            missingChecked = true;
            break;
        }
    }
    check(missingChecked, "unmounted drive negative control exercised");

    TVolumeInfo sessionVolume;
    TVolumeInfo explicitVolume;
    error = session.SetSessionPath(_L("E:\\"));
    check(!error && session.Volume(sessionVolume) == KErrNone
        && session.Volume(explicitVolume, EDriveE) == KErrNone
        && sessionVolume.iUniqueID == explicitVolume.iUniqueID
        && sessionVolume.iSize == explicitVolume.iSize,
        "default volume follows this RFs session drive");

    uint64_t before = 0;
    bool beforeOK = WTF::SymbianSystem::volumeInformation(EDriveC, capacity, before);
    const TPtrC directory(_L("C:\\Data\\WebKit\\"));
    error = session.MkDirAll(directory);
    if (error == KErrAlreadyExists)
        error = KErrNone;
    TFileName filename;
    RFile output;
    if (!error)
        error = output.Temp(session, directory, filename, EFileWrite | EFileShareExclusive);
    if (!error) {
        TBuf8<4096> bytes;
        bytes.SetLength(bytes.MaxLength());
        bytes.Fill('V');
        for (unsigned block = 0; block < 512 && !error; ++block)
            error = output.Write(bytes);
        if (!error)
            error = output.Flush();
        output.Close();
    }
    uint64_t after = 0;
    bool afterOK = WTF::SymbianSystem::volumeInformation(EDriveC, capacity, after);
    report("VOLUME_WRITE before=%llu after=%llu error=%d\n",
        static_cast<unsigned long long>(before), static_cast<unsigned long long>(after), error);
    check(beforeOK && afterOK && !error && after < before && before - after >= (2ULL << 20),
        "flushed 2 MiB file reduces real available space");
    TInt cleanupError = filename.Length() ? session.Delete(filename) : KErrNotFound;
    check(!cleanupError, "probe removes only its unique temporary file");
    session.Close();
    report("NATIVE_VOLUME_RESULT %u/%u %s\n", passed, passed + failed, failed ? "FAIL" : "PASS");
    report("RESULT {\"probe\":\"native-volume\",\"passed\":%u,\"failed\":%u}\n", passed, failed);
    report("%s: native-volume %u/%u\n", failed ? "FAIL" : "PASS", passed, passed + failed);
    report("native-volume exiting %d\n", failed ? 1 : 0);
    fclose(logFile);
    return failed ? 1 : 0;
}
