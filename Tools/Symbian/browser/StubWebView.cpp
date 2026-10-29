// A stand-in for the engine, for trying the shell without WebKit: one text
// field that takes the input method's text, drawn as blocks, with every call
// logged through RDebug.

#include <WebKitSymbian.h>

#include <e32debug.h>
#include <e32std.h>
#include <string.h>

namespace WebKitSymbian {

namespace {

void (*gWakeUp)(void*);
void* gWakeUpContext;

class StubWebView final : public WebView {
public:
    StubWebView(WebViewClient& client, int width, int height)
        : m_client(client), m_width(width), m_height(height)
    {
    }

    void setSize(int width, int height) final
    {
        m_width = width;
        m_height = height;
        m_client.didInvalidate({ 0, 0, width, height });
    }

    void paint(uint8_t* pixels, int stride, const Rect& area) final
    {
        for (int y = area.y; y < area.y + area.height; ++y) {
            uint32_t* row = reinterpret_cast<uint32_t*>(pixels + y * stride);
            for (int x = area.x; x < area.x + area.width; ++x)
                row[x] = 0xff000000 | colorAt(x, y);
        }
    }

    void load(const char* url) final
    {
        RDebug::Printf("stub: load %s", url);
        TBuf<256> text;
        for (const char* c = url; *c && text.Length() < text.MaxLength(); ++c)
            text.Append(TChar(static_cast<unsigned char>(*c)));
        m_client.didChangeURL({ reinterpret_cast<const uint16_t*>(text.Ptr()), text.Length() });
        m_client.didChangeProgress(false, 1);
    }
    void reload() final { }
    void stop() final { }
    void goBack() final { }
    void goForward() final { }
    bool canGoBack() const final { return false; }
    bool canGoForward() const final { return false; }

    void setFocused(bool focused) final { RDebug::Printf("stub: focused %d", focused); }

    void handlePointer(PointerEvent event, int x, int y) final
    {
        if (event != PointerEvent::Up)
            return;
        bool inField = y >= fieldTop && y < fieldTop + fieldHeight;
        RDebug::Printf("stub: tap %d,%d field %d", x, y, inField);
        if (inField != m_editing) {
            m_editing = inField;
            m_client.didChangeEditingState();
        }
    }

    void scrollBy(int, int) final { }

    bool handleKey(bool isDown, int keyCode, Text text, unsigned) final
    {
        if (!isDown || !m_editing)
            return false;
        RDebug::Printf("stub: key %d text %d", keyCode, text.length ? text.characters[0] : 0);
        if (keyCode == 8) {
            if (m_cursor > 0) {
                memmove(m_text + m_cursor - 1, m_text + m_cursor, (m_length - m_cursor) * 2);
                --m_length;
                --m_cursor;
            }
        } else if (text.length && text.characters[0] >= ' ')
            insert(text);
        changed();
        return true;
    }

    InputType focusedInputType() const final { return m_editing ? InputType::Text : InputType::None; }
    int editableTextLength() const final { return m_length; }
    int maximumEditableTextLength() const final { return capacity; }
    int copyEditableText(int start, int length, uint16_t* buffer) const final
    {
        if (start >= m_length)
            return 0;
        int count = length < m_length - start ? length : m_length - start;
        memcpy(buffer, m_text + start, count * 2);
        return count;
    }
    void selection(int& anchor, int& focus) const final { anchor = focus = m_cursor; }
    void setSelection(int, int focus) final { m_cursor = focus < 0 ? 0 : (focus > m_length ? m_length : focus); }

    void setComposition(Text text, int cursor) final
    {
        RDebug::Printf("stub: composition length %d cursor %d", text.length, cursor);
        removeComposition();
        m_compositionStart = m_cursor;
        insert(text);
        m_compositionLength = text.length;
        changed();
    }

    void confirmComposition(Text text) final
    {
        RDebug::Printf("stub: confirm length %d", text.length);
        removeComposition();
        insert(text);
        changed();
    }

    void cancelComposition() final
    {
        removeComposition();
        changed();
    }

    Rect caretRect() const final { return { 8 + m_cursor * 12, fieldTop + 4, 2, fieldHeight - 8 }; }

private:
    static const int capacity = 256;
    static const int fieldTop = 40;
    static const int fieldHeight = 40;

    void insert(Text text)
    {
        int count = text.length;
        if (m_length + count > capacity)
            count = capacity - m_length;
        memmove(m_text + m_cursor + count, m_text + m_cursor, (m_length - m_cursor) * 2);
        memcpy(m_text + m_cursor, text.characters, count * 2);
        m_length += count;
        m_cursor += count;
    }

    void removeComposition()
    {
        if (!m_compositionLength)
            return;
        memmove(m_text + m_compositionStart, m_text + m_compositionStart + m_compositionLength,
            (m_length - m_compositionStart - m_compositionLength) * 2);
        m_length -= m_compositionLength;
        m_cursor = m_compositionStart;
        m_compositionLength = 0;
    }

    void changed() { m_client.didInvalidate({ 0, fieldTop, m_width, fieldHeight }); }

    uint32_t colorAt(int x, int y) const
    {
        if (y < fieldTop || y >= fieldTop + fieldHeight || x < 4 || x >= m_width - 4)
            return 0xffffff;
        if (y == fieldTop || y == fieldTop + fieldHeight - 1)
            return m_editing ? 0x307cf0 : 0x888888;
        int index = (x - 8) / 12;
        if (x >= 8 && index < m_length && (x - 8) % 12 < 10 && y > fieldTop + 8 && y < fieldTop + fieldHeight - 8) {
            bool composing = index >= m_compositionStart && index < m_compositionStart + m_compositionLength;
            return composing ? 0xd04040 : 0x202020;
        }
        return 0xffffff;
    }

    WebViewClient& m_client;
    int m_width;
    int m_height;
    bool m_editing { false };
    uint16_t m_text[capacity];
    int m_length { 0 };
    int m_cursor { 0 };
    int m_compositionStart { 0 };
    int m_compositionLength { 0 };
};

} // namespace

void WebView::initializeProcess(const char*) { }

void WebView::setWakeUpCallback(void (*wakeUp)(void*), void* context)
{
    gWakeUp = wakeUp;
    gWakeUpContext = context;
}

void WebView::runPendingWork() { }

int64_t WebView::microsecondsUntilNextTimer() { return -1; }


WebView* WebView::create(WebViewClient& client, int width, int height)
{
    return new StubWebView(client, width, height);
}

} // namespace WebKitSymbian
