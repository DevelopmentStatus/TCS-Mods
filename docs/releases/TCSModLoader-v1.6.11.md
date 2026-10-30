# TCS Mod Loader 1.6.11

Supports character packs that supply character files and collection entries but
omit some or all of CHARS/CHARS.TXT. The importer now infers a missing roster
record from a named character config, the matching _PC.GHG in the same game
directory, and a supplied collection entry for that character. This also works
for small inherited configs using txt_file, such as Obi Wan Kenobi Expansion 1.

Explicit roster entries and existing baseline registrations are retained.
Inferred entries are inserted once during the normal priority-ordered merge;
this does not concatenate arbitrary text files or globally remove duplicate
lines. Collection unlock requirements, prices, cheat codes and ownership remain
unchanged. Configs without a supplied collection entry are not assumed to be
playable. Ambiguous directories, missing models or unreadable name IDs are not
guessed. Coordinated level/world-pack tables are excluded from inference.

The import report records auto_registrations (evidence/candidates) and
registration_diagnostics (registered, existing, or unresolved). All registrations
are in generated overlay files; original archives and DAT files remain untouched.

Close the game and replace both d3d9.dll and TCSAssetManager.exe. Keep the existing
Mods/Config folder and compressed mod archives. The cache version changes for
this importer update, so the overlay rebuilds once on startup. Previously
generated folders do not need to be removed manually.

Validation: all 50 Python tests passed. Seven new scenarios cover inherited
configs, missing evidence, ambiguous directories, explicit/partial rosters,
duplicate providers, existing baseline IDs, cache reuse and removal. The actual
Obi-Wan archive was processed against a copy of the installed merged tables:
exactly OBIWANKENOBI_ARMORED, OBIWANKENOBI_CLW and OBIWANKENOBI_GEN were added,
all 369 existing roster entries were preserved, and the Dooku-area unlock rules
remained unchanged. In-game selection of these three characters is not verified.
The startup/display fixes from 1.6.10 remain included.

Installed-stack verification: the game was closed, loader files were backed up,
and the generated overlay was rebuilt with the packaged importer. The installed
roster now has 372 entries (369 retained plus the three Obi-Wan variants), all
three matching portrait names are in the preload catalog, and an audit found no
remaining supplied/configured collection characters missing from the roster.
