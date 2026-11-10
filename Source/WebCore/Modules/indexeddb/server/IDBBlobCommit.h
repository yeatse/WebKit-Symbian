/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <cstddef>

namespace WebCore::IDBServer {

enum class IDBBlobCommitResult { Success, PrepareFailed, CommitFailed };

// The callbacks operate on new destination files only. Existing Blob files
// are removed by the caller after success, never during rollback.
template<typename Prepare, typename RemovePrepared, typename Commit, typename Rollback>
IDBBlobCommitResult commitWithPreparedIDBBlobs(size_t count, Prepare&& prepare, RemovePrepared&& removePrepared, Commit&& commit, Rollback&& rollback)
{
    for (size_t index = 0; index < count; ++index) {
        if (!prepare(index)) {
            rollback();
            removePrepared(); // Includes a partially copied destination.
            return IDBBlobCommitResult::PrepareFailed;
        }
    }
    if (!commit()) {
        rollback();
        removePrepared();
        return IDBBlobCommitResult::CommitFailed;
    }
    return IDBBlobCommitResult::Success;
}

} // namespace WebCore::IDBServer
