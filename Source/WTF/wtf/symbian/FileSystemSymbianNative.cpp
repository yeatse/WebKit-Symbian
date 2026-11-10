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

// Native headers are isolated from WTF/libc++ declarations.
#include "FileSystemSymbianNative.h"

#include <f32file.h>

namespace WTF::SymbianSystem {

bool volumeInformation(unsigned drive, uint64_t& capacity, uint64_t& freeSpace)
{
    if (drive > EDriveZ)
        return false;
    RFs session;
    if (session.Connect() != KErrNone)
        return false;
    TVolumeInfo information;
    TInt error = session.Volume(information, static_cast<TInt>(drive));
    session.Close();
    if (error != KErrNone || information.iSize < 0 || information.iFree < 0 || information.iFree > information.iSize)
        return false;
    capacity = static_cast<uint64_t>(information.iSize);
    freeSpace = static_cast<uint64_t>(information.iFree);
    return true;
}

} // namespace WTF::SymbianSystem
