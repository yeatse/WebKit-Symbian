#include "NetworkConnection.h"

#include "BrowserLog.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

namespace {

// Resolves a name as curl's resolver thread does (IPv4, stream sockets)
// and logs the result, to tell the resolver apart from the connection.
void* ProbeResolver(void* aLabel)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* result = nullptr;
    errno = 0;
    int error = getaddrinfo("example.com", "443", &hints, &result);
    if (error)
        BrowserLog("network: %s getaddrinfo: error %d, errno %d", static_cast<const char*>(aLabel), error, errno);
    else {
        char address[INET_ADDRSTRLEN] = { };
        inet_ntop(AF_INET, &reinterpret_cast<struct sockaddr_in*>(result->ai_addr)->sin_addr, address, sizeof(address));
        BrowserLog("network: %s getaddrinfo: %s", static_cast<const char*>(aLabel), address);
        freeaddrinfo(result);
    }
    errno = 0;
    struct hostent* host = gethostbyname("example.com");
    if (!host || !host->h_addr_list[0])
        BrowserLog("network: %s gethostbyname: h_errno %d, errno %d", static_cast<const char*>(aLabel), h_errno, errno);
    else {
        char address[INET_ADDRSTRLEN] = { };
        inet_ntop(AF_INET, host->h_addr_list[0], address, sizeof(address));
        BrowserLog("network: %s gethostbyname: %s", static_cast<const char*>(aLabel), address);
    }
    return nullptr;
}

void ProbeResolvers()
{
    ProbeResolver(const_cast<char*>("main thread"));
    pthread_t thread;
    if (!pthread_create(&thread, nullptr, ProbeResolver, const_cast<char*>("other thread")))
        pthread_join(thread, nullptr);
    else
        BrowserLog("network: cannot start a thread");
}

} // namespace

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
    TInt error = iConnection.GetDesSetting(_L("IAP\\Name"), name);
    if (error) {
        TUint32 iap = 0;
        iConnection.GetIntSetting(_L("IAP\\Id"), iap);
        BrowserLog("network: access point %u has no name (%d)", static_cast<unsigned>(iap), error);
        User::Leave(error);
    }
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
    if (setdefaultif(&request) == -1) {
        BrowserLog("network: setdefaultif failed, errno %d", errno);
        User::Leave(KErrNotReady);
    }
    // FIXME: Remove once host names resolve on phones.
    ProbeResolvers();
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
