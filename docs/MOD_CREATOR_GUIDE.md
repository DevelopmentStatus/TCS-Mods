# TCS MOD LOADER - MOD CREATOR GUIDE
**API version 1**

This guide explains how to make a native mod package that the TCS Mod Loader can discover. The loader targets the validated 32-bit Windows Steam PC version of LEGO Star Wars: The Complete Saga. It is a game-specific native plugin system; it does not load BepInEx or Unity mods.

## Quick checklist
- Build a 32-bit Windows DLL for the game's supported build.
- Export TCSMod_Initialize with the TCS_MOD_EXPORT macro.
- Register a stable plugin ID, name and version through the host API.
- Put manifest.ini and the entry DLL at the root of one ZIP file.
- Place the ZIP directly in <game folder>\Mods and restart the game.

## 1. Include the API header
Use src\mod_api.h from this project. The plugin interface uses the Windows C ABI and __cdecl callbacks. Compile your plugin for x86 / Win32, not x64. The header declares TCS_MOD_API_VERSION as 1. At startup, check both apiVersion and structSize before calling host functions. Do not assume that a different API version has the same struct layout.

Your DLL must export this exact initialization symbol:
    TCSMod_Initialize

Use TCS_MOD_EXPORT so the symbol is exported without C++ name mangling. The loader also recognizes the common underscore-prefixed 32-bit export form. Keep the plugin ID stable and unique; the ID in manifest.ini must exactly match the ID passed to registerMod.

## 2. Minimal plugin example (C++)

    #include "mod_api.h"

    static int enabled = 1;

    static void __cdecl OnEnabled(TCSSettingValue value, void*) {
        enabled = value.integer ? 1 : 0;
        // Apply the feature change here.
    }

    TCS_MOD_EXPORT BOOL __cdecl TCSMod_Initialize(
        const TCSModHost* api, TCSModHandle* outHandle) {
        if (!api || !outHandle ||
            api->apiVersion != TCS_MOD_API_VERSION ||
            api->structSize < sizeof(TCSModHost) ||
            !api->registerMod || !api->registerSetting)
            return FALSE;

        TCSModInfo info{};
        info.structSize = sizeof(info);
        info.id = "example.plugin";
        info.name = "Example Plugin";
        info.version = "1.0.0";

        TCSModHandle mod = api->registerMod(&info);
        if (!mod) return FALSE;

        TCSSettingDescriptor setting{};
        setting.structSize = sizeof(setting);
        setting.category = "Gameplay";
        setting.key = "Enabled";
        setting.label = "Enable feature";
        setting.type = TCS_SETTING_BOOL;
        setting.defaultValue.integer = 1;
        setting.minimum = 0;
        setting.maximum = 1;
        setting.step = 1;
        setting.changed = OnEnabled;
        if (!api->registerSetting(mod, &setting)) return FALSE;

        *outHandle = mod;
        return TRUE;
    }

The setting callback is called when the setting is registered and again when its value changes. Keep callback functions and context data valid for the lifetime of the loaded DLL. The loader currently keeps successful plugins loaded for the life of the game process; do not rely on a shutdown callback being called.

## 3. Per-mod settings menus
The loader adds **Mod Settings** directly below **Effects Settings** in the pause menu. It owns the menu and generates a control for every registered setting, so plugins do not need their own pause-menu buttons, rendering code, or a TCS Modern Options dependency to expose configuration.

Each setting needs a unique key within its plugin. Keep plugin IDs and keys stable between versions so saved values remain attached to the same options. Settings are saved in:
    <game folder>\Mods\Config\<plugin-id-with-unsafe-characters-replaced-by-underscores>.ini

The loader stores registered values in the [Settings] section. Plugins can also use host->configPath(handle, buffer, bufferSize) to obtain their config path. If a package contains a root-level default.ini and that config file does not already exist, the loader copies default.ini there before returning the path.

Supported setting types:
- TCS_SETTING_BOOL: value.integer is 0 or 1.
- TCS_SETTING_INT: value.integer is clamped to minimum and maximum.
- TCS_SETTING_FLOAT: value.real is clamped to minimum and maximum; step controls left/right adjustment.
- TCS_SETTING_KEY: value.integer is a Windows virtual-key code. The menu can capture a new key.
- TCS_SETTING_ENUM: value.integer is a zero-based choice index. Provide choices and choiceCount; minimum/maximum are normalized to the available indices.

Selecting a mod name opens all settings registered by that mod. The category string remains useful descriptor metadata; plugins do not need to create their own menu buttons. Long lists receive page controls automatically.

For enum settings, keep the choice strings and the choices array alive while the plugin is loaded. Use UTF-8 for IDs, names, labels, categories, keys and enum labels. Keep labels short enough to fit the options menu.

The host also exposes setVideoMode(width, height, mode, vsync) and setFrameCap(fps). Mode values are 0 for windowed, 1 for borderless and 2 for exclusive fullscreen. These functions are intended for a game options plugin such as TCS Modern Options. Video mode changes are confirmed by the user and revert after 15 seconds if they are not confirmed.

## 4. Optional game callbacks
- registerDeviceCallbacks(handle, created, reset, endScene, context): receive device creation, device reset notifications and EndScene callbacks.
- registerWindowCallback(handle, callback, context): observe game-window messages. Set *handled to TRUE and return a result only when the plugin intentionally consumes a message.

The loader currently stores one shared context pointer for the device and window callbacks; use the same context pointer for both registrations. Device-reset callbacks receive before=1 before reset and before=0 after reset. Render/window callbacks run on the game thread. Keep callbacks quick and avoid blocking the game.

The loader uses fixed Direct3D9 hooks and reverse-engineered game behavior. Only use addresses and assumptions verified for the supported game executable. A bad native plugin can still crash the game; isolating failed loads does not sandbox plugin code.

## 5. Create the ZIP package
Create a staging folder. Put these files directly inside it:

    ExamplePlugin.zip
    |-- manifest.ini
    |-- ExamplePlugin.dll
    |-- default.ini       (optional)
    |-- dependency.dll    (only if your plugin needs it)
    `-- other required assets

The ZIP must have manifest.ini at its root; do not zip the containing staging folder as an extra top-level directory. The DLL name must match the Entry value exactly. Entry must be a filename in the package root, not a path to another folder.

Example manifest.ini:

    [Mod]
    Id=example.plugin
    Name=Example Plugin
    Version=1.0.0
    Entry=ExamplePlugin.dll

Id must match info.id registered by your plugin. Name and Version are descriptive package metadata. The runtime menu uses the name and version registered by the DLL.

To create a ZIP in File Explorer, select the files inside the staging folder (not the staging folder itself), right-click, and choose Send to > Compressed (zipped) folder. Alternatively, in PowerShell, run this from the parent of the staging folder:

    Compress-Archive -Path .\ExamplePlugin\* -DestinationPath .\ExamplePlugin.zip

Check the archive by opening it: manifest.ini and ExamplePlugin.dll should appear immediately, with no extra folder level.

## 6. Install and verify
1. Install the TCS Mod Loader once. See README.md in the loader package.
2. Copy ExamplePlugin.zip into <game folder>\Mods. Leave it zipped.
3. Start or restart the game. The loader extracts packages to Mods\Loaded and loads each plugin independently.
4. Pause the game, select Mod Settings directly below Effects Settings, and choose your mod name to find its registered settings. This works with the loader alone; TCS Modern Options is optional.
5. Edit a setting, exit and restart, then confirm it persists in Mods\Config.

If the plugin has an invalid manifest, missing entry DLL, unsupported API, duplicate ID or initialization failure, the loader skips that package and continues with other mods. To update a package, replace its ZIP while the game is closed. The loader refreshes the extracted folder at next startup. Keep user settings in Mods\Config when updating.


The loader renders these controls using native game-menu drawing and input. Settings are paginated automatically; no custom mod UI or pause-menu hooks are needed.
