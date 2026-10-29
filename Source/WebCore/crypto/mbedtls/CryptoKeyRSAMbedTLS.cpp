/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "config.h"
#include "CryptoKeyRSA.h"

#if ENABLE(WEB_CRYPTO) && USE(MBEDTLS)

#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyPair.h"
#include "CryptoKeyRSAComponents.h"
#include "ExceptionOr.h"
#include "MbedTLSRSADER.h"
#include "MbedTLSRSARandom.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/TypedArrayInlines.h>
#include <climits>
#include <mbedtls/pk.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

CryptoKeyRSA::CryptoKeyRSA(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, CryptoKeyType type, PlatformRSAKeyContainer&& key, bool extractable, CryptoKeyUsageBitmap usages)
    : CryptoKey(identifier, type, extractable, usages)
    , m_platformKey(WTF::move(key))
    , m_restrictedToSpecificHash(hasHash)
    , m_hash(hash)
{
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::create(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, const CryptoKeyRSAComponents& data, bool extractable, CryptoKeyUsageBitmap usages)
{
    using P = MbedTLS::RSAParameters;
    P parameters;
    parameters.values[P::N] = data.modulus().span();
    parameters.values[P::E] = data.exponent().span();
    parameters.isPrivate = data.type() == CryptoKeyRSAComponents::Type::Private;
    if (parameters.isPrivate) {
        parameters.values[P::D] = data.privateExponent().span();
        if (data.hasAdditionalPrivateKeyParameters()) {
            if (!data.otherPrimeInfos().isEmpty())
                return nullptr;
            parameters.hasCRT = true;
            parameters.values[P::P] = data.firstPrimeInfo().primeFactor.span();
            parameters.values[P::Q] = data.secondPrimeInfo().primeFactor.span();
            parameters.values[P::DP] = data.firstPrimeInfo().factorCRTExponent.span();
            parameters.values[P::DQ] = data.secondPrimeInfo().factorCRTExponent.span();
            parameters.values[P::QI] = data.secondPrimeInfo().factorCRTCoefficient.span();
        }
    }
    auto key = MbedTLS::importRSA(parameters);
    if (!key)
        return nullptr;
    return create(identifier, hash, hasHash, parameters.isPrivate ? CryptoKeyType::Private : CryptoKeyType::Public, WTF::move(key), extractable, usages);
}

bool CryptoKeyRSA::isRestrictedToHash(CryptoAlgorithmIdentifier& hash) const
{
    if (!m_restrictedToSpecificHash)
        return false;
    hash = m_hash;
    return true;
}

size_t CryptoKeyRSA::keySizeInBits() const
{
    return MbedTLS::rsaModulusBits(platformKey());
}

static std::optional<int> generationExponent(std::span<const uint8_t> bytes)
{
    unsigned value = 0;
    for (auto byte : bytes) {
        if (value > (static_cast<unsigned>(INT_MAX) - byte) / 256)
            return std::nullopt;
        value = value * 256 + byte;
    }
    if (value < 3 || !(value & 1))
        return std::nullopt;
    return static_cast<int>(value);
}

void CryptoKeyRSA::generatePair(CryptoAlgorithmIdentifier identifier, CryptoAlgorithmIdentifier hash, bool hasHash, unsigned modulusLength, const Vector<uint8_t>& exponentBytes, bool extractable, CryptoKeyUsageBitmap usages, KeyPairCallback&& callback, VoidCallback&& failureCallback, ScriptExecutionContext* context)
{
    auto exponent = generationExponent(exponentBytes.span());
    if (!context || !exponent || modulusLength < 128 || modulusLength > MBEDTLS_MPI_MAX_SIZE * 8 || modulusLength % 2) {
        failureCallback();
        return;
    }
    // A bounded serial queue keeps primality testing off the script thread.
    // No mutable RSA state is shared with other WebCrypto operations.
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("WebCrypto RSA key generation"_s));
    queue.get()->dispatch([identifier, hash, hasHash, modulusLength, exponent = *exponent, extractable, usages, contextIdentifier = context->identifier(), callback = WTF::move(callback), failureCallback = WTF::move(failureCallback)]() mutable {
        auto privateKey = MbedTLS::makeRSAKey();
        auto publicKey = MbedTLS::makeRSAKey();
        MbedTLS::MPI n, e;
        bool success = !mbedtls_rsa_gen_key(privateKey.get(), MbedTLS::rsaRandom, nullptr, modulusLength, exponent)
            && !mbedtls_rsa_check_privkey(privateKey.get())
            && !mbedtls_rsa_export(privateKey.get(), n.get(), nullptr, nullptr, nullptr, e.get())
            && !mbedtls_rsa_import(publicKey.get(), n.get(), nullptr, nullptr, nullptr, e.get())
            && !mbedtls_rsa_complete(publicKey.get())
            && !mbedtls_rsa_check_pubkey(publicKey.get());
        ScriptExecutionContext::postTaskTo(contextIdentifier, [identifier, hash, hasHash, extractable, usages, success, privateKey = WTF::move(privateKey), publicKey = WTF::move(publicKey), callback = WTF::move(callback), failureCallback = WTF::move(failureCallback)](auto&) mutable {
            if (!success) {
                failureCallback();
                return;
            }
            auto publicResult = create(identifier, hash, hasHash, CryptoKeyType::Public, WTF::move(publicKey), true, usages);
            auto privateResult = create(identifier, hash, hasHash, CryptoKeyType::Private, WTF::move(privateKey), extractable, usages);
            callback(CryptoKeyPair { WTF::move(publicResult), WTF::move(privateResult) });
        });
    });
}

static RefPtr<CryptoKeyRSA> importDER(CryptoAlgorithmIdentifier identifier, std::optional<CryptoAlgorithmIdentifier> hash, std::span<const uint8_t> data, bool isPrivate, bool extractable, CryptoKeyUsageBitmap usages)
{
    auto parameters = MbedTLS::RSADER::parse(data, isPrivate);
    if (!parameters)
        return nullptr;
    auto key = MbedTLS::importRSA(*parameters);
    if (!key)
        return nullptr;
    return CryptoKeyRSA::create(identifier, hash.value_or(CryptoAlgorithmIdentifier::SHA_1), hash.has_value(), isPrivate ? CryptoKeyType::Private : CryptoKeyType::Public, WTF::move(key), extractable, usages);
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importSpki(CryptoAlgorithmIdentifier identifier, std::optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&& data, bool extractable, CryptoKeyUsageBitmap usages)
{
    return importDER(identifier, hash, data.span(), false, extractable, usages);
}

RefPtr<CryptoKeyRSA> CryptoKeyRSA::importPkcs8(CryptoAlgorithmIdentifier identifier, std::optional<CryptoAlgorithmIdentifier> hash, Vector<uint8_t>&& data, bool extractable, CryptoKeyUsageBitmap usages)
{
    return importDER(identifier, hash, data.span(), true, extractable, usages);
}

static ExceptionOr<Vector<uint8_t>> exportDER(const mbedtls_rsa_context* rsa, bool isPrivate)
{
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    auto freePK = makeScopeExit([&] { mbedtls_pk_free(&pk); });
    if (mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA)) || mbedtls_rsa_copy(mbedtls_pk_rsa(pk), rsa))
        return Exception { ExceptionCode::OperationError };
    Vector<uint8_t> buffer(MbedTLS::RSADER::maximumDERSize);
    auto eraseBuffer = makeScopeExit([&] { mbedtls_platform_zeroize(buffer.mutableSpan().data(), buffer.size()); });
    int length = isPrivate ? mbedtls_pk_write_key_der(&pk, buffer.mutableSpan().data(), buffer.size()) : mbedtls_pk_write_pubkey_der(&pk, buffer.mutableSpan().data(), buffer.size());
    if (length <= 0 || static_cast<size_t>(length) > buffer.size())
        return Exception { ExceptionCode::OperationError };
    auto bytes = buffer.span().last(length);
    if (!isPrivate)
        return Vector<uint8_t>(bytes);
    auto wrapped = MbedTLS::RSADER::pkcs8(bytes);
    if (wrapped.empty())
        return Exception { ExceptionCode::OperationError };
    auto result = Vector<uint8_t>(std::span<const uint8_t>(wrapped));
    mbedtls_platform_zeroize(wrapped.data(), wrapped.size());
    return result;
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportSpki() const
{
    if (type() != CryptoKeyType::Public)
        return Exception { ExceptionCode::InvalidAccessError };
    return exportDER(platformKey(), false);
}

ExceptionOr<Vector<uint8_t>> CryptoKeyRSA::exportPkcs8() const
{
    if (type() != CryptoKeyType::Private)
        return Exception { ExceptionCode::InvalidAccessError };
    return exportDER(platformKey(), true);
}

static Vector<uint8_t> mpiBytes(const MbedTLS::MPI& value)
{
    Vector<uint8_t> result(mbedtls_mpi_size(value.get()));
    if (mbedtls_mpi_write_binary(value.get(), result.mutableSpan().data(), result.size()))
        return { };
    return result;
}

std::unique_ptr<CryptoKeyRSAComponents> CryptoKeyRSA::exportData() const
{
    MbedTLS::MPI n, e, d, p, q, dp, dq, qi;
    bool isPrivate = type() == CryptoKeyType::Private;
    if (mbedtls_rsa_export(platformKey(), n.get(), isPrivate ? p.get() : nullptr, isPrivate ? q.get() : nullptr, isPrivate ? d.get() : nullptr, e.get()))
        return nullptr;
    if (!isPrivate)
        return CryptoKeyRSAComponents::createPublic(mpiBytes(n), mpiBytes(e));
    if (mbedtls_rsa_export_crt(platformKey(), dp.get(), dq.get(), qi.get()))
        return nullptr;
    CryptoKeyRSAComponents::PrimeInfo first { mpiBytes(p), mpiBytes(dp), { } };
    CryptoKeyRSAComponents::PrimeInfo second { mpiBytes(q), mpiBytes(dq), mpiBytes(qi) };
    return CryptoKeyRSAComponents::createPrivateWithAdditionalData(mpiBytes(n), mpiBytes(e), mpiBytes(d), WTF::move(first), WTF::move(second), { });
}

auto CryptoKeyRSA::algorithm() const -> KeyAlgorithm
{
    MbedTLS::MPI exponent;
    if (mbedtls_rsa_export(platformKey(), nullptr, nullptr, nullptr, nullptr, exponent.get()))
        return CryptoRsaKeyAlgorithm { };
    auto bytes = mpiBytes(exponent);
    if (m_restrictedToSpecificHash) {
        CryptoRsaHashedKeyAlgorithm result;
        result.name = CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier());
        result.modulusLength = keySizeInBits();
        result.publicExponent = Uint8Array::tryCreate(bytes.span());
        result.hash.name = CryptoAlgorithmRegistry::singleton().name(m_hash);
        return result;
    }
    CryptoRsaKeyAlgorithm result;
    result.name = CryptoAlgorithmRegistry::singleton().name(algorithmIdentifier());
    result.modulusLength = keySizeInBits();
    result.publicExponent = Uint8Array::tryCreate(bytes.span());
    return result;
}

} // namespace WebCore

#endif
