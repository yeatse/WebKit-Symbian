// The packet data or WLAN connection the engine's sockets use. Open C's
// sockets, which curl uses from its resolver and transfer threads, cannot
// ask the user for an access point themselves, so the browser starts the
// connection (the system asks for an access point if its settings say to)
// and makes its access point the process's default interface.
#pragma once
#include "NativeHeapDelete.h"

#include <e32base.h>
#include <es_sock.h>

class MNetworkConnectionObserver {
public:
    // aError is KErrNone once the connection is up.
    virtual void NetworkConnectionReady(TInt aError) = 0;
};

class CNetworkConnection : public CActive {
public:
    SYMBIAN_BROWSER_NATIVE_DELETE
    static CNetworkConnection* NewL(MNetworkConnectionObserver& aObserver);
    ~CNetworkConnection();

    // Starts the connection unless it is up or starting.
    void StartL();
    TBool IsReady() const { return iReady; }

private:
    explicit CNetworkConnection(MNetworkConnectionObserver& aObserver);
    void RunL();
    void DoCancel();
    TInt RunError(TInt aError);
    void UseAccessPointL();

    MNetworkConnectionObserver& iObserver;
    RSocketServ iSocketServ;
    RConnection iConnection;
    TBool iOpen { EFalse };
    TBool iReady { EFalse };
};
