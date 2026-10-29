# Bounded audio prefetch on growing video files

The TLS candidate's ordinary HTTP video control case exposed an interior
33.9167 ms audio dropout. Its host log reports a corrupt H.264 packet followed
by closing and reopening the decoder. The successful HTTP candidate has only
one open of that source. These observations remain separate from the following
deterministic host probes; no synchronized audio/decoder timestamp is inferred.

For the fixture's video packet at DTS 22528, the expected position is 80134
and size is 2166 bytes. Opening a file with only 81699 bytes makes FFmpeg return
1565 bytes and `AV_PKT_FLAG_CORRUPT`. The NAL prefix requests 2162 payload bytes
but only 1561 remain, exactly matching the guest log. Appending the rest of the
710764-byte file does not change an already cloned packet. That packet still
fails decoding; reopening the completed file returns a valid 2166-byte packet.

The EKA reader intends to stop once it has a video packet and about half a
second of decoded audio. It checked the decoded-sample ring before decoding
the compressed audio packets, however, so its first reading pass kept going
to the temporary EOF or packet-count cap. Patch 0017 decodes pending audio
inside that reading loop before evaluating its existing bounds. It changes
neither the half-second target nor packet limits, and does not discard corrupt
packets or suppress errors. Existing stop/cancel and partial-file reopen paths
remain in place.

`read-ahead.cpp` uses the same FFmpeg archives, actual AAC/H.264 decoding, and
the production packet limits. It deterministically simulates sample consumption
at the fixture's 15 fps; it is not a realtime PCM or guest acceptance test.
The remaining file bytes are appended after the first reader cycle in both
versions, eliminating download timing as a variable.

| Initial file bytes | Original reading cycle | Decode inside reader |
| --- | --- | --- |
| 65536, then complete | 18 video packets, 0 decoded samples; cached corrupt packet fails | 8 video packets, 22528 decoded samples; all 180 frames decode |
| 81699, then complete | 23 video packets, 0 decoded samples; cached corrupt packet fails | 8 video packets, 22528 decoded samples; all 180 frames decode |
| Permanently truncated at either boundary | Expected failure | Still fails; corrupt packets are not hidden |
| Complete before opening | Valid source | All 180 frames decode |

Run `run-host-probes.sh FIXTURE SCRATCH_DIRECTORY` with `FFMPEG_PREFIX` set to
the emulator's FFmpeg install directory and `AMR_LIBRARY` set to its linked
opencore-amrnb archive. A C/C++ compiler, zlib, pthreads and libatomic are needed.
The script verifies the fixture hash and saves both JSONL results and decoder
logs. `inputs.json` identifies the exact libraries and source inputs used here.
The ordered patch replay applies 14 patches to the pinned EKA base and verifies
the resulting video source matches the live source. Guest PCM and playback
validation of the changed emulator are recorded separately when available.
