// Exercise the production EC key, DER and operation helpers with fixed WPT vectors.
#include "MbedTLSECDER.h"
#include "MbedTLSECOperations.h"
#include "vectors.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <stdexcept>
#include <sys/random.h>

using namespace WebCore::MbedTLS;
static unsigned checks;
static void check(bool condition, const char* label)
{
    if (!condition)
        throw std::runtime_error(label);
    ++checks;
    printf("PASS %s\n", label);
}
static std::vector<uint8_t> unhex(const char* hex)
{
    std::vector<uint8_t> result;
    auto digit = [](char value) -> unsigned {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        throw std::runtime_error("invalid fixture hex");
    };
    for (size_t index = 0; hex[index]; index += 2)
        result.push_back(digit(hex[index]) * 16 + digit(hex[index + 1]));
    return result;
}
static int randomBytes(void*, unsigned char* data, size_t size)
{
    while (size) {
        auto count = getrandom(data, size, 0);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return -1;
        data += count;
        size -= count;
    }
    return 0;
}
static int failingRandom(void*, unsigned char*, size_t) { return -1; }
static mbedtls_md_type_t hashType(const char* name)
{
    if (!strcmp(name, "SHA-1")) return MBEDTLS_MD_SHA1;
    if (!strcmp(name, "SHA-256")) return MBEDTLS_MD_SHA256;
    if (!strcmp(name, "SHA-384")) return MBEDTLS_MD_SHA384;
    if (!strcmp(name, "SHA-512")) return MBEDTLS_MD_SHA512;
    throw std::runtime_error("unknown fixture hash");
}
static mbedtls_ecp_group_id curveType(const char* name)
{
    if (!strcmp(name, "P-256")) return MBEDTLS_ECP_DP_SECP256R1;
    if (!strcmp(name, "P-384")) return MBEDTLS_ECP_DP_SECP384R1;
    if (!strcmp(name, "P-521")) return MBEDTLS_ECP_DP_SECP521R1;
    throw std::runtime_error("unknown fixture curve");
}

int main()
{
    try {
        for (const auto& vector : ecdsaVectors) {
            printf("VECTOR %s\n", vector.name);
            auto curve = curveType(vector.curve);
            auto spki = unhex(vector.spki), pkcs8 = unhex(vector.pkcs8);
            auto publicKey = ECDER::importKey(curve, spki, false, randomBytes);
            auto privateKey = ECDER::importKey(curve, pkcs8, true, randomBytes);
            check(publicKey && privateKey, "ECDSA known SPKI and PKCS8 accepted");
            auto message = unhex(vector.data), expected = unhex(vector.signature);
            auto hash = hashType(vector.hash);
            check(ecVerify(*publicKey, hash, expected, message) == true, "ECDSA accepts independent WPT raw r||s signature");
            auto signedData = ecSign(*privateKey, hash, message, randomBytes);
            check(signedData && signedData->size() == expected.size() && ecVerify(*publicKey, hash, *signedData, message) == true, "ECDSA signs and verifies fixed-width raw r||s");
            auto changed = message; changed[0] ^= 1;
            check(ecVerify(*publicKey, hash, expected, changed) == false, "ECDSA rejects changed message");
            changed = expected; changed[changed.size() / 2] ^= 1;
            check(ecVerify(*publicKey, hash, changed, message) == false, "ECDSA rejects changed signature");
            changed = expected; changed.pop_back();
            check(ecVerify(*publicKey, hash, changed, message) == false, "ECDSA rejects short raw signature");
            changed = expected; changed.push_back(0);
            check(ecVerify(*publicKey, hash, changed, message) == false, "ECDSA rejects long raw signature");
            changed.assign(expected.size(), 0);
            check(ecVerify(*publicKey, hash, changed, message) == false, "ECDSA rejects zero r and s");
            changed.assign(expected.size(), 0xff);
            check(ecVerify(*publicKey, hash, changed, message) == false, "ECDSA rejects out-of-range r and s");
            check(ecVerify(*publicKey, hash, { }, message) == false, "ECDSA rejects empty signature");
        }

        for (const auto& vector : ecdhVectors) {
            printf("VECTOR %s\n", vector.name);
            auto curve = curveType(vector.curve);
            auto spki = unhex(vector.spki), pkcs8 = unhex(vector.pkcs8);
            auto publicKey = ECDER::importKey(curve, spki, false, randomBytes);
            auto privateKey = ECDER::importKey(curve, pkcs8, true, randomBytes);
            check(publicKey && privateKey, "ECDH known SPKI and PKCS8 accepted");
            auto expected = unhex(vector.secret);
            check(ecDerive(*privateKey, *publicKey, randomBytes) == expected, "ECDH equals independent WPT secret including leading zero");
            auto raw = exportECPublic(*publicKey), scalar = exportECPrivate(*privateKey);
            auto ownPublic = exportECPublic(*privateKey);
            check(raw.size() == 1 + 2 * expected.size() && raw[0] == 4 && scalar.size() == expected.size(), "EC exports exact-width uncompressed public point and private scalar");
            auto importedPublic = importECPublic(curve, raw);
            auto importedPrivate = importECPrivate(curve, scalar, ownPublic, randomBytes);
            check(importedPublic && importedPrivate && ecDerive(*importedPrivate, *importedPublic, randomBytes) == expected, "EC raw and JWK component imports preserve ECDH result");
            auto scalarOnly = importECPrivate(curve, scalar, { }, randomBytes);
            check(scalarOnly && exportECPublic(*scalarOnly) == ownPublic, "EC computes missing private public point from scalar");
            auto exportedSPKI = ECDER::exportKey(*publicKey, false);
            auto exportedPKCS8 = ECDER::exportKey(*privateKey, true);
            auto importedSPKI = ECDER::importKey(curve, exportedSPKI, false, randomBytes);
            auto importedPKCS8 = ECDER::importKey(curve, exportedPKCS8, true, randomBytes);
            check(exportedSPKI == spki && importedSPKI && exportECPublic(*importedSPKI) == raw, "EC SPKI export matches independent standard encoding");
            check(exportedPKCS8 == pkcs8 && importedPKCS8 && exportECPrivate(*importedPKCS8) == scalar && exportECPublic(*importedPKCS8) == ownPublic, "EC PKCS8 export matches independent standard encoding and preserves complete private key");
            auto append = [](std::vector<uint8_t>& to, std::span<const uint8_t> from) { to.insert(to.end(), from.begin(), from.end()); };
            auto algorithmFields = ECDER::wrap(0x06, ECDER::ecPublicKeyOID);
            append(algorithmFields, ECDER::wrap(0x06, ECDER::curveOID(curve)));
            auto algorithm = ECDER::wrap(0x30, algorithmFields);
            auto packPrivate = [&](std::span<const uint8_t> inner, std::span<const uint8_t> attributes = { }) {
                std::vector<uint8_t> fields { 2, 1, 0 };
                append(fields, algorithm);
                append(fields, ECDER::wrap(0x04, ECDER::wrap(0x30, inner)));
                append(fields, attributes);
                return ECDER::wrap(0x30, fields);
            };
            std::vector<uint8_t> privateFields { 2, 1, 1 };
            append(privateFields, ECDER::wrap(0x04, scalar));
            auto absentPublic = ECDER::importKey(curve, packPrivate(privateFields), true, randomBytes);
            check(absentPublic && exportECPublic(*absentPublic) == ownPublic, "EC PKCS8 accepts SEC1 private key without optional public field");
            auto explicitCurve = privateFields;
            append(explicitCurve, ECDER::wrap(0xa0, ECDER::wrap(0x06, ECDER::curveOID(curve))));
            check(!!ECDER::importKey(curve, packPrivate(explicitCurve), true, randomBytes), "EC PKCS8 accepts matching optional SEC1 named curve");
            auto wrongCurve = privateFields;
            auto alternateCurve = curve == MBEDTLS_ECP_DP_SECP256R1 ? MBEDTLS_ECP_DP_SECP384R1 : MBEDTLS_ECP_DP_SECP256R1;
            append(wrongCurve, ECDER::wrap(0xa0, ECDER::wrap(0x06, ECDER::curveOID(alternateCurve))));
            check(!ECDER::importKey(curve, packPrivate(wrongCurve), true, randomBytes), "EC PKCS8 rejects conflicting inner named curve");
            auto pointField = raw; pointField.insert(pointField.begin(), 0);
            auto mismatchedPublic = privateFields;
            append(mismatchedPublic, ECDER::wrap(0xa1, ECDER::wrap(0x03, pointField)));
            check(!ECDER::importKey(curve, packPrivate(mismatchedPublic), true, randomBytes), "EC PKCS8 rejects private scalar with conflicting SEC1 public point");
            check(!ECDER::importKey(curve, ECDER::wrap(0x30, privateFields), true, randomBytes), "bare SEC1 private encoding cannot masquerade as PKCS8");
            auto emptyAttributes = ECDER::wrap(0xa0, { });
            check(!!ECDER::importKey(curve, packPrivate(privateFields, emptyAttributes), true, randomBytes), "EC PKCS8 accepts empty optional attributes");
            auto duplicateAttributes = emptyAttributes; append(duplicateAttributes, emptyAttributes);
            check(!ECDER::importKey(curve, packPrivate(privateFields, duplicateAttributes), true, randomBytes), "EC PKCS8 rejects duplicate optional attributes fields");
            auto invalidBits = algorithm; pointField[0] = 1;
            append(invalidBits, ECDER::wrap(0x03, pointField));
            check(!ECDER::importKey(curve, ECDER::wrap(0x30, invalidBits), false, randomBytes), "EC SPKI rejects nonzero BIT STRING unused-bit count");
            auto wrongOID = algorithmFields; wrongOID[2 + ECDER::ecPublicKeyOID.size() - 1] ^= 1;
            auto wrongOIDFields = ECDER::wrap(0x30, wrongOID); pointField[0] = 0;
            append(wrongOIDFields, ECDER::wrap(0x03, pointField));
            check(!ECDER::importKey(curve, ECDER::wrap(0x30, wrongOIDFields), false, randomBytes), "EC SPKI rejects a different public-key algorithm OID");
            auto nonminimal = pkcs8; nonminimal[1] = 0x82; nonminimal.insert(nonminimal.begin() + 2, 0);
            check(!ECDER::importKey(curve, nonminimal, true, randomBytes), "EC PKCS8 rejects nonminimal long-form DER length");
            auto indefinite = pkcs8; indefinite[1] = 0x80;
            check(!ECDER::importKey(curve, indefinite, true, randomBytes), "EC PKCS8 rejects indefinite DER length");
            auto compressed = raw; compressed[0] = 2 + (raw.back() & 1); compressed.resize(1 + expected.size());
            check(!importECPublic(curve, compressed), "EC import consistently rejects unsupported compressed points");
            check(!ECDER::importKey(curve, pkcs8, true, failingRandom), "EC private import fails when public-point computation entropy fails");
            check(!importECPrivate(curve, scalar, raw, randomBytes), "EC rejects private scalar with a different valid public point");
            auto shortScalar = scalar; shortScalar.pop_back();
            check(!importECPrivate(curve, shortScalar, ownPublic, randomBytes), "EC JWK rejects short private scalar");
            auto zeroScalar = scalar; std::fill(zeroScalar.begin(), zeroScalar.end(), 0);
            check(!importECPrivate(curve, zeroScalar, { }, randomBytes), "EC rejects zero private scalar");
            auto largeScalar = scalar; std::fill(largeScalar.begin(), largeScalar.end(), 0xff);
            check(!importECPrivate(curve, largeScalar, { }, randomBytes), "EC rejects out-of-range private scalar");
            auto invalidRaw = raw; invalidRaw[0] = 0;
            check(!importECPublic(curve, invalidRaw), "EC rejects invalid SEC1 point prefix");
            invalidRaw = raw; invalidRaw.pop_back();
            check(!importECPublic(curve, invalidRaw), "EC rejects truncated uncompressed point");
            invalidRaw = raw; invalidRaw.push_back(0);
            check(!importECPublic(curve, invalidRaw), "EC rejects oversized uncompressed point");
            invalidRaw = raw; std::fill(invalidRaw.begin() + 1, invalidRaw.end(), 0);
            check(!importECPublic(curve, invalidRaw), "EC rejects point outside named curve");
            check(!importECPublic(curve, { }), "EC rejects empty public point");
            auto otherCurve = curve == MBEDTLS_ECP_DP_SECP256R1 ? MBEDTLS_ECP_DP_SECP384R1 : MBEDTLS_ECP_DP_SECP256R1;
            check(!ECDER::importKey(otherCurve, spki, false, randomBytes) && !ECDER::importKey(otherCurve, pkcs8, true, randomBytes), "EC rejects SPKI and PKCS8 named-curve mismatch");
            check(!ECDER::importKey(curve, spki, true, randomBytes) && !ECDER::importKey(curve, pkcs8, false, randomBytes), "EC DER public and private formats cannot be interchanged");
            auto trailing = spki; trailing.push_back(0);
            check(!ECDER::importKey(curve, trailing, false, randomBytes), "EC SPKI rejects trailing bytes");
            trailing = pkcs8; trailing.push_back(0);
            check(!ECDER::importKey(curve, trailing, true, randomBytes), "EC PKCS8 rejects trailing bytes");
            pkcs8.pop_back(); spki.pop_back();
            check(!ECDER::importKey(curve, pkcs8, true, randomBytes) && !ECDER::importKey(curve, spki, false, randomBytes), "EC DER rejects truncated public and private encodings");
            check(!ecDerive(*privateKey, *publicKey, failingRandom), "ECDH fails when blinding entropy fails");
            check(!ecSign(*privateKey, MBEDTLS_MD_SHA256, expected, failingRandom), "ECDSA fails when signing entropy fails");
            check(!generateECKey(curve, failingRandom), "EC generation fails when entropy fails");
            auto generatedA = generateECKey(curve, randomBytes), generatedB = generateECKey(curve, randomBytes);
            check(generatedA && generatedB, "EC generation succeeds for configured named curve");
            auto derivedA = ecDerive(*generatedA, *generatedB, randomBytes);
            auto derivedB = ecDerive(*generatedB, *generatedA, randomBytes);
            check(derivedA && derivedA->size() == expected.size() && derivedA == derivedB, "generated EC keys derive identical fixed-width secrets");
        }

        auto privateKey = ECDER::importKey(MBEDTLS_ECP_DP_SECP256R1, unhex(ecdsaVectors[0].pkcs8), true, randomBytes);
        auto publicKey = ECDER::importKey(MBEDTLS_ECP_DP_SECP256R1, unhex(ecdsaVectors[0].spki), false, randomBytes);
        auto message = unhex(ecdsaVectors[0].data);
        auto otherKey = generateECKey(MBEDTLS_ECP_DP_SECP384R1, randomBytes);
        check(privateKey && publicKey && otherKey, "EC concurrency inputs available");
        check(!ecDerive(*privateKey, *otherKey, randomBytes), "ECDH rejects curve mismatch");
        check(!ecSign(*publicKey, MBEDTLS_MD_SHA256, message, randomBytes), "ECDSA helper rejects signing with public-only key");
        check(!ecDerive(*publicKey, *privateKey, randomBytes), "ECDH helper rejects public-only base key");
        check(!ecSign(*privateKey, MBEDTLS_MD_NONE, message, randomBytes), "ECDSA rejects unsupported hash identifier");
        std::atomic<bool> concurrentOK { true };
        struct ConcurrentData { const ECKeyData* privateKey; const ECKeyData* publicKey; std::span<const uint8_t> message; std::atomic<bool>* ok; } concurrent { privateKey.get(), publicKey.get(), message, &concurrentOK };
        pthread_t threads[4]; pthread_attr_t attributes;
        check(!pthread_attr_init(&attributes) && !pthread_attr_setstacksize(&attributes, 128 * 1024), "EC concurrency uses bounded 128 KiB worker stacks");
        for (auto& thread : threads) {
            if (pthread_create(&thread, &attributes, [](void* opaque) -> void* {
                auto& state = *static_cast<ConcurrentData*>(opaque);
                for (unsigned attempt = 0; attempt < 2; ++attempt) {
                    auto signature = ecSign(*state.privateKey, MBEDTLS_MD_SHA256, state.message, randomBytes);
                    if (!signature || ecVerify(*state.publicKey, MBEDTLS_MD_SHA256, *signature, state.message) != true)
                        state.ok->store(false);
                    if (!ecDerive(*state.privateKey, *state.publicKey, randomBytes))
                        state.ok->store(false);
                }
                return nullptr;
            }, &concurrent))
                throw std::runtime_error("EC thread creation");
        }
        pthread_attr_destroy(&attributes);
        for (auto& thread : threads)
            pthread_join(thread, nullptr);
        check(concurrentOK.load(), "EC concurrent same-key signing verification and derivation succeed");
        printf("PASS: ec-helpers %u/%u\n", checks, checks);
        return 0;
    } catch (const std::exception& exception) {
        fprintf(stderr, "FAIL %s after %u checks\n", exception.what(), checks);
        return 1;
    }
}
