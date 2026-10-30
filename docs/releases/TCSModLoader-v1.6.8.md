# TCS Mod Loader 1.6.8

The supplied Saga Tweaks 1.2.0 archive now works directly from Mods without
Universal ASI Loader. ZIP/RAR/7z inspection recognizes the supported ASI, stages
it separately from game assets, and publishes a native-module catalog. Only the
exact supported binary is accepted; both importer and native loader validate
SHA-256 `1ddc6cb2abac5ccf05521c68bec7d4072d00a696388cec6587a85d858af82efd`.
The native loader loads it outside DllMain before optional TCS plugins. Windows
invokes Saga's DLL initialization; no general ASI export is called.

Saga Tweaks' startup signature check conflicts with Skip Splash's patch, and its
display settings overlap Modern Options. Those two TCS plugins are skipped for
the run when verified Saga Tweaks is active. Archives/configs are retained. They
load normally again after Saga is disabled/removed and the game restarted.

Saga's native Tweaks/Drop Out/Quit dispatch is preserved. The loader appends Mod
Settings after the existing pause rows, restores selection after Saga's draw-time
clamp, and returns to its dynamic row when closing settings. The supplied Saga
binary remains unchanged. Already-loaded verified Saga modules are not loaded
or initialized a second time.

## Installation

1. Close the game. Back up the current d3d9.dll and TCSAssetManager.exe.
2. Extract this loader ZIP beside LEGOStarWarsSaga.exe, replacing both files.
3. Leave the supplied SagaTweaks 1.2.0 .7z directly in Mods, compressed.
4. Start the game. No Universal ASI Loader/dinput8.dll installation is needed.

Saga's generated ASI, INI and log are under Mods/Loaded/ASI/SagaTweaks. That
stable folder survives cache-generation changes; original mod archives and save
files are unchanged. Manage Mod Priorities / Enabled controls can disable the
Saga archive for the next launch. Generated modules are loaded only when named
by the current catalog, so removing an archive cannot reactivate a stale copy.

Validation: 43 Python tests passed, including importing the supplied 7z,
checksum rejection, changed-archive rejection, duplicate selection, cache reuse
and disabling. Fifteen native tests passed, including CNG verification of the
supplied ASI, compatibility-handler resolution, plugin conflict policy, dynamic
pause rows, selection-clamp recovery and two-player row changes. One existing
Shop Ownership runtime test was skipped because its fixed probe address was
unavailable. The frozen importer also passed a supplied-archive/catalog smoke
check. Full in-game Saga Tweaks initialization and gameplay remain unverified.
