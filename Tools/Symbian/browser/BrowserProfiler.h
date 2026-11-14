// A sampling profiler for the UI thread, for the "profile" setting: where
// its program counter is, two hundred times a second while it is running.
// Tools/Symbian/tests/benchmarks/symbolize-profile.py names the addresses.
#pragma once

// Call on the UI thread. The samples so far go to the log after every 60 s
// of running, and sampling stops by itself after ten minutes of it.
void BrowserProfilerStart();
// Stops sampling and writes the samples to the log.
void BrowserProfilerDump();
