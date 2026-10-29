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
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>

namespace WebCore::IDBServer {

// This accounting object has no I/O and is confined to the IDB server queue.
// The adapter assigns opaque IDs and supplies authoritative database sizes.
// SQLite allows only one active read/write transaction per backing store;
// transactions in different stores still consume the same origin/port budget.
class IDBStorageQuota {
public:
    using Identifier = uint64_t;
    struct Limits {
        uint64_t origin;
        uint64_t total;
        uint64_t freeDiskReserve;
    };

    IDBStorageQuota(Limits limits, uint64_t committedTotal)
        : m_limits(limits)
        , m_committedTotal(committedTotal)
    {
    }

    void addOrigin(Identifier identifier, uint64_t committedSize)
    {
        m_origins.try_emplace(identifier, committedSize);
    }

    void addDatabase(Identifier identifier, Identifier origin, uint64_t committedSize)
    {
        m_databases.try_emplace(identifier, Database { origin, committedSize, committedSize });
    }

    bool addTransaction(Identifier identifier, Identifier database)
    {
        if (!m_databases.contains(database))
            return false;
        return m_transactions.try_emplace(identifier, Transaction { database, 0 }).second;
    }

    void setProjectedSize(Identifier database, std::optional<uint64_t> size)
    {
        if (auto entry = m_databases.find(database); entry != m_databases.end())
            entry->second.projectedSize = size;
    }

    bool reserve(Identifier transaction, uint64_t amount, std::optional<uint64_t> freeDisk, uint64_t futureDiskBytes)
    {
        // Control, reads, deletion and rollback must remain possible at quota.
        if (!amount)
            return true;
        auto entry = m_transactions.find(transaction);
        if (entry == m_transactions.end())
            return false;
        auto reserved = add(entry->second.reserved, amount);
        if (!reserved)
            return false;
        auto previous = entry->second.reserved;
        entry->second.reserved = *reserved;
        bool granted = withinLimits(entry->second.database, std::nullopt)
            && diskHasSpace(freeDisk, futureDiskBytes);
        if (!granted)
            entry->second.reserved = previous;
        return granted;
    }

    bool canCommit(Identifier transaction, std::optional<uint64_t> freeDisk, uint64_t futureDiskBytes, std::optional<uint64_t> ownFutureDiskBytes = std::nullopt) const
    {
        auto entry = m_transactions.find(transaction);
        if (entry == m_transactions.end())
            return false;
        const auto& database = m_databases.at(entry->second.database);
        if (!database.projectedSize)
            return false;
        // Existing over-limit data can be read/cleaned without increasing its
        // footprint. New Blob copies can still need additional physical space.
        if (*database.projectedSize <= database.committedSize)
            return !ownFutureDiskBytes.value_or(futureDiskBytes) || diskHasSpace(freeDisk, futureDiskBytes);
        // At this point use the real pending SQLite/Blob footprint, not this
        // transaction's conservative sum of operation estimates. Reservations
        // belonging to other transactions are retained.
        return withinLimits(entry->second.database, transaction)
            && diskHasSpace(freeDisk, futureDiskBytes);
    }

    bool finishTransaction(Identifier transaction, uint64_t committedSize)
    {
        auto entry = m_transactions.find(transaction);
        if (entry == m_transactions.end())
            return true; // Completion and teardown notifications are idempotent.
        auto database = entry->second.database;
        if (!updateCommittedSize(database, committedSize))
            return false;
        m_transactions.erase(entry);
        return true;
    }

    bool updateCommittedSize(Identifier identifier, uint64_t size)
    {
        auto entry = m_databases.find(identifier);
        if (entry == m_databases.end())
            return false;
        auto& database = entry->second;
        auto origin = m_origins.find(database.origin);
        if (origin == m_origins.end() || origin->second < database.committedSize || m_committedTotal < database.committedSize)
            return false;
        auto originSize = add(origin->second - database.committedSize, size);
        auto totalSize = add(m_committedTotal - database.committedSize, size);
        if (!originSize || !totalSize)
            return false;
        origin->second = *originSize;
        m_committedTotal = *totalSize;
        database.committedSize = size;
        database.projectedSize = size;
        return true;
    }

    std::optional<uint64_t> reservedBytes(std::optional<Identifier> excluding = std::nullopt) const
    {
        uint64_t result = 0;
        for (const auto& [identifier, transaction] : m_transactions) {
            if (excluding == identifier)
                continue;
            auto sum = add(result, transaction.reserved);
            if (!sum)
                return std::nullopt;
            result = *sum;
        }
        return result;
    }

    std::optional<uint64_t> totalCharge() const { return charge(std::nullopt, std::nullopt); }
    std::optional<uint64_t> originCharge(Identifier origin) const { return charge(origin, std::nullopt); }

private:
    struct Database {
        Identifier origin;
        uint64_t committedSize;
        std::optional<uint64_t> projectedSize;
    };
    struct Transaction {
        Identifier database;
        uint64_t reserved;
    };

    static std::optional<uint64_t> add(uint64_t first, uint64_t second)
    {
        if (second > std::numeric_limits<uint64_t>::max() - first)
            return std::nullopt;
        return first + second;
    }

    std::optional<uint64_t> charge(std::optional<Identifier> origin, std::optional<Identifier> committing) const
    {
        uint64_t result = m_committedTotal;
        if (origin) {
            auto entry = m_origins.find(*origin);
            if (entry == m_origins.end())
                return std::nullopt;
            result = entry->second;
        }
        for (const auto& [identifier, database] : m_databases) {
            if (origin && database.origin != *origin)
                continue;
            if (!database.projectedSize)
                return std::nullopt;
            uint64_t reserved = 0;
            for (const auto& [transactionIdentifier, transaction] : m_transactions) {
                if (transaction.database != identifier || committing == transactionIdentifier)
                    continue;
                auto sum = add(reserved, transaction.reserved);
                if (!sum)
                    return std::nullopt;
                reserved = *sum;
            }
            uint64_t growth = *database.projectedSize > database.committedSize ? *database.projectedSize - database.committedSize : 0;
            // Materialized but uncommitted pages/Blobs and their reservation
            // describe the same growth. Charge the greater amount, not both.
            auto sum = add(result, std::max(growth, reserved));
            if (!sum)
                return std::nullopt;
            result = *sum;
        }
        return result;
    }

    bool withinLimits(Identifier identifier, std::optional<Identifier> committing) const
    {
        auto database = m_databases.find(identifier);
        if (database == m_databases.end())
            return false;
        auto origin = charge(database->second.origin, committing);
        auto total = charge(std::nullopt, committing);
        return origin && total && *origin <= m_limits.origin && *total <= m_limits.total;
    }

    bool diskHasSpace(std::optional<uint64_t> freeDisk, uint64_t futureDiskBytes) const
    {
        auto required = add(m_limits.freeDiskReserve, futureDiskBytes);
        return freeDisk && required && *required <= *freeDisk;
    }

    Limits m_limits;
    uint64_t m_committedTotal;
    std::map<Identifier, uint64_t> m_origins;
    std::map<Identifier, Database> m_databases;
    std::map<Identifier, Transaction> m_transactions;
};

} // namespace WebCore::IDBServer
