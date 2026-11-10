# SDK runtime rebuild

Checks allocator selection, runtime archive members and ARM ABI settings in the
SDK runtime rebuild. Requires Python 3 and a completed SDK runtime rebuild
directory containing its `base/` and `install/` archives.

```sh
python3 Tools/Symbian/tests/sdk-runtime/test-runtime-rebuild.py \
  --work /path/to/sdk-runtime-work
```

The runner verifies the rebuilt archive and exercises the rebuild helpers.
Passing results confirm the expected runtime objects, allocator configuration
and archive layout.
