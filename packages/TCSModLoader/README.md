# TCS Mod Loader 1.8.2

For the supported Windows Steam version of LEGO Star Wars: The Complete Saga.
Provides the Direct3D 9 proxy, asset overlay, plugin host and native Mod Settings.

## Install or update

1. Close the game and extract the complete ZIP beside LEGOStarWarsSaga.exe.
   TCSAssetManager.exe includes Python; no separate runtime folder is needed.
2. Put desired plugin and supported asset ZIP/RAR files directly in Mods.
   Keep mod archives compressed. Character packs requiring expanded tables,
   private names or portrait support also need the separate optional
   TCSCharacterShopExtender-v1.0.0.zip in Mods.
3. Open Manage Mod Priorities.cmd. Choose Save & prepare to apply mods without
   launching, or Launch game to prepare and start the game. Steam installations
   launch through Steam; other folders launch their LEGOStarWarsSaga.exe.
4. After successful preparation, normal Steam/EXE launches use those mods.
   Repeat Save & prepare after changing archives or asset settings. Large packs
   can take several minutes. Preparation errors keep the editor open to fix them.

Manage Mod Priorities.cmd is the only command script in this package. Remove
Launch Modded Game.cmd and Prepare Mods.cmd left by older versions. An old
TCSAssetRuntime folder is no longer used. Keep Mods/Config and all saved games.

The loader reads prepared files and does not run an importer inside the game.
Starting with unprepared plugins now explains the required manager step.

## Settings and storage

Open Pause > Mod Settings for supported mod settings. Higher asset priorities
load later. Changes require preparation and a restart; live DLL unloading is
not supported. The manager retains the active generated cache and at most one
previous generation per cache. Plugin installers share one rotating previous
package at Mods/Backups/PreviousInstall.zip.bak instead of timestamped copies.

Character/shop expansion and companion saves remain in the separate optional
extender. Keep each .tcs-characters file beside its corresponding native save.
Dependent overlays are refused if the extender fails to initialize.
Saga Tweaks / ASI integration remains removed.

This build's antivirus status is unverified. Earlier scan results apply only
to their exact earlier files. Required license notices are in licenses/.

Source and release notes: https://github.com/DevelopmentStatus/TCS-Mods
