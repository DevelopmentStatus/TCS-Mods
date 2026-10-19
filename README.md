# LEGO Star Wars: The Complete Saga — TCS Mods

Native Windows mods and a shared mod loader for **LEGO Star Wars: The Complete Saga**, by **DevelopmentStatus**. This repository collects the mod source code, tests, package metadata, installation guides, and existing downloadable builds from the development workspace.

Targets the validated **32-bit Windows Steam PC build**. These are native C++ DLL plugins for this game. Other editions, executable revisions, and platforms have not been validated.

## Downloads and mod catalog

Download the ZIPs below using GitHub's **Download raw file** button. Install the loader first, then choose any combination of plugins.

| Component | Included build | What it does | Main source |
| --- | --- | --- | --- |
| [TCS Mod Loader](dist/TCSModLoader-v1.0.0.zip) | 1.0.0 | Direct3D 9 proxy, ZIP discovery, plugin API, saved settings, and a native pause-menu settings interface | [loader.cpp](src/loader.cpp), [native_pause_menu.cpp](src/native_pause_menu.cpp) |
| [TCS First Person](dist/TCSFirstPerson-v0.4.0.zip) | 0.4.0 | First-person camera, animated head camera style, orbit camera, mouse aiming, visible body, and PC controls | [mod.cpp](src/mod.cpp) |
| [TCS Modern Options](dist/TCSModernOptions-v0.4.0.zip) | 0.4.0 | Resolution, window mode, VSync, and frame cap controls | [modern_options.cpp](src/modern_options.cpp) |
| [TCS Ally Damage](dist/TCSAllyDamage-v1.1.1.zip) | 1.1.1 | Damage eligibility and active combat behavior for AI party companions | [ally_damage.cpp](src/ally_damage.cpp) |
| [TCS Skip Splash](dist/TCSSkipSplash-v1.0.0.zip) | 1.0.0 | Bypasses startup splash screens through the game's synchronous startup path | [skip_splash.cpp](src/skip_splash.cpp) |

The ZIPs are preserved builds from the original workspace. The source is the current workspace snapshot, not a reconstructed source history for every ZIP. [Older packages](dist/archive/) are retained for reference; see the [download archive guide](dist/README.md) before using them.

## Installation

1. Close the game. Open its installation folder containing `LEGOStarWarsSaga.exe`. In Steam, use **Properties > Installed Files > Browse**.
2. Back up any existing `d3d9.dll` before installing the loader. Multiple proxy DLLs cannot occupy the same filename; compatibility or chaining with other injectors has not been established.
3. Extract `TCSModLoader-v1.0.0.zip` into the game folder. Its `d3d9.dll` must sit beside the game executable. Create a `Mods` folder if it is missing.
4. Copy the desired plugin ZIPs directly into `Mods`. **Keep plugin ZIPs compressed.** Do not extract them yourself or nest them inside another folder.
5. Start the game. Open **Pause > Mod Settings**, directly below **Effects Settings**, then select a mod to configure it.

Example installation:

```text
LEGO Star Wars Saga/
├── LEGOStarWarsSaga.exe        # Your existing game executable
├── d3d9.dll                    # TCS Mod Loader
├── TCSModLoader.ini
└── Mods/
    ├── TCSFirstPerson-v0.4.0.zip
    ├── TCSModernOptions-v0.4.0.zip
    ├── TCSAllyDamage-v1.1.1.zip
    ├── TCSSkipSplash-v1.0.0.zip
    ├── Config/                # Created by the loader
    └── Loaded/                # Created by the loader
```

Only install one ZIP per plugin ID. Move older versions out of the top level of `Mods` before upgrading. Keep `Mods/Config` to retain settings. The loader refreshes extracted packages under `Mods/Loaded` at startup.

The optional helper scripts [Install-FirstPersonPlugin.ps1](Install-FirstPersonPlugin.ps1) and [Install-AllyDamagePlugin.ps1](Install-AllyDamagePlugin.ps1) install the included plugin builds and back up matching older ZIPs. Supply your own game folder:

```powershell
.\Install-FirstPersonPlugin.ps1 -GamePath 'C:\Games\LEGO Star Wars Saga'
.\Install-AllyDamagePlugin.ps1 -GamePath 'C:\Games\LEGO Star Wars Saga'
```

## Mod features and controls

### First Person

Choose **Normal** or **Animated** under **First person style**. Animated follows a skeleton attachment with position smoothing and immediate mouse look, while removing animation-driven camera roll. The character's head and attached joints are hidden while the body remains visible. Characters without a usable head, including GonkDroid, use an orbit fallback.

**Aim-bot (native targeting)** defaults to Off: projectiles leave the gun muzzle in the camera's direction. Enabling it restores native target selection. Third Person and Orbit retain native targeting. Horizontal field of view is configurable from 60 to 120 degrees; sensitivity, look limits, orbit distance, head hiding, crosshair, PC controls, and key bindings are configurable too.

| Default input | Action |
| --- | --- |
| C | Cycle First Person, Third Person, and Orbit camera slots |
| WASD / Shift | Camera-relative movement / walk |
| Left mouse | Attack, fire, or melee |
| Right mouse | Block for melee characters; narrow first-person view for ranged characters |
| Space | Jump |
| E or F | Native special action, interaction, build, or Force action |
| Q | Nearby character switch/tag |
| Tab / R | Previous / next character when permitted |
| Mouse wheel | Character cycling in First Person or Third Person; zoom in Orbit |
| F5 / F6 / F7 | Toggle custom camera / release or resume mouse look / recenter |

Animated is a setting within the First Person slot, not an additional C-cycle slot. Native menus and controller input remain available. There are no added crouch, reload, or sprint actions.

### Modern Options

Provides eight resolution choices from 640 × 480 through 2560 × 1440, windowed/borderless/exclusive fullscreen modes, VSync, and frame caps of Unlimited, 30, 60, 120, 144, or 240 FPS. Display changes use the game's device lifecycle and require confirmation within 15 seconds; Escape or timeout reverts them. This plugin is optional: the loader owns the shared Mod Settings interface.

### Ally Damage

**AI allies deal damage** enables player-style damage eligibility for AI-controlled registered party companions. **Active combat AI** lets lightsaber/blaster companions with a native combat opponent aim, draw weapons, attack at a controlled pace, and use brief defensive windows before counterattacking. Both options default to On and can be toggled independently. The game retains opponent selection and navigation. See the [investigation notes](docs/research/ally-damage-notes.md) for implementation evidence and verification limits.

### Skip Splash

Skips startup logos, legal splash, opening text, and the startup language-selection page. Required startup assets still load, so a brief black screen can occur. Existing language configuration is retained; configure language before enabling the mod if needed. The title menu and gameplay cutscenes remain available. **Skip splash (restart)** takes effect on the next launch. The plugin checks startup instructions and refuses a conflicting or unsupported patch. No game executable is modified on disk.

## Settings, removal, and troubleshooting

Use the game's up/down controls to select a setting, and confirm or left/right to change it. Use the page row for additional options. Select a key binding and press the desired key; Escape cancels capture. Back/Escape returns through the mod list to the pause menu.

Registered settings persist in `Mods/Config`. To reset a mod, close the game and back up or remove its config file. To uninstall a plugin, remove its ZIP while the game is closed. Generated `Mods/Loaded` folders can be removed with the game closed. To uninstall the loader, restore the original `d3d9.dll` backup, or delete the loader's DLL if no DLL existed originally.

| Symptom | Check |
| --- | --- |
| No Mod Settings entry | Loader DLL is beside the executable, the game build is supported, and another proxy has not replaced it |
| Plugin missing | ZIP is directly in `Mods`, has `manifest.ini` and its entry DLL at its root, and has no duplicate plugin ID |
| Plugin refuses to load | Check `TCSModernOptions.log` in the game folder for loader/plugin messages and possible build or patch conflicts |
| Display change looks wrong | Press Escape or wait for the 15-second rollback |
| Mouse remains captured | F6 releases/resumes mouse look |
| Old settings persist after update | Configuration is intentionally retained under `Mods/Config` |

When reporting issues, include the plugin version, game edition/build, other installed mods, reproduction steps, and relevant log excerpts.

## Repository layout

```text
src/                 C++ loader, four plugins, shared headers, and plugin API
tests/               Camera, menu, API, startup patch, and ally combat tests
docs/                Installation and native plugin development guides
docs/research/       Selected authored investigation notes (no game dumps)
packages/            Current package manifests, defaults, and package readmes
dist/                Latest preserved ZIP for each component and SHA-256 checksums
dist/archive/        Older preserved ZIPs
scripts/             Build packaging helper
CMakeLists.txt       Win32 CMake targets and CTest registration
```

Original game binaries/assets, runtime memory dumps, third-party tool checkouts, local logs, and generated build output are excluded. Selected investigation notes may reference files in the original private workspace that are not distributed here.

## Build from source

Requirements: Windows, **Visual Studio 2022** with Desktop development with C++ and a Windows SDK, and **CMake 3.20+**. Use **Win32/x86**, because the game and native addresses are 32-bit. The project uses C++17 and the static MSVC runtime.

From the repository root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Outputs are `build/Release/d3d9.dll`, `TCSFirstPerson.dll`, `TCSModernOptions.dll`, `TCSAllyDamage.dll`, and `TCSSkipSplash.dll`. To package locally built DLLs using the supplied manifests and defaults:

```powershell
.\scripts\Package-Mods.ps1
```

Generated ZIPs go under `build/packages`, leaving preserved downloads intact. The package helper copies current documentation into the loader package. Change version metadata before publishing a modified build.

`TCS_BUILD_RUNTIME_PROBE=ON` enables an optional native video lifecycle probe for controlled in-game investigation. It is off by default and is not included by the package helper.

## Testing and compatibility limits

CTest covers camera math, pause dispatch, per-mod navigation, First Person and Modern Options API contracts, startup patch validation, ally native hooks, synthetic DLL runtime behavior, and combat decisions. The ally runtime test may report **Skipped** when its required process memory layout is unavailable (exit code 77). A passing suite does not validate real gameplay, every character, all controller inputs, or actual display transitions. Those still require testing in the supported game.

The mods use game-specific hooks and executable assumptions. Compatibility with other game revisions, proxy injectors, or mods that patch the same routines is not established. See [installation details](docs/MODDING.md), the [mod creator guide](docs/MOD_CREATOR_GUIDE.md), and [research notes](docs/research/).

## Creating plugins

Use [src/mod_api.h](src/mod_api.h), API version 1, and build a 32-bit DLL exporting `TCSMod_Initialize`. Validate the host API version/size, register a stable plugin ID and typed settings, and package the DLL with a root-level `manifest.ini`. The manifest ID must match the runtime ID. The loader creates settings controls automatically. See the [complete mod creator guide](docs/MOD_CREATOR_GUIDE.md) for callbacks, persistence, packaging, and a minimal plugin example.

## Credits and rights

Mod author: **DevelopmentStatus**. This is an unofficial fan mod project, unaffiliated with or endorsed by LEGO, Lucasfilm, Disney, TT Games, or the game's publishers. All game names and trademarks belong to their respective owners. A separately obtained copy of the game is required.

No software license was present in the supplied project, so this repository does not assign a new license or grant rights to third-party game materials. Source is provided for inspection; ask the author about reuse or redistribution permissions.
