/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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
#include "SQLiteIDBTransaction.h"

#include "IDBCursorInfo.h"
#if OS(SYMBIAN)
#include "IDBBlobCommit.h"
#include <algorithm>
#include <wtf/CheckedArithmetic.h>
#endif
#include "IndexedDB.h"
#include "Logging.h"
#include "SQLiteIDBBackingStore.h"
#include "SQLiteIDBCursor.h"
#include "SQLiteTransaction.h"
#include <wtf/FileSystem.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace IDBServer {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SQLiteIDBTransaction);

SQLiteIDBTransaction::SQLiteIDBTransaction(SQLiteIDBBackingStore& backingStore, const IDBTransactionInfo& info)
    : m_info(info)
    , m_backingStore(backingStore)
{
}

SQLiteIDBTransaction::~SQLiteIDBTransaction()
{
    if (inProgress())
        m_sqliteTransaction->rollback();
#if OS(SYMBIAN)
    deleteBlobTemporaryFiles();
#endif

    // Explicitly clear cursors, as that also unregisters them from the backing store.
    clearCursors();
}

IDBError SQLiteIDBTransaction::begin(SQLiteDatabase& database)
{
    ASSERT(!m_sqliteTransaction);

    if (isReadOnly()) {
        m_sqliteDatabase = &database;
        return IDBError { };
    }

    m_sqliteTransaction = makeUnique<SQLiteTransaction>(database, true);
    m_sqliteTransaction->begin();

    if (m_sqliteTransaction->inProgress())
        return IDBError { };

    return IDBError { ExceptionCode::UnknownError, "Could not start SQLite transaction in database backend"_s };
}

IDBError SQLiteIDBTransaction::commit()
{
    LOG(IndexedDB, "SQLiteIDBTransaction::commit");

    if (isReadOnly()) {
        reset();
        return IDBError { };
    }

    if (!m_sqliteTransaction || !m_sqliteTransaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "No SQLite transaction in progress to commit"_s };

#if OS(SYMBIAN)
    // Symbian copies Blobs (there are no filesystem hard links). Prepare every
    // destination while SQL can still roll back; an I/O failure must not leave
    // a successfully committed record pointing at a missing Blob.
    Vector<String> preparedPaths;
    auto result = commitWithPreparedIDBBlobs(m_blobTemporaryAndStoredFilenames.size(), [&](size_t index) {
        auto& entry = m_blobTemporaryAndStoredFilenames[index];
        if (m_blobRemovedFilenames.contains(entry.second))
            return true; // Added and then deleted/replaced in this transaction.
        auto target = FileSystem::pathByAppendingComponent(m_backingStore->databaseDirectory(), entry.second);
        auto sourceSize = FileSystem::fileSize(entry.first);
        if (!sourceSize || FileSystem::fileExists(target))
            return false;
        // Symbian copy uses O_EXCL and cleans its own partial destination on
        // failure. Acquire cleanup ownership only after successful creation;
        // otherwise a racing creator's file could be removed on rollback.
        if (!FileSystem::hardLinkOrCopyFile(entry.first, target))
            return false;
        preparedPaths.append(target);
        return FileSystem::fileSize(target) == sourceSize;
    }, [&] {
        for (auto& path : preparedPaths)
            FileSystem::deleteFile(path);
    }, [&] {
        m_sqliteTransaction->commit();
        return !m_sqliteTransaction->inProgress();
    }, [&] {
        if (m_sqliteTransaction->inProgress())
            m_sqliteTransaction->rollback();
    });

    deleteBlobTemporaryFiles();
    m_blobTemporaryAndStoredFilenames.clear();
    if (result != IDBBlobCommitResult::Success) {
        // These files belonged to the old committed state, which rollback kept.
        m_blobRemovedFilenames.clear();
        reset();
        return IDBError { ExceptionCode::UnknownError, result == IDBBlobCommitResult::PrepareFailed
            ? "Unable to prepare Blob files; SQLite transaction was rolled back"_s
            : "Unable to commit SQLite transaction in database backend"_s };
    }
    deleteBlobFilesIfNecessary();
#else
    m_sqliteTransaction->commit();
    if (m_sqliteTransaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Unable to commit SQLite transaction in database backend"_s };

    deleteBlobFilesIfNecessary();
    moveBlobFilesIfNecessary();
#endif

    reset();
    return IDBError { };
}

#if OS(SYMBIAN)
void SQLiteIDBTransaction::registerBlobTemporaryFiles(const Vector<String>& paths)
{
    for (auto& path : paths)
        m_blobTemporaryFiles.add(path);
}

void SQLiteIDBTransaction::releaseUnusedBlobTemporaryFiles(const Vector<String>& paths)
{
    for (auto& path : paths) {
        if (std::ranges::any_of(m_blobTemporaryAndStoredFilenames, [&](auto& entry) { return entry.first == path; }))
            continue;
        if (m_blobTemporaryFiles.remove(path))
            FileSystem::deleteFile(path);
    }
}

void SQLiteIDBTransaction::deleteBlobTemporaryFiles()
{
    for (auto& path : m_blobTemporaryFiles)
        FileSystem::deleteFile(path);
    m_blobTemporaryFiles.clear();
}

std::optional<uint64_t> SQLiteIDBTransaction::pendingBlobBytes() const
{
    CheckedUint64 total = 0;
    HashSet<String> destinations;
    for (auto& entry : m_blobTemporaryAndStoredFilenames) {
        if (m_blobRemovedFilenames.contains(entry.second) || !destinations.add(entry.second).isNewEntry)
            continue;
        auto size = FileSystem::fileSize(entry.first);
        if (!size)
            return std::nullopt;
        total += *size;
        if (total.hasOverflowed())
            return std::nullopt;
    }
    return total.value();
}
#endif

void SQLiteIDBTransaction::moveBlobFilesIfNecessary()
{
    ASSERT(!isReadOnly());

    String databaseDirectory = m_backingStore->databaseDirectory();
    for (auto& entry : m_blobTemporaryAndStoredFilenames) {
        if (!FileSystem::hardLinkOrCopyFile(entry.first, FileSystem::pathByAppendingComponent(databaseDirectory, entry.second)))
            LOG_ERROR("Failed to link/copy temporary blob file '%s' to location '%s'", entry.first.utf8().data(), FileSystem::pathByAppendingComponent(databaseDirectory, entry.second).utf8().data());

        FileSystem::deleteFile(entry.first);
    }

    m_blobTemporaryAndStoredFilenames.clear();
}

void SQLiteIDBTransaction::deleteBlobFilesIfNecessary()
{
    ASSERT(!isReadOnly());

    if (m_blobRemovedFilenames.isEmpty())
        return;

    String databaseDirectory = m_backingStore->databaseDirectory();
    for (auto& entry : m_blobRemovedFilenames) {
        String fullPath = FileSystem::pathByAppendingComponent(databaseDirectory, entry);

        FileSystem::deleteFile(fullPath);
    }

    m_blobRemovedFilenames.clear();
}

IDBError SQLiteIDBTransaction::abort()
{
    if (isReadOnly()) {
        reset();
        return IDBError { };
    }

#if OS(SYMBIAN)
    deleteBlobTemporaryFiles();
#else
    for (auto& entry : m_blobTemporaryAndStoredFilenames)
        FileSystem::deleteFile(entry.first);
#endif

    m_blobTemporaryAndStoredFilenames.clear();

    if (!m_sqliteTransaction || !m_sqliteTransaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "No SQLite transaction in progress to abort"_s };

    m_sqliteTransaction->rollback();

    if (m_sqliteTransaction->inProgress())
        return IDBError { ExceptionCode::UnknownError, "Unable to abort SQLite transaction in database backend"_s };

    reset();
    return IDBError { };
}

void SQLiteIDBTransaction::reset()
{
    m_sqliteTransaction = nullptr;
    clearCursors();
    ASSERT(m_blobTemporaryAndStoredFilenames.isEmpty());
}

std::unique_ptr<SQLiteIDBCursor> SQLiteIDBTransaction::maybeOpenBackingStoreCursor(IDBObjectStoreIdentifier objectStoreID, std::optional<IDBIndexIdentifier> indexID, const IDBKeyRangeData& range)
{
    ASSERT(inProgressOrReadOnly());

    auto cursor = SQLiteIDBCursor::maybeCreateBackingStoreCursor(*this, objectStoreID, indexID, range);

    if (cursor)
        m_backingStoreCursors.add(cursor.get());

    return cursor;
}

SQLiteIDBCursor* SQLiteIDBTransaction::maybeOpenCursor(const IDBCursorInfo& info)
{
    if (m_sqliteTransaction && !m_sqliteTransaction->inProgress())
        return nullptr;

    auto addResult = m_cursors.add(info.identifier(), SQLiteIDBCursor::maybeCreate(*this, info));
    ASSERT(addResult.isNewEntry);

    // It is possible the cursor failed to create and we just stored a null value.
    if (!addResult.iterator->value) {
        m_cursors.remove(addResult.iterator);
        return nullptr;
    }

    return addResult.iterator->value.get();
}

void SQLiteIDBTransaction::closeCursor(SQLiteIDBCursor& cursor)
{
    if (auto backingStoreTake = m_backingStoreCursors.take(&cursor)) {
        ASSERT(!m_cursors.contains(cursor.identifier()));
        return;
    }

    ASSERT(m_cursors.contains(cursor.identifier()));

    m_backingStore->unregisterCursor(cursor);
    m_cursors.remove(cursor.identifier());
}

void SQLiteIDBTransaction::notifyCursorsOfChanges(IDBObjectStoreIdentifier objectStoreID)
{
    ASSERT(!isReadOnly());

    for (auto& i : m_cursors) {
        CheckedRef cursor = *i.value;
        if (cursor->objectStoreID() == objectStoreID)
            cursor->objectStoreRecordsChanged();
    }

    for (CheckedPtr cursor : m_backingStoreCursors) {
        if (cursor->objectStoreID() == objectStoreID)
            cursor->objectStoreRecordsChanged();
    }
}

void SQLiteIDBTransaction::clearCursors()
{
    for (auto& cursor : m_cursors.values())
        m_backingStore->unregisterCursor(*cursor);

    m_cursors.clear();
}

bool SQLiteIDBTransaction::inProgress() const
{
    return m_sqliteTransaction && m_sqliteTransaction->inProgress();
}

bool SQLiteIDBTransaction::inProgressOrReadOnly() const
{
    return isReadOnly() || inProgress();
}

void SQLiteIDBTransaction::addBlobFile(const String& temporaryPath, const String& storedFilename)
{
    ASSERT(!isReadOnly());

#if OS(SYMBIAN)
    m_blobTemporaryFiles.add(temporaryPath);
#endif
    m_blobTemporaryAndStoredFilenames.append({ temporaryPath, storedFilename });
}

void SQLiteIDBTransaction::addRemovedBlobFile(const String& removedFilename)
{
    ASSERT(!isReadOnly());
    ASSERT(!m_blobRemovedFilenames.contains(removedFilename));

    m_blobRemovedFilenames.add(removedFilename);
}

SQLiteDatabase* SQLiteIDBTransaction::sqliteDatabase() const
{
    if (m_sqliteTransaction)
        return &m_sqliteTransaction->database();

    return m_sqliteDatabase.get();
}


} // namespace IDBServer
} // namespace WebCore
