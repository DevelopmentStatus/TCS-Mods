# TCS Mod Loader 1.8.1

Installing 1.8.0 over an older setup left plugin caches unprepared and the asset
catalog outdated until the external manager was run. This package adds
**Launch Modded Game.cmd**. It prepares the installed plugins and assets, then
starts the game only if preparation succeeds. A preparation error leaves the
game closed and the console open with the error.

## Single-EXE packaging revision

The helper now bundles its Python runtime inside **TCSAssetManager.exe**, as in
older versions. There is no separate TCSAssetRuntime directory or nested ZIP
entry in the installer. The helper temporarily extracts its embedded runtime
when run; users do not need to install Python. The helper displays version 1.8.1.
The loader DLL and preparation behavior are unchanged from 1.8.0.

This replaces the earlier directory-runtime 1.8.1 package. Its old ZIP SHA-256 was
`6fa5e3b9cd54ec3c2deff83787425229ac333e9cee5d314bad89735052857022`.

## Installation

1. Close the game and helper, then extract the complete loader ZIP beside
   LEGOStarWarsSaga.exe. TCSAssetManager.exe includes its runtime. An old
   TCSAssetRuntime folder is no longer used and may be removed.
2. Keep supported mod archives compressed in Mods. Character packs requiring
   expanded tables also need the separate
   [Character & Shop Extender 1.0.0](https://github.com/DevelopmentStatus/TCS-Mods/releases/tag/TCSCharacterShopExtender-v1.0.0)
   ZIP in Mods. It remains a separate download.
3. Double-click **Launch Modded Game.cmd**. Use it again after mod/archive or asset
   setting changes. Manage Mod Priorities.cmd > Launch game also prepares mods;
   Prepare Mods.cmd prepares without launching.

Keep existing settings and saves. The extender's companion-save format is unchanged.

## Validation and scan status

- 34 asset import, manager, launch, path and conflict tests passed. The test runner
  used a workspace temporary directory to avoid default-temp access failures.
- The rebuilt package passes ZIP integrity checks, contains no separate runtime
  folder or nested ZIP entry, and preserves the original 1.8.1 loader DLL bytes.
- The actual packaged single EXE passed its Tk GUI-runtime check and prepared an
  empty game directory without a separate runtime folder.
- The same packaged EXE prepared the optional extender plugin cache and a fixture
  cosmetic asset overlay successfully without a separate runtime folder.
- No new full gameplay test was performed. Earlier native checks apply to the
  unchanged loader DLL; this revision changes the helper packaging and version.
- No further VirusTotal submissions or checks were performed. The unchanged DLL
  has the previously recorded
  [0/70 result](https://www.virustotal.com/gui/file/5e73a7fe09f4fb6138ff4a148fe46a32486dc5a9a583e2658821a45dfaa45d5d).
  The rebuilt helper and ZIP are **antivirus status unverified**. This packaging
  change does not establish the cause of the earlier quarantine or guarantee
  acceptance. See [1.8.0 evidence and limitations](TCSModLoader-v1.8.0.md).

ZIP SHA-256: `aee3ce20c6ffd6549bd48398e87c0c130aa7a6e7258d8ee9faf508e701eac257`.

Helper SHA-256: `9bbdf114fc1bc3b835452d56d01b0105b9c7fa610d0bb64010a5d845f709c7b9`.
