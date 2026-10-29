/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include "Cookie.h"
#include "SameSiteInfo.h"

namespace WebCore {

// Deliberately no "Lax-allowing-unsafe" two-minute exception. Cookies without
// a recognized SameSite attribute, including migrated rows, use Lax.
inline Cookie::SameSitePolicy cookieSameSitePolicyFromDatabase(int value)
{
    switch (value) {
    case static_cast<int>(Cookie::SameSitePolicy::None):
        return Cookie::SameSitePolicy::None;
    case static_cast<int>(Cookie::SameSitePolicy::Strict):
        return Cookie::SameSitePolicy::Strict;
    default:
        return Cookie::SameSitePolicy::Lax;
    }
}

inline bool sameSiteCookieMayBeSent(Cookie::SameSitePolicy policy, const SameSiteInfo& context, bool forHTTPHeader)
{
    if (policy == Cookie::SameSitePolicy::None || context.isSameSite)
        return true;
    // document.cookie and WebSockets are not top-level navigations.
    return policy == Cookie::SameSitePolicy::Lax && forHTTPHeader && context.isTopSite && context.isSafeHTTPMethod;
}

inline bool sameSiteCookieMayBeSet(Cookie::SameSitePolicy policy, const SameSiteInfo& context, bool fromHTTPResponse)
{
    return policy == Cookie::SameSitePolicy::None || context.isSameSite || (fromHTTPResponse && context.isTopSite);
}

} // namespace WebCore
