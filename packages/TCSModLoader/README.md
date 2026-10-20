# TCS Mod Loader 1.1.0

Supports native DLL plugin ZIPs and legacy TCS asset mods in ZIP/RAR archives.

Close the game and back up your existing d3d9.dll. Extract this package beside
LEGOStarWarsSaga.exe. Keep both d3d9.dll and TCSAssetManager.exe there.
No QuickBMS or separate Python download is required.

Drop supported mod archives directly into Mods and restart. Keep them compressed.
Under Pause > Mod Settings, asset packages have Enabled and Priority controls;
changes take effect on restart. Higher priority wins file conflicts.

To remove an asset mod, close the game, remove its archive, and restart. The
original DAT asset returns unless another enabled mod replaces the same file.
Original game DATs are never rewritten. Mods/Assets contains generated caches;
it is safe to remove that folder while the game is closed.

Initial compatibility targets: Darth Nihilus ver 0.0.4, RTX Obiwan, and TIE Fighter
Pilot / AT-AT Driver (2010 Version). Other archives may require import adapters.
See docs/ASSET_IMPORT.md for details, conflict rules and validation limits.

Validated x86 Steam game build, on 64-bit Windows 10/11. Third-party runtime
license notices are included in licenses/. Nexus mods and game assets are not
included in this loader package.
