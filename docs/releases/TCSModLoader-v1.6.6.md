# TCS Mod Loader 1.6.6

When enabled asset mods overlap and share a priority, the loader now runs
Auto-sort, saves the resulting priorities and continues game startup without
waiting for Auto-sort, Save settings or Launch game clicks. The prepared overlay
is rebuilt from that saved order before the game uses it.

Distinct saved priorities remain unchanged. You can still move a Boba Fett
replacement below an overhaul, give it a higher number and save that custom
order. Opening Manage Mod Priorities.cmd also resolves tied overlaps and saves
them automatically, while leaving the editor open for manual changes. The
Auto-sort button remains available to explicitly recalculate any order.

Sorting uses the existing Auto-sort rules: broad level/world packs before
focused replacements, using file coverage to order packs. It cannot infer
every mod dependency or automatically choose a preferred replacement. Import
errors still open the editor so affected archives can be disabled. If automatic
saving fails, the editor opens for a manual retry.

Close the game and extract TCSModLoader-v1.6.6.zip beside LEGOStarWarsSaga.exe,
replacing both d3d9.dll and TCSAssetManager.exe. Keep Mods and Mods/Config.

Validation: 33 Python tests passed, covering automatic sorting and persistence, startup without
the editor, save-failure fallback, rebuilding the selected file winner, preserving
a manually chosen winner and reusing the resulting overlay on the next launch.
The bundled executable also passed automatic startup sorting/saving, cached
startup without settings rewrites, and preservation of a manual override.
