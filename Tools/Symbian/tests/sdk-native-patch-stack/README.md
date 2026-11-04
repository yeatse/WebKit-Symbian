# Native SDK patches

Checks the SDK's native API patches against the Symbian platform source tree.
Requires Git, Python 3 and an upstream Symbian platform checkout.

```sh
python3 Tools/Symbian/tests/sdk-native-patch-stack/test_patch_stack.py \
  --upstream /path/to/symbian-platform --output /tmp/native-stack-tests
```

The runner applies the patch stack in temporary trees and checks the resulting
API declarations and exports. A successful run leaves the expected native API
set available for SDK builds.
