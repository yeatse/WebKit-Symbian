#include "DownloadFileNative.h"
#include <f32file.h>
#include <cstdio>
#include <cstdlib>
#include <memory>

using namespace WebKitSymbian;
using namespace NativeFileTest;
static unsigned checks;
static void check(bool value, const char* name)
{
    ++checks;
    if (!value) { std::printf("FAIL: %s\n", name); std::exit(1); }
    std::printf("PASS: %s\n", name);
}
static constexpr char16_t target[] = u"C:\\Data\\result.bin";
static constexpr unsigned targetLength = sizeof(target) / sizeof(target[0]) - 1;
static std::unique_ptr<DownloadFile> file() { return std::unique_ptr<DownloadFile>(createNativeDownloadFile()); }
static int open(DownloadFile& f) { return f.open(reinterpret_cast<const uint16_t*>(target), targetLength); }
int main()
{
    const uint8_t bytes[] = { 0, 1, 2, 255 };
    reset();
    {
        auto f = file();
        check(!open(*f) && !files.contains(target) && files.size() == 1, "opening never creates the target");
        check(!f->append(bytes, sizeof(bytes)) && !files.contains(target), "streaming modifies only the owned temporary file");
        check(!f->finish() && files.at(target) == std::vector<uint8_t>(bytes, bytes + 4), "successful flush and rename publish exact bytes");
        check(!f->cancel() && deleted.empty(), "cancel after success does not delete the published file");
    }
    check(files.size() == 1 && deleted.empty(), "destruction retains the finished target");
    {
        auto f = file();
        check(open(*f) == KErrAlreadyExists, "existing target is refused before temporary creation");
    }
    check(files.size() == 1 && deleted.empty(), "existing target survives refused download and destructor");
    reset();
    {
        auto f = file(); check(!open(*f), "new target can be staged");
        files[target] = { 9, 8, 7 };
        check(f->finish() == KErrAlreadyExists, "target created during transfer prevents publication");
        check(!f->cancel() && files.at(target) == std::vector<uint8_t>({ 9, 8, 7 }) && files.size() == 1, "racing target is retained while own temporary is removed");
    }
    for (int stage = 0; stage < 3; ++stage) {
        reset(); auto f = file(); check(!open(*f), "failure case opens its own temporary");
        int result;
        if (!stage) { writeError = -26; result = f->append(bytes, sizeof(bytes)); }
        else { if (stage == 1) flushError = -26; else renameError = -21; result = f->finish(); }
        check(result == (stage == 2 ? -21 : -26), "write, flush or rename native error is propagated");
        check(!f->cancel() && files.empty() && deleted.size() == 1, "failure cleanup removes only the temporary");
        check(!f->cancel() && deleted.size() == 1, "repeated cancellation is idempotent");
    }
    reset();
    {
        auto f = file(); check(!open(*f), "cleanup error case opens temporary"); deleteError = -21;
        check(f->cancel() == -21 && files.size() == 1, "cleanup failure remains an explicit error");
        deleteError = 0; check(!f->cancel() && files.empty(), "cleanup can retry its own retained temporary");
    }
    reset();
    {
        auto f = file(); check(!open(*f), "size boundary case opens temporary");
        check(f->append(bytes, 0x80000000u) == KErrTooBig && files.begin()->second.empty(), "over-limit append rejected before dereferencing bytes");
    }
    check(files.empty(), "destructor cancels unpublished temporary");
    for (auto error : { -12, -18, -21, -26 }) {
        reset(); tempError = error; files[u"C:\\Data\\existing.dat"] = { 7 }; auto f = file();
        check(open(*f) == error && !f->cancel() && files.size() == 1 && files.at(u"C:\\Data\\existing.dat")[0] == 7 && deleted.empty(), "temporary creation failure never acquires unrelated cleanup ownership");
    }
    std::printf("RESULT checks=%u failures=0 scope=native-adapter-with-fault-injection\n", checks);
}
