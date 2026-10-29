void RunLoop::setWakeUpCallback(WTF::Function<void()>&& function)
{
    RunLoop::currentSingleton().m_wakeUpCallback = WTF::move(function);
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
