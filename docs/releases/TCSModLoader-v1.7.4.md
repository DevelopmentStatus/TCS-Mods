# TCS Mod Loader 1.7.4

Fixes wrong character names when separate packs reuse the same language IDs.
For example, Imperial Trooper and Anakin packs both supplied IDs 1764–1768;
later text replaced earlier names. Darth Nihilus supplied its name only in the
German table while the English baseline retained "Temple Soldier" at ID 279.

Winning character configs now receive private text IDs tied to their supplying
pack's labels. A replacement skin may inherit the name from an earlier provider
of the same config. Unrelated UI rows and coordinated level-pack text remain
intact. Supplied translations are preserved; missing translations fall back to
the pack's English name, or its supplied name when English is absent. Original
archives remain unchanged. The generated report lists each remapped config,
provider, old ID and new ID.

The native text table and both language-reader bounds grow together from 1,805
to 8,192 entries. Private names occupy unused IDs 4,096–8,191; all incoming
language/config IDs and original UI IDs are reserved. Exhaustion fails the new
generation without replacing the previously active overlay.

Tests cover colliding packs, native UI preservation, translations, foreign-only
names, replacement skins, priority changes, mod removal, quoted Unicode names,
level-pack precedence, caching and exhaustion. Python suite: 59 passed, one
existing test skipped. Native suite: 18 passed, two existing address-sensitive
plugin probes skipped. Native capacity signatures were checked against the PC
dump. Installed-stack file audit found 43 private name groups across 54 configs.
The user confirmed the installed fix works in-game.

Replace both d3d9.dll and TCSAssetManager.exe with the game closed. The importer
cache changes to 1.7.4 and rebuilds the overlay once. Saves, ownership, models,
portraits and purchase requirements are unchanged.
