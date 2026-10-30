# TCS Mod Loader 1.7.3

Fixes blank portraits in the character shop and selection grid with large mod
collections. The original object/icon registry filled at 750 entries, leaving
later characters without icon IDs even when their portrait files were present.
The registry now has 4,096 slots and 256 KiB of name storage, including normal
and locked portraits. The user confirmed that the installed fix works in-game.

Includes all improvements since the previous published 1.6.6 release:

- Up to 1,000 registered characters and character-shop entries, with matching
  spawning, Free Play, purchase and cheat-code storage fixes.
- Save-loading crash fix for rosters above 340 characters. Native stack security
  checks remain enabled.
- Added-character progress stored in per-save `.tcs-characters` companions.
  Keep companions beside native saves when backing up or transferring progress.
- Missing roster registrations inferred from matching character configs, models
  and collection definitions.
- Individual portrait preloads, corrected known packaged icon aliases and
  priority-aware shared-scene fallbacks.
- Direct loading of the supported Saga Tweaks 1.2.0 archive without Universal
  ASI Loader, with Mod Settings and Modern Options compatibility.
- Startup/window-mode fixes and the existing automatic priority sorting.

Normal purchases and story unlocks still apply. Other people's mod assets are
not included. ASI compatibility is restricted to the supported Saga Tweaks build.

Validation: 50 Python tests passed; 19 native CTest checks passed and one existing
fixed-address plugin probe skipped. Native regression tests execute the actual
registry initializer and add-extra code beyond entry 750, verify normal/locked
icons and storage bounds, and retain the save-load crash regression with 340,
373 and 1,000 characters. The icon fix was also confirmed in-game by the user.

Install with the game closed. Replace both `d3d9.dll` and `TCSAssetManager.exe`
from the ZIP and retain `Mods/Config`. Native saves keep their original format.
This native update retains importer/cache version 1.7.0; prepared assets do not
need rebuilding solely for this fix.
