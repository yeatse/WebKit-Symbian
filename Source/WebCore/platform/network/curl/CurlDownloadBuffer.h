/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <atomic>
#include <cstddef>

namespace WebCore {

// One curl worker produces chunks; the main thread consumes them. Reserve
// before allocating the queued buffer. A paused chunk remains with libcurl and
// is retried without being counted twice. A zero limit opts out completely.
class CurlDownloadBuffer {
public:
    enum class Reservation { Disabled, Accepted, Paused, TooLarge };

    // Configure before start or at the response pause, before any body data.
    bool setLimit(size_t limit)
    {
        if (m_pending.load())
            return false;
        m_limit = limit;
        return true;
    }

    size_t limit() const { return m_limit.load(); }
    size_t pending() const { return m_pending.load(); }
    bool full() const { auto maximum = limit(); return maximum && pending() >= maximum; }

    Reservation reserve(size_t size)
    {
        auto maximum = limit();
        if (!maximum)
            return Reservation::Disabled;
        if (size > maximum)
            return Reservation::TooLarge;
        if (pending() > maximum - size)
            return Reservation::Paused;
        // The only other thread subtracts, so this cannot exceed the limit
        // between the load and fetch_add. size > maximum was checked first.
        m_pending.fetch_add(size);
        return Reservation::Accepted;
    }

    bool release(size_t size)
    {
        auto value = pending();
        do {
            if (size > value)
                return false;
        } while (!m_pending.compare_exchange_weak(value, value - size));
        return true;
    }

private:
    std::atomic<size_t> m_limit { 0 };
    std::atomic<size_t> m_pending { 0 };
};

} // namespace WebCore
