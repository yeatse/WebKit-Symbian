/* Copyright (C) 2026 The symbian-webkit authors. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <e32std.h>
#include <w32std.h>

class TSurfaceConfiguration;

// The window server's ScreenPlay surface calls, looked up in ws32.dll by
// ordinal when a presenter starts instead of imported.
//
// On the Nokia 808 the loader refuses (KErrNotSupported) an executable that
// imports UnregisterSurface, GetBackgroundSurface or
// SetBackgroundSurface(TSurfaceConfiguration) at these ordinals, although
// RLibrary::Lookup resolves all of them there and their code matches other
// devices'. Open() returns KErrNotSupported unless every call is present, so
// the browser draws through bitmaps instead.
class RScreenPlayWindowServer {
public:
    TInt Open()
    {
        TInt error = iLibrary.Load(_L("ws32.dll"));
        if (error != KErrNone)
            return error;
        iRegister = reinterpret_cast<TRegister>(iLibrary.Lookup(KRegisterSurface));
        iUnregister = reinterpret_cast<TUnregister>(iLibrary.Lookup(KUnregisterSurface));
        iGet = reinterpret_cast<TGet>(iLibrary.Lookup(KGetBackgroundSurface));
        iSet = reinterpret_cast<TSet>(iLibrary.Lookup(KSetBackgroundSurface));
        iRemove = reinterpret_cast<TRemove>(iLibrary.Lookup(KRemoveBackgroundSurface));
        if (!iRegister || !iUnregister || !iGet || !iSet || !iRemove) {
            Close();
            return KErrNotSupported;
        }
        return KErrNone;
    }
    void Close()
    {
        iRegister = nullptr;
        iUnregister = nullptr;
        iGet = nullptr;
        iSet = nullptr;
        iRemove = nullptr;
        iLibrary.Close();
    }
    TInt RegisterSurface(RWsSession& aWs, TInt aScreen, const TSurfaceId& aSurface) const
    {
        return iRegister(&aWs, aScreen, aSurface);
    }
    void UnregisterSurface(RWsSession& aWs, TInt aScreen, const TSurfaceId& aSurface) const
    {
        iUnregister(&aWs, aScreen, aSurface);
    }
    TInt GetBackgroundSurface(const RWindowBase& aWindow,
        TSurfaceConfiguration& aConfiguration) const
    {
        return iGet(&aWindow, aConfiguration);
    }
    TInt SetBackgroundSurface(RWindowBase& aWindow,
        const TSurfaceConfiguration& aConfiguration, TBool aTriggerRedraw) const
    {
        return iSet(&aWindow, aConfiguration, aTriggerRedraw);
    }
    void RemoveBackgroundSurface(RWindowBase& aWindow, TBool aTriggerRedraw) const
    {
        iRemove(&aWindow, aTriggerRedraw);
    }

private:
    // ws32.dll ordinals frozen in the SDK's interface definition.
    enum {
        KRegisterSurface = 591,
        KUnregisterSurface = 592,
        KGetBackgroundSurface = 596,
        KSetBackgroundSurface = 598,
        KRemoveBackgroundSurface = 600,
    };
    // Nonvirtual member functions: the object is the first argument.
    typedef TInt (*TRegister)(RWsSession*, TInt, const TSurfaceId&);
    typedef void (*TUnregister)(RWsSession*, TInt, const TSurfaceId&);
    typedef TInt (*TGet)(const RWindowBase*, TSurfaceConfiguration&);
    typedef TInt (*TSet)(RWindowBase*, const TSurfaceConfiguration&, TBool);
    typedef void (*TRemove)(RWindowBase*, TBool);

    RLibrary iLibrary;
    TRegister iRegister { nullptr };
    TUnregister iUnregister { nullptr };
    TGet iGet { nullptr };
    TSet iSet { nullptr };
    TRemove iRemove { nullptr };
};
