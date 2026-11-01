// Finite test-only controller. Drives real RConnection operations, never a
// WebCore hook or a connectivity switch; starts and normally closes its browser.
#include <apgtask.h>
#include <e32base.h>
#include <es_sock.h>
#define SYMBIAN_ENABLE_SPLIT_HEADERS
#include <es_enum.h>
#include <rconnmon.h>
#include <w32std.h>
#include <cstdio>
#include <cstring>

static char runToken[49];
static constexpr TUint32 browserUID = 0xe1a5c002;

static void require(TInt result, const char* operation)
{
    std::printf("CONTROL: %s rc=%d\n", operation, result);
    std::fflush(stdout);
    User::LeaveIfError(result);
}

struct Context {
    RSocketServ sockets;
    RConnection connection;
    RConnectionMonitor monitor;
    RProcess browser;
    bool launched = false;
    ~Context() { browser.Close(); monitor.Close(); connection.Close(); sockets.Close(); }
};

static bool containsMarker(const char* marker)
{
    FILE* file = std::fopen("C:/Data/WebKit/log.txt", "rb");
    if (!file)
        return false;
    char buffer[4096 + 256];
    std::size_t carry = 0, total = 0;
    bool found = false;
    while (total < 2 * 1024 * 1024) {
        std::size_t count = std::fread(buffer + carry, 1, 4096, file);
        buffer[carry + count] = 0;
        if (std::strstr(buffer, marker)) { found = true; break; }
        total += count;
        if (count != 4096) break;
        const std::size_t available = carry + count;
        carry = 255;
        std::memmove(buffer, buffer + available - carry, carry);
    }
    std::fclose(file);
    return found;
}

static void waitFor(Context& context, const char* phase, TInt attempts = 150)
{
    char marker[160];
    std::snprintf(marker, sizeof(marker), "NETWORKSTATE %s %s", phase, runToken);
    while (attempts--) {
        if (containsMarker(marker)) {
            std::printf("ACK: %s\n", marker);
            std::fflush(stdout);
            return;
        }
        if (context.browser.ExitType() != EExitPending)
            User::Leave(KErrDied);
        User::After(100000);
    }
    std::printf("FAIL: missing marker %s\n", marker);
    std::fflush(stdout);
    User::Leave(KErrTimedOut);
}

static void requireCount(Context& context, TUint expected)
{
    TUint count = 0;
    TRequestStatus status;
    context.monitor.GetConnectionCount(count, status);
    User::WaitForRequest(status);
    require(status.Int(), "ConnMon count query");
    std::printf("CONTROL: active interfaces=%u expected=%u\n", count, expected);
    std::fflush(stdout);
    if (count != expected) User::Leave(KErrGeneral);
}

static void runL(Context& context)
{
    require(context.sockets.Connect(), "socket server connect");
    require(context.connection.Open(context.sockets), "controller connection open");
    require(context.monitor.ConnectL(), "ConnMon connect");
    requireCount(context, 0); // Never stop another application's pre-existing interface.

    TBuf<192> command(_L(" file:///C:/Data/WebKit/network-state-browser.html?run="));
    TBuf<48> token;
    token.Copy(TPtrC8(reinterpret_cast<const TUint8*>(runToken)));
    command.Append(token);
    require(context.browser.Create(_L("C:\\sys\\bin\\browser.exe"), command), "create browser");
    if (context.browser.SecureId().iId != browserUID) User::Leave(KErrPermissionDenied);
    context.launched = true;
    context.browser.Resume();
    std::printf("CONTROL: browser pid=%llu uid=e1a5c002\n",
        static_cast<unsigned long long>(context.browser.Id().Id()));
    std::fflush(stdout);
    waitFor(context, "READY", 300);
    requireCount(context, 1);

    TUint count = 0;
    TConnectionInfoBuf info;
    require(context.connection.EnumerateConnections(count), "enumerate browser interface");
    if (count != 1) User::Leave(KErrGeneral);
    require(context.connection.GetConnectionInfo(1, info), "get browser interface");
    require(context.connection.Attach(info, RConnection::EAttachTypeNormal), "attach browser interface");
    require(context.connection.Stop(RConnection::EStopAuthoritative), "stop first interface");
    requireCount(context, 0);
    waitFor(context, "OFFLINE1");
    require(context.connection.Start(), "start first reconnect");
    requireCount(context, 1);
    waitFor(context, "ONLINE1");

    RConnection extra;
    require(extra.Open(context.sockets), "extra attachment open");
    TInt result = context.connection.EnumerateConnections(count);
    if (!result) result = context.connection.GetConnectionInfo(1, info);
    if (!result) result = extra.Attach(info, RConnection::EAttachTypeNormal);
    extra.Close();
    require(result, "extra normal attachment and close");
    requireCount(context, 1);
    waitFor(context, "STABLE1");
    require(context.connection.Stop(RConnection::EStopAuthoritative), "stop second interface");
    requireCount(context, 0);
    waitFor(context, "OFFLINE2");
    require(context.connection.Start(), "start second reconnect");
    requireCount(context, 1);
    waitFor(context, "DONE");
    char successful[160];
    std::snprintf(successful, sizeof(successful), "NETWORKSTATE RESULT %s passed=19 failed=0", runToken);
    if (!containsMarker(successful)) User::Leave(KErrGeneral);
    // Keep the completed page visible for the external observer's screenshot.
    // This is not part of the state-transition assertions or an exit shortcut.
    std::puts("CONTROL: final page visible for 5 seconds");
    std::fflush(stdout);
    User::After(5000000);
}

static TInt closeBrowser(Context& context)
{
    if (!context.launched || context.browser.ExitType() != EExitPending)
        return KErrNotReady;
    RWsSession session;
    RThread thread;
    RProcess target;
    RTimer timer;
    auto finish = [&](TInt result) { timer.Close(); target.Close(); thread.Close(); session.Close(); return result; };
    TInt error = session.Connect();
    if (error) return finish(error);
    TApaTaskList tasks(session);
    TApaTask task = tasks.FindApp(TUid::Uid(browserUID));
    if (!task.Exists()) return finish(KErrNotFound);
    if ((error = thread.Open(task.ThreadId()))) return finish(error);
    if ((error = thread.Process(target))) return finish(error);
    if (target.Id() != context.browser.Id() || target.SecureId().iId != browserUID)
        return finish(KErrPermissionDenied);
    if ((error = timer.CreateLocal())) return finish(error);
    TRequestStatus exited, deadline;
    context.browser.Logon(exited);
    timer.After(deadline, 10000000);
    std::puts("CONTROL: matching browser PID/SID; requesting official EndTask");
    std::fflush(stdout);
    task.EndTask();
    session.Flush();
    User::WaitForRequest(exited, deadline);
    if (exited == KRequestPending) {
        context.browser.LogonCancel(exited);
        User::WaitForRequest(exited);
        return finish(KErrTimedOut);
    }
    timer.Cancel();
    User::WaitForRequest(deadline);
    std::printf("BROWSER EXIT: type=%d reason=%d logon=%d\n",
        context.browser.ExitType(), context.browser.ExitReason(), exited.Int());
    std::fflush(stdout);
    return finish(context.browser.ExitType() == EExitKill && !context.browser.ExitReason() ? KErrNone : KErrGeneral);
}

int main()
{
    if (!std::freopen("C:/Data/WebKit/network-state-controller.log", "w", stdout)) return 2;
    TBuf<80> arguments;
    if (User::CommandLineLength() > arguments.MaxLength()) return 2;
    User::CommandLine(arguments);
    arguments.Trim();
    if (arguments.Length() >= 2 && arguments[0] == '"' && arguments[arguments.Length() - 1] == '"') {
        arguments.Delete(arguments.Length() - 1, 1);
        arguments.Delete(0, 1);
    }
    if (arguments.Length() < 8 || arguments.Length() > 48) return 2;
    for (TInt index = 0; index < arguments.Length(); ++index) {
        auto c = arguments[index];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 2;
        runToken[index] = static_cast<char>(c);
    }
    std::printf("BEGIN: network-state-controller version=1 run=%s\n", runToken);
    std::fflush(stdout);
    CTrapCleanup* cleanup = CTrapCleanup::New();
    if (!cleanup) return 2;
    Context context;
    TRAPD(error, runL(context));
    TInt closeError = context.launched ? closeBrowser(context) : KErrNotReady;
    std::printf("CONTROL: sequence error=%d browser-close error=%d\n", error, closeError);
    bool success = !error && !closeError;
    std::printf("RESULT: passed=%d failed=%d\n", success ? 1 : 0, success ? 0 : 1);
    std::printf("network-state-controller exiting %d\n", success ? 0 : 1);
    std::fflush(stdout);
    delete cleanup;
    return success ? 0 : 1;
}
