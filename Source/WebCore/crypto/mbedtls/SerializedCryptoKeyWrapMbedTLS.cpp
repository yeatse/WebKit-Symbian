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
#include "SerializedCryptoKeyWrap.h"

#include "WrappedCryptoKey.h"

namespace WebCore {

// Symbian has no secure system master-key store for this port.
std::optional<Vector<uint8_t>> defaultWebCryptoMasterKey()
{
    return std::nullopt;
}

bool deleteDefaultWebCryptoMasterKey()
{
    return false;
}

bool wrapSerializedCryptoKey(const Vector<uint8_t>&, const Vector<uint8_t>&, Vector<uint8_t>& result)
{
    result.clear();
    return false;
}

std::optional<WrappedCryptoKey> readSerializedCryptoKey(const Vector<uint8_t>&)
{
    return std::nullopt;
}

std::optional<Vector<uint8_t>> unwrapCryptoKey(const Vector<uint8_t>&, const WrappedCryptoKey&)
{
    return std::nullopt;
}

} // namespace WebCore
