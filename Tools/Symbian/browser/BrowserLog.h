// The browser's log, C:\Data\WebKit\log.txt. The engine's own messages
// (WTFLogAlways and anything else on stderr) go to the same file; the
// previous run's log is kept as log.old.txt.
#pragma once

void BrowserLogOpen(const char* aDirectory);
void BrowserLog(const char* aFormat, ...) __attribute__((format(printf, 1, 2)));

// Logs every thread of the process (priority, CPU time) every two seconds,
// from a thread above them all, so the log shows what a hang is doing.
void BrowserThreadWatchStart();
