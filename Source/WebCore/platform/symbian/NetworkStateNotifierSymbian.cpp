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

#include "config.h"
#include "NetworkStateNotifier.h"
#include "NetworkStateNotifierSymbianNative.h"

#include <wtf/MainThread.h>

namespace WebCore {

// NetworkStateNotifier is process-lived. The native observer uses the same UI
// active-scheduler thread; its callback only queues WebCore main-thread work.
static void* networkObserver;

void NetworkStateNotifier::updateStateWithoutNotifying()
{
    startObserving();
    bool online;
    if (SymbianNetworkState::state(networkObserver, online))
        m_isOnLine = online;
}

void NetworkStateNotifier::startObserving()
{
    ASSERT(isMainThread());
    if (networkObserver)
        return;
    networkObserver = SymbianNetworkState::createObserver([](void*) {
        callOnMainThread([] {
            singleton().updateStateSoon();
        });
    }, nullptr);
}

} // namespace WebCore
