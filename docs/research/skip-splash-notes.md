# TCS Skip Splash 1.0.0

Target: LEGO Star Wars: The Complete Saga, Steam Win32.
Installed executable SHA-256: 085688e73561c61f4619fcf1f3006c56425ca365e80ea5fc0175bab334cec671.
Runtime analysis image SHA-256: e4666017907099507a2939ef77a7bfaaabdb51586b6cf3491323e5c1574014d6.

Capstone inspection of research/TCS.runtime-analysis.exe identified LoadPerm
at 0x4ca5b0. Its native synchronous path at 0x4cad3a sets the language-selection
state to 3, calls LoadPermData at 0x4c9fb0, cleans up the legal material and
restores WORLD. The normal branch posts LoadPermData to a background worker
and displays the startup sequence while loading. Historical source in
research/saga/src/legogame/startup.cpp helped identify the path; its Android
addresses are not used for the PC patch.

The plugin replaces the six-byte conditional branch at 0x4ca5e2 with a
five-byte jump plus NOP to the existing synchronous path. Original entry
and target instructions must match before any write. No executable or asset
file is modified on disk. Disabling takes effect after restarting the game.
Startup language selection is also bypassed; saved language is retained.

Validation: Win32 Release DLL built; all six CTest checks passed, including
signature rejection and jump-destination checks. Installed package hash
matches the distribution. The game loader log confirmed plugin loading and
successful startup-branch installation alongside First Person and Modern
Options. The user confirmed "it works" during the live launch test.

REA MCP had no configured Ghidra provider. CLI analysis with the installed
Ghidra encountered temporary-file admission failure inside the sandbox; a
subsequent analysis was cancelled once native byte inspection and the live
game/user confirmation established the result. No REA evidence claim is made
for the Capstone inspection. Computer Use window capture was not approved,
so no screenshot verification was performed.
