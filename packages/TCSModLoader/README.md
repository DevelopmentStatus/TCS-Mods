# TCS Mod Loader 1.6.4

Supports native DLL plugin ZIPs and legacy TCS asset mods in ZIP/RAR archives.

Close the game and back up your existing d3d9.dll. Extract this package beside
LEGOStarWarsSaga.exe. Keep both d3d9.dll and TCSAssetManager.exe there.
No QuickBMS or separate Python download is required.

Drop supported mod archives directly into Mods and restart. Keep them compressed.
Under Pause > Mod Settings, asset packages have Enabled and Priority controls;
changes take effect on restart. Higher priority wins file conflicts.

To remove an asset mod, close the game, remove its archive, and restart. The
original DAT asset returns unless another enabled mod replaces the same file.
Original game DATs are never rewritten. Mods/Assets contains generated caches;
it is safe to remove that folder while the game is closed.

New in 1.6.4: a visible preparation window appears before the asset helper starts.
Unchanged launches reuse the overlay even when default INI settings are written
or formatting changes. Changed archives, order, enabled state and variants rebuild.
The first launch after this update rebuilds once; later unchanged launches reuse it.

Also included: DLL-plugin scanning checks the ZIP directory for root manifest.ini
before extraction. Asset-only ZIPs are no longer extracted a second time into
Mods/Loaded. Plugin extraction pumps window messages and stops timed-out workers.
This fixes the duplicate extraction observed during the delayed game startup.
Update BOTH d3d9.dll and TCSAssetManager.exe. Existing generated caches are retained.

Also included: the game pumps window messages while waiting for asset preparation,
unchanged archives reuse cached hashes, and a required load-order window is brought
forward. Startup diagnostics are written to TCSModernOptions.log. This addresses
loader waits that could leave Steam showing Running before a usable window appears.
Replace BOTH d3d9.dll and TCSAssetManager.exe when updating.

Also included: the window and header show the loader version. The action bar keeps
its own space, the lists shrink instead of hiding Launch game, toolbars wrap at
higher DPI, and minimum size adapts to the screen and scaled controls.

Also included: the startup priority editor opens only when enabled mods overlap
AND two or more providers have the same priority. Distinct priorities skip the
popup, even when files overlap. Manual editing remains available at any time.

Auto-sort suggests broad world packs first, then focused replacements, and assigns
unique priorities. It uses package structure/file coverage, not archive names or
a complete dependency database. Review mod-specific requirements before saving.

Save settings saves without closing the editor. Launch game saves and continues
the waiting game; from the manual editor it prepares the overlay and starts
LEGOStarWarsSaga.exe. An exact-state review token prevents a second identical
startup popup; changed archives or configs are checked again.

Drag mods or use Load earlier/later, edit priorities, disable packs and choose
variants. The overlapping-file view shows the last provider and distinguishes
merge-aware character tables from complete file replacements. Close the editor
to discard unsaved edits and continue with saved settings.
Double-click Manage Mod Priorities.cmd to open it manually with the game closed.
Python and Tcl/Tk are bundled; no Python installation is required.
DLL patch conflicts and missing dependencies are outside this file-overlap check.

Existing features: infer loose character paths using game registries and neighboring
files, select whole model variants in Mod Settings, and import larger overhauls
(up to 8 GiB extracted / 512 MiB per file). Adds MOVIES/BIK/FNT/CSV support.
Clones works directly with normal/antenna/visor options; Modern Overhaul 2.2.2
fits the expanded limits. Give addons a higher Priority than their base overhaul.

Legacy character packs can supply roster, shop and English-name
records in TXT documentation. Only records tied to supplied characters are read;
installation commands are never run. Loose ICONS/TEXT folders map into STUFF.
The original Mandalorian Season 2 archive works directly, without a repack.

Also includes general level/world package support, including cutscenes, terrain,
AI/scripts, music configs and pack resources. Detection uses game directory layout,
not archive names. Shared level-pack tables are preserved as full replacements;
priority resolves file conflicts. The extracted package limit is 8 GiB.

Additional Levels 2.3.1 and its Modern Overhaul 1.4 variant are supported by this
general path. Install ONE variant; the Overhaul variant requires Modern Overhaul.
Keep archives compressed in Mods. Other level packs with the same supported
layout work through the same importer, subject to dependencies and file conflicts.

Existing character compatibility targets: Darth Nihilus ver 0.0.4, RTX Obiwan, and TIE Fighter
Pilot / AT-AT Driver (2010 Version). Other archives may require import adapters.
See docs/ASSET_IMPORT.md for details, conflict rules and validation limits.

Validated x86 Steam game build, on 64-bit Windows 10/11. Third-party runtime
license notices are included in licenses/. Nexus mods and game assets are not
included in this loader package.
