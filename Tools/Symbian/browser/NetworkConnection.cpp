#include "NetworkConnection.h"

#include "BrowserLog.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <string.h>

CNetworkConnection* CNetworkConnection::NewL(MNetworkConnectionObserver& aObserver)
{
    return new (ELeave) CNetworkConnection(aObserver);
}

CNetworkConnection::CNetworkConnection(MNetworkConnectionObserver& aObserver)
    : CActive(EPriorityStandard)
    , iObserver(aObserver)
{
    CActiveScheduler::Add(this);
}

CNetworkConnection::~CNetworkConnection()
{
    Cancel();
    if (iReady)
        unsetdefaultif();
    iConnection.Close();
    iSocketServ.Close();
}

void CNetworkConnection::StartL()
{
    if (iReady || IsActive())
        return;
    if (!iOpen) {
        User::LeaveIfError(iSocketServ.Connect());
        TInt error = iConnection.Open(iSocketServ);
        if (error) {
            iSocketServ.Close();
            User::Leave(error);
        }
        iOpen = ETrue;
    }
    BrowserLog("network: starting the connection");
    // The default preferences: the default destination, or a prompt.
    iConnection.Start(iStatus);
    SetActive();
}

void CNetworkConnection::RunL()
{
    TInt error = iStatus.Int();
    if (!error)
        TRAP(error, UseAccessPointL());
    if (error)
        BrowserLog("network: no connection (%d)", error);
    iReady = !error;
    iObserver.NetworkConnectionReady(error);
}

void CNetworkConnection::UseAccessPointL()
{
    TBuf<KMaxName> name;
    User::LeaveIfError(iConnection.GetDesSetting(_L("IAP\\Name"), name));
    // Open C takes the access point's name in UTF-8.
    struct ifreq request;
    memset(&request, 0, sizeof(request));
    TUint length = 0;
    for (TInt i = 0; i < name.Length(); ++i) {
        TUint c = name[i];
        TUint8 bytes[3];
        TUint count = 0;
        if (c < 0x80)
            bytes[count++] = c;
        else if (c < 0x800) {
            bytes[count++] = 0xC0 | (c >> 6);
            bytes[count++] = 0x80 | (c & 0x3F);
        } else {
            bytes[count++] = 0xE0 | (c >> 12);
            bytes[count++] = 0x80 | ((c >> 6) & 0x3F);
            bytes[count++] = 0x80 | (c & 0x3F);
        }
        if (length + count >= sizeof(request.ifr_name))
            User::Leave(KErrOverflow);
        memcpy(request.ifr_name + length, bytes, count);
        length += count;
    }
    BrowserLog("network: connected through \"%s\"", request.ifr_name);
    // Open C's sockets now attach to this connection instead of starting
    // one of their own.
    if (setdefaultif(&request) == -1)
        User::Leave(KErrNotReady);
}

void CNetworkConnection::DoCancel()
{
    iConnection.Stop();
}

TInt CNetworkConnection::RunError(TInt aError)
{
    BrowserLog("network: error %d", aError);
    return KErrNone;
}
