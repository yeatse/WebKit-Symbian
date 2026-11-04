# ScreenPlay surface updates

Checks the production Surface Update client and its asynchronous notification
lifecycle. Requires Python 3 and a host C++ compiler.

```sh
python3 Tools/Symbian/tests/screenplay-update/run.py \
  --output /tmp/screenplay-update-results.json
```

Passing results submit updates, deliver notifications and complete cancellation
and close operations. The runner prints the result and writes the JSON report.
