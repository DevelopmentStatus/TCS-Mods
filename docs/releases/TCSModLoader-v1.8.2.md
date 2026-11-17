# TCS Mod Loader 1.8.2

One shortcut, preparation that applies saved settings, and Steam-aware launching.

The installed game's 1.8.1 log showed every plugin skipped as unprepared and no
asset catalog. Steam/EXE launches read prepared files, while the old manual
editor saved settings without applying them. Save & prepare now prepares native
plugins and assets while leaving the editor open. A failure stays visible and
editable. Launch game prepares first, then launches matching Steam installations
through Steam (`-applaunch 32440`), or the local EXE for standalone folders.
The helper resets its bundled runtime DLL search path for native child processes
and restores its own path afterward. This prevents helper runtime directories
from entering the game's native DLL search path.

## Installation

Close the game and extract the complete ZIP beside LEGOStarWarsSaga.exe. Open
**Manage Mod Priorities.cmd**, then choose **Save & prepare** or **Launch game**.
After preparation, normal Steam/EXE launches use the prepared files. Repeat after
archive or asset setting changes. Large packs can take several minutes.

The package contains only Manage Mod Priorities.cmd. Remove Launch Modded
Game.cmd and Prepare Mods.cmd left by older versions. Starting with unprepared
plugins or a missing asset catalog now explains the manager step in a dialog.
Character & Shop Extender remains separate; keep configs and all saved games.

## Storage

Generated asset and plugin caches keep the active generation and at most one
previous generation per cache. A new asset build preserves the published overlay
while pruning older attempts. The optional plugin installers share one rotating
backup, Mods/Backups/PreviousInstall.zip.bak; repeated installation of identical
bytes does not replace the last different version. Their old timestamped backup
files are consolidated. Unrelated user backups and saved games are untouched.

## Validation

- 73 Python tests ran: 72 passed, one Windows junction test skipped.
- Real Tk checks passed for save/preparation, failure recovery, launch intent,
  priority editing and layout at three display scaling levels.
- Native tests include the actual packaged helper preparing/loading a plugin,
  and launching native EXE/Steam stand-ins with a clean inherited DLL search path.
- Native suite: 20 passed, one runtime-layout test skipped, two existing shop
  capacity fixture executables could not be started by the test runner. These
  two checks remain unverified for this build.
- ZIP integrity, single-command packaging and packaged binary equality passed.
- Installed locally and launched the actual Steam game through the rebuilt
  packaged helper. The new game log confirms all four native plugins and all
  18 asset packages registered, the asset overlay hooks installed, graphics
  initialized, and the expanded roster parsed. This validates startup/mod loading;
  a full gameplay session was not exercised.
- The installed folder has one CMD and one rotating installation backup.

No antivirus submission was performed. These new loader, helper and ZIP bytes
have **unverified antivirus status**; historical scan results cover earlier files.

ZIP SHA-256: `129cd8f3ed7218b21f860833c1e655154b035d42543a42d05763970271cdc3b7`.

Helper SHA-256: `bcb445fac6d66800cc76c2415d804cf8c81ac7152c747351fc7bdf9bb48ea8d3`.

Loader SHA-256: `8bde3b3e8a265cf653becd7816b65ab1a7f85732a7d0dedbb6ed0a222c3fea32`.
