// Keep native filesystem paths as UTF-16. Open C's narrow path conversion does
// not preserve supplementary characters on the measured Belle runtime.
#pragma once

#include <wtf/Platform.h>

#if OS(SYMBIAN)
#include <array>
#include <errno.h>
#include <wchar.h>
#include <wtf/text/StringView.h>

namespace WTF::FileSystemImpl {

class SymbianFilePath {
public:
    static constexpr unsigned maximumLength = 256; // Native KMaxFileName.

    explicit SymbianFilePath(StringView path)
    {
        static_assert(sizeof(wchar_t) == 2 && alignof(wchar_t) == 2);
        if (path.isEmpty()) {
            errno = ENOENT;
            return;
        }
        if (path.length() > maximumLength) {
            errno = ENAMETOOLONG;
            return;
        }
        for (unsigned i = 0; i < path.length(); ++i) {
            auto character = path[i];
            if (!character) {
                errno = EINVAL;
                return;
            }
            m_characters[i] = static_cast<wchar_t>(character);
        }
        // Preserve every UTF-16 code unit, including both surrogate halves.
        // Storage stays local until the synchronous Open C call has returned.
        m_characters[path.length()] = 0;
        m_valid = true;
    }

    explicit operator bool() const { return m_valid; }
    const wchar_t* data() const { return m_valid ? m_characters.data() : nullptr; }

private:
    std::array<wchar_t, maximumLength + 1> m_characters;
    bool m_valid { false };
};

} // namespace WTF::FileSystemImpl
#endif // OS(SYMBIAN)
