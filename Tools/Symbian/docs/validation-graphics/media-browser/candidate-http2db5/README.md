# HTTP media candidate 2db5: PASS

The exact `2db5f849...` browser and clean `ff218e2e...` emulator pass all
14 HTTP groups. The fresh page report agrees with the console and strict
runner; the 180-second observation ends by normal window shutdown, with no
guest access violation or panic. TLS has its own run and evidence.

The corrected natural-end test observes exactly one real `ended` event and
requires the element to be paused at its duration. Progressive playback begins
with 245,760 of 710,764 bytes received. The stalled transfer records `waiting`
at 2.066667 seconds and resumes beyond 2.733333 seconds without premature end
or error. The trailing-index file waits for the complete response, and the
truncated response produces a real media error. All cases remain in the report.

The seven original screenshots show decoded test-pattern motion and a stable
paused frame. An independent comparison of the 5,568 pixels inside rectangle
`(274,137,390,185)` finds 1,142 changed RGB pixels while playing, zero while
paused, and 2,140 after resuming. The operator's separate video analysis uses
its own recorded rectangle and channel-count units. The browser log records
the switch to software page painting for native MMF video; this is not evidence
of simultaneous EGL page composition with the MMF background surface.

Independent PCM analysis passes the same eight named playing, paused, muted
and unmuted control windows: 440 Hz WAV, 880 Hz video audio, silence while
paused/muted, and approximately half the original RMS after volume is set to
0.5. No clipping or extra exact-zero gap of at least 1 ms occurs in these
active windows. The previous candidate's 30.75 ms interior dropout does not
recur in this capture. This analysis does not assert continuity during other
cases, expected network stalls, source changes, or the idle remainder.

`evidence.json` retains exact browser, host, ICU, fixture and capture identities.
Native DLL hashes were checked by the operator before launch; they are labeled
as such rather than presented as automatic runner output. Earlier failed
candidates remain in their original evidence directories.
