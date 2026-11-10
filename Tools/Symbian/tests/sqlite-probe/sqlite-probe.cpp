#include <e32base.h>
#include <e32debug.h>
#include <sqlite3.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static FILE* output;
static int failures;
static void report(const char* format, ...)
{
    char line[1000];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    TUint16 units[1000];
    int length = 0;
    while (line[length] && length < 999) {
        units[length] = static_cast<unsigned char>(line[length]);
        ++length;
    }
    RDebug::RawPrint(TPtrC16(units, length));
    if (output) {
        fprintf(output, "%s\n", line);
        fflush(output);
    }
}
static void sqlLog(void*, int code, const char* message)
{
    report("SQLITE_LOG code=%d message=%s", code, message);
}
static int row(void*, int count, char** values, char** names)
{
    for (int i = 0; i < count; ++i)
        report("ROW %s=%s", names[i], values[i] ? values[i] : "NULL");
    return 0;
}
static void result(sqlite3* db, const char* operation, int code)
{
    report("%s rc=%d extended=%d errmsg=%s errno=%d", operation, code,
        db ? sqlite3_extended_errcode(db) : code,
        db ? sqlite3_errmsg(db) : sqlite3_errstr(code), errno);
}
static int exec(sqlite3* db, const char* sql)
{
    char* error = nullptr;
    errno = 0;
    int rc = sqlite3_exec(db, sql, row, nullptr, &error);
    result(db, sql, rc);
    if (error) { report("EXEC_MESSAGE %s", error); sqlite3_free(error); }
    return rc;
}
static void expect(bool success, const char* name)
{
    report("%s: %s", success ? "PASS" : "FAIL", name);
    if (!success) ++failures;
}
static bool hasOriginalValue(sqlite3* db)
{
    sqlite3_stmt* statement = nullptr;
    int rc = sqlite3_prepare_v2(db, "SELECT value FROM Probe WHERE name=\'cookie\'", -1, &statement, nullptr);
    int step = rc == SQLITE_OK ? sqlite3_step(statement) : rc;
    const char* value = step == SQLITE_ROW ? reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)) : nullptr;
    report("VERIFY_ORIGINAL prepare=%d step=%d extended=%d value=%s message=%s", rc, step,
        sqlite3_extended_errcode(db), value ? value : "NULL", sqlite3_errmsg(db));
    bool good = rc == SQLITE_OK && step == SQLITE_ROW && value && !strcmp(value, "persisted");
    if (statement) sqlite3_finalize(statement);
    return good;
}
static void fileProbe()
{
    const char* path = "C:/Data/WebKit/sqlite-probe-lock.tmp";
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0600);
    report("POSIX open fd=%d errno=%d", fd, errno);
    if (fd < 0) return;
    struct stat info = {};
    errno = 0;
    int rc = fstat(fd, &info);
    report("POSIX fstat rc=%d errno=%d mode=%o ino=%lu dev=%lu", rc, errno,
        static_cast<unsigned>(info.st_mode), static_cast<unsigned long>(info.st_ino), static_cast<unsigned long>(info.st_dev));
    errno = 0; rc = write(fd, "original", 8);
    report("POSIX write rc=%d errno=%d", rc, errno);
    errno = 0; rc = fstat(fd, &info);
    report("POSIX fstat after write rc=%d size=%ld errno=%d", rc, static_cast<long>(info.st_size), errno);
    errno = 0; rc = stat(path, &info);
    report("POSIX stat after write rc=%d size=%ld errno=%d", rc, static_cast<long>(info.st_size), errno);
    char contents[9] = {};
    lseek(fd, 0, SEEK_SET); rc = read(fd, contents, 8);
    report("POSIX read first rc=%d value=%s errno=%d", rc, contents, errno);
    int second = open(path, O_RDWR);
    report("POSIX second open fd=%d errno=%d", second, errno);
    if (second >= 0) {
        rc = write(second, "changed!", 8);
        report("POSIX second write rc=%d errno=%d", rc, errno);
        rc = fsync(second);
        report("POSIX second fsync rc=%d errno=%d", rc, errno);
        memset(contents, 0, sizeof(contents));
        lseek(fd, 0, SEEK_SET); rc = read(fd, contents, 8);
        report("POSIX first read after second fsync rc=%d value=%s errno=%d", rc, contents, errno);
        close(second);
    }
    struct flock lock = {};
    lock.l_type = F_RDLCK; lock.l_whence = SEEK_SET; lock.l_start = 0x40000000; lock.l_len = 1;
    errno = 0; rc = fcntl(fd, F_GETLK, &lock);
    report("POSIX F_GETLK rc=%d errno=%d type=%d", rc, errno, lock.l_type);
    lock.l_type = F_RDLCK;
    errno = 0; rc = fcntl(fd, F_SETLK, &lock);
    report("POSIX F_SETLK/read rc=%d errno=%d", rc, errno);
    lock.l_type = F_UNLCK;
    errno = 0; rc = fcntl(fd, F_SETLK, &lock);
    report("POSIX F_SETLK/unlock rc=%d errno=%d", rc, errno);
    close(fd);
    report("POSIX O_BINARY=%d O_BUFFERED=%d", O_BINARY, O_BUFFERED);
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC | O_BINARY, 0600);
    if (fd >= 0) {
        char page[4096]; memset(page, 'P', sizeof(page));
        lseek(fd, 0, SEEK_SET); write(fd, page, 512);
        lseek(fd, 512, SEEK_SET); write(fd, page, 4);
        lseek(fd, 516, SEEK_SET); write(fd, page, sizeof(page));
        lseek(fd, 4612, SEEK_SET); write(fd, page, 4);
        fstat(fd, &info);
        report("POSIX segmented fstat size=%ld errno=%d", static_cast<long>(info.st_size), errno);
        stat(path, &info);
        report("POSIX segmented stat size=%ld errno=%d", static_cast<long>(info.st_size), errno);
        fsync(fd); fstat(fd, &info);
        report("POSIX segmented fsync then fstat size=%ld errno=%d", static_cast<long>(info.st_size), errno);
        close(fd);
    }
}
static void databaseProbe(const char* path, const char* selectedVfs = nullptr, const char* journal = "PRAGMA journal_mode=WAL")
{
    report("DATABASE path=%s requestedVFS=%s", path, selectedVfs ? selectedVfs : "default");
    sqlite3* db = nullptr;
    int flags = SQLITE_OPEN_AUTOPROXY | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    errno = 0;
    int rc = sqlite3_open_v2(path, &db, flags, selectedVfs);
    result(db, "sqlite3_open_v2", rc);
    if (rc != SQLITE_OK) { if (selectedVfs) expect(false, "dotfile database opens"); if (db) sqlite3_close(db); return; }
    sqlite3_extended_result_codes(db, 1);
    char* vfs = nullptr;
    rc = sqlite3_file_control(db, "main", SQLITE_FCNTL_VFSNAME, &vfs);
    report("VFS rc=%d name=%s", rc, vfs ? vfs : "NULL");
    sqlite3_free(vfs);
    exec(db, "PRAGMA temp_store=MEMORY");
    exec(db, journal);
    result(db, "sqlite3_wal_checkpoint_v2", sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr));
    exec(db, "PRAGMA auto_vacuum=INCREMENTAL");
    exec(db, "CREATE TABLE IF NOT EXISTS Probe (name TEXT PRIMARY KEY, value TEXT)");
    exec(db, "INSERT OR REPLACE INTO Probe VALUES ('cookie', 'persisted')");
    exec(db, "SELECT * FROM Probe");
    if (selectedVfs) {
        expect(exec(db, "BEGIN EXCLUSIVE") == SQLITE_OK, "exclusive transaction begins");
        sqlite3* other = nullptr;
        rc = sqlite3_open_v2(path, &other, flags, selectedVfs);
        result(other, "contender open", rc);
        expect(rc == SQLITE_OK, "second connection opens");
        if (rc == SQLITE_OK) {
            rc = sqlite3_exec(other, "INSERT OR REPLACE INTO Probe VALUES ('contender', 'must-be-locked')", nullptr, nullptr, nullptr);
            result(other, "contender insert expects SQLITE_BUSY", rc);
            expect(rc == SQLITE_BUSY || rc == SQLITE_LOCKED, "second connection blocked while first holds lock");
        }
        expect(exec(db, "COMMIT") == SQLITE_OK, "first connection releases lock");
        if (other) {
            expect(exec(other, "INSERT OR REPLACE INTO Probe VALUES ('released', 'success')") == SQLITE_OK,
                "second connection writes after lock release");
            sqlite3_close(other);
        }
        expect(exec(db, "BEGIN") == SQLITE_OK, "rollback transaction begins");
        expect(exec(db, "UPDATE Probe SET value='rolled-back' WHERE name='cookie'") == SQLITE_OK,
            "rollback transaction writes");
        sqlite3_file* journalFile = nullptr;
        int journalRc = sqlite3_file_control(db, "main", SQLITE_FCNTL_JOURNAL_POINTER, &journalFile);
        sqlite3_int64 journalSize = -1;
        if (journalRc == SQLITE_OK && journalFile && journalFile->pMethods)
            journalRc = journalFile->pMethods->xFileSize(journalFile, &journalSize);
        report("JOURNAL_BEFORE_ROLLBACK rc=%d size=%ld", journalRc, static_cast<long>(journalSize));
        char journalPath[256]; snprintf(journalPath, sizeof(journalPath), "%s-journal", path);
        struct stat journalStat = {};
        int journalStatRc = stat(journalPath, &journalStat);
        report("JOURNAL_PATH_STAT rc=%d size=%ld errno=%d", journalStatRc, static_cast<long>(journalStat.st_size), errno);

        expect(exec(db, "ROLLBACK") == SQLITE_OK, "rollback succeeds");
        expect(hasOriginalValue(db), "rollback preserves committed value");
    }
    rc = sqlite3_close(db); report("sqlite3_close rc=%d", rc);
    db = nullptr;
    rc = sqlite3_open_v2(path, &db, flags, selectedVfs);
    result(db, "sqlite3_reopen", rc);
    if (rc == SQLITE_OK) exec(db, "SELECT * FROM Probe");
    if (selectedVfs) expect(rc == SQLITE_OK && hasOriginalValue(db), "close and reopen preserve committed value");
    if (db) sqlite3_close(db);
}
// SQLite keeps OS function pointers; the converter needs corresponding PLT slots.
static void retainSyscallPltEntries(const char* path)
{
    char buffer[2]; struct stat info;
    access(path, 0); getcwd(buffer, sizeof(buffer)); stat(path, &info);
    ftruncate(-1, 0); read(-1, buffer, 1); write(-1, buffer, 1); fchmod(-1, 0600);
    unlink(path); mkdir(path, 0700); rmdir(path); geteuid();
    readlink(path, buffer, sizeof(buffer)); lstat(path, &info);
}
int main(int argc, char** argv)
{
    if (argc == 42) retainSyscallPltEntries("C:/Data/WebKit/sqlite-probe-imports.tmp");
    output = fopen("C:/Data/WebKit/sqlite-probe.log", "w");
    report("BEGIN: sqlite-probe version=%s threadsafe=%d logfile=%s", sqlite3_libversion(), sqlite3_threadsafe(), output ? "open" : "failed");
    report("CONFIG_LOG rc=%d", sqlite3_config(SQLITE_CONFIG_LOG, sqlLog, nullptr));
    report("INITIALIZE rc=%d OMIT_WAL=%d", sqlite3_initialize(), sqlite3_compileoption_used("OMIT_WAL"));
    for (sqlite3_vfs* vfs = sqlite3_vfs_find(nullptr); vfs; vfs = vfs->pNext)
        report("AVAILABLE_VFS %s", vfs->zName);
    fileProbe();
    databaseProbe("C:/Data/WebKit/sqlite-probe.db");
    databaseProbe("C:\\Data\\WebKit\\sqlite-probe-backslash.db");
    databaseProbe("C:/Data/WebKit/sqlite-probe-dotfile.db", "unix-dotfile");
    databaseProbe("C:/Data/WebKit/sqlite-probe-delete.db", "unix-dotfile", "PRAGMA journal_mode=DELETE");
    report("%s: SQLite dotfile locking, rollback and persistence", failures ? "FAIL" : "PASS");
    report("DONE: sqlite-probe failures=%d", failures);
    report("sqlite-probe exiting %d", failures ? 1 : 0);
    if (output) fclose(output);
    return failures ? 1 : 0;
}
