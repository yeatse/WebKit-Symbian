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
#include "ExceptionOr.h"
#include "CryptoAlgorithmRegistry.h"

#if ENABLE(WEB_CRYPTO) && USE(MBEDTLS)

#include "CryptoAlgorithmAESCBC.h"
#include "CryptoAlgorithmAESCTR.h"
#include "CryptoAlgorithmAESGCM.h"
#include "CryptoAlgorithmAESKW.h"
#include "CryptoAlgorithmHKDF.h"
#include "CryptoAlgorithmHMAC.h"
#include "CryptoAlgorithmPBKDF2.h"
#include "CryptoAlgorithmSHA1.h"
#include "CryptoAlgorithmSHA256.h"
#include "CryptoAlgorithmSHA384.h"
#include "CryptoAlgorithmSHA512.h"

namespace WebCore {

void CryptoAlgorithmRegistry::platformRegisterAlgorithms()
{
    // SubtleCrypto in WebKit 2.52 rejects the deprecated AES-CFB-8 API.
    registerAlgorithm<CryptoAlgorithmAESCBC>();
    registerAlgorithm<CryptoAlgorithmAESCTR>();
    registerAlgorithm<CryptoAlgorithmAESGCM>();
    registerAlgorithm<CryptoAlgorithmAESKW>();
    registerAlgorithm<CryptoAlgorithmHKDF>();
    registerAlgorithm<CryptoAlgorithmHMAC>();
    registerAlgorithm<CryptoAlgorithmPBKDF2>();
    registerAlgorithm<CryptoAlgorithmSHA1>();
    registerAlgorithm<CryptoAlgorithmSHA256>();
    registerAlgorithm<CryptoAlgorithmSHA384>();
    registerAlgorithm<CryptoAlgorithmSHA512>();
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO) && USE(MBEDTLS)
