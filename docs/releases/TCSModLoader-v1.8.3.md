# TCS Mod Loader 1.8.3

Missing or stale preparation now opens **Manage Mod Priorities** automatically
instead of displaying a warning and continuing into an unprepared game.

Start through Steam. The loader checks preparation before loading any mod DLLs
or installing gameplay hooks. If preparation is needed, it opens the manager
and exits that initial game startup. The manager waits for that process to exit.
Choose **Launch game** to prepare mods and start through Steam, or **Save & prepare**
to apply changes and start later. Closing the manager leaves the game closed.
Prepared setups start normally without opening a helper.

## What triggers the manager

- Missing preparation state, plugin cache, asset catalog or active overlay.
- Added, removed, renamed or changed mod archives.
- Changed asset enable/priority/variant settings or base GAME.DAT metadata.

Preparation publishes a small UTF-16 receipt only after the plugin/asset build
and required-plugin checks succeed. Startup checks archive names, sizes and
write times, asset config stamps, the catalog and overlay directory, and native
plugin receipts/entry DLLs. Normal plugin settings do not reopen the manager.
Missing helper files produce a specific installation error and stop startup.
Steam must accept an EXE startup before the loader runs; for Steam application-
load errors, use Steam or Launch game in the manager.

## Install or update

Close the game and manager, then extract the full ZIP beside LEGOStarWarsSaga.exe.
Keep mod archives compressed in Mods, and keep Mods/Config and all saved games.
Manage Mod Priorities.cmd remains the only command script. Character & Shop
Extender remains a separate optional mod. One rotating installation backup and
at most one previous generation per generated cache remain the storage policy.

## Validation

- 77 Python tests ran: 76 passed and one Windows junction test skipped.
- Six relevant native integration tests passed, including the actual bundled
  helper, prepared/stale receipt checks, missing overlay/entry files, multiple
  archives, native EXE/Steam stand-in launches, and automatic manager handoff.
- Prepared plugin loading without an installed helper still passes.
- Unsupported-host safety and existing loader/menu tests passed.
- The actual Steam game was launched with missing preparation: its startup
  exited before any plugins loaded and the 1.8.3 manager window opened.
- ZIP integrity, one-CMD packaging and packaged binary equality passed.
- A full gameplay session was not exercised for this update.

No antivirus submission was performed. These new loader, helper and ZIP bytes
have **unverified antivirus status**; previous results cover only earlier files.

ZIP SHA-256: `e8310a7a998ae8a44622e49f1e68335201d4497124a012b63e5cddbb78c8f639`.

Helper SHA-256: `0de565043e424c0c38cdad17551f8ac6ac69d4d2198663871ef82f6239a74936`.

Loader SHA-256: `0ccffddf2cab814f7ea9187dbde0c9bfd056708085094d4711f401390681169d`.
