/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <wtf/ExportMacros.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

// Where the main thread's time goes, for frame-stats. Scopes mark script,
// style, layout and the other kinds of work; time is charged to the innermost
// open scope only, so nested work is not counted twice. A task is everything
// between an outermost scope's start and end; a task of 100 ms or more is
// logged with its breakdown, and about once a second a summary of the
// thread's busy time is. Off, a scope costs one load of a flag.

namespace WTF::MainThreadProfile {

enum class Category : uint8_t {
    Other, // Inside a task, outside every scope below (run-loop tasks, input).
    Script,
    Style,
    Layout,
    Parse,
    Paint,
    Decode,
    GC,
    Frame, // The compositor's own work in a frame: layer flush, tiles, GL.
    // Script work that is not running the page's code:
    ScriptParse,
    Bytecode, // Generating bytecode from the syntax tree.
    JIT, // Compiling on the main thread.
    // Work that is not script, formerly all "other":
    Network, // Handling responses and data from the network thread.
    Media, // The media player: writing downloaded media to its file.
    Timers, // Engine timers that none of the kinds above account for.
    Count
};

constexpr unsigned categoryCount = static_cast<unsigned>(Category::Count);

WTF_EXPORT_PRIVATE extern bool s_enabled;
inline bool enabled() { return s_enabled; }
WTF_EXPORT_PRIVATE void setEnabled(bool);
WTF_EXPORT_PRIVATE const char* categoryName(unsigned);

// A JIT compilation, from any thread: how long the plan waited for a
// compiler thread and how long compiling took. Compiler threads run below
// the main thread, so a busy page can leave its code in the interpreter.
enum class CompileTier : uint8_t { Baseline, DFG };
WTF_EXPORT_PRIVATE void recordCompile(CompileTier, double waitMilliseconds, double compileMilliseconds, bool onMainThread);

// Milliseconds charged to each category since the profile was enabled,
// open scopes included up to now.
struct Totals {
    double milliseconds[categoryCount] { };
    double operator[](Category category) const { return milliseconds[static_cast<unsigned>(category)]; }
};
WTF_EXPORT_PRIVATE Totals totals();

WTF_EXPORT_PRIVATE void push(Category, const char* label);
WTF_EXPORT_PRIVATE void pop(const String& detail);

class Scope {
    WTF_MAKE_NONCOPYABLE(Scope);
public:
    // A script scope names the slowest script of a long task: label says
    // what ran it ("eval", "event", "timer"), detail a URL or event type.
    explicit Scope(Category category, const char* label = nullptr)
        : m_active(enabled() && isMainThread())
    {
        if (m_active)
            push(category, label);
    }
    ~Scope()
    {
        if (m_active)
            pop(m_detail);
    }
    bool active() const { return m_active; }
    void setDetail(const String& detail)
    {
        if (m_active)
            m_detail = detail;
    }

private:
    bool m_active;
    String m_detail;
};

// Names the script that the next script scope inside it runs, for callers
// that know more than the JS entry point does (an event's type, a timer).
class ScriptLabel {
    WTF_MAKE_NONCOPYABLE(ScriptLabel);
public:
    WTF_EXPORT_PRIVATE ScriptLabel(const char* label, const String& detail = { });
    WTF_EXPORT_PRIVATE ~ScriptLabel();

private:
    bool m_active;
    const char* m_previousLabel { nullptr };
    String m_previousDetail;
};

} // namespace WTF::MainThreadProfile
