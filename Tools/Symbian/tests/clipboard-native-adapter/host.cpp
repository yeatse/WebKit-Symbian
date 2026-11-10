#include "WebKitSymbianClipboard.h"
#include <cassert>
#include <cstdio>
#include <string>

using namespace WebKitSymbian;
using TInt = int;
using TUint16 = char16_t;
constexpr TInt EDriveA = 0, EDriveY = 24;
constexpr TInt KErrNotSupported = -5, KErrArgument = -6, KErrTooBig = -40;
namespace User {
[[noreturn]] void Leave(int error) { throw error; }
void LeaveIfError(int error) { if (error) Leave(error); }
}
namespace HAL {
constexpr int EClipboardDrive = 0;
int drive = 3, error = 0;
int Get(int, int& value) { value = drive; return error; }
}
class TDes {
public:
    explicit TDes(std::u16string& value) : m_value(value) { }
    TInt Length() const { return static_cast<TInt>(m_value.length()); }
    char16_t& operator[](TInt index) { return m_value.at(index); }
    void SetLength(TInt length) { m_value.resize(length); }
    void Append(char16_t character) { m_value.push_back(character); }
    const std::u16string& value() const { return m_value; }
private:
    std::u16string& m_value;
};
using TPtr = TDes;
class HBufC {
public:
    static HBufC* NewLC(TInt length) { ++allocations; auto* result = new HBufC; result->m_value.reserve(length); return result; }
    TPtr Des() { return TPtr(m_value); }
    static unsigned allocations;
private:
    std::u16string m_value;
};
unsigned HBufC::allocations;
struct Reader {
    TInt length;
    TInt ReadInt32L() { return length; }
};
#include "production-boundaries.inc"

static unsigned checks;
static void check(bool condition, const char* label)
{
    if (!condition) { std::fprintf(stderr, "FAIL %s\n", label); std::abort(); }
    ++checks;
    std::printf("PASS %s\n", label);
}
template<typename Function> static bool leaves(int expected, Function function)
{
    try { function(); } catch (int error) { return error == expected; }
    return false;
}
int main()
{
    for (auto source : { u"", u"\r", u"\r\n", u"\n", u"\u2028", u"\u2029" }) {
        std::u16string text(source); TDes descriptor(text); NormalizeForWeb(descriptor);
        check(text == (*source ? u"\n" : u""), "web normalization handles empty and each native line ending");
    }
    std::u16string text = u"你\U0001f642\r\nA\rB\nC\u2028D\u2029";
    TDes descriptor(text); NormalizeForWeb(descriptor);
    check(text == u"你\U0001f642\nA\nB\nC\nD\n", "web normalization preserves Unicode and surrogate code units");
    std::u16string embedded { u'A', 0, u'B' }; TDes embeddedDescriptor(embedded); NormalizeForWeb(embeddedDescriptor);
    check(embedded.size() == 3 && !embedded[1], "web normalization preserves embedded null code units");
    const std::u16string input = u"你\U0001f642\r\nA\rB\n";
    check(normalizedWrite({ reinterpret_cast<const uint16_t*>(input.data()), static_cast<int>(input.size()) }) == u"你\U0001f642\u2029A\u2029B\u2029", "native write normalization preserves Unicode and folds CRLF once");
    check(normalizedWrite({ nullptr, 0 }).empty(), "native empty string is admitted");
    unsigned allocations = HBufC::allocations;
    check(leaves(KErrArgument, [] { normalizedWrite({ nullptr, 1 }); }), "nonempty null pointer rejected");
    check(leaves(KErrArgument, [] { normalizedWrite({ nullptr, -1 }); }), "negative write length rejected");
    check(leaves(KErrArgument, [] { normalizedWrite({ reinterpret_cast<const uint16_t*>(1), maximumClipboardTextLength + 1 }); }), "oversized write rejected without dereferencing source");
    check(HBufC::allocations == allocations, "invalid writes are rejected before native allocation");
    for (int length : { 0, maximumClipboardTextLength }) { Reader reader { length }; checkSerializedLength(reader); check(true, "serialized text boundary accepted"); }
    for (int length : { -1, maximumClipboardTextLength + 1 }) { Reader reader { length }; check(leaves(KErrTooBig, [&] { checkSerializedLength(reader); }), "serialized text outside bounds rejected"); }
    checkStoreBounds(KMaximumStoreBytes, KMaximumFormats); check(true, "maximum store and dictionary counts accepted");
    check(leaves(KErrTooBig, [] { checkStoreBounds(KMaximumStoreBytes + 1, 1); }), "oversized physical store rejected");
    check(leaves(KErrTooBig, [] { checkStoreBounds(1, KMaximumFormats + 1); }), "oversized dictionary count rejected");
    for (int drive : { -1, 25 }) { HAL::drive = drive; check(leaves(KErrNotSupported, [] { CheckDriveL(); }), "unsupported drive rejected before panic-prone CClipboard calls"); }
    HAL::drive = 3; HAL::error = -18;
    check(leaves(-18, [] { CheckDriveL(); }), "HAL error propagates before native clipboard construction");
    std::printf("RESULT checks=%u failures=0\n", checks);
}
