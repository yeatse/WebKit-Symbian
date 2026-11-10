// The generated include contains the actual BlobRegistry producer and writer
// bodies. Only the queue, Blob objects and filesystem I/O seams are supplied.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#define OS(name) 1
#define ASSERT(value) do { if (!(value)) throw std::runtime_error("ASSERT " #value); } while (0)
#define LOG_ERROR(...) do { } while (0)
using String = std::string;
struct TestString : String {
    using String::String;
    TestString(String value) : String(std::move(value)) { }
    TestString isolatedCopy() const { return *this; }
    bool isEmpty() const { return empty(); }
};
#define String TestString
String operator""_s(const char* value, size_t length) { return String(value, length); }
template<typename T> struct Vector : std::vector<T> {
    using std::vector<T>::vector;
    void append(T value) { this->push_back(std::move(value)); }
    bool isEmpty() const { return this->empty(); }
};
template<typename T> using Ref = std::shared_ptr<T>;
template<typename... T> using Variant = std::variant<T...>;
template<typename T> using CompletionHandler = std::function<T>;
struct DataSegment {
    std::vector<uint8_t> bytes;
    size_t size() const { return bytes.size(); }
    std::span<const uint8_t> span() const { return bytes; }
};
namespace WTF {
using std::move;
template<typename... T> struct Visitor : T... { using T::operator()...; };
template<typename... T> Visitor(T...) -> Visitor<T...>;
template<typename V, typename... T> auto switchOn(const V& value, T&&... handlers) { return std::visit(Visitor { std::forward<T>(handlers)... }, value); }
}
namespace FileSystem {
namespace fs = std::filesystem;
static fs::path root;
static unsigned sequence, writes, failWrite, failOpen;
static std::set<std::string> openPaths;
class FileHandle {
public:
    FileHandle() = default;
    FileHandle(FILE* file, String path) : file(file), path(std::move(path)) { if (file) openPaths.insert(this->path); }
    FileHandle(const FileHandle&) = delete;
    FileHandle(FileHandle&& other) noexcept : file(std::exchange(other.file, nullptr)), path(std::move(other.path)) { }
    FileHandle& operator=(FileHandle&& other) noexcept { close(); file = std::exchange(other.file, nullptr); path = std::move(other.path); return *this; }
    ~FileHandle() { close(); }
    explicit operator bool() const { return file; }
    void close() { if (file) { fclose(file); file = nullptr; openPaths.erase(path); } }
    int64_t write(std::span<const uint8_t> bytes) {
        ++writes;
        size_t size = writes == failWrite ? std::min<size_t>(2, bytes.size()) : bytes.size();
        return fwrite(bytes.data(), 1, size, file);
    }
    bool appendFileContents(const String& source) {
        std::ifstream input(source, std::ios::binary);
        if (!input) return false;
        std::vector<uint8_t> data { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
        return !input.bad() && write(data) == static_cast<int64_t>(data.size());
    }
private:
    FILE* file { nullptr };
    String path;
};
static std::pair<String, FileHandle> openTemporaryFile(const String&) {
    ++sequence;
    if (sequence == failOpen) return { String { }, FileHandle { } };
    String path((root / ("Blob" + std::to_string(sequence))).string());
    return { path, FileHandle(fopen(path.c_str(), "wb"), path) };
}
static bool deleteFile(const String& path) {
    if (openPaths.contains(path)) throw std::runtime_error("temporary file deleted before close");
    return fs::remove(fs::path(std::string(path)));
}
}
struct Queue { template<typename F> void dispatch(F&& function) { function(); } };
Queue& blobUtilityQueueSingleton() { static Queue queue; return queue; }
template<typename F> void callOnMainThread(F&& function) { function(); }
class BlobRegistryImpl {
public:
    struct BlobForFileWriting { Vector<Variant<String, Ref<DataSegment>>> filePathsOrDataBuffers; };
    Vector<BlobForFileWriting> inputs;
    bool populateBlobsForFileWriting(const Vector<String>&, Vector<BlobForFileWriting>& result) { result = inputs; return true; }
    void writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>&, CompletionHandler<void(Vector<String>&&)>&&);
};
#include "producer.inc"

static void check(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
    std::cout << "PASS " << description << '\n';
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        namespace fs = std::filesystem;
        fs::path base = argv[1]; fs::create_directories(base);
        auto source = base / "original-input.txt";
        std::ofstream(source) << "original File source survives staging failure";
        for (unsigned mode = 0; mode < 4; ++mode) {
            FileSystem::root = base / std::to_string(mode); fs::create_directories(FileSystem::root);
            FileSystem::sequence = FileSystem::writes = 0;
            FileSystem::failWrite = mode == 1 ? 1 : mode == 2 ? 2 : 0;
            FileSystem::failOpen = mode == 3 ? 2 : 0;
            BlobRegistryImpl registry;
            auto data = std::make_shared<DataSegment>(); data->bytes.assign(32, 'a');
            registry.inputs.append({ Vector<Variant<String, Ref<DataSegment>>> { data } });
            registry.inputs.append({ Vector<Variant<String, Ref<DataSegment>>> { String(source.string()) } });
            Vector<String> result;
            registry.writeBlobsToTemporaryFilesForIndexedDB({ }, [&](auto&& paths) { result = std::move(paths); });
            if (!mode) {
                check(result.size() == 2 && fs::file_size(fs::path(std::string(result[0]))) == 32, "successful producer returns both distinct staging copies");
                check(result[1] != source.string(), "File input source path is never handed to the IDB backend");
                for (auto& path : result) FileSystem::deleteFile(path);
            } else
                check(result.empty(), "failed producer reports no partial Blob list");
            check(fs::is_empty(FileSystem::root), "failed/consumed request leaves no partial or earlier staging files");
            check(FileSystem::openPaths.empty(), "all output handles are closed before cleanup");
            std::ifstream input(source); std::string contents { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
            check(contents == "original File source survives staging failure", "original File input remains byte-for-byte intact");
        }
        std::cout << "RESULT checks=17 failures=0\n";
    } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
