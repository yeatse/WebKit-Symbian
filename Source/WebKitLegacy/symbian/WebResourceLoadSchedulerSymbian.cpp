/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "WebResourceLoadScheduler.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>

using namespace WebCore;

ResourceError WebResourceLoadScheduler::cancelledError(const ResourceRequest& request) const
{
    return { "WebKitErrorDomain"_s, -999, request.url(), "Load cancelled"_s, ResourceError::Type::Cancellation };
}

ResourceError WebResourceLoadScheduler::blockedErrorFromRequest(const ResourceRequest& request)
{
    return { "WebKitErrorDomain"_s, 103, request.url(), "Access to the address or port is restricted"_s, ResourceError::Type::AccessControl };
}

ResourceError WebResourceLoadScheduler::blockedError(const ResourceRequest& request) const
{
    return blockedErrorFromRequest(request);
}

ResourceError WebResourceLoadScheduler::blockedByContentBlockerError(const ResourceRequest& request) const
{
    return { "WebKitErrorDomain"_s, 104, request.url(), "Blocked by a content blocker"_s, ResourceError::Type::AccessControl };
}

ResourceError WebResourceLoadScheduler::cannotShowURLError(const ResourceRequest& request) const
{
    return { "WebKitErrorDomain"_s, 101, request.url(), "Unsupported URL"_s };
}

ResourceError WebResourceLoadScheduler::interruptedForPolicyChangeError(const ResourceRequest& request) const
{
    return { "WebKitErrorDomain"_s, 102, request.url(), "Load interrupted by policy"_s, ResourceError::Type::Cancellation };
}

ResourceError WebResourceLoadScheduler::cannotShowMIMETypeError(const ResourceResponse& response) const
{
    return { "WebKitErrorDomain"_s, 100, response.url(), "Unsupported content type"_s };
}

ResourceError WebResourceLoadScheduler::fileDoesNotExistError(const ResourceResponse& response) const
{
    return { "WebKitErrorDomain"_s, -1100, response.url(), "File does not exist"_s };
}

ResourceError WebResourceLoadScheduler::httpsUpgradeRedirectLoopError(const ResourceRequest& request) const
{
    return { "WebKitErrorDomain"_s, 304, request.url(), "HTTPS upgrade redirect loop"_s };
}

ResourceError WebResourceLoadScheduler::httpNavigationWithHTTPSOnlyError(const ResourceRequest& request) const
{
    return { "WebKitErrorDomain"_s, 305, request.url(), "HTTPS is required for this navigation"_s, ResourceError::Type::AccessControl };
}

ResourceError WebResourceLoadScheduler::pluginWillHandleLoadErrorFromResponse(const ResourceResponse& response)
{
    return { "WebKitErrorDomain"_s, 204, response.url(), "Load handled by a plug-in"_s, ResourceError::Type::Cancellation };
}

ResourceError WebResourceLoadScheduler::pluginWillHandleLoadError(const ResourceResponse& response) const
{
    return pluginWillHandleLoadErrorFromResponse(response);
}

#if ENABLE(CONTENT_FILTERING)
ResourceError WebResourceLoadScheduler::blockedByContentFilterError(const ResourceRequest& request) const
{
    return { "WebKitErrorDomain"_s, 104, request.url(), "Blocked by a content filter"_s, ResourceError::Type::AccessControl };
}
#endif
