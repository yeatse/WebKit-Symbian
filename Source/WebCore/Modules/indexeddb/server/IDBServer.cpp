/*
 * Copyright (C) 2015-2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IDBServer.h"

#include "IDBRequestData.h"
#if OS(SYMBIAN)
#include "IDBStorageQuota.h"
#include <wtf/CheckedArithmetic.h>
#endif
#include "IDBResultData.h"
#include "Logging.h"
#include "MemoryIDBBackingStore.h"
#include "SQLiteFileSystem.h"
#include "SQLiteIDBBackingStore.h"
#include "SecurityOrigin.h"
#include <algorithm>
#include <wtf/CompletionHandler.h>
#include <wtf/Locker.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace IDBServer {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IDBBackingStore);
WTF_MAKE_TZONE_ALLOCATED_IMPL(IDBServer);

#if OS(SYMBIAN)
// Count only the immediate files of each database directory. A top origin's
// nested partitioned origins contain another directory level and get separate
// ClientOrigin budgets. Every enumeration/stat failure propagates as unknown.
static std::optional<uint64_t> quotaOriginUsage(const String& root, const ClientOrigin& origin)
{
    CheckedUint64 result = 0;
    for (auto version : { "v0"_s, "v1"_s }) {
        auto originPath = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin, root, version);
        auto size = FileSystem::directorySizeIfExists(originPath);
        if (!size)
            return std::nullopt;
        if (!*size)
            continue;
        auto directories = FileSystem::tryListDirectory(originPath);
        if (!directories)
            return std::nullopt;
        for (auto& entry : *directories) {
            auto directory = FileSystem::pathByAppendingComponent(originPath, entry);
            auto type = FileSystem::fileType(directory);
            if (!type)
                return std::nullopt;
            if (*type != FileSystem::FileType::Directory)
                continue;
            auto files = FileSystem::tryListDirectory(directory);
            if (!files)
                return std::nullopt;
            for (auto& name : *files) {
                auto path = FileSystem::pathByAppendingComponent(directory, name);
                auto fileType = FileSystem::fileType(path);
                if (!fileType)
                    return std::nullopt;
                if (*fileType != FileSystem::FileType::Regular)
                    continue;
                auto size = FileSystem::fileSize(path);
                if (!size)
                    return std::nullopt;
                result += *size;
                if (result.hasOverflowed())
                    return std::nullopt;
            }
        }
    }
    return result.value();
}

struct IDBServer::QuotaState {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(QuotaState);
public:
    static constexpr IDBStorageQuota::Limits limits { 32 * 1024 * 1024, 128 * 1024 * 1024, 16 * 1024 * 1024 };
    explicit QuotaState(const String& root)
        : root(root)
        , ledger(limits, FileSystem::directorySize(root).value_or(UINT64_MAX))
    {
    }

    uint64_t ensureDatabase(const IDBDatabaseIdentifier& identifier, const String& path)
    {
        if (auto iterator = databases.find(identifier); iterator != databases.end())
            return iterator->value.identifier;
        auto originEntry = origins.find(identifier.origin());
        if (originEntry == origins.end()) {
            auto size = quotaOriginUsage(root, identifier.origin());
            if (!size)
                return 0;
            auto id = ++nextIdentifier;
            ledger.addOrigin(id, *size);
            originEntry = origins.add(identifier.origin(), id).iterator;
        }
        auto size = FileSystem::directorySizeIfExists(path);
        if (!size)
            return 0;
        auto id = ++nextIdentifier;
        ledger.addDatabase(id, originEntry->value, *size);
        databases.add(identifier, Database { id, path, 0, false });
        return id;
    }

    uint64_t ensureTransaction(const IDBResourceIdentifier& transaction, uint64_t database)
    {
        return transactions.ensure(transaction, [&] {
            auto id = ++nextIdentifier;
            if (!ledger.addTransaction(id, database))
                valid = false;
            return id;
        }).iterator->value;
    }

    void refresh(IDBServer& server)
    {
        for (auto& [identifier, database] : databases) {
            auto active = server.m_uniqueIDBDatabaseMap.find(identifier);
            auto usage = active == server.m_uniqueIDBDatabaseMap.end() ? std::nullopt : active->value->quotaUsage();
            if (!usage && active != server.m_uniqueIDBDatabaseMap.end() && !active->value->filePath().isEmpty()) {
                ledger.setProjectedSize(database.identifier, std::nullopt);
            } else if (usage) {
                ledger.setProjectedSize(database.identifier, usage->totalSize);
                database.pendingDiskSpace = usage->pendingDiskSpace;
            } else {
                auto size = FileSystem::directorySizeIfExists(database.path);
                ledger.setProjectedSize(database.identifier, size);
                database.pendingDiskSpace = 0;
            }
        }
    }

    std::optional<uint64_t> pendingDiskSpace(uint64_t newReservation = 0, std::optional<uint64_t> committing = std::nullopt) const
    {
        auto reserved = ledger.reservedBytes(committing);
        if (!reserved)
            return std::nullopt;
        // Include every database's pending Blob copies and main-file growth.
        // Two copies of not-yet-materialized reservations cover SQLite data
        // and rollback journal growth; the final copy remains fallible and is
        // rolled back atomically if another process consumes the free space.
        CheckedUint64 total = *reserved;
        total += newReservation;
        total *= uint64_t { 2 };
        for (auto& database : databases.values())
            total += database.pendingDiskSpace;
        if (total.hasOverflowed())
            return std::nullopt;
        return total.value();
    }

    void reconcile(const IDBDatabaseIdentifier& identifier, const std::optional<IDBResourceIdentifier>& transaction = std::nullopt)
    {
        auto database = databases.find(identifier);
        if (database == databases.end())
            return;
        auto size = FileSystem::directorySizeIfExists(database->value.path);
        if (!size) {
            valid = false;
            return;
        }
        if (transaction) {
            auto entry = transactions.find(*transaction);
            if (entry == transactions.end())
                return;
            valid &= ledger.finishTransaction(entry->value, *size);
            transactions.remove(entry);
        } else
            valid &= ledger.updateCommittedSize(database->value.identifier, *size);
        database->value.pendingDiskSpace = 0;
    }

    struct Database {
        uint64_t identifier;
        String path;
        uint64_t pendingDiskSpace;
        bool openingNewDatabase;
    };
    String root;
    IDBStorageQuota ledger;
    HashMap<ClientOrigin, uint64_t> origins;
    HashMap<IDBDatabaseIdentifier, Database> databases;
    HashMap<IDBResourceIdentifier, uint64_t> transactions;
    uint64_t nextIdentifier { 0 };
    bool valid { true };
    bool needsRescan { false };
};
#endif

IDBServer::IDBServer(const String& databaseDirectoryPath, SpaceRequester&& spaceRequester, Lock& lock)
    : m_spaceRequester(WTF::move(spaceRequester))
    , m_lock(lock)
{
    ASSERT(!isMainThread());
    ASSERT(databaseDirectoryPath.isSafeToSendToAnotherThread());

    m_databaseDirectoryPath = databaseDirectoryPath;
    upgradeFilesIfNecessary();
#if OS(SYMBIAN)
    if (!m_databaseDirectoryPath.isEmpty()) {
        FileSystem::makeAllDirectories(m_databaseDirectoryPath);
        m_quota = makeUnique<QuotaState>(m_databaseDirectoryPath);
    }
#endif
}

IDBServer::~IDBServer()
{
    ASSERT(!isMainThread());

    for (auto& database : m_uniqueIDBDatabaseMap.values())
        database->immediateClose();
}

void IDBServer::registerConnection(IDBConnectionToClient& connection)
{
    ASSERT(!isMainThread());
    ASSERT(!m_connectionMap.contains(connection.identifier()));
    m_connectionMap.set(connection.identifier(), connection);
}

void IDBServer::unregisterConnection(IDBConnectionToClient& connection)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());
    ASSERT(m_connectionMap.contains(connection.identifier()));
    ASSERT(m_connectionMap.get(connection.identifier()) == &connection);

    connection.connectionToClientClosed();

    m_connectionMap.remove(connection.identifier());
}

void IDBServer::registerTransaction(UniqueIDBDatabaseTransaction& transaction)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());
    ASSERT(!m_transactions.contains(transaction.info().identifier()));
    m_transactions.set(transaction.info().identifier(), &transaction);
}

void IDBServer::unregisterTransaction(UniqueIDBDatabaseTransaction& transaction)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());
    ASSERT(m_transactions.contains(transaction.info().identifier()));
    ASSERT(m_transactions.get(transaction.info().identifier()) == &transaction);

    m_transactions.remove(transaction.info().identifier());
#if OS(SYMBIAN)
    if (m_quota && m_quota->needsRescan && m_transactions.isEmpty())
        m_quota = makeUnique<QuotaState>(m_databaseDirectoryPath);
#endif
}

void IDBServer::registerConnection(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(!m_databaseConnections.contains(connection.identifier()));
    m_databaseConnections.set(connection.identifier(), &connection);
}

void IDBServer::unregisterConnection(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(m_databaseConnections.contains(connection.identifier()));
    m_databaseConnections.remove(connection.identifier());
}

UniqueIDBDatabase& IDBServer::getOrCreateUniqueIDBDatabase(const IDBDatabaseIdentifier& identifier)
{
    ASSERT(!isMainThread());

    auto uniqueIDBDatabase = m_uniqueIDBDatabaseMap.add(identifier, nullptr);
    if (uniqueIDBDatabase.isNewEntry)
        uniqueIDBDatabase.iterator->value = makeUnique<UniqueIDBDatabase>(*this, identifier);

    return *uniqueIDBDatabase.iterator->value;
}

std::unique_ptr<IDBBackingStore> IDBServer::createBackingStore(const IDBDatabaseIdentifier& identifier)
{
    ASSERT(!isMainThread());
    if (m_databaseDirectoryPath.isEmpty())
        return makeUnique<MemoryIDBBackingStore>(identifier);

    if (identifier.isTransient())
        return makeUnique<MemoryIDBBackingStore>(identifier);

    auto directory = upgradedDatabaseDirectory(identifier);
#if OS(SYMBIAN)
    if (m_quota)
        m_quota->ensureDatabase(identifier, directory);
#endif
    return makeUnique<SQLiteIDBBackingStore>(identifier, directory);
}

void IDBServer::openDatabase(const IDBOpenRequestData& requestData)
{
    LOG(IndexedDB, "IDBServer::openDatabase");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    CheckedRef uniqueIDBDatabase = getOrCreateUniqueIDBDatabase(requestData.databaseIdentifier());

    auto connectionIdentifier = requestData.requestIdentifier().connectionIdentifier();
    if (!connectionIdentifier)
        return;

    RefPtr connection = m_connectionMap.get(*connectionIdentifier);
    if (!connection) {
        // If the connection back to the client is gone, there's no way to open the database as
        // well as no way to message back failure.
        return;
    }

    uniqueIDBDatabase->openDatabaseConnection(*connection, requestData);
}

void IDBServer::deleteDatabase(const IDBOpenRequestData& requestData)
{
    LOG(IndexedDB, "IDBServer::deleteDatabase - %s", requestData.databaseIdentifier().loggingString().utf8().data());
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    auto connectionIdentifier = requestData.requestIdentifier().connectionIdentifier();
    if (!connectionIdentifier)
        return;

    RefPtr connection = m_connectionMap.get(*connectionIdentifier);
    if (!connection) {
        // If the connection back to the client is gone, there's no way to delete the database as
        // well as no way to message back failure.
        return;
    }

    IDBDatabaseIdentifier databaseIdentifier;
    {
        CheckedPtr database = m_uniqueIDBDatabaseMap.get(requestData.databaseIdentifier());
        if (!database)
            database = &getOrCreateUniqueIDBDatabase(requestData.databaseIdentifier());

        database->handleDelete(*connection, requestData);
        if (!database->tryClose())
            return;
        databaseIdentifier = database->identifier();
    }
    m_uniqueIDBDatabaseMap.remove(databaseIdentifier);
}

void IDBServer::abortTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::abortTransaction");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        // If there is no transaction there is nothing to abort.
        // We also have no access to a connection over which to message failure-to-abort.
        return;
    }

    transaction->abort();
}

RefPtr<UniqueIDBDatabaseTransaction> IDBServer::idbTransaction(const IDBRequestData& requestData) const
{
    return m_transactions.get(requestData.transactionIdentifier());
}

void IDBServer::createObjectStore(const IDBRequestData& requestData, const IDBObjectStoreInfo& info)
{
    LOG(IndexedDB, "IDBServer::createObjectStore");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->createObjectStore(requestData, info);
}

void IDBServer::deleteObjectStore(const IDBRequestData& requestData, const String& objectStoreName)
{
    LOG(IndexedDB, "IDBServer::deleteObjectStore");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->deleteObjectStore(requestData, objectStoreName);
}

void IDBServer::renameObjectStore(const IDBRequestData& requestData, IDBObjectStoreIdentifier objectStoreIdentifier, const String& newName)
{
    LOG(IndexedDB, "IDBServer::renameObjectStore");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->renameObjectStore(requestData, objectStoreIdentifier, newName);
}

void IDBServer::clearObjectStore(const IDBRequestData& requestData, IDBObjectStoreIdentifier objectStoreIdentifier)
{
    LOG(IndexedDB, "IDBServer::clearObjectStore");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    transaction->clearObjectStore(requestData, objectStoreIdentifier);
}

void IDBServer::createIndex(const IDBRequestData& requestData, const IDBIndexInfo& info)
{
    LOG(IndexedDB, "IDBServer::createIndex");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->createIndex(requestData, info);
}

void IDBServer::deleteIndex(const IDBRequestData& requestData, IDBObjectStoreIdentifier objectStoreIdentifier, const String& indexName)
{
    LOG(IndexedDB, "IDBServer::deleteIndex");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->deleteIndex(requestData, objectStoreIdentifier, indexName);
}

void IDBServer::renameIndex(const IDBRequestData& requestData, IDBObjectStoreIdentifier objectStoreIdentifier, IDBIndexIdentifier indexIdentifier, const String& newName)
{
    LOG(IndexedDB, "IDBServer::renameIndex");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    ASSERT(transaction->isVersionChange());
    transaction->renameIndex(requestData, objectStoreIdentifier, indexIdentifier, newName);
}

void IDBServer::putOrAdd(const IDBRequestData& requestData, const IDBKeyData& keyData, const IDBValue& value, const IndexIDToIndexKeyMap& indexKeys, IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    LOG(IndexedDB, "IDBServer::putOrAdd");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction || !transaction->database()) {
#if OS(SYMBIAN)
        for (auto& path : value.blobFilePaths())
            FileSystem::deleteFile(path);
#endif
        return;
    }

    transaction->putOrAdd(requestData, keyData, value, indexKeys, overwriteMode);
}

void IDBServer::getRecord(const IDBRequestData& requestData, const IDBGetRecordData& getRecordData)
{
    LOG(IndexedDB, "IDBServer::getRecord");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    transaction->getRecord(requestData, getRecordData);
}

void IDBServer::getAllRecords(const IDBRequestData& requestData, const IDBGetAllRecordsData& getAllRecordsData)
{
    LOG(IndexedDB, "IDBServer::getAllRecords");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    transaction->getAllRecords(requestData, getAllRecordsData);
}

void IDBServer::getCount(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBServer::getCount");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    transaction->getCount(requestData, keyRangeData);
}

void IDBServer::deleteRecord(const IDBRequestData& requestData, const IDBKeyRangeData& keyRangeData)
{
    LOG(IndexedDB, "IDBServer::deleteRecord");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    transaction->deleteRecord(requestData, keyRangeData);
}

void IDBServer::openCursor(const IDBRequestData& requestData, const IDBCursorInfo& info)
{
    LOG(IndexedDB, "IDBServer::openCursor");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    transaction->openCursor(requestData, info);
}

void IDBServer::iterateCursor(const IDBRequestData& requestData, const IDBIterateCursorData& data)
{
    LOG(IndexedDB, "IDBServer::iterateCursor");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    transaction->iterateCursor(requestData, data);
}

void IDBServer::establishTransaction(IDBDatabaseConnectionIdentifier databaseConnectionIdentifier, const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "IDBServer::establishTransaction");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    IDBDatabaseIdentifier databaseIdentifier;
    {
        CheckedPtr database = databaseConnection->database();
        databaseConnection->establishTransaction(info);
        if (!database->tryClose())
            return;
        databaseIdentifier = database->identifier();
    }
    m_uniqueIDBDatabaseMap.remove(databaseIdentifier);
}

void IDBServer::commitTransaction(const IDBResourceIdentifier& transactionIdentifier, uint64_t handledRequestResultsCount)
{
    LOG(IndexedDB, "IDBServer::commitTransaction");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr transaction = m_transactions.get(transactionIdentifier);
    if (!transaction) {
        // If there is no transaction there is nothing to commit.
        // We also have no access to a connection over which to message failure-to-commit.
        return;
    }

    transaction->commit(handledRequestResultsCount);
}

void IDBServer::didFinishHandlingVersionChangeTransaction(IDBDatabaseConnectionIdentifier databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::didFinishHandlingVersionChangeTransaction - %s", transactionIdentifier.loggingString().utf8().data());
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    if (RefPtr connection = m_databaseConnections.get(databaseConnectionIdentifier))
        connection->didFinishHandlingVersionChange(transactionIdentifier);
}

void IDBServer::databaseConnectionPendingClose(IDBDatabaseConnectionIdentifier databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBServer::databaseConnectionPendingClose - %" PRIu64, databaseConnectionIdentifier.toUInt64());
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    databaseConnection->connectionPendingCloseFromClient();
}

void IDBServer::databaseConnectionClosed(IDBDatabaseConnectionIdentifier databaseConnectionIdentifier)
{
    LOG(IndexedDB, "IDBServer::databaseConnectionClosed - %" PRIu64, databaseConnectionIdentifier.toUInt64());
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    RefPtr databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    IDBDatabaseIdentifier databaseIdentifier;
    {
        CheckedPtr database = databaseConnection->database();
        databaseConnection->connectionClosedFromClient();
        if (!database->tryClose())
            return;
        databaseIdentifier = database->identifier();
    }
    m_uniqueIDBDatabaseMap.remove(databaseIdentifier);
}

void IDBServer::abortOpenAndUpgradeNeeded(IDBDatabaseConnectionIdentifier databaseConnectionIdentifier, const std::optional<IDBResourceIdentifier>& transactionIdentifier)
{
    LOG(IndexedDB, "IDBServer::abortOpenAndUpgradeNeeded");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    if (transactionIdentifier) {
        if (RefPtr transaction = m_transactions.get(*transactionIdentifier))
            transaction->abortWithoutCallback();
    }

    RefPtr databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier);
    if (!databaseConnection)
        return;

    databaseConnection->connectionClosedFromClient();
}

void IDBServer::didFireVersionChangeEvent(IDBDatabaseConnectionIdentifier databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosed)
{
    LOG(IndexedDB, "IDBServer::didFireVersionChangeEvent");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    if (RefPtr databaseConnection = m_databaseConnections.get(databaseConnectionIdentifier))
        databaseConnection->didFireVersionChangeEvent(requestIdentifier, connectionClosed);
}

void IDBServer::didGenerateIndexKeyForRecord(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& requestIdentifier, const IDBIndexInfo& indexInfo, const IDBKeyData& key, const IndexKey& indexKey, std::optional<int64_t> recordID)
{
    if (RefPtr transaction = m_transactions.get(transactionIdentifier))
        transaction->didGenerateIndexKeyForRecord(requestIdentifier, indexInfo, key, indexKey, recordID);
}

void IDBServer::openDBRequestCancelled(const IDBOpenRequestData& requestData)
{
    LOG(IndexedDB, "IDBServer::openDBRequestCancelled");
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    IDBDatabaseIdentifier databaseIdentifier;
    {
        CheckedPtr uniqueIDBDatabase = m_uniqueIDBDatabaseMap.get(requestData.databaseIdentifier());
        if (!uniqueIDBDatabase)
            return;

        uniqueIDBDatabase->openDBRequestCancelled(requestData.requestIdentifier());
        if (!uniqueIDBDatabase->tryClose())
            return;

        databaseIdentifier = uniqueIDBDatabase->identifier();
    }
    m_uniqueIDBDatabaseMap.remove(databaseIdentifier);
}

static void getDatabaseNameAndVersionFromOriginDirectory(const String& directory, HashSet<String>& excludedDatabasePaths, Vector<IDBDatabaseNameAndVersion>& result)
{
    Vector<String> databaseDirectoryNames = FileSystem::listDirectory(directory);
    for (auto& databaseDirectoryName : databaseDirectoryNames) {
        auto fullDatabasePath = SQLiteIDBBackingStore::fullDatabasePathForDirectory(FileSystem::pathByAppendingComponent(directory, databaseDirectoryName));
        if (excludedDatabasePaths.contains(fullDatabasePath))
            continue;

        if (auto nameAndVersion = SQLiteIDBBackingStore::databaseNameAndVersionFromFile(fullDatabasePath))
            result.append(WTF::move(*nameAndVersion));
    }
}

void IDBServer::getAllDatabaseNamesAndVersions(IDBConnectionIdentifier serverConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, const ClientOrigin& origin)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    Vector<IDBDatabaseNameAndVersion> result;
    HashSet<String> visitedDatabasePaths;

    for (auto& database : m_uniqueIDBDatabaseMap.values()) {
        if (database->identifier().origin() != origin)
            continue;

        auto path = database->filePath();
        if (!path.isEmpty())
            visitedDatabasePaths.add(path);

        if (auto nameAndVersion = database->nameAndVersion())
            result.append(WTF::move(*nameAndVersion));
    }

    auto oldDirectory = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin, m_databaseDirectoryPath, "v0"_s);
    getDatabaseNameAndVersionFromOriginDirectory(oldDirectory, visitedDatabasePaths, result);

    auto directory = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin, m_databaseDirectoryPath, "v1"_s);
    getDatabaseNameAndVersionFromOriginDirectory(directory, visitedDatabasePaths, result);

    RefPtr connection = m_connectionMap.get(serverConnectionIdentifier);
    if (!connection)
        return;

    connection->didGetAllDatabaseNamesAndVersions(requestIdentifier, WTF::move(result));
}

static void collectOriginsForVersion(const String& versionPath, HashSet<WebCore::SecurityOriginData>& securityOrigins)
{
    for (auto& databaseIdentifier : FileSystem::listDirectory(versionPath)) {
        if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier)) {
            securityOrigins.add(WTF::move(*securityOrigin));
        
            for (auto& databaseIdentifier : FileSystem::listDirectory(FileSystem::pathByAppendingComponent(versionPath, databaseIdentifier))) {
                if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
                    securityOrigins.add(WTF::move(*securityOrigin));
            }
        }
    }
}

HashSet<SecurityOriginData> IDBServer::getOrigins() const
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    if (m_databaseDirectoryPath.isEmpty())
        return { };

    HashSet<WebCore::SecurityOriginData> securityOrigins;
    collectOriginsForVersion(FileSystem::pathByAppendingComponent(m_databaseDirectoryPath, "v0"_s), securityOrigins);
    collectOriginsForVersion(FileSystem::pathByAppendingComponent(m_databaseDirectoryPath, "v1"_s), securityOrigins);

    return securityOrigins;
}

void IDBServer::closeAndDeleteDatabasesModifiedSince(WallTime modificationTime)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    // If the modification time is in the future, don't both doing anything.
    if (modificationTime > WallTime::now())
        return;

    for (auto& database : m_uniqueIDBDatabaseMap.values())
        database->immediateClose();

    m_uniqueIDBDatabaseMap.clear();

    if (!m_databaseDirectoryPath.isEmpty()) {
        removeDatabasesModifiedSinceForVersion(modificationTime, "v0"_s);
        removeDatabasesModifiedSinceForVersion(modificationTime, "v1"_s);
    }
#if OS(SYMBIAN)
    rescanQuotaAfterAdministrativeChange();
#endif
}

void IDBServer::closeDatabasesForOrigins(const Vector<SecurityOriginData>& targetOrigins, Function<bool(const SecurityOriginData&, const ClientOrigin&)>&& filter)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    HashSet<CheckedPtr<UniqueIDBDatabase>> openDatabases;
    for (auto& database : m_uniqueIDBDatabaseMap.values()) {
        const auto& databaseOrigin = database->identifier().origin();
        bool filtered = std::ranges::any_of(targetOrigins, [&databaseOrigin, &filter](auto& targetOrigin) {
            return filter(targetOrigin, databaseOrigin);
        });
        if (filtered)
            openDatabases.add(database.get());
    }

    for (auto& database : openDatabases) {
        database->immediateClose();
        m_uniqueIDBDatabaseMap.remove(database->identifier());
    }
}

void IDBServer::closeAndDeleteDatabasesForOrigins(const Vector<SecurityOriginData>& origins)
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    closeDatabasesForOrigins(origins, [](const SecurityOriginData& origin, const ClientOrigin& databaseOrigin) -> bool {
        return databaseOrigin.isRelated(origin);
    });

    if (!m_databaseDirectoryPath.isEmpty()) {
        removeDatabasesWithOriginsForVersion(origins, "v0"_s);
        removeDatabasesWithOriginsForVersion(origins, "v1"_s);
    }
#if OS(SYMBIAN)
    rescanQuotaAfterAdministrativeChange();
#endif
}

static void removeAllDatabasesForFullOriginPath(const String& originPath, WallTime modifiedSince)
{
    LOG(IndexedDB, "removeAllDatabasesForOriginPath with originPath %s", originPath.utf8().data());
    Vector<String> databaseNames = FileSystem::listDirectory(originPath);

    for (auto& databaseName : databaseNames) {
        auto databasePath = FileSystem::pathByAppendingComponent(originPath, databaseName);
        String databaseFile = FileSystem::pathByAppendingComponent(databasePath, "IndexedDB.sqlite3"_s);
        if (modifiedSince > -WallTime::infinity() && FileSystem::fileExists(databaseFile)) {
            auto modificationTime = FileSystem::fileModificationTime(databaseFile);
            if (!modificationTime)
                continue;

            if (modificationTime.value() < modifiedSince)
                continue;
        }

        // Deleting this database means we need to delete all files that represent it.
        // This includes:
        //     - The directory itself, which is named after the database.
        //     - IndexedDB.sqlite3 and related SQLite files.
        //     - Blob files that we stored in the directory.
        //
        // To be conservative, we should *not* try to delete files that are unexpected;
        // We should only delete files we think we put there.
        for (auto& fileName : FileSystem::listDirectory(databasePath)) {
            // IndexedDB blob files are named "N.blob" where N is a decimal integer,
            // so those are the only blob files we should be trying to delete.
            auto fileNameLength = fileName.length();
            if (fileNameLength < 6)
                continue;
            if (!fileName.endsWith(".blob"_s))
                continue;

            bool validFileName = true;
            for (unsigned i = 0; i < fileNameLength - 5; ++i) {
                if (!isASCIIDigit(fileName[i])) {
                    validFileName = false;
                    break;
                }
            }

            if (validFileName)
                FileSystem::deleteFile(FileSystem::pathByAppendingComponent(databasePath, fileName));
        }

        // Now delete IndexedDB.sqlite3 and related SQLite files.
        SQLiteFileSystem::deleteDatabaseFile(databaseFile);

        // And finally, if we can, delete the empty directory.
        FileSystem::deleteEmptyDirectory(databasePath);
    }

    // If no databases remain for this origin, we can delete the origin directory as well.
    FileSystem::deleteEmptyDirectory(originPath);
}

static void removeAllDatabasesForOriginPath(const String& originPath, WallTime modifiedSince)
{
    String databaseIdentifier = FileSystem::lastComponentOfPathIgnoringTrailingSlash(originPath);
    if (!SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
        return;
    
    auto directoryNames = FileSystem::listDirectory(originPath);
    for (auto& databaseIdentifier : directoryNames) {
        if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
            removeAllDatabasesForFullOriginPath(FileSystem::pathByAppendingComponent(originPath, databaseIdentifier), modifiedSince);
    }
    
    removeAllDatabasesForFullOriginPath(originPath, modifiedSince);
}

void IDBServer::removeDatabasesModifiedSinceForVersion(WallTime modifiedSince, const String& version)
{
    String versionPath = FileSystem::pathByAppendingComponent(m_databaseDirectoryPath, version);
    for (auto& databaseIdentifier : FileSystem::listDirectory(versionPath)) {
        if (auto securityOrigin = SecurityOriginData::fromDatabaseIdentifier(databaseIdentifier))
            removeAllDatabasesForOriginPath(FileSystem::pathByAppendingComponent(versionPath, databaseIdentifier), modifiedSince);
    }
}

void IDBServer::removeDatabasesWithOriginsForVersion(const Vector<SecurityOriginData> &origins, const String& version)
{
    String versionPath = FileSystem::pathByAppendingComponent(m_databaseDirectoryPath, version);
    for (const auto& origin : origins) {
        String originPath = FileSystem::pathByAppendingComponent(versionPath, origin.databaseIdentifier());
        removeAllDatabasesForOriginPath(originPath, -WallTime::infinity());
        
        for (auto& topOrigin : FileSystem::listDirectory(versionPath)) {
            auto topOriginPath = FileSystem::pathByAppendingComponent(versionPath, topOrigin);
            originPath = FileSystem::pathByAppendingComponent(topOriginPath, origin.databaseIdentifier());
            removeAllDatabasesForOriginPath(originPath, -WallTime::infinity());
        }
    }
}

void IDBServer::renameOrigin(const WebCore::SecurityOriginData& oldOrigin, const WebCore::SecurityOriginData& newOrigin)
{
    Vector<SecurityOriginData> targetOrigins = { oldOrigin };
    closeDatabasesForOrigins(targetOrigins, [](const SecurityOriginData& targetOrigin, const ClientOrigin& databaseOrigin) -> bool {
        return databaseOrigin.topOrigin == targetOrigin;
    });

    auto versionPath = FileSystem::pathByAppendingComponent(m_databaseDirectoryPath, "v1"_s);
    auto oldOriginPath = FileSystem::pathByAppendingComponent(versionPath, oldOrigin.databaseIdentifier());
    auto newOriginPath = FileSystem::pathByAppendingComponent(versionPath, newOrigin.databaseIdentifier());
    if (FileSystem::fileExists(oldOriginPath))
        FileSystem::moveFile(oldOriginPath, newOriginPath);
#if OS(SYMBIAN)
    rescanQuotaAfterAdministrativeChange();
#endif
}

void IDBServer::requestSpace(const ClientOrigin& origin, uint64_t taskSize, CompletionHandler<void(bool)>&& completionHandler) WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ASSERT(!isMainThread());
    ASSERT(m_lock.isHeld());

    // Release lock because space requesting could be blocked.
    m_lock.unlock();
    bool result = m_spaceRequester(origin, taskSize);
    m_lock.lock();

    completionHandler(result);
}

#if OS(SYMBIAN)
void IDBServer::rescanQuotaAfterAdministrativeChange()
{
    if (!m_quota)
        return;
    // A scan while unrelated transactions are live would turn uncommitted
    // pages into committed baseline and lose their reservations. Retain the
    // conservative old charge until all those transactions finish.
    if (m_transactions.isEmpty())
        m_quota = makeUnique<QuotaState>(m_databaseDirectoryPath);
    else
        m_quota->needsRescan = true;
}

void IDBServer::requestSpaceForOpen(const IDBDatabaseIdentifier& identifier, uint64_t size, CompletionHandler<void(bool)>&& completion)
{
    ASSERT(m_lock.isHeld());
    if (!m_quota || identifier.isTransient())
        return requestSpace(identifier.origin(), size, WTF::move(completion));
    auto path = upgradedDatabaseDirectory(identifier);
    auto database = m_quota->ensureDatabase(identifier, path);
    // Existing databases stay readable, including old data above today's quota.
    if (FileSystem::fileExists(SQLiteIDBBackingStore::fullDatabasePathForDirectory(path)))
        return completion(true);
    if (!database)
        return completion(false);
    m_quota->refresh(*this);
    // Opening a new SQLite schema is synchronous on this queue. Check a
    // conservative initial allowance here, then verify the actual schema in
    // didOpenBackingStore before reporting open success.
    auto temporary = ++m_quota->nextIdentifier;
    m_quota->ledger.addTransaction(temporary, database);
    constexpr uint64_t schemaAllowance = 64 * 1024;
    auto pending = m_quota->pendingDiskSpace(schemaAllowance);
    bool granted = m_quota->valid && pending && m_quota->ledger.reserve(temporary, schemaAllowance, FileSystem::volumeFreeSpace(m_databaseDirectoryPath), *pending);
    m_quota->valid &= m_quota->ledger.finishTransaction(temporary, FileSystem::directorySize(path).value_or(UINT64_MAX));
    m_quota->databases.find(identifier)->value.openingNewDatabase = granted;
    completion(granted);
}

void IDBServer::requestSpaceForTransaction(const IDBDatabaseIdentifier& identifier, const IDBResourceIdentifier& transaction, uint64_t size, CompletionHandler<void(bool)>&& completion)
{
    ASSERT(m_lock.isHeld());
    if (!m_quota || identifier.isTransient())
        return requestSpace(identifier.origin(), size, WTF::move(completion));
    if (!size)
        return completion(true);
    auto database = m_quota->ensureDatabase(identifier, upgradedDatabaseDirectory(identifier));
    if (!database)
        return completion(false);
    auto quotaTransaction = m_quota->ensureTransaction(transaction, database);
    m_quota->refresh(*this);
    auto pending = m_quota->pendingDiskSpace(size);
    completion(m_quota->valid && pending && m_quota->ledger.reserve(quotaTransaction, size, FileSystem::volumeFreeSpace(m_databaseDirectoryPath), *pending));
}

IDBError IDBServer::checkQuotaBeforeCommit(UniqueIDBDatabaseTransaction& transaction, IDBBackingStore& backingStore)
{
    ASSERT(m_lock.isHeld());
    if (!m_quota || backingStore.isEphemeral() || transaction.isReadOnly())
        return IDBError { };
    auto& identifier = transaction.database()->identifier();
    auto database = m_quota->ensureDatabase(identifier, upgradedDatabaseDirectory(identifier));
    if (!database)
        return IDBError { ExceptionCode::QuotaExceededError, "Unable to determine IndexedDB storage usage"_s };
    auto quotaTransaction = m_quota->ensureTransaction(transaction.info().identifier(), database);
    m_quota->refresh(*this);
    // Do not replace a failed active-store usage query with its smaller on-disk
    // size: uncommitted pages or a missing staged Blob would then evade quota.
    auto usage = backingStore.quotaUsage();
    m_quota->ledger.setProjectedSize(database, usage ? std::optional<uint64_t> { usage->totalSize } : std::nullopt);
    auto pending = m_quota->pendingDiskSpace(0, quotaTransaction);
    if (!m_quota->valid || !pending || !m_quota->ledger.canCommit(quotaTransaction, FileSystem::volumeFreeSpace(m_databaseDirectoryPath), *pending, usage ? usage->pendingDiskSpace : UINT64_MAX))
        return IDBError { ExceptionCode::QuotaExceededError, "IndexedDB origin, port or free-disk quota exceeded"_s };
    return IDBError { };
}

IDBError IDBServer::didOpenBackingStore(const IDBDatabaseIdentifier& identifier, IDBBackingStore& backingStore)
{
    if (!m_quota || backingStore.isEphemeral())
        return IDBError { };
    auto entry = m_quota->databases.find(identifier);
    if (entry == m_quota->databases.end())
        return IDBError { }; // Existing data remains readable after a failed quota scan.
    auto& database = entry->value;
    if (std::exchange(database.openingNewDatabase, false)) {
        m_quota->refresh(*this);
        auto usage = backingStore.quotaUsage();
        m_quota->ledger.setProjectedSize(database.identifier, usage ? std::optional<uint64_t> { usage->totalSize } : std::nullopt);
        auto temporary = ++m_quota->nextIdentifier;
        m_quota->ledger.addTransaction(temporary, database.identifier);
        auto pending = m_quota->pendingDiskSpace();
        bool granted = m_quota->valid && pending && m_quota->ledger.canCommit(temporary, FileSystem::volumeFreeSpace(m_databaseDirectoryPath), *pending);
        if (!granted)
            backingStore.deleteBackingStore(); // Only the newly created schema.
        auto size = FileSystem::directorySizeIfExists(database.path);
        m_quota->valid &= m_quota->ledger.finishTransaction(temporary, size.value_or(UINT64_MAX));
        if (!granted)
            return IDBError { ExceptionCode::QuotaExceededError, "IndexedDB schema exceeds available storage quota"_s };
    } else
        m_quota->reconcile(identifier);
    return IDBError { };
}

void IDBServer::didDeleteBackingStore(const IDBDatabaseIdentifier& identifier)
{
    if (m_quota && !identifier.isTransient())
        m_quota->reconcile(identifier);
}

void IDBServer::didFinishQuotaTransaction(UniqueIDBDatabaseTransaction& transaction)
{
    if (m_quota && transaction.database())
        m_quota->reconcile(transaction.database()->identifier(), transaction.info().identifier());
}
#endif

uint64_t IDBServer::diskUsage(const String& rootDirectory, const ClientOrigin& origin)
{
    ASSERT(!isMainThread());

    auto oldVersionOriginDirectory = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin, rootDirectory, "v0"_s);
    auto newVersionOriginDirectory = IDBDatabaseIdentifier::databaseDirectoryRelativeToRoot(origin, rootDirectory, "v1"_s);
    return SQLiteIDBBackingStore::databasesSizeForDirectory(oldVersionOriginDirectory) + SQLiteIDBBackingStore::databasesSizeForDirectory(newVersionOriginDirectory);
}

void IDBServer::upgradeFilesIfNecessary()
{
    if (m_databaseDirectoryPath.isEmpty() || !FileSystem::fileExists(m_databaseDirectoryPath))
        return;

    String newVersionDirectory = FileSystem::pathByAppendingComponent(m_databaseDirectoryPath, "v1"_s);
    if (!FileSystem::fileExists(newVersionDirectory))
        FileSystem::makeAllDirectories(newVersionDirectory);
}

String IDBServer::upgradedDatabaseDirectory(const WebCore::IDBDatabaseIdentifier& identifier)
{
    String oldOriginDirectory = identifier.databaseDirectoryRelativeToRoot(m_databaseDirectoryPath, "v0"_s);
    String oldDatabaseDirectory = FileSystem::pathByAppendingComponent(oldOriginDirectory, SQLiteIDBBackingStore::encodeDatabaseName(identifier.databaseName()));
    String newOriginDirectory = identifier.databaseDirectoryRelativeToRoot(m_databaseDirectoryPath, "v1"_s);
    String fileNameHash = SQLiteFileSystem::computeHashForFileName(identifier.databaseName());
    String newDatabaseDirectory = FileSystem::pathByAppendingComponent(newOriginDirectory, fileNameHash);
    FileSystem::makeAllDirectories(newDatabaseDirectory);

    if (FileSystem::fileExists(oldDatabaseDirectory)) {
        FileSystem::moveFile(oldDatabaseDirectory, newDatabaseDirectory);
        FileSystem::deleteEmptyDirectory(oldOriginDirectory);
    }

    return newDatabaseDirectory;
}

} // namespace IDBServer
} // namespace WebCore
