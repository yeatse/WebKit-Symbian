# Font dependency and host verification

Pinned ARM decoder and fontconfig builds succeeded. The four-library WOFF2 link check used the actual OptionsSymbian block. Actual patched fontconfig sources passed 11 host checks; pinned WOFF2/Brotli plus host FreeType passed seven valid/malformed-font checks. Logs, dependency hashes and checked source hashes are alongside this file. See [the fixture instructions](../../tests/web-fonts/README.md) for reproduction and guest acceptance.

The candidate2 browser `91cf3692…` passed the real seven-check page in EKA2L1
`12c275e7…`: CSS WOFF2 loading, exact metrics, rasterized pixels, existing TTF,
both malformed fonts rejected, and a valid font still usable afterward. The
fresh run completed with normal WM shutdown and host exit 0. Independent review
confirmed the screenshot, all seven log markers, and byte-identical deployment
of `fonts.conf` and every fixture. The complete result, screenshot, guest log,
compressed host log and hashes are in [guest-candidate2](guest-candidate2/).

The main configuration was opened from `C:/resource/fontconfig/fonts.conf` and
ROM fonts were discovered. One unused template-directory probe still expands
an absolute path to `C:/C:/resource/fontconfig/conf.avail`; arbitrary absolute
configuration-file lookup remains a separate correction. This limitation does
not turn the seven successful browser checks into a broader claim about every
fontconfig path. This was direct fixture deployment, not SIS-install validation.
No firmware, SDK archive or proprietary font is included.

The subsequent [absolute-path correction](absolute-config-path/verification.json)
extends the existing drive-prefix check to `FcConfigFileExists`. Two new checks
against the actual host library failed before the change and passed afterward;
the expanded suite passes 13/13. Only the ARM fontconfig archive was rebuilt,
to `5dc8313a…`; the four WOFF2/Brotli archives stayed byte-identical. Reapplying
the source patch is harmless, and unexpected source content is rejected before
any of the three files is written.

[Candidate3](guest-candidate3/) (`d21abb2a…`, same emulator `12c275e7…`) then
passed the unchanged seven-check browser page and normal shutdown. Independent
review confirmed all deployed fixture/configuration bytes and the screenshot.
The actual host filesystem trace now probes
`C:/resource/fontconfig/conf.avail`, with no duplicate drive prefix, and opens
the primary configuration successfully. Candidate2 evidence above is retained
unchanged to show the discovered issue and the difference after correction.

The separate [system-font fixture](../../tests/web-fonts/system-fonts.html) passed
7/7 on two launches of candidate5 `139e140d…` with emulator `5f8704eb…`.
[Recorded evidence](system-candidate5/) includes visible English, equal `i`/`W`
monospace advances (132 pixels for six characters), distinct Chinese `中`/`国`
fallback pixels, a different unsupported-character mask, and different serif
and sans pixels. The [ROM cmap inventory](system-font-inventory.json) confirms
Arial lacks the Chinese characters while the observed MHei font contains them.
No proprietary font data is redistributed.

Both launches opened the existing 60,432-byte C: fontconfig cache; its SHA-256,
size and modification timestamp remained unchanged. Render measurements and
the full screenshot content region are identical across launches; only the URL
nonce differs. Launch1's outer wrapper originally reported FAIL because its
substring-based query parser matched `autorun=` before `run=`. That original
record remains untouched. A separate offline `validation.json` checks the real
nonce with standard URL parsing, the seven actual checks and normal inner driver
exit. Launch2's original wrapper passed. These are additional system-font checks,
separate from the original WOFF2 seven-check suite.
