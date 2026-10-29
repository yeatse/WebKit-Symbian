# Utility2 on the clean host

The exact host, native-only DLL and probe hashes are in `result.json`. This
run passes 16/16 API and surface checks, with guest/host exit zero and no panic.
The host log has no temporary `MMFSTART` diagnostics.

Unmodified screenshots show frame 79 at 5.267 seconds, then frame 111 at
7.400 seconds. The decoded shapes visibly move. After Close, the white probe
border remains around a black video area; no decoded frame remains attached.
The [independent AAC capture](../audio/README.md) is from this same run and
passes the strict 880 Hz playback/pause/resume/stop analysis.

`evidence.json` records hashes of the original screenshot files and portable
log/result. Earlier candidate screenshots are preserved in adjacent folders.
These native results do not stand in for browser HTML-media integration.
