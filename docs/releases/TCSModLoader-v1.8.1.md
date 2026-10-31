# TCS Mod Loader 1.8.1

Installing 1.8.0 over an older setup left plugin caches unprepared and the asset
catalog outdated until the external manager was run. The loader correctly skipped
those stale inputs, but copying the ZIP alone appeared to leave all mods inactive.

This package adds **Launch Modded Game.cmd**. It prepares the installed plugins
and assets, then starts the game only if preparation succeeds. A preparation error
leaves the game closed and the console open with the error. The installer README
and Nexus description now direct users to this shortcut.

## Installation

1. Close the game and extract the complete loader ZIP beside LEGOStarWarsSaga.exe.
   Keep TCSAssetRuntime beside TCSAssetManager.exe.
2. Keep supported mod archives compressed in Mods. Character packs requiring
   expanded tables also need the separate
   [Character & Shop Extender 1.0.0](https://github.com/DevelopmentStatus/TCS-Mods/releases/tag/TCSCharacterShopExtender-v1.0.0)
   ZIP in Mods. It remains a separate download.
3. Double-click **Launch Modded Game.cmd**. Use it again after mod/archive or asset
   setting changes. Manage Mod Priorities.cmd > Launch game also prepares mods;
   Prepare Mods.cmd prepares without launching.

Keep existing settings and saves. The extender's companion-save format is unchanged.

## Validation and scan status

- The user confirmed their existing installation works after preparation and adding
  the required extender. Preparation completed for 18 asset packages and four
  plugins, with no errors or changes to existing settings.
- All 992 installed helper runtime files matched the release package.
- The 1.8.1 ZIP passes archive integrity checks. Its loader DLL, helper EXE and
  complete runtime are byte-for-byte identical to 1.8.0. No native/Python code was
  changed; the helper may still display version 1.8.0.
- Previous native and Python checks remain applicable to those unchanged binaries;
  this update does not claim a new full gameplay test of the shortcut.
- No further VirusTotal submissions or checks were performed, as requested.
  The exact unchanged DLL has the previously verified
  [0/70 result](https://www.virustotal.com/gui/file/5e73a7fe09f4fb6138ff4a148fe46a32486dc5a9a583e2658821a45dfaa45d5d).
  The new 1.8.1 ZIP is **antivirus status unverified**. A DLL result does not describe
  the entire package. See [1.8.0 evidence and limitations](TCSModLoader-v1.8.0.md).

ZIP SHA-256: `6fa5e3b9cd54ec3c2deff83787425229ac333e9cee5d314bad89735052857022`.
