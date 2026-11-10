/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "MbedTLSRSAKey.h"
#include <limits>
#include <optional>
#include <vector>

namespace WebCore::MbedTLS::RSADER {

constexpr size_t maximumDERSize = 16 * MBEDTLS_MPI_MAX_SIZE + 1024;
constexpr std::array<uint8_t, 9> rsaEncryptionOID { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01 };

class Reader {
public:
    struct Element {
        uint8_t tag;
        std::span<const uint8_t> encoded;
        std::span<const uint8_t> value;
    };
    explicit Reader(std::span<const uint8_t> input) : m_input(input) { }
    bool empty() const { return m_input.empty(); }
    std::optional<Element> readElement()
    {
        if (m_input.size() < 2)
            return std::nullopt;
        uint8_t tag = m_input[0];
        size_t prefix = 1;
        if ((tag & 0x1f) == 0x1f) {
            // High tag numbers use a minimal, terminating base-128 encoding.
            if (!(m_input[prefix] & 0x7f) || m_input[prefix] < 31)
                return std::nullopt;
            while (m_input[prefix++] & 0x80) {
                if (prefix == m_input.size())
                    return std::nullopt;
            }
        }
        if (prefix == m_input.size())
            return std::nullopt;
        size_t length = m_input[prefix++];
        if (length & 0x80) {
            auto count = length & 0x7f;
            if (!count || count > sizeof(size_t) || count > m_input.size() - prefix || !m_input[prefix])
                return std::nullopt;
            length = 0;
            for (size_t index = 0; index < count; ++index) {
                if (length > (std::numeric_limits<size_t>::max() >> 8))
                    return std::nullopt;
                length = (length << 8) | m_input[prefix++];
            }
            if (length < 128)
                return std::nullopt;
        }
        if (length > m_input.size() - prefix)
            return std::nullopt;
        Element result { tag, m_input.first(prefix + length), m_input.subspan(prefix, length) };
        m_input = m_input.subspan(prefix + length);
        return result;
    }
    std::optional<std::span<const uint8_t>> read(uint8_t tag)
    {
        if (m_input.empty() || m_input[0] != tag)
            return std::nullopt;
        auto element = readElement();
        if (!element)
            return std::nullopt;
        return element->value;
    }
    std::optional<std::span<const uint8_t>> positiveInteger()
    {
        auto value = read(0x02);
        if (!value || value->empty() || ((*value)[0] & 0x80))
            return std::nullopt;
        if (!(*value)[0]) {
            if (value->size() == 1 || !((*value)[1] & 0x80))
                return std::nullopt;
            *value = value->subspan(1);
        }
        if (value->size() > MBEDTLS_MPI_MAX_SIZE)
            return std::nullopt;
        return value;
    }
private:
    std::span<const uint8_t> m_input;
};

inline bool validOID(std::span<const uint8_t> bytes)
{
    if (bytes.empty() || (bytes.back() & 0x80))
        return false;
    bool first = true;
    for (auto byte : bytes) {
        if (first && byte == 0x80)
            return false;
        first = !(byte & 0x80);
    }
    return true;
}

// PKCS8 attributes are metadata, not RSA parameters. Validate their bounded
// ASN.1 structure before ignoring them. Limit nesting independently of key size.
inline bool validAttributeValue(const Reader::Element& element, unsigned depth)
{
    if (depth > 16)
        return false;
    auto tag = element.tag;
    auto bytes = element.value;
    if (!(tag & 0xc0)) {
        auto type = tag & 0x1f;
        bool constructedType = type == 8 || type == 11 || type == 16 || type == 17 || type == 29;
        if (!type || (type != 31 && constructedType != !!(tag & 0x20)))
            return false;
        switch (type) {
        case 1:
            return bytes.size() == 1 && (!bytes[0] || bytes[0] == 0xff);
        case 2:
        case 10:
            return !bytes.empty() && (bytes.size() == 1 || !((!bytes[0] && !(bytes[1] & 0x80)) || (bytes[0] == 0xff && (bytes[1] & 0x80))));
        case 3:
            return !bytes.empty() && bytes[0] <= 7 && (bytes.size() == 1 ? !bytes[0] : !(bytes.back() & ((1u << bytes[0]) - 1)));
        case 5:
            return bytes.empty();
        case 6:
            return validOID(bytes);
        default:
            break;
        }
    }
    if (!(tag & 0x20))
        return true;
    Reader children(bytes);
    std::span<const uint8_t> previous;
    while (!children.empty()) {
        auto child = children.readElement();
        if (!child || !validAttributeValue(*child, depth + 1))
            return false;
        if (tag == 0x31 && std::lexicographical_compare(child->encoded.begin(), child->encoded.end(), previous.begin(), previous.end()))
            return false;
        previous = child->encoded;
    }
    return true;
}

inline bool attributes(Reader& parent)
{
    auto implicitSet = parent.read(0xa0);
    if (!implicitSet)
        return false;
    Reader items(*implicitSet);
    std::span<const uint8_t> previous;
    while (!items.empty()) {
        auto item = items.readElement();
        if (!item || item->tag != 0x30 || std::lexicographical_compare(item->encoded.begin(), item->encoded.end(), previous.begin(), previous.end()))
            return false;
        previous = item->encoded;
        Reader fields(item->value);
        auto oid = fields.read(0x06);
        auto values = fields.readElement();
        if (!oid || !validOID(*oid) || !values || values->tag != 0x31 || values->value.empty() || !validAttributeValue(*values, 0) || !fields.empty())
            return false;
    }
    return true;
}

inline bool algorithm(Reader& parent)
{
    auto sequence = parent.read(0x30);
    if (!sequence)
        return false;
    Reader fields(*sequence);
    auto oid = fields.read(0x06);
    if (!oid || !std::ranges::equal(*oid, rsaEncryptionOID))
        return false;
    // rsaEncryption uses NULL parameters; absent parameters are interoperable.
    if (!fields.empty()) {
        auto null = fields.read(0x05);
        if (!null || !null->empty())
            return false;
    }
    return fields.empty();
}

inline std::optional<RSAParameters> components(std::span<const uint8_t> input, bool isPrivate)
{
    Reader outer(input);
    auto sequence = outer.read(0x30);
    if (!sequence || !outer.empty())
        return std::nullopt;
    Reader fields(*sequence);
    RSAParameters result { { }, isPrivate, isPrivate };
    if (isPrivate) {
        auto version = fields.read(0x02);
        if (!version || version->size() != 1 || (*version)[0])
            return std::nullopt; // Only two-prime RSA, version zero.
    }
    unsigned count = isPrivate ? RSAParameters::Count : RSAParameters::D;
    for (unsigned index = 0; index < count; ++index) {
        auto value = fields.positiveInteger();
        if (!value)
            return std::nullopt;
        result.values[index] = *value;
    }
    if (!fields.empty())
        return std::nullopt;
    return result;
}

inline std::optional<RSAParameters> parse(std::span<const uint8_t> input, bool isPrivate)
{
    if (input.size() > maximumDERSize)
        return std::nullopt;
    Reader outer(input);
    auto sequence = outer.read(0x30);
    if (!sequence || !outer.empty())
        return std::nullopt;
    Reader fields(*sequence);
    if (isPrivate) {
        auto version = fields.read(0x02);
        if (!version || version->size() != 1 || (*version)[0])
            return std::nullopt;
    }
    if (!algorithm(fields))
        return std::nullopt;
    auto key = fields.read(isPrivate ? 0x04 : 0x03);
    if (!key)
        return std::nullopt;
    if (isPrivate && !fields.empty() && !attributes(fields))
        return std::nullopt;
    if (!fields.empty())
        return std::nullopt;
    if (!isPrivate) {
        if (key->empty() || (*key)[0])
            return std::nullopt;
        *key = key->subspan(1); // BIT STRING has no unused bits.
    }
    return components(*key, isPrivate);
}

inline std::vector<uint8_t> wrap(uint8_t tag, std::span<const uint8_t> input)
{
    if (input.size() > maximumDERSize)
        return { };
    std::vector<uint8_t> result;
    result.reserve(input.size() + sizeof(size_t) + 2);
    result.push_back(tag);
    if (input.size() < 128)
        result.push_back(static_cast<uint8_t>(input.size()));
    else {
        std::array<uint8_t, sizeof(size_t)> length;
        size_t value = input.size(), count = 0;
        while (value) {
            length[length.size() - ++count] = value & 0xff;
            value >>= 8;
        }
        result.push_back(0x80 | count);
        result.insert(result.end(), length.end() - count, length.end());
    }
    result.insert(result.end(), input.begin(), input.end());
    return result;
}

inline std::vector<uint8_t> pkcs8(std::span<const uint8_t> pkcs1)
{
    if (!components(pkcs1, true))
        return { };
    auto oid = wrap(0x06, rsaEncryptionOID);
    oid.insert(oid.end(), { 0x05, 0x00 });
    auto algorithm = wrap(0x30, oid);
    auto key = wrap(0x04, pkcs1);
    std::vector<uint8_t> fields { 0x02, 0x01, 0x00 };
    fields.insert(fields.end(), algorithm.begin(), algorithm.end());
    fields.insert(fields.end(), key.begin(), key.end());
    return wrap(0x30, fields);
}

} // namespace WebCore::MbedTLS::RSADER
