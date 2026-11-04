// Exercise the production RSA import, DER and operation helpers.
#include "MbedTLSRSADER.h"
#include "MbedTLSRSAOperations.h"
#include "vectors.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <stdexcept>
#if defined(RSA_NATIVE_PROBE)
#include "MbedTLSRSARandom.h"
#else
#include <sys/random.h>
#endif

using namespace WebCore::MbedTLS;
static unsigned checks;
[[noreturn]] static void fail(const char* label)
{
#if defined(RSA_NATIVE_PROBE)
    printf("FAIL %s\nrsa-probe exiting 1\n", label);
    exit(1);
#else
    throw std::runtime_error(label);
#endif
}
static void check(bool condition, const char* label)
{
    if (!condition)
        fail(label);
    ++checks;
    printf("PASS %s\n", label);
}
static std::vector<uint8_t> unhex(const char* hex)
{
    std::vector<uint8_t> result;
    auto digit = [](char value) -> unsigned {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        fail("invalid fixture hex");
    };
    for (size_t index = 0; hex[index]; index += 2)
        result.push_back(digit(hex[index]) * 16 + digit(hex[index + 1]));
    return result;
}
static int randomBytes(void*, unsigned char* data, size_t size)
{
#if defined(RSA_NATIVE_PROBE)
    return rsaRandom(nullptr, data, size);
#else
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
#endif
}
static int failingRandom(void*, unsigned char*, size_t) { return -1; }
static mbedtls_md_type_t hashType(const char* name)
{
    if (!strcmp(name, "SHA-1")) return MBEDTLS_MD_SHA1;
    if (!strcmp(name, "SHA-256")) return MBEDTLS_MD_SHA256;
    if (!strcmp(name, "SHA-384")) return MBEDTLS_MD_SHA384;
    if (!strcmp(name, "SHA-512")) return MBEDTLS_MD_SHA512;
    fail("unknown fixture hash");
}
static RSAKey readKey(std::span<const uint8_t> bytes, bool privateKey)
{
    auto parameters = RSADER::parse(bytes, privateKey);
    return parameters ? importRSA(*parameters) : nullptr;
}

int main()
{
#if defined(RSA_NATIVE_PROBE)
    if (!freopen("C:/Data/WebKit/rsa-probe.log", "w", stdout)) return 2;
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("BEGIN: rsa-probe version=1\n");
#else
    try {
#endif
        for (const auto& vector : vectors) {
            auto spki = unhex(vector.spki), pkcs8 = unhex(vector.pkcs8);
            auto publicKey = readKey(spki, false), privateKey = readKey(pkcs8, true);
            check(publicKey && privateKey && rsaModulusBits(publicKey.get()) == 2048, "WPT SPKI/PKCS8 mathematical keys accepted");
            auto data = unhex(vector.data), expected = unhex(vector.result), label = unhex(vector.label);
            auto hash = hashType(vector.hash);
            printf("VECTOR %s\n", vector.name);
            if (!strcmp(vector.algorithm, "RSA-OAEP")) {
                auto actual = rsaOAEP(privateKey.get(), hash, expected, label, true, randomBytes);
                check(actual && *actual == data, "OAEP decrypt matches independent known plaintext");
                auto encrypted = rsaOAEP(publicKey.get(), hash, data, label, false, randomBytes);
                auto decrypted = encrypted ? rsaOAEP(privateKey.get(), hash, *encrypted, label, true, randomBytes) : std::nullopt;
                check(decrypted && *decrypted == data, "OAEP randomized encryption roundtrip");
                auto changed = label; changed.push_back(0);
                check(!rsaOAEP(privateKey.get(), hash, expected, changed, true, randomBytes), "OAEP rejects mismatched label including zero byte");
                expected[3] ^= 1;
                check(!rsaOAEP(privateKey.get(), hash, expected, label, true, randomBytes), "OAEP rejects tampered ciphertext");
            } else {
                std::optional<size_t> salt;
                if (vector.salt >= 0)
                    salt = static_cast<size_t>(vector.salt);
                check(rsaVerify(publicKey.get(), hash, expected, data, salt) == true, "RSA verify accepts independent known signature");
                auto signature = rsaSign(privateKey.get(), hash, data, salt, randomBytes);
                check(signature && rsaVerify(publicKey.get(), hash, *signature, data, salt) == true, "RSA sign and verify roundtrip");
                if (vector.salt <= 0)
                    check(signature && *signature == expected, "deterministic RSA signature matches exact known bytes");
                expected[3] ^= 1;
                check(rsaVerify(publicKey.get(), hash, expected, data, salt) == false, "RSA verify rejects tampered signature");
            }
        }
        auto spki = unhex(vectors[0].spki), pkcs8 = unhex(vectors[0].pkcs8);
        auto publicKey = readKey(spki, false), privateKey = readKey(pkcs8, true);
        const std::array<uint8_t, 3> message { 1, 0, 255 };
        for (size_t salt : { size_t(0), size_t(32), size_t(33), size_t(222) }) {
            auto signature = rsaSign(privateKey.get(), MBEDTLS_MD_SHA256, message, salt, randomBytes);
            check(signature && rsaVerify(publicKey.get(), MBEDTLS_MD_SHA256, *signature, message, salt) == true, "PSS supports exact zero/hash/hash+1/maximum salt");
            check(signature && rsaVerify(publicKey.get(), MBEDTLS_MD_SHA256, *signature, message, salt + 1) == false, "PSS does not accept a different salt length");
        }
        for (size_t salt : { size_t(223), size_t(INT_MAX), size_t(UINT32_MAX) }) {
            check(!rsaSign(privateKey.get(), MBEDTLS_MD_SHA256, message, salt, randomBytes), "PSS rejects oversized salt before signed conversion");
            std::vector<uint8_t> signature(256);
            check(rsaVerify(publicKey.get(), MBEDTLS_MD_SHA256, signature, message, salt) == false, "PSS oversized verify salt cannot become ANY");
        }
        for (size_t length : { size_t(0), size_t(190) }) {
            std::vector<uint8_t> plaintext(length);
            auto encrypted = rsaOAEP(publicKey.get(), MBEDTLS_MD_SHA256, plaintext, { }, false, randomBytes);
            auto decrypted = encrypted ? rsaOAEP(privateKey.get(), MBEDTLS_MD_SHA256, *encrypted, { }, true, randomBytes) : std::nullopt;
            check(decrypted && *decrypted == plaintext, "OAEP empty and maximum-length plaintext accepted");
        }
        std::vector<uint8_t> oversized(191), shortCipher(255), longCipher(257), shortSignature(255);
        check(!rsaOAEP(publicKey.get(), MBEDTLS_MD_SHA256, oversized, { }, false, randomBytes), "OAEP rejects plaintext one byte too long");
        check(!rsaOAEP(privateKey.get(), MBEDTLS_MD_SHA256, shortCipher, { }, true, randomBytes) && !rsaOAEP(privateKey.get(), MBEDTLS_MD_SHA256, longCipher, { }, true, randomBytes), "OAEP checks ciphertext length before native read");
        check(rsaVerify(publicKey.get(), MBEDTLS_MD_SHA256, shortSignature, message, std::nullopt) == false, "RSASSA checks signature length before native read");
        check(!rsaSign(privateKey.get(), MBEDTLS_MD_SHA256, message, std::nullopt, failingRandom), "RSASSA fails when blinding entropy fails");
        check(!rsaSign(privateKey.get(), MBEDTLS_MD_SHA256, message, 0, failingRandom), "PSS zero salt still requires blinding entropy");
        check(!rsaOAEP(publicKey.get(), MBEDTLS_MD_SHA256, message, { }, false, failingRandom), "OAEP encryption fails when padding entropy fails");
        auto cipher = rsaOAEP(publicKey.get(), MBEDTLS_MD_SHA256, message, { }, false, randomBytes);
        check(cipher && !rsaOAEP(privateKey.get(), MBEDTLS_MD_SHA256, *cipher, { }, true, failingRandom), "OAEP decryption fails when blinding entropy fails");
        auto parsed = RSADER::parse(pkcs8, true);
        auto minimal = *parsed; minimal.hasCRT = false;
        check(!!importRSA(minimal), "minimal N/E/D private key is completed and checked");
        for (auto field : { RSAParameters::DP, RSAParameters::DQ, RSAParameters::QI }) {
            auto invalid = *parsed;
            std::vector<uint8_t> changed(invalid.values[field].begin(), invalid.values[field].end());
            changed.back() ^= 1; invalid.values[field] = changed;
            check(!importRSA(invalid), "supplied inconsistent CRT parameter is rejected");
        }
        auto trailing = pkcs8; trailing.push_back(0);
        check(!readKey(trailing, true), "DER trailing data rejected");
        check(!readKey(std::span(pkcs8).first(pkcs8.size() - 1), true), "DER truncated data rejected");
        auto noncanonical = pkcs8; noncanonical[1] = 0x83; noncanonical.insert(noncanonical.begin() + 2, 0);
        check(!readKey(noncanonical, true), "DER nonminimal long length rejected");
        auto indefinite = pkcs8; indefinite[1] = 0x80;
        check(!readKey(indefinite, true), "DER indefinite length rejected");
        auto wrongAlgorithm = pkcs8; wrongAlgorithm[19] = 2;
        check(!readKey(wrongAlgorithm, true), "DER wrong algorithm OID rejected");
        std::array<uint8_t, 8> overflow { 0x30, 0x84, 0xff, 0xff, 0xff, 0xff, 0, 0 };
        check(!readKey(overflow, true), "DER oversized 32-bit length rejected");
        check(!readKey(spki, true) && !readKey(pkcs8, false), "DER public/private formats cannot be interchanged");
        RSADER::Reader outer(pkcs8); auto seq = outer.read(0x30); RSADER::Reader fields(*seq); fields.read(0x02); RSADER::algorithm(fields); auto raw = fields.read(0x04);
        check(!readKey(*raw, true), "bare PKCS1 cannot masquerade as PKCS8");
        auto wrapped = RSADER::pkcs8(*raw);
        check(wrapped == pkcs8 && !!readKey(wrapped, true), "PKCS8 export wrapper matches exact standard structure");
        auto addAttributes = [&](std::span<const uint8_t> attributes) {
            std::vector<uint8_t> contents(seq->begin(), seq->end());
            contents.insert(contents.end(), attributes.begin(), attributes.end());
            return RSADER::wrap(0x30, contents);
        };
        auto attribute = [](std::span<const uint8_t> oid, std::span<const uint8_t> values) {
            auto fields = RSADER::wrap(0x06, oid);
            auto set = RSADER::wrap(0x31, values);
            fields.insert(fields.end(), set.begin(), set.end());
            return RSADER::wrap(0x30, fields);
        };
        const std::array<uint8_t, 9> friendlyNameOID { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x14 };
        const std::array<uint8_t, 9> opaqueOID { 0x2b, 0x06, 0x01, 0x04, 0x01, 0x83, 0xb2, 0x03, 0x01 };
        const std::array<uint8_t, 6> friendlyName { 0x1e, 4, 0, 'A', 0, 'B' };
        auto friendly = attribute(friendlyNameOID, friendlyName);
        auto optionalAttributes = RSADER::wrap(0xa0, friendly);
        check(!!readKey(addAttributes(RSADER::wrap(0xa0, { })), true), "PKCS8 accepts empty optional attributes set");
        auto attributedKey = readKey(addAttributes(optionalAttributes), true);
        check(attributedKey && rsaSign(attributedKey.get(), MBEDTLS_MD_SHA256, message, std::nullopt, randomBytes) == rsaSign(privateKey.get(), MBEDTLS_MD_SHA256, message, std::nullopt, randomBytes), "PKCS8 friendlyName metadata preserves exact private key operations");
        const std::array<uint8_t, 3> highTagValue { 0x9f, 0x1f, 0 };
        check(!!readKey(addAttributes(RSADER::wrap(0xa0, attribute(opaqueOID, highTagValue))), true), "PKCS8 accepts canonical high-tag attribute values");
        auto duplicate = optionalAttributes; duplicate.insert(duplicate.end(), optionalAttributes.begin(), optionalAttributes.end());
        check(!readKey(addAttributes(duplicate), true), "PKCS8 rejects duplicate attributes fields");
        auto wrongAttributesTag = optionalAttributes; wrongAttributesTag[0] = 0xa1;
        check(!readKey(addAttributes(wrongAttributesTag), true), "PKCS8 rejects unexpected trailing field tag");
        auto truncatedAttribute = optionalAttributes; truncatedAttribute.pop_back();
        check(!readKey(addAttributes(truncatedAttribute), true), "PKCS8 rejects truncated attribute value");
        const std::array<uint8_t, 2> badOID { 0x80, 0x01 };
        check(!readKey(addAttributes(RSADER::wrap(0xa0, attribute(badOID, friendlyName))), true), "PKCS8 rejects noncanonical attribute OID");
        check(!readKey(addAttributes(RSADER::wrap(0xa0, attribute(opaqueOID, { }))), true), "PKCS8 rejects empty Attribute values set");
        for (const auto& value : { std::vector<uint8_t> { 4, 0x81, 0 }, std::vector<uint8_t> { 2, 2, 0, 1 }, std::vector<uint8_t> { 1, 1, 1 }, std::vector<uint8_t> { 0x9f, 0x1e, 0 }, std::vector<uint8_t> { 0x9f, 0x81 } })
            check(!readKey(addAttributes(RSADER::wrap(0xa0, attribute(opaqueOID, value))), true), "PKCS8 rejects noncanonical nested attribute encoding");
        const std::array<uint8_t, 6> sortedValues { 4, 1, 1, 4, 1, 2 };
        const std::array<uint8_t, 6> reversedValues { 4, 1, 2, 4, 1, 1 };
        check(!!readKey(addAttributes(RSADER::wrap(0xa0, attribute(opaqueOID, sortedValues))), true), "PKCS8 accepts DER-sorted attribute values");
        check(!readKey(addAttributes(RSADER::wrap(0xa0, attribute(opaqueOID, reversedValues))), true), "PKCS8 rejects unsorted DER attribute values");
        auto first = attribute(opaqueOID, friendlyName), second = attribute(opaqueOID, sortedValues);
        if (first > second) std::swap(first, second);
        auto ordered = first; ordered.insert(ordered.end(), second.begin(), second.end());
        auto reversed = second; reversed.insert(reversed.end(), first.begin(), first.end());
        check(!!readKey(addAttributes(RSADER::wrap(0xa0, ordered)), true), "PKCS8 accepts DER-sorted multiple attributes");
        check(!readKey(addAttributes(RSADER::wrap(0xa0, reversed)), true), "PKCS8 rejects unsorted DER attributes");
        std::vector<uint8_t> nested { 4, 0 };
        for (unsigned depth = 0; depth < 20; ++depth) nested = RSADER::wrap(0xa1, nested);
        check(!readKey(addAttributes(RSADER::wrap(0xa0, attribute(opaqueOID, nested))), true), "PKCS8 bounds attribute recursion independently of input size");
        std::atomic<bool> concurrentOK { true };
        struct ConcurrentData { const mbedtls_rsa_context* privateKey; const mbedtls_rsa_context* publicKey; std::span<const uint8_t> message; std::atomic<bool>* ok; } concurrent { privateKey.get(), publicKey.get(), message, &concurrentOK };
        pthread_t threads[4]; pthread_attr_t attributes;
        if (pthread_attr_init(&attributes) || pthread_attr_setstacksize(&attributes, 128 * 1024)) fail("pthread stack attributes");
        for (auto& thread : threads) {
            if (pthread_create(&thread, &attributes, [](void* opaque) -> void* {
                auto& data = *static_cast<ConcurrentData*>(opaque);
                for (unsigned index = 0; index < 16; ++index) {
                    auto signature = rsaSign(data.privateKey, MBEDTLS_MD_SHA256, data.message, index, randomBytes);
                    if (!signature || rsaVerify(data.publicKey, MBEDTLS_MD_SHA256, *signature, data.message, index) != true) *data.ok = false;
                }
                return nullptr;
            }, &concurrent)) fail("pthread creation");
        }
        pthread_attr_destroy(&attributes);
        for (auto& thread : threads) if (pthread_join(thread, nullptr)) fail("pthread join");
        check(concurrentOK, "four 128 KiB pthreads safely use the same immutable RSA key");
        printf("RESULT checks=%u failures=0 pointerBits=%u\n", checks, unsigned(sizeof(void*) * 8));
#if defined(RSA_NATIVE_PROBE)
        printf("RESULT {\"probe\":\"rsa-probe\",\"passed\":%u,\"failed\":0}\nPASS: rsa-probe %u/%u\nrsa-probe exiting 0\n", checks, checks, checks);
        return 0;
#else
    } catch (const std::exception& exception) {
        fprintf(stderr, "FAIL %s\n", exception.what());
        return 1;
    }
#endif
}
