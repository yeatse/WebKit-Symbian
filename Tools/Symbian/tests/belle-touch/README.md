# Touch input

Exercises the production touch tracker and gesture handling: contact lifetimes,
taps and scrolling gestures. Requires a host C++ compiler.

```sh
bash Tools/Symbian/tests/belle-touch/run-host-tests.sh
```

The fixture checks the resulting pointer and gesture events and exits
successfully when their order, coordinates and contact state match.
