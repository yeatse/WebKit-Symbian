# ScreenPlay presenter

Exercises surface buffers, painting, submission and presenter lifecycle using the
production ScreenPlay sources. Requires Python 3 and a host C++ compiler.

```sh
python3 Tools/Symbian/tests/screenplay/run.py --output /tmp/screenplay-results
```

Passing results preserve submitted pixels, reuse available buffers and handle
resize and detach through the expected surface lifecycle. The output directory
contains the test log and result report.
