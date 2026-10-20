# Drag-and-drop asset mods - loader 1.1.0

For the validated 32-bit Steam PC game on 64-bit Windows 10/11.

## Install and use

1. Close the game. Back up your existing loader d3d9.dll.
2. Extract the loader update beside LEGOStarWarsSaga.exe. Both d3d9.dll and
   TCSAssetManager.exe must be beside the game executable.
3. Drop supported Nexus ZIP or RAR archives directly into Mods. Keep them
   compressed. Existing DLL plugin ZIPs continue to work.
4. Start the game. Under Pause > Mod Settings, each imported asset package has
   Enabled (restart) and Priority (higher wins, restart) controls.

No QuickBMS or separate Python installation is needed. The packaged helper
includes Python and uses the Windows built-in tar.exe to read RAR files.
The first import takes longer; unchanged archives/settings reuse the overlay.

## Removal and conflicts

Close the game, remove an archive from the top level of Mods, and restart.
The active overlay is rebuilt from the remaining enabled archives. If no other
mod replaces the same asset, the game loads its original DAT asset again.
If another mod also replaces it, that mod's replacement becomes active instead.
Disabling a package has the same effect after restart. Changes are not hot-reloaded.

Higher priority wins binary-file conflicts. Equal priorities are resolved by
filename order, with the later filename winning. Shared character lists are
merged only for characters defined by the package. Language edits are limited
to name IDs used by the package's character definitions. Combined binary icon
resources are replacements, not merged images, and can conflict with other
icon packs. Inspect Mods/Assets/import.log for the selected winners/errors.

The loader never modifies or repacks the installed DAT files and never copies
replacement assets over the original game directories. It redirects native
open, existence, size and bulk-read operations to Mods/Assets when an enabled
replacement exists; other reads continue through the game normally.

Old generated folders do not activate removed mods. With the game closed,
Mods/Assets may be deleted to clear generated caches; the next launch rebuilds
them. Keep Mods/Config to preserve enabled/priority settings.

## Initial supported packages

| Archive | Import behavior |
| --- | --- |
| DARTHNIHILUS ver 0.0.4 | Strip wrappers; merge Nihilus roster/shop entry and owned German name ID; stage models, icons and animations |
| RTX Obiwan | Map bare OBIWANKENOBI_EP3_PC.GHG to CHARS/OBIWANKENOBI/ |
| TIE Fighter Pilot and AT-AT Driver (2010 Version) | Strip wrapper; stage characters/icons; add the supplied AT-AT Driver shop rule through a fixed adapter |

Archive readmes, links and executable instructions are never executed. Other
packages with recognized game directory layouts can import, but packages that
require scripts, custom binary merges or other manual instructions need adapters.
Do not assume every Nexus archive is automatically compatible.

If importing fails, asset hooks stay disabled for that launch and the game uses
native assets. Existing plugin loading remains available. See
Mods/Assets/import.log and the failed generation's report.json.

## Validation

Seven Python tests cover path validation, shared-file isolation, merging,
priority, disabling, removal, unchanged DAT bytes, caching and atomic publication.
The native suite covers routing through open/size/existence/bulk reads, write
pass-through and fallback. An isolated copy of the real game checked all 145
imported files byte-for-byte through native open/read/size/bulk loading with zero
failures. After removing the three archive copies, a second real-game run checked
that the overlay was empty and the vanilla roster loaded, again with zero failures.
These checks do not establish visual/animation correctness for every character.

Developer checks:

```powershell
python -m unittest discover -s tests -p 'test_asset*.py'
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\Build-AssetManager.ps1
.\scripts\Package-Mods.ps1
```

The opt-in TCS_BUILD_RUNTIME_PROBE target includes an asset probe for isolated
game integration checks; it is never included in release packages.
