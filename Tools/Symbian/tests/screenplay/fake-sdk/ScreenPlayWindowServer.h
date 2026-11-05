#pragma once
// Presenter-only API mock: forwards to the fake window server directly.
#include "fake_symbian.h"
class RScreenPlayWindowServer {
public:
    TInt Open() { return KErrNone; }
    void Close() {}
    TInt RegisterSurface(RWsSession& w, TInt s, const TSurfaceId& id) const { return w.RegisterSurface(s, id); }
    void UnregisterSurface(RWsSession& w, TInt s, const TSurfaceId& id) const { w.UnregisterSurface(s, id); }
    TInt GetBackgroundSurface(const RWindow& w, TSurfaceConfiguration& c) const { return w.GetBackgroundSurface(c); }
    TInt SetBackgroundSurface(RWindow& w, const TSurfaceConfiguration& c, TBool r) const { return w.SetBackgroundSurface(c, r); }
    void RemoveBackgroundSurface(RWindow& w, TBool r) const { w.RemoveBackgroundSurface(r); }
};
