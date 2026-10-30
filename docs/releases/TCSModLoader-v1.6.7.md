# TCS Mod Loader 1.6.7

Added character rosters can outlive the portrait resources they need when mods
share icon files. The importer now validates portrait filenames and scene object
names, repairs the known KOTOR Anakin-template mismatch in generated aliases,
and keeps normal/fallen Bastila distinct. Original mod archives remain untouched.

The loader preloads valid individual portrait scenes through the native icon
loader. Shared scene conflicts retain the priority winner; earlier resources
are consulted only for names absent from both the winner and valid preloaded
individual scenes. Supplemental scenes are released during native icon teardown
and loaded again with the next area. There are no fake character registrations
or save/unlock changes. Portrait diagnostics are included in the import report.

Close the game and extract this ZIP beside LEGOStarWarsSaga.exe, replacing both
d3d9.dll and TCSAssetManager.exe. Keep Mods and Mods/Config. The importer rebuilds
the overlay once for this version, then reuses its normal cache.

Validation: 38 Python tests and 14 native tests passed. An offline copy of the
installed stack repaired all 12 checked KOTOR character registrations, validated
160 individual preload names, and retained the two missing Modern Overhaul
portrait pairs through one shared fallback resource. Native tests cover source
precedence, name restrictions, scene reuse, cleanup and reload. Full gameplay
and the exact question-mark slots in the user's screenshot remain unverified.

This release does not add Saga Tweaks ASI loading or merge arbitrary text files.
