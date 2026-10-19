# TCS native mod packages

The TCS Mod Loader targets the validated 32-bit Steam PC build of LEGO Star Wars: The Complete Saga.

## Install the loader

1. Close the game and find its installation folder, the folder containing `LEGOStarWarsSaga.exe` (Steam: Properties → Installed Files → Browse).
2. If a `d3d9.dll` already exists there, copy it somewhere safe as a backup.
3. Copy the loader package's `d3d9.dll` beside `LEGOStarWarsSaga.exe`.
4. Create a `Mods` folder in that same location if needed. The loader ZIP contains this folder too, so extracting the full loader ZIP into the game folder creates it automatically.

## Install mods

Keep each mod as a ZIP. Put mod ZIPs directly into `<game folder>\Mods`; do not extract them. For example, put `TCSFirstPerson-v0.4.0.zip` and `TCSModernOptions-v0.4.0.zip` together there. Launch or restart the game to load them. Each package is extracted under `Mods\Loaded`; configuration is stored under `Mods\Config`.

Pause the game and choose **Mod Settings**, directly below **Effects Settings**, to see installed mods, then select a mod name to open its settings. This menu belongs to the loader and works without TCS Modern Options. Registered settings receive controls automatically, rendered with the game's own menu text and navigation. Select the mod name, use the page row for additional settings, and confirm or left/right to change a value. Back or Escape returns to the pause menu. Display changes use the native game resource-release/reset/rebuild sequence. Choose Keep display changes to save them; unconfirmed changes revert after 15 seconds.

To remove a mod, close the game and delete its ZIP. To remove the loader, close the game and restore the original `d3d9.dll` backup. Delete the generated `Mods\Loaded` folder only while the game is closed; keep `Mods\Config` to preserve settings.

## Native plugin development

Include `src/mod_api.h` when compiling a 32-bit Windows DLL. Packages are ZIP files with `manifest.ini` at the archive root and the DLL entry named by its `[Mod] Entry` key. Example:

```ini
[Mod]
Id=example.plugin
Name=Example Plugin
Version=1.0
Entry=ExamplePlugin.dll
```

The plugin registers its ID, display name and version, then typed settings and optional device, reset, render and window-message callbacks. Stable IDs and keys retain user config. Every registered mod gets its own selectable entry. Its registered settings appear together on its settings page; categories remain descriptor metadata. Supported settings are Boolean, integer, float, key binding and enum. The loader automatically displays controls for registered settings in Mod Settings; plugins do not need to create pause-menu buttons or draw their own settings UI. Callbacks and context pointers must remain valid until shutdown. The API is game-specific; it does not load BepInEx or Unity plugins.

For the native API and ZIP packaging steps, see [Mod creator guide](MOD_CREATOR_GUIDE.md).
