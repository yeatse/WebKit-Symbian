# Partial HTTP integration, d844 browser

The full run remains **FAIL**: eleven console groups pass, then the guest
raises an access violation at the transition after progressive cleanup, before
a complete 14-group page report. The raw console and crash excerpt are retained.
PC/LR are in generated JIT code; this does not by itself establish the cause.

Actual window pixels independently demonstrate decoded video and pause. The
playing pair differs in 1,117 of 5,568 picture-interior pixels; the paused pair
has zero differing pixels in the same rectangle, excluding overlaid controls.
The unmodified screenshots, host observation timestamps, exact browser/host/ICU
identities and hashes are retained. The first playing capture occurs during the
GPU-to-software transition, before the surrounding page has been repainted;
the next capture shows the page and video together. The console explicitly
records the native-video software fallback. GPU page/MMF composition is not
claimed.

Independent PCM analysis also records a failure: the initial 880 Hz video
playback contains a 30.75 ms exact-zero gap, followed by another 577 ms of audio
before the sustained pause. This is an interior dropout, not a pause boundary.
`audio-control-analysis.json` retains the unchanged analysis and original PCM
hash; the full capture remains external. Host logs show this source was opened
again after a partial-file decode error, but lack the timing needed to prove
that the reopen caused this gap. No whole-suite or final acceptance claim is
made from these partial API and pixel results.
