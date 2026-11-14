// A sampling profiler for the UI thread, for the "profile" setting: where
// its program counter is, two hundred times a second while it is running.
// Tools/Symbian/tests/benchmarks/symbolize-profile.py names the addresses.
#pragma once

// Call on the UI thread. Sampling stops by itself after ten minutes.
void BrowserProfilerStart();
// Stops sampling and writes the samples to the log.
void BrowserProfilerDump();
