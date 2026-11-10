#include "DownloadFilename.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

using namespace WebKitSymbian;
static unsigned checks;
static void check(bool value, const char* name)
{
    ++checks;
    if (!value) {
        std::printf("FAIL: %s\n", name);
        std::exit(1);
    }
    std::printf("PASS: %s\n", name);
}
static std::u16string name(std::u16string_view input, unsigned capacity = 128)
{
    std::array<uint16_t, 130> output;
    output.fill(0xbeef);
    auto length = makeDownloadFilename(reinterpret_cast<const uint16_t*>(input.data()), input.size(), output.data() + 1, capacity);
    check(length <= capacity && output[0] == 0xbeef && output[capacity + 1] == 0xbeef, "bounded output leaves guards intact");
    return std::u16string(reinterpret_cast<const char16_t*>(output.data() + 1), length);
}
static bool destination(std::u16string_view path)
{
    return isValidDownloadDestination(reinterpret_cast<const uint16_t*>(path.data()), path.size());
}
int main()
{
    check(name(u"../../private/evil.bin") == u"evil.bin", "server path never becomes the destination path");
    check(name(u"C:\\sys\\bin\\payload.exe") == u"payload.exe", "backslash server path is reduced to basename");
    check(name(u"C:bad?.txt") == u"C_bad_.txt", "drive-relative and wildcard characters are not retained");
    check(name(u"\u202epdf.exe\u2066") == u"_pdf.exe_", "bidirectional controls cannot disguise the filename");
    check(name(std::u16string_view(u"a\0b\r\nc", 6)) == u"a_b__c", "NUL and header line breaks remain visible replacements");
    check(name(u"..") == u"download" && name(u"/ ") == u"download", "dot and empty basenames get a safe fallback");
    check(name(u"报告 \U0001f600.txt") == u"报告 \U0001f600.txt", "Unicode and complete surrogate pairs survive");
    check(name(u"a\U0001f600.txt", 2) == u"a", "truncation does not split a surrogate pair");
    check(name(std::u16string({ 0xd800, u'x', 0xdc00 })) == u"_x_", "malformed UTF-16 is replaced");
    check(name(u"name.  ") == u"name", "trailing spaces and dots are removed");
    check(name(u"abc", 0).empty(), "zero capacity produces no write");
    check(destination(u"C:\\Data\\报告.txt") && destination(u"e:/Downloads/emoji-\U0001f600.bin"), "absolute Unicode local paths accepted");
    for (auto path : { u"../x", u"C:x", u"\\\\host\\share\\x", u"C:\\", u"C:\\a\\..\\x", u"C:\\a\\.\\x", u"C:\\a\\\\x", u"C:\\a\\x.", u"C:\\a\\x ", u"C:\\a\\x:y", u"C:\\a\\*.bin", u"C:\\a\\\u202ex" })
        check(!destination(path), "relative, traversal, ambiguous or malformed destination rejected");
    check(!destination(std::u16string(257, u'x')), "native descriptor length limit enforced before copy");
    check(!isValidDownloadDestination(nullptr, 10), "missing destination rejected");
    const uint8_t latin1[] = { 'a', 0xe9, '.', 'b' };
    uint16_t output[4];
    check(makeDownloadFilename(latin1, 4, output, 4) == 4 && output[1] == 0xe9, "8-bit engine strings are sanitized without copying the whole name to UTF-16");
    std::printf("RESULT checks=%u failures=0\n", checks);
}
