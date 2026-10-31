# TCS Mod Loader 1.8.0

For the supported Windows Steam version of LEGO Star Wars: The Complete Saga.
Provides the Direct3D 9 proxy, asset overlay, plugin host and native Mod Settings.

Character/shop expansion and companion saves are now a separate optional mod:
TCSCharacterShopExtender-v1.0.0.zip. It is not included in this loader package.
Character packs that change roster/shop tables, private names or generated
portrait support require that plugin. Ordinary cosmetic replacements can run
without it. The manager explains missing dependencies before launching.

## Install or update

1. Close the game and back up any existing d3d9.dll.
2. Extract this ZIP beside LEGOStarWarsSaga.exe, including TCSAssetManager.exe
   and the complete TCSAssetRuntime folder. Keep that runtime folder intact.
3. Put desired plugin and supported asset ZIP/RAR files directly in Mods.
   Keep them compressed. Add the separate extender ZIP for character packs.
4. Run Manage Mod Priorities.cmd and choose Launch game. Alternatively run
   Prepare Mods.cmd, then launch normally. Repeat after changing archives or
   asset settings, including settings changed in the game.

The loader reads prepared files. It does not launch importers or extract plugin
DLLs during game startup. The helper loads its runtime from TCSAssetRuntime.
Keep Mods/Config when updating to preserve settings.
Built-in Saga Tweaks / ASI loading remains removed; install ASI mods separately
following their authors' instructions. Compatibility is not established.

## Settings and saves

Open Pause > Mod Settings for supported mod settings. Higher asset priority
loads later. Save/Close in the manager saves settings only; Launch game also
prepares them. Mods require a restart; live DLL unloading is not supported.

The core loader does not install character/save hooks. If using the extender,
keep each .tcs-characters file beside its corresponding native save. Existing
companion files remain compatible with the separate plugin and are not deleted
when the plugin is removed. Required character overlays are refused when the
extender fails to initialize, so they cannot overflow the game's native limits.

Core-loader and optional-extender antivirus results are reported separately in
the repository release notes. Installing the extender also installs its native
patching code; a low detection count for the loader does not describe the plugin.

Full guide, download links and hash-specific scan results:
https://github.com/DevelopmentStatus/TCS-Mods

Required third-party license notices are in licenses/.
