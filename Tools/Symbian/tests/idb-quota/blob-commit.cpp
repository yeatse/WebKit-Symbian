#include "IDBBlobCommit.h"
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;
using namespace WebCore::IDBServer;
static unsigned checks;
static void check(bool result, const char* name)
{
    if (!result)
        throw std::runtime_error(name);
    ++checks;
    std::cout << "PASS " << name << '\n';
}
static bool sql(sqlite3* db, const char* statement) { return sqlite3_exec(db, statement, nullptr, nullptr, nullptr) == SQLITE_OK; }
static int count(sqlite3* db)
{
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM records", -1, &statement, nullptr) != SQLITE_OK)
        throw std::runtime_error("prepare count");
    auto result = sqlite3_step(statement) == SQLITE_ROW ? sqlite3_column_int(statement, 0) : -1;
    sqlite3_finalize(statement);
    return result;
}
static void write(const fs::path& path, const std::string& content) { std::ofstream(path, std::ios::binary) << content; }
static std::string read(const fs::path& path) { std::ifstream stream(path, std::ios::binary); return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() }; }

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    try {
        fs::path root = argv[1];
        fs::create_directories(root);
        for (int mode = 0; mode < 5; ++mode) {
            auto directory = root / std::to_string(mode);
            fs::create_directories(directory);
            sqlite3* db = nullptr;
            if (sqlite3_open((directory / "records.sqlite3").c_str(), &db) != SQLITE_OK)
                throw std::runtime_error("sqlite open");
            if (!sql(db, "PRAGMA journal_mode=DELETE; CREATE TABLE records (id INTEGER PRIMARY KEY, blob TEXT); INSERT INTO records VALUES(1,'old.blob'); BEGIN IMMEDIATE; DELETE FROM records; INSERT INTO records VALUES(2,'new0.blob'),(3,'new1.blob');"))
                throw std::runtime_error("initial transaction");
            write(directory / "old.blob", "original committed payload");
            write(directory / "temp0", std::string(1024, 'a'));
            write(directory / "temp1", std::string(1536, 'b'));
            if (mode == 3)
                write(directory / "new1.blob", "pre-existing destination must survive");
            std::vector<fs::path> owned;
            unsigned commitCalls = 0;
            unsigned rollbackCalls = 0;
            auto result = commitWithPreparedIDBBlobs(2, [&](size_t index) {
                auto source = directory / ("temp" + std::to_string(index));
                auto target = directory / ("new" + std::to_string(index) + ".blob");
                if (fs::exists(target))
                    return false;
                if (mode == 1 && index == 1) {
                    write(target, "partial copy before ENOSPC");
                    // The real Symbian O_EXCL copy helper removes its own
                    // partially created file before returning failure.
                    fs::remove(target);
                    return false;
                }
                if (mode == 4 && index == 1)
                    write(target, "pre-existing destination must survive"); // O_EXCL race.
                std::error_code error;
                if (!fs::copy_file(source, target, fs::copy_options::none, error))
                    return false;
                owned.push_back(target);
                return fs::file_size(source) == fs::file_size(target);
            }, [&] {
                for (auto& target : owned)
                    fs::remove(target);
            }, [&] {
                ++commitCalls;
                if (mode == 2) {
                    // A real SQLite COMMIT failure: a second reader retains a
                    // shared lock while this connection tries to commit.
                    sqlite3* reader = nullptr;
                    if (sqlite3_open((directory / "records.sqlite3").c_str(), &reader) != SQLITE_OK
                        || !sql(reader, "BEGIN; SELECT * FROM records;"))
                        throw std::runtime_error("reader lock");
                    int rc = sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
                    check(rc == SQLITE_BUSY && !sqlite3_get_autocommit(db), "injected SQL COMMIT is genuinely BUSY with transaction active");
                    sql(reader, "ROLLBACK");
                    sqlite3_close(reader);
                    return rc == SQLITE_OK;
                }
                return sql(db, "COMMIT");
            }, [&] { ++rollbackCalls; if (!sqlite3_get_autocommit(db) && !sql(db, "ROLLBACK")) throw std::runtime_error("rollback"); });
            if (result == IDBBlobCommitResult::Success)
                fs::remove(directory / "old.blob");
            fs::remove(directory / "temp0");
            fs::remove(directory / "temp1");
            if (!mode) {
                check(commitCalls == 1 && !rollbackCalls && count(db) == 2, "successful Blob preparation commits both SQL records");
                check(!fs::exists(directory / "old.blob") && read(directory / "new0.blob") == std::string(1024, 'a') && read(directory / "new1.blob") == std::string(1536, 'b'), "successful commit preserves exact new payloads then removes old Blob");
            } else {
                check(result == (mode == 2 ? IDBBlobCommitResult::CommitFailed : IDBBlobCommitResult::PrepareFailed), "failure reports correct preparation or SQL stage");
                check(rollbackCalls == 1 && count(db) == 1 && sqlite3_get_autocommit(db), "failed commit rolls SQL back to original record");
                check(read(directory / "old.blob") == "original committed payload" && !fs::exists(directory / "new0.blob"), "failed commit preserves old Blob and removes prepared new file");
                check(mode >= 3 ? read(directory / "new1.blob") == "pre-existing destination must survive" : !fs::exists(directory / "new1.blob"), "cleanup removes partial file but never an existing destination");
                if (mode != 2)
                    check(!commitCalls, "failed preparation never attempts SQL COMMIT");
            }
            check(!fs::exists(directory / "temp0") && !fs::exists(directory / "temp1"), "staging files are released after outcome");
            sqlite3_close(db);
            if (sqlite3_open((directory / "records.sqlite3").c_str(), &db) != SQLITE_OK)
                throw std::runtime_error("reopen");
            check(count(db) == (!mode ? 2 : 1), "close and reopen preserves atomic SQL outcome");
            sqlite3_close(db);
        }
        std::cout << "RESULT checks=" << checks << " failures=0\n";
    } catch (const std::exception& exception) {
        std::cerr << "FAIL " << exception.what() << '\n';
        return 1;
    }
}
