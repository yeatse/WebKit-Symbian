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
#include "CryptoKeyEC.h"
#include "CryptoKeyRSA.h"

#include "CryptoAlgorithmAESCTR.h"
#include "CryptoAlgorithmECDH.h"
#include "CryptoAlgorithmECDSA.h"
#include "CryptoAlgorithmRSASSA_PKCS1_v1_5.h"
#include "CryptoAlgorithmRSA_OAEP.h"
#include "CryptoAlgorithmRSA_PSS.h"
#include "CryptoKeyRSAComponents.h"
#include "ExceptionOr.h"

namespace WebCore {

// These algorithms are deliberately absent from the registry. Reject key
// deserialization too, so unsupported keys cannot bypass that boundary.
bool CryptoKeyEC::platformSupportedCurve(NamedCurve) { return false; }
std::optional<CryptoKeyPair> CryptoKeyEC::platformGeneratePair(CryptoAlgorithmIdentifier, NamedCurve, bool, CryptoKeyUsageBitmap) { return std::nullopt; }
RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportRaw(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPublic(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPrivate(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, Vector<uint8_t>&&, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportSpki(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportPkcs8(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
Vector<uint8_t> CryptoKeyEC::platformExportRaw() const { return { }; }
Vector<uint8_t> CryptoKeyEC::platformExportSpki() const { return { }; }
Vector<uint8_t> CryptoKeyEC::platformExportPkcs8() const { return { }; }
bool CryptoKeyEC::platformAddFieldElements(JsonWebKey&) const { return false; }
size_t CryptoKeyEC::keySizeInBits() const { return 0; }

CryptoKeyRSA::CryptoKeyRSA(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType type, PlatformRSAKeyContainer&& key, bool extractable, CryptoKeyUsageBitmap usages)
    : CryptoKey(identifier, type, extractable, usages)
    , m_platformKey(WTF::move(key))
    , m_restrictedToSpecificHash(hasHash)
    , m_hash(hash)
{
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::create(CryptoAlgorithmIdentifier, CryptoAlgorithmIdentifier, bool, const CryptoKeyRSAComponents&, bool, CryptoKeyUsageBitmap) { return nullptr; }
void CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier, CryptoAlgorithmIdentifier, bool, unsigned, const Vector<uint8_t>&, bool, CryptoKeyUsageBitmap, KeyPairCallback&&, VoidCallback&& failureCallback, ScriptExecutionContext*)
{
    failureCallback();
}
RefPtr<CryptoKeyRSA> CryptoKeyRSA::importSpki(CryptoAlgorithmIdentifier, std::optional<CryptoAlgorithmIdentifier>, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
RefPtr<CryptoKeyRSA> CryptoKeyRSA::importPkcs8(CryptoAlgorithmIdentifier, std::optional<CryptoAlgorithmIdentifier>, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap) { return nullptr; }
ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportSpki() const { return Exception { ExceptionCode::NotSupportedError }; }
ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportPkcs8() const { return Exception { ExceptionCode::NotSupportedError }; }
std::unique_ptr<CryptoKeyRSAComponents> CryptoKeyRSA::exportData() const { return nullptr; }
size_t CryptoKeyRSA::keySizeInBits() const { return 0; }
bool CryptoKeyRSA::isRestrictedToHash(CryptoAlgorithmIdentifier&) const { return false; }
auto CryptoKeyRSA::algorithm() const -> KeyAlgorithm { RELEASE_ASSERT_NOT_REACHED(); }

std::optional<Vector<uint8_t>> CryptoAlgorithmECDH::platformDeriveBits(const CryptoKeyEC&, const CryptoKeyEC&) { return std::nullopt; }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCTR::platformEncrypt(const CryptoAlgorithmAesCtrParams&, const CryptoKeyAES&, const Vector<uint8_t>&) { return Exception { ExceptionCode::NotSupportedError }; }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAESCTR::platformDecrypt(const CryptoAlgorithmAesCtrParams&, const CryptoKeyAES&, const Vector<uint8_t>&) { return Exception { ExceptionCode::NotSupportedError }; }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmECDSA::platformSign(const CryptoAlgorithmEcdsaParams&, const CryptoKeyEC&, const Vector<uint8_t>&) { return Exception { ExceptionCode::NotSupportedError }; }
ExceptionOr<bool> CryptoAlgorithmECDSA::platformVerify(const CryptoAlgorithmEcdsaParams&, const CryptoKeyEC&, const Vector<uint8_t>&, const Vector<uint8_t>&) { return Exception { ExceptionCode::NotSupportedError }; }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformSign(const CryptoKeyRSA&, const Vector<uint8_t>&) { return Exception { ExceptionCode::NotSupportedError }; }
ExceptionOr<bool> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformVerify(const CryptoKeyRSA&, const Vector<uint8_t>&, const Vector<uint8_t>&) { return Exception { ExceptionCode::NotSupportedError }; }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformEncrypt(const CryptoAlgorithmRsaOaepParams&, const CryptoKeyRSA&, const Vector<uint8_t>&) { return Exception { ExceptionCode::NotSupportedError }; }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformDecrypt(const CryptoAlgorithmRsaOaepParams&, const CryptoKeyRSA&, const Vector<uint8_t>&) { return Exception { ExceptionCode::NotSupportedError }; }
ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_PSS::platformSign(const CryptoAlgorithmRsaPssParams&, const CryptoKeyRSA&, const Vector<uint8_t>&) { return Exception { ExceptionCode::NotSupportedError }; }
ExceptionOr<bool> CryptoAlgorithmRSA_PSS::platformVerify(const CryptoAlgorithmRsaPssParams&, const CryptoKeyRSA&, const Vector<uint8_t>&, const Vector<uint8_t>&) { return Exception { ExceptionCode::NotSupportedError }; }

} // namespace WebCore
