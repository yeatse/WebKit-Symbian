#include <common/fileutils.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <sys/stat.h>

using namespace eka2l1;

static bool write(const std::string& path, const char* mode, const char* content)
{
    FILE* file = common::open_c_file(path, mode);
    if (!file)
        return false;
    bool ok = fputs(content, file) >= 0;
    return fclose(file) == 0 && ok;
}

static std::string read(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

struct Result {
    bool sameSecond;
    bool exactFound;
    bool aliasFound;
    bool refreshedFound;
    bool sourcePreserved;
};

static Result creation(const std::string& root, const char* mode)
{
    // Retry only if the real clock crosses a second boundary. No sleeps or fake
    // timestamps: the regression specifically exercises the production stamp.
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        const std::string base = root + "/attempt" + std::to_string(attempt);
        common::create_directories(base + "/Parent");
        struct stat before { }, after { };
        stat((base + "/Parent").c_str(), &before);
        const std::string absent = common::resolve_case_insensitive_path(base, "parent/SOURCE.BIN");
        if (common::exists(absent) || !write(base + "/Parent/Source.bin", mode, "source-sentinel"))
            return { };
        stat((base + "/Parent").c_str(), &after);
        if (before.st_mtime != after.st_mtime)
            continue;
        std::string alias = common::resolve_case_insensitive_path(base, "parent/SOURCE.BIN");
        bool found = common::exists(alias);
        common::invalidate_folded_directory_indexes();
        std::string refreshed = common::resolve_case_insensitive_path(base, "parent/SOURCE.BIN");
        return { true, common::exists(base + "/Parent/Source.bin"), found,
            common::exists(refreshed), read(base + "/Parent/Source.bin") == "source-sentinel" };
    }
    return { };
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    const std::string root = argv[1];
    common::create_directories(root);
    if (common::is_path_case_insensitive(root)) {
        std::cerr << "case-sensitive host filesystem required\n";
        return 2;
    }
    Result truncateCreate = creation(root + "/write", "wb");
    Result appendCreate = creation(root + "/append", "ab");

    const std::string upper = root + "/collision/Data/WebKit";
    const std::string lower = root + "/collision/Data/webkit";
    common::create_directories(upper);
    common::create_directories(lower);
    bool collisionPrepared = write(upper + "/Source.bin", "wb", "upper-source")
        && write(lower + "/Other.bin", "wb", "lower-other");
    struct stat upperBefore { }, lowerBefore { }, upperAfter { }, lowerAfter { };
    stat(upper.c_str(), &upperBefore);
    stat(lower.c_str(), &lowerBefore);
    std::string collisionAlias = common::resolve_case_insensitive_path(root + "/collision", "data/webkit/SOURCE.BIN");
    bool collisionAliasFound = common::exists(collisionAlias);
    // Also exercise an ordinary creating open while both existing directories
    // are present. The cache fix must not rename, merge or discard either one.
    bool unrelatedWrite = write(root + "/unrelated.bin", "wb", "unrelated");
    stat(upper.c_str(), &upperAfter);
    stat(lower.c_str(), &lowerAfter);
    bool collisionPreserved = collisionPrepared && unrelatedWrite
        && upperBefore.st_ino != lowerBefore.st_ino
        && upperBefore.st_ino == upperAfter.st_ino && lowerBefore.st_ino == lowerAfter.st_ino
        && read(upper + "/Source.bin") == "upper-source" && read(lower + "/Other.bin") == "lower-other";
    bool failedOpenSafe = !write(root + "/absent-parent/file.bin", "wb", "bad")
        && read(upper + "/Source.bin") == "upper-source";
    bool exactWriteWorks = write(root + "/unrelated.bin", "wb", "updated")
        && read(root + "/unrelated.bin") == "updated";
    std::cout << "{\"write_same_second\":" << truncateCreate.sameSecond
        << ",\"write_alias_found\":" << truncateCreate.aliasFound
        << ",\"append_same_second\":" << appendCreate.sameSecond
        << ",\"append_alias_found\":" << appendCreate.aliasFound
        << ",\"exact_and_source_preserved\":" << (truncateCreate.exactFound && appendCreate.exactFound && truncateCreate.sourcePreserved && appendCreate.sourcePreserved)
        << ",\"explicit_refresh_finds_both\":" << (truncateCreate.refreshedFound && appendCreate.refreshedFound)
        << ",\"collision_alias_found\":" << collisionAliasFound
        << ",\"collision_directories_preserved\":" << collisionPreserved
        << ",\"failed_open_safe\":" << failedOpenSafe
        << ",\"existing_write_works\":" << exactWriteWorks << "}\n";
}
