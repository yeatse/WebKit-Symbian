void RunLoop::setWakeUpCallback(WTF::Function<void()>&& function)
{
    auto& runLoop = RunLoop::currentSingleton();
#if OS(SYMBIAN)
    // wakeUpWithLock can run on a worker thread. Clearing the native driver
    // callback must wait until any invocation using its raw context finishes.
    Locker locker { runLoop.m_loopLock };
#endif
    runLoop.m_wakeUpCallback = WTF::move(function);
}

void RunLoop::wakeUpWithLock()
{
    m_pendingTasks = true;
    m_readyToRun.notifyOne();

    if (m_wakeUpCallback)
        m_wakeUpCallback();
}

void RunLoop::wakeUp()
{
    Locker locker { m_loopLock };
    wakeUpWithLock();
}
