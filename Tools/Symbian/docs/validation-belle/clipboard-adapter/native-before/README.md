# First native adapter run — failed

The production adapter passed 28 of 31 checks. The three failures were
read-only write refusal and exclusive-handle read/write refusal. EKA's
`fs_server_client::set_entry` parsed the requested attribute masks but returned
success without applying them; `new_node` exempted additional opens by the same
process from an existing exclusive handle. The fixture and production bridge
retain the native contract. A later emulator correction must rerun the same
31 checks, not reclassify these failures.

The valid text, explicit empty plain stream, malformed store bounds, SCSU,
failed writes preserving contents, and repeated heap/handle cleanup checks
passed. The whole run still **failed**: guest exited 1 and host shutdown exited
-11. See the unmodified guest log and result for the exact checks and identities.
The original clipboard was absent and the sole runtime operator restored that
absence in `finally`; no original clipboard data is archived.
