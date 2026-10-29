/*
 * Copyright (C) 2026 The symbian-webkit authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// The embedding interface of the Symbian port. It uses neither WebCore nor
// Symbian types, so a native AVKON application can include it next to the
// OS headers. Everything runs on the thread that called initializeProcess();
// coordinates are view pixels, text is UTF-16.

#pragma once

#include <stdint.h>

namespace WebKitSymbian {

struct Rect {
    int x { 0 };
    int y { 0 };
    int width { 0 };
    int height { 0 };
};

struct Text {
    const uint16_t* characters { nullptr };
    int length { 0 };
};

// What kind of field has the focus, for choosing the input mode.
enum class InputType { None, Text, Password, Number, Telephone, Email, URL, Search, MultiLine };

enum class PointerEvent { Down, Move, Up };

enum Modifier : unsigned {
    ShiftKey = 1 << 0,
    ControlKey = 1 << 1,
    AltKey = 1 << 2,
};

class WebViewClient {
public:
    // The area needs painting; call WebView::paint() for it soon.
    virtual void didInvalidate(const Rect&) = 0;
    virtual void didChangeTitle(Text) { }
    virtual void didChangeURL(Text) { }
    // Progress is from 0 to 1.
    virtual void didChangeProgress(bool /* isLoading */, double /* progress */) { }
    // The focus moved into or out of an editable field.
    virtual void didChangeEditingState() { }
    // The text or the selection of the focused field changed, other than
    // through the composition calls below.
    virtual void didChangeSelection() { }
    virtual void runJavaScriptAlert(Text /* message */) { }
    virtual bool runJavaScriptConfirm(Text /* message */) { return false; }
    // A <select> was tapped: show the items and return the chosen index, or
    // -1 if the choice was cancelled.
    virtual int runSelectPopup(const Text* /* items */, int /* count */, int /* selectedIndex */) { return -1; }

protected:
    ~WebViewClient() = default;
};

// Native MMF media callbacks run on the application's event loop. A backend
// must cancel its callbacks before it is destroyed. Errors are native TInt
// values (zero means success); dimensions are zero for an audio-only file.
class MediaPlayerClient {
public:
    virtual ~MediaPlayerClient() = default;
    virtual void mediaPlayerOpened(int error, double duration, int width, int height, bool hasAudio) = 0;
    virtual void mediaPlayerEnded(int error) = 0;
};

class MediaPlayer {
public:
    virtual ~MediaPlayer() = default;
    // WebCore owns network policy and downloading. Native players receive
    // local files; the writer must make appended bytes visible to the reader.
    virtual int createFile(const char* utf8Path) = 0;
    virtual int appendToFile(const char*, unsigned length) = 0;
    virtual int flushFile() = 0;
    virtual void closeFile() = 0;
    virtual void open(const char* utf8Path) = 0;
    virtual int play() = 0;
    virtual int pause() = 0;
    virtual int position(double& seconds) const = 0;
    virtual int setPosition(double seconds) = 0;
    virtual int setVolume(double volume) = 0;
    // Window-relative coordinates; an empty clip hides the native surface.
    virtual int setVideoRect(const Rect&, const Rect&) { return 0; }
};

class WebView {
public:
    // Once per process, before the first WebView. dataDirectory holds the
    // cookie database and the caches.
    static void initializeProcess(const char* dataDirectory);

    // The engine's timers and tasks run when the application calls
    // runPendingWork(). wakeUp is called, from any thread, whenever there is
    // new work; it must only signal the application's event loop.
    static void setWakeUpCallback(void (*wakeUp)(void* context), void* context);
    static void runPendingWork();
    // Microseconds until the next timer is due, or -1 if none is scheduled.
    static int64_t microsecondsUntilNextTimer();

    using MediaPlayerFactory = MediaPlayer* (*)(void* context, MediaPlayerClient&, bool video);
    static void setMediaPlayerFactory(MediaPlayerFactory, void* context);

    static WebView* create(WebViewClient&, int width, int height);
    virtual ~WebView() = default;

    virtual void setSize(int width, int height) = 0;
    // Native RWindow lifetime extends until setNativeWindow(nullptr).
    virtual bool setNativeWindow(void*) { return false; }
    virtual bool usesAcceleratedCompositing() const { return false; }
    virtual void renderCompositedFrame() { }
    // Paints area into a 32-bit buffer of premultiplied ARGB words
    // (0xAARRGGBB) that covers the whole view.
    virtual void paint(uint8_t* pixels, int stride, const Rect& area) = 0;

    // url is UTF-8; text without a scheme is treated as http://.
    virtual void load(const char* url) = 0;
    virtual void reload() = 0;
    virtual void stop() = 0;
    virtual void goBack() = 0;
    virtual void goForward() = 0;
    virtual bool canGoBack() const = 0;
    virtual bool canGoForward() const = 0;

    virtual void setFocused(bool) = 0;
    // Taps become clicks; drags scroll.
    virtual void handlePointer(PointerEvent, int x, int y) = 0;
    virtual void scrollBy(int dx, int dy) = 0;
    // windowsKeyCode is a Windows virtual key code (VK_*), text the characters
    // the key produces. Returns whether the page handled the key.
    virtual bool handleKey(bool isDown, int windowsKeyCode, Text, unsigned modifiers) = 0;

    // Editing in the focused field, for the input method. Offsets are UTF-16
    // positions in the field's text, which includes the composition.
    virtual InputType focusedInputType() const = 0;
    virtual int editableTextLength() const = 0;
    virtual int maximumEditableTextLength() const = 0;
    // Copies up to length characters from start and returns how many it copied.
    virtual int copyEditableText(int start, int length, uint16_t* buffer) const = 0;
    virtual void selection(int& anchor, int& focus) const = 0;
    virtual void setSelection(int anchor, int focus) = 0;
    // Replaces the selection, or the current composition, with marked text;
    // cursor is a position within that text.
    virtual void setComposition(Text, int cursor) = 0;
    // Replaces the composition with text and ends it.
    virtual void confirmComposition(Text) = 0;
    // Removes the composition.
    virtual void cancelComposition() = 0;
    // The caret in view coordinates.
    virtual Rect caretRect() const = 0;
};

} // namespace WebKitSymbian
