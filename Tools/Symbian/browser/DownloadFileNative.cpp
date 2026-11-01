/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "DownloadFileNative.h"

#include <DownloadFilename.h>
#include <f32file.h>

namespace WebKitSymbian {

class NativeDownloadFile final : public DownloadFile {
public:
    ~NativeDownloadFile() final
    {
        cancel();
        m_fs.Close();
    }

    int open(const uint16_t* path, unsigned length) final
    {
        if (m_started || !isValidDownloadDestination(path, length))
            return KErrArgument;
        m_started = true;
        for (unsigned i = 0; i < length; ++i)
            m_destination.Append(TChar(path[i] == '/' ? '\\' : path[i]));
        TInt error = m_fs.Connect();
        if (error)
            return error;
        if (!m_fs.IsValidName(m_destination))
            return KErrBadName;
        TEntry entry;
        error = m_fs.Entry(m_destination, entry);
        if (!error)
            return KErrAlreadyExists;
        if (error != KErrNotFound)
            return error;
        TInt separator = m_destination.LocateReverse('\\');
        if (separator < 2)
            return KErrBadName;
        TFileName temporary;
        error = m_writer.Temp(m_fs, m_destination.Left(separator + 1), temporary,
            EFileWrite | EFileShareExclusive);
        if (!error)
            m_temporary = temporary;
        else
            m_writer.Close();
        // A failed native call does not grant ownership of its output name.
        return error;
    }

    int append(const uint8_t* data, unsigned length) final
    {
        if (!m_writer.SubSessionHandle())
            return KErrNotReady;
        if ((!data && length) || length > maximumSize - m_bytes)
            return KErrTooBig;
        if (!length)
            return KErrNone;
        TInt error = m_writer.Write(TPtrC8(reinterpret_cast<const TUint8*>(data), length));
        if (!error)
            m_bytes += length;
        return error;
    }

    int finish() final
    {
        if (!m_writer.SubSessionHandle())
            return KErrNotReady;
        TInt error = m_writer.Flush();
        m_writer.Close();
        if (!error)
            error = m_fs.Rename(m_temporary, m_destination);
        if (!error) {
            m_temporary.Zero();
            m_fs.Close();
        }
        // On failure the caller invokes cancel(), so a cleanup error can be
        // reported separately without hiding the original write/flush error.
        return error;
    }

    int cancel() final
    {
        m_writer.Close();
        TInt error = KErrNone;
        if (m_fs.Handle() && m_temporary.Length()) {
            error = m_fs.Delete(m_temporary);
            if (error == KErrNotFound || error == KErrPathNotFound)
                error = KErrNone;
            if (!error)
                m_temporary.Zero();
        }
        if (!error)
            m_fs.Close();
        return error;
    }

private:
    RFs m_fs;
    RFile m_writer;
    TFileName m_destination;
    TFileName m_temporary;
    uint64_t m_bytes { 0 };
    bool m_started { false };
};

DownloadFile* createNativeDownloadFile()
{
    return new NativeDownloadFile;
}

} // namespace WebKitSymbian
