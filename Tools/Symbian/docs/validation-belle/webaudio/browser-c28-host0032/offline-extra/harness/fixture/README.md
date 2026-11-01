# Three independent public offline checks

No native AudioContext, user-input control, microphone, media source, or PCM capture is used. A fresh page creates only OfflineAudioContexts and sends exactly three fixed reports to its finite owned localhost server. A/B report every sample; the Python validator independently checks all 512 frames and channels against fixed impulses rather than trusting a reported mismatch count.

| Case | Public graph/settings | Independent expected values |
| --- | --- | --- |
| A | stereo L0=1,L1=-.5,R3=.25; split L→Delay128→merger input0, R→input1 | output L128=1,L129=-.5,R3=.25; all other samples exactly zero |
| B | mono 1,-.5 at0/1→Gain(1)→Delay128→destination | output128=1,129=-.5; all other samples exactly zero |
| C lowpass | Fs32768, fc8192, detune0, Q=-10log10(2) dB; public getFrequencyResponse | frequencies0/8192/16384: mag1/√.5/0; phase at first two only0/-π/2; tol1e-5 |
| C allpass | independent allpass, fc8192, detune0, Q=1 linear | frequencies0/4096/8192/12288/16384: mag1 tol1e-5; all phase values finite |

Delay time is exactly 1/256 second at32768Hz, hence128 frames. Case A reverses the original fixture's merger input order. Case B retains the single-output processing path. Neither modifies or replaces the original strict C28 fixture.

The WebAudio specification defines lowpass/highpass Q in dB and allpass Q as a positive linear value: https://webaudio.github.io/web-audio-api/#dom-biquadfilternode-q . For lowpass, QdB=-10log10(2) corresponds to the Butterworth linear resonance1/√2. At fc=Fs/4 the cutoff response is -j/√2, giving the stated standard magnitude and phase values. No C++ coefficient or frequency-response implementation is copied into the reference. Zero-magnitude Nyquist phase is intentionally not reported or asserted.

The allpass identity has unit magnitude independent of frequency; its non-axis frequency points exercise a wider complex-response path. These observations validate the actual public Biquad complex amplitude/phase path in the pinned guest image. JS numeric values alone do not identify a particular target helper symbol such as atan2l; any claim about exact SDK symbol execution needs separate target evidence.

The validator returns OFFLINE_EXTRA_API_COMPLETE or FAIL, always native_audio_verified=false. Only the preserved wrapper can add normal owned-parent close, fresh health, unchanged inputs, and complete restoration to produce OFFLINE_EXTRA_RUN_COMPLETE. Neither result is an overall native-audio/PCM PASS.
