# EKA2L1 integration

These patches provide the emulator interfaces used by the browser and its test suites.

| Patch | Purpose |
| --- | --- |
| `file-cache.patch` | Refresh case-insensitive directory lookups after file creation. |
| `iap-service.patch` | Read the active connection's IAP service settings. |
| `keyboard.patch` | Translate editing keys, modifiers and key repeat. |
| `mapped-files.patch` | Map private writable copies of ROM-backed files. |
| `network.patch` | Select TCP address families and resolve IPv4 and IPv6 hosts. |
| `screenplay.patch` | Provide Surface Manager, Surface Update and window surface presentation. |

The patches target the port's EKA2L1 integration with GLES2, native MMF media
and connection-observer support. Apply the relevant patch from that integration's
source checkout, then build the emulator with its CMake instructions.

See [Development](../../docs/Development.md) for the browser build and [Testing](../../docs/TESTING.md) for emulator setup and test commands.
