# ScreenPlay fixture content v2 (record protocol v1)

The moving element installs its exact-target slide animationend listener before the animate class. Only that event reads the final rect and records css_motion against the unchanged (144,168) position and 0.75 CSS-pixel tolerance. This avoids timing the animation from script entry while synchronous Canvas/WebGL initialization can delay its actual start.

The 2500/8500 timer only arms finishReady. Complete waits for genuine outcomes of all named checks. Missing animationend/checks still fail through the unchanged 9000 watchdog; no condition is filled with an automatic pass. Public record schema/parser, expected colors/ROIs, layout, animation duration and optional scopes are unchanged. This content revision is not a runtime/display pass by itself; old fixtures and failed runs remain immutable.
