// Explicit host StringView view adapter: same Latin1/UTF16 code-unit access.
#pragma once
#include <string_view>
namespace WTF {
class StringView {
public:
    StringView(std::string_view view) : m_latin1(view), m_is8Bit(true) { }
    StringView(std::u16string_view view) : m_utf16(view) { }
    bool isEmpty() const { return !length(); }
    unsigned length() const { return m_is8Bit ? m_latin1.size() : m_utf16.size(); }
    char16_t operator[](unsigned i) const { return m_is8Bit ? static_cast<unsigned char>(m_latin1[i]) : m_utf16[i]; }
private:
    std::string_view m_latin1;
    std::u16string_view m_utf16;
    bool m_is8Bit { false };
};
}
