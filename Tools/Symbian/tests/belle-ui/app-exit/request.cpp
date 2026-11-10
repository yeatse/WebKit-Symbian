// Test-only official application-exit request. Never kills a process.
#include <apgtask.h>
#include <e32base.h>
#include <w32std.h>
#include <cstdio>

static int requestBrowserExit(FILE* log)
{
    const TUid browserUID = TUid::Uid(0xe1a5c002);
    RWsSession session;
    RThread thread;
    RProcess process;
    RTimer timer;
    auto finish = [&](int result) {
        timer.Close();
        process.Close();
        thread.Close();
        session.Close();
        return result;
    };
    auto fail = [&](const char* stage, TInt error) {
        std::fprintf(log, "FAIL: browser-exit-request stage=%s error=%d\n", stage, error);
        std::fflush(log);
        return finish(2);
    };
    TInt error = session.Connect();
    if (error)
        return fail("window-server-connect", error);
    TApaTaskList tasks(session);
    TApaTask task = tasks.FindApp(browserUID);
    if (!task.Exists())
        return fail("browser-task-not-found", KErrNotFound);
    if ((error = thread.Open(task.ThreadId())))
        return fail("open-browser-thread", error);
    if ((error = thread.Process(process)))
        return fail("open-browser-process", error);
    TFileName filename = process.FileName();
    if (process.SecureId().iId != static_cast<TUint32>(browserUID.iUid)
        || filename.Length() < 11 || filename.Right(11).CompareF(_L("browser.exe")))
        return fail("target-identity-mismatch", KErrPermissionDenied);
    if (process.ExitType() != EExitPending)
        return fail("browser-already-exited", KErrNotReady);
    if ((error = timer.CreateLocal()))
        return fail("create-observation-timer", error);

    TBuf8<KMaxFileName> printableName;
    printableName.Copy(filename);
    std::fprintf(log, "TARGET: uid=e1a5c002 pid=%llu name=%.*s\n",
        static_cast<unsigned long long>(process.Id().Id()), printableName.Length(), printableName.Ptr());
    TRequestStatus exited;
    TRequestStatus deadline;
    process.Logon(exited);
    timer.After(deadline, 10000000);
    std::fputs("REQUEST: TApaTask::EndTask; no KillTask, Terminate, or host WM close\n", log);
    std::fflush(log);
    task.EndTask();
    session.Flush();
    std::fputs("REQUEST_SENT: official application shutdown event\n", log);
    std::fflush(log);

    // EKA --run may exit its host as soon as the browser ends. In that mode
    // this observer can be stopped before it writes EXIT; the browser's own
    // destructor log and emulator callback remain required external evidence.
    User::WaitForRequest(exited, deadline);
    if (exited == KRequestPending) {
        process.LogonCancel(exited);
        User::WaitForRequest(exited);
        return fail("browser-did-not-exit-within-10s", KErrTimedOut);
    }
    timer.Cancel();
    User::WaitForRequest(deadline);
    std::fprintf(log, "EXIT: type=%d reason=%d logon=%d\n",
        static_cast<int>(process.ExitType()), process.ExitReason(), exited.Int());
    bool normal = process.ExitType() == EExitKill && !process.ExitReason();
    std::fputs(normal ? "PASS: browser-exit-request observed normal guest exit\n"
                     : "FAIL: browser-exit-request observed abnormal guest exit\n", log);
    std::fflush(log);
    return finish(normal ? 0 : 1);
}

int main()
{
    FILE* log = std::fopen("C:/Data/WebKit/app-exit-request.log", "wb");
    if (!log)
        return 2;
    std::fputs("BEGIN: browser-exit-request v1\n", log);
    std::fflush(log);
    int result = requestBrowserExit(log);
    std::fclose(log);
    return result;
}
