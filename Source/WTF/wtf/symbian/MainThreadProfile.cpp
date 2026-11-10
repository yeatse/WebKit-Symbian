/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "config.h"
#include <wtf/symbian/MainThreadProfile.h>

#include <wtf/DataLog.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/text/CString.h>

namespace WTF::MainThreadProfile {

bool s_enabled;

namespace {

constexpr const char* categoryNames[categoryCount] = { "other", "script", "style", "layout", "parse", "paint", "decode", "gc", "frame", "jsparse", "bytecode", "jit" };

// A task this long or longer is logged on its own.
constexpr double longTaskMilliseconds = 100;

struct Entry {
    Category category;
    MonotonicTime start;
    const char* label;
    String detail;
};

// Main thread only: every scope checks isMainThread() before it is counted.
struct State {
    static constexpr unsigned maximumDepth = 64;
    Entry stack[maximumDepth];
    unsigned depth { 0 };
    unsigned overflow { 0 };
    // Start of the time not yet charged to the innermost open scope.
    MonotonicTime mark;
    double totals[categoryCount] { };

    // The task in progress.
    MonotonicTime taskStart;
    double taskStartTotals[categoryCount] { };
    double topScriptMilliseconds { 0 };
    const char* topScriptLabel { nullptr };
    String topScriptDetail;
    unsigned scriptDepth { 0 };

    // The summary in progress.
    MonotonicTime windowStart;
    double windowStartTotals[categoryCount] { };
    unsigned windowTasks { 0 };
    unsigned windowLongTasks { 0 };
    double windowLongest { 0 };

    const char* pendingLabel { nullptr };
    String pendingDetail;
};

State& state()
{
    static State* instance = new State;
    return *instance;
}

void charge(State& profile, MonotonicTime now)
{
    if (profile.depth)
        profile.totals[static_cast<unsigned>(profile.stack[profile.depth - 1].category)] += (now - profile.mark).milliseconds();
    profile.mark = now;
}

struct CompileStats {
    unsigned count { 0 };
    unsigned onMainThread { 0 };
    double wait { 0 };
    double maximumWait { 0 };
    double compile { 0 };
};

Lock s_compileLock;
CompileStats s_compileStats[2] WTF_GUARDED_BY_LOCK(s_compileLock);

void logCompileStats()
{
    CompileStats stats[2];
    {
        Locker locker { s_compileLock };
        std::copy(std::begin(s_compileStats), std::end(s_compileStats), stats);
        s_compileStats[0] = { };
        s_compileStats[1] = { };
    }
    if (!stats[0].count && !stats[1].count)
        return;
    // wait: total and longest time plans spent queued before compiling.
    WTFLogAlways("jit: baseline n=%u main=%u wait=%.0f/%.0fms compile=%.0fms dfg n=%u main=%u wait=%.0f/%.0fms compile=%.0fms",
        stats[0].count, stats[0].onMainThread, stats[0].wait, stats[0].maximumWait, stats[0].compile,
        stats[1].count, stats[1].onMainThread, stats[1].wait, stats[1].maximumWait, stats[1].compile);
}

void formatBreakdown(char* buffer, size_t size, const double* totals, const double* since)
{
    size_t used = 0;
    buffer[0] = 0;
    for (unsigned i = 0; i < categoryCount && used < size; ++i) {
        int written = snprintf(buffer + used, size - used, "%s%s=%.0f", used ? " " : "", categoryNames[i], totals[i] - since[i]);
        if (written < 0)
            break;
        used += written;
    }
}

void endTask(State& profile, MonotonicTime now)
{
    double duration = (now - profile.taskStart).milliseconds();
    ++profile.windowTasks;
    if (duration > profile.windowLongest)
        profile.windowLongest = duration;
    if (duration >= longTaskMilliseconds) {
        ++profile.windowLongTasks;
        char breakdown[280];
        formatBreakdown(breakdown, sizeof(breakdown), profile.totals, profile.taskStartTotals);
        if (profile.topScriptLabel) {
            CString detail = profile.topScriptDetail.utf8();
            // The browser log truncates long lines: keep the URL's start.
            WTFLogAlways("long-task: %.0fms %s top-script=%.0fms %s %.120s", duration, breakdown,
                profile.topScriptMilliseconds, profile.topScriptLabel, detail.data());
        } else
            WTFLogAlways("long-task: %.0fms %s", duration, breakdown);
    }
    profile.topScriptMilliseconds = 0;
    profile.topScriptLabel = nullptr;
    profile.topScriptDetail = String();

    double window = (now - profile.windowStart).milliseconds();
    if (window < 1000)
        return;
    double busy = 0;
    for (unsigned i = 0; i < categoryCount; ++i)
        busy += profile.totals[i] - profile.windowStartTotals[i];
    char breakdown[280];
    formatBreakdown(breakdown, sizeof(breakdown), profile.totals, profile.windowStartTotals);
    WTFLogAlways("main-thread: window=%.0fms busy=%.0fms tasks=%u long=%u longest=%.0fms %s", window, busy,
        profile.windowTasks, profile.windowLongTasks, profile.windowLongest, breakdown);
    logCompileStats();
    profile.windowStart = now;
    std::copy(std::begin(profile.totals), std::end(profile.totals), profile.windowStartTotals);
    profile.windowTasks = 0;
    profile.windowLongTasks = 0;
    profile.windowLongest = 0;
}

} // namespace

void setEnabled(bool enabled)
{
    if (!isMainThread() || enabled == s_enabled)
        return;
    auto& profile = state();
    // Scopes open now were not counted: start over from no open scope.
    profile.depth = 0;
    profile.overflow = 0;
    profile.scriptDepth = 0;
    profile.windowStart = MonotonicTime::now();
    std::copy(std::begin(profile.totals), std::end(profile.totals), profile.windowStartTotals);
    profile.windowTasks = 0;
    profile.windowLongTasks = 0;
    profile.windowLongest = 0;
    s_enabled = enabled;
}

const char* categoryName(unsigned category)
{
    return category < categoryCount ? categoryNames[category] : "";
}

void recordCompile(CompileTier tier, double waitMilliseconds, double compileMilliseconds, bool onMainThread)
{
    if (!enabled())
        return;
    Locker locker { s_compileLock };
    auto& stats = s_compileStats[static_cast<unsigned>(tier)];
    ++stats.count;
    if (onMainThread)
        ++stats.onMainThread;
    stats.wait += waitMilliseconds;
    stats.maximumWait = std::max(stats.maximumWait, waitMilliseconds);
    stats.compile += compileMilliseconds;
}

Totals totals()
{
    Totals result;
    if (!enabled() || !isMainThread())
        return result;
    auto& profile = state();
    charge(profile, MonotonicTime::now());
    std::copy(std::begin(profile.totals), std::end(profile.totals), result.milliseconds);
    return result;
}

void push(Category category, const char* label)
{
    auto& profile = state();
    if (profile.depth == State::maximumDepth) {
        ++profile.overflow;
        return;
    }
    MonotonicTime now = MonotonicTime::now();
    charge(profile, now);
    if (!profile.depth) {
        profile.taskStart = now;
        std::copy(std::begin(profile.totals), std::end(profile.totals), profile.taskStartTotals);
    }
    auto& entry = profile.stack[profile.depth++];
    entry.category = category;
    entry.start = now;
    entry.label = label;
    entry.detail = String();
    if (category == Category::Script) {
        if (!profile.scriptDepth++ && !label) {
            entry.label = profile.pendingLabel ? profile.pendingLabel : "call";
            entry.detail = profile.pendingDetail;
        }
    }
}

void pop(const String& detail)
{
    auto& profile = state();
    if (profile.overflow) {
        --profile.overflow;
        return;
    }
    // setEnabled() dropped scopes that were open then.
    if (!profile.depth)
        return;
    MonotonicTime now = MonotonicTime::now();
    charge(profile, now);
    auto& entry = profile.stack[--profile.depth];
    if (entry.category == Category::Script && profile.scriptDepth && !--profile.scriptDepth) {
        double inclusive = (now - entry.start).milliseconds();
        if (inclusive > profile.topScriptMilliseconds) {
            profile.topScriptMilliseconds = inclusive;
            profile.topScriptLabel = entry.label;
            profile.topScriptDetail = detail.isEmpty() ? entry.detail : detail;
        }
    }
    entry.detail = String();
    if (!profile.depth)
        endTask(profile, now);
}

ScriptLabel::ScriptLabel(const char* label, const String& detail)
    : m_active(enabled() && isMainThread())
{
    if (!m_active)
        return;
    auto& profile = state();
    m_previousLabel = std::exchange(profile.pendingLabel, label);
    m_previousDetail = std::exchange(profile.pendingDetail, detail);
}

ScriptLabel::~ScriptLabel()
{
    if (!m_active)
        return;
    auto& profile = state();
    profile.pendingLabel = m_previousLabel;
    profile.pendingDetail = WTF::move(m_previousDetail);
}

} // namespace WTF::MainThreadProfile
