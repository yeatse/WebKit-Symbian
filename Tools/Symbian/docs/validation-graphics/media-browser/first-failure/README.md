# First HTML media integration failure

This real guest run uses the browser, clean emulator and fixture identities in
`result.json`. Only the format-rejection group finished. The first WAV was
loaded into the native player, but the JavaScript `play()` Promise stayed
pending. The strict runner failed after 180 seconds with no complete report;
the emulator closed normally and had no guest panic.

Independent source review found the embedding constructed Page without a
`PageIdentifier`. In WebKit 2.52, `Page::mediaSessionManager()` returns null
for such a page. `PlatformMediaSession::clientWillBeginPlayback()` therefore
completes false, and HTMLMediaElement returns without beginning playback or
settling the pending Promise. The embedding now generates a Page ID, as the
Legacy Mac embedding does. A fresh browser run is required to validate that
correction; this failed run is not reclassified.

The fixture now bounds every awaited play Promise to 12 seconds and records
ready/network/paused/time/error state, so a stalled Promise fails its group
instead of stopping all diagnostics. Playback criteria are unchanged. A
separate backend guard prevents a second native Play when a synchronous
ready-state notification already started playback.
