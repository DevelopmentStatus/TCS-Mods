# TCS First Person and Mod Loader

A native camera and input mod for the validated 32-bit Steam PC build of LEGO Star Wars: The Complete Saga.

## Install

Install the separate TCS Mod Loader once: back up any existing `d3d9.dll`, then copy the loader's `d3d9.dll` beside `LEGOStarWarsSaga.exe`. Create `<game folder>\Mods` if needed. Then place mod ZIP archives directly in `Mods` and restart. For first-person play, add `TCSFirstPerson-v0.3.11.zip`. Add `TCSModernOptions-v0.4.0.zip` if you also want its display options. Keep the ZIPs compressed; the loader extracts them at startup.

Pause the game and choose **Mod Settings**, directly below **Effects Settings**, to open the loader's list of installed mods. Select a mod name, such as **TCS Modern Options**, to open only its settings. The loader creates controls automatically for registered mod settings; TCS Modern Options is optional. Display changes can be reverted by pressing Escape or waiting for the 15-second confirmation timer. Per-mod options are saved in `Mods\Config`. The legacy `MenuKey` option is ignored. See [MODDING.md](docs/MODDING.md) for setup and removal. Mod creators can use [MOD_CREATOR_GUIDE.md](docs/MOD_CREATOR_GUIDE.md) for the native API and ZIP packaging steps.

Select a mod name to open its settings. Use the game's up/down controls to select a setting; confirm or left/right changes its value. Select the page row for more settings. Select a key binding and press its new key, or Escape to cancel. Choose **Back to mods** or press Escape to return to the mod list; from there, **Back to pause menu** or Escape returns without resuming play.

## First Person controls

- **C:** cycle first person → animated head camera → third-person orbit → original game camera.
- First person hides the controlled character's head and attached skeleton joints while keeping the body visible. Characters without a usable head, including GonkDroid, use orbit fallback.
- **WASD:** camera-relative movement; **Shift:** walk; diagonal movement is normalized.
- **Left mouse:** attack/fire/melee. **Right mouse:** block with a lightsaber/melee character or narrow the first-person view for ranged characters.
- **Space:** jump; **E or F:** native special ability/interact/build/Force action.
- **Q:** nearby-character switch/tag. **Tab/R:** previous/next character when permitted.
- **Mouse wheel:** cycle characters in first person or original camera; zoom orbit. **F5:** toggle custom camera; **F6:** release/resume mouse look; **F7:** recenter.
- Controller input and native menus remain available. The mod does not invent crouch, reload or sprint actions.

## Remove

Close the game, then delete mod ZIPs from `Mods`. Remove the loader by restoring the `d3d9.dll` backup you made before installation; if none existed, delete the loader's `d3d9.dll`. Generated `Mods\Loaded` extraction folders can also be deleted while the game is closed. Config files remain under `Mods\Config` unless you choose to remove them.

## Build and validation

Build with Visual Studio 2022 C++ tools and CMake for Win32 Release, then run CTest. The current suite checks camera math, pause-row dispatch, per-mod navigation and plugin registration contracts. The settings panel uses native game rendering and navigation. Real display changes and full keyboard/controller coverage still require in-game verification.
