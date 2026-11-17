# TCS Mod Loader 1.8.3

For the supported Windows Steam version of LEGO Star Wars: The Complete Saga.
Provides the Direct3D 9 proxy, asset overlay, plugin host and native Mod Settings.

## Install or update

1. Close the game and extract the complete ZIP beside LEGOStarWarsSaga.exe.
   TCSAssetManager.exe includes Python; no separate runtime folder is needed.
2. Put desired plugin and supported asset ZIP/RAR files directly in Mods.
   Keep mod archives compressed. Character packs requiring expanded tables,
   private names or portrait support also need the separate optional
   TCSCharacterShopExtender-v1.0.0.zip in Mods.
3. Start through Steam. If mods need preparation, the
   initial game startup stops and Manage Mod Priorities opens automatically.
   Choose Launch game to prepare your mods and start through Steam, or choose
   Save & prepare to apply changes without starting the game.
4. Prepared setups start normally. Archive or asset priority/variant changes,
   missing plugin caches or a missing overlay reopen the manager when needed.
   Large packs can take several minutes. Closing the manager does not launch
   the game; preparation errors stay in the editor for you to fix.
   You can also open Manage Mod Priorities.cmd whenever you want to edit order.
   For Steam application-load errors, start from Steam or use Launch game in
   the manager. Steam must accept startup before the loader can run.

Manage Mod Priorities.cmd is the only command script in this package. Remove
Launch Modded Game.cmd and Prepare Mods.cmd left by older versions. An old
TCSAssetRuntime folder is no longer used. Keep Mods/Config and all saved games.

The loader reads prepared files and does not run an importer inside the game.
It checks preparation before loading plugins and automatically opens the manager
when needed. The manager waits until the initial game process has exited.

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
