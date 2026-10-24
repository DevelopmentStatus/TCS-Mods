# Drag-and-drop asset mods - loader 1.6.6

For the validated 32-bit Steam PC game on 64-bit Windows 10/11.

## Install and use

1. Close the game. Back up your existing loader d3d9.dll.
2. Extract the loader update beside LEGOStarWarsSaga.exe. Both d3d9.dll and
   TCSAssetManager.exe must be beside the game executable.
3. Drop supported Nexus ZIP or RAR archives directly into Mods. Keep them
   compressed. Existing DLL plugin ZIPs continue to work.
4. Start the game. Under Pause > Mod Settings, each imported asset package has
   Enabled (restart) and Priority (higher wins, restart) controls.

No QuickBMS or separate Python installation is needed. The packaged helper
includes Python and uses the Windows built-in tar.exe to read RAR files.
The first import takes longer; unchanged archives/settings reuse the overlay.

In 1.6.6, overlapping enabled mods with equal priorities are automatically
Auto-sorted and saved before preparing the overlay. Startup continues without
opening the priority editor unless an import error or failed save needs attention.
Distinct saved priorities keep your custom order. The manual editor resolves
tied overlaps too, and stays open so you can adjust and save preferred winners.

In 1.6.5, Manage Mod Priorities.cmd opens before asset extraction. A broken or
unsupported archive appears with an Import error label; select it for the reason,
then disable it and save or replace the archive. Disabled archives are not opened
or staged. Saving/closing the manual editor does not build an overlay. Launch game
builds the enabled selection before starting the game. Failures list the archive,
reason and full report.json path; include that report when requesting support.
Startup waits service the preparation window and defer game messages until
Direct3D initialization resumes.

## Removal and conflicts

Close the game, remove an archive from the top level of Mods, and restart.
The active overlay is rebuilt from the remaining enabled archives. If no other
mod replaces the same asset, the game loads its original DAT asset again.
If another mod also replaces it, that mod's replacement becomes active instead.
Disabling a package has the same effect after restart. Changes are not hot-reloaded.

Higher priority wins binary-file conflicts. Equal priorities are resolved by
filename order, with the later filename winning. Shared character lists are
merged only for characters defined by the package. Language edits are limited
to name IDs used by the package's character definitions. Combined binary icon
resources are replacements, not merged images, and can conflict with other
icon packs. Inspect Mods/Assets/import.log for the selected winners/errors.

The loader never modifies or repacks the installed DAT files and never copies
replacement assets over the original game directories. It redirects native
open, existence, size and bulk-read operations to Mods/Assets when an enabled
replacement exists; other reads continue through the game normally.

Old generated folders do not activate removed mods. With the game closed,
Mods/Assets may be deleted to clear generated caches; the next launch rebuilds
them. Keep Mods/Config to preserve enabled/priority settings.

## General level and world packages (1.2.0)

The importer detects game directories inside optional wrapper folders. Packages
containing LEVELS/ or CUT/ assets use the general level-pack profile, regardless
of archive filename, author, level names or Nexus ID. Character-only packages
retain their ownership-based roster/language merges.

Level packs import scene geometry, terrain, collision, lighting, particles,
scripts, AI, animations, cutscenes, music configuration, audio and pack resources
alongside character assets. Recognized roots are CHARS, STUFF, LEVELS, CUT,
AUDIO, AI and SCRIPTS. Supported extensions are TXT, GHG, GSC, AN3, BSA, DDS,
WAV, OGG, SCP, RTL, GIT, GIZ, PTL, TER, BUR, CU2, AI2, PAK, PAR, ANM, CFG and
FPK. Empty files are preserved: an empty FPK can intentionally disable a bundled
cache. The per-package extracted limit is 8 GiB, with a 512 MiB per-file limit.

Level packs supply coordinated shared tables, so their character roster,
collection/unlock rules, level/area registries, music configuration and language
files are full replacements, byte-for-byte, rather than character-only merges.
Higher priority wins overlapping files; equal priority uses filename order.
Distinct level packs do not automatically merge their registries or binary packs.
A higher-priority character package can merge its owned entries onto the active
level pack's roster/language tables. Put a level pack below character add-ons if
those add-ons should retain their own entries. Archive readmes are never executed.

Both Additional Levels 2.3.1 and Additional Levels (Modern Overhaul) 1.4 use this
same general path. Use one variant at a time: they replace the same levels and
shared tables. The Modern Overhaul variant still requires the separately obtained
Modern Overhaul base mod; this loader does not provide or install that dependency.
For other total conversions, respect the mod author's dependencies and load order.

## Character package examples

| Archive | Import behavior |
| --- | --- |
| DARTHNIHILUS ver 0.0.4 | Strip wrappers; merge Nihilus roster/shop entry and owned German name ID; stage models, icons and animations |
| RTX Obiwan | Map bare OBIWANKENOBI_EP3_PC.GHG to CHARS/OBIWANKENOBI/ |
| TIE Fighter Pilot and AT-AT Driver (2010 Version) | Strip wrapper; stage characters/icons; add the supplied AT-AT Driver shop rule through a fixed adapter |

Archive readmes, links and executable instructions are never executed. Other
packages with recognized game directory layouts can import, but packages that
require scripts, custom binary merges or other manual instructions need adapters.
Do not assume every Nexus archive is automatically compatible.

If importing fails, asset hooks stay disabled for that launch and the game uses
native assets. Existing plugin loading remains available. See
Mods/Assets/import.log and the failed generation's report.json.

## Validation

Python regression tests cover path validation, shared-file isolation, merging,
priority, disabling, removal, unchanged DAT bytes, caching and atomic publication.
The native suite covers routing through open/size/existence/bulk reads, write
pass-through and fallback. An isolated copy of the real game checked all 145
imported files byte-for-byte through native open/read/size/bulk loading with zero
failures. After removing the three archive copies, a second real-game run checked
that the overlay was empty and the vanilla roster loaded, again with zero failures.
These checks do not establish visual/animation correctness for every character.

Developer checks:

```powershell
python -m unittest discover -s tests -p 'test_asset*.py'
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\Build-AssetManager.ps1
.\scripts\Package-Mods.ps1
```

The opt-in TCS_BUILD_RUNTIME_PROBE target includes an asset probe for isolated
game integration checks; it is never included in release packages.


## Version 1.2.0 verification

The new general profile is tested using an unrelated synthetic world layout.
Both user-supplied Additional Levels archives are also imported independently by
the packaged executable, with every recognized file compared byte-for-byte against
the archive (315 standard / 320 Modern Overhaul; only the root readme is ignored).
Removal and cache reuse are checked in isolated fixture directories. The native
regression suite passes (one unrelated ally runtime test skips). These checks
validate import and routing, not completion of the new levels in actual gameplay
or compatibility with every total conversion.


## Legacy character documentation snippets (1.3.0)

Some character downloads ship only models/configs plus a manual installation
readme. Version 1.3.0 reads declarative game-data records in small TXT documents:
`char_start`/`dir`/`file`/`char_end` registrations, single-line `collect` shop
rules, and numeric quoted English name entries. It only accepts registrations
matching an actual supplied character config path, shop rules for supplied
characters, and names matching their name_id values. Missing trailing name quotes
are repaired. Existing CHARS/CHARS.TXT, CHARS/COLLECTION.TXT or English tables take
precedence over document snippets. Generated tables still use ownership-based
merging, so other base characters retain their entries.

Loose ICONS/ and TEXT/ directories inside wrapper folders map to STUFF/ICONS/
and STUFF/TEXT/. This behavior is independent of archive names or character IDs.
No document commands or links are executed. This is a constrained game-data
parser, not an installer-script interpreter. Packs with other syntax, missing
registration/shop data, binary patch requirements or undisclosed dependencies
still need conversion; the loader does not invent purchase prices or unlocks.

The original Mandalorian Season 2 Character Pack ZIP uses this path directly:
eight supplied character registrations, eight 10,000-stud shop rules, English
name IDs 352-359 and eight individual icon resources. Keep the original archive
compressed in Mods; no converted copy is required. If a level pack replaces the
same global tables, give this character pack a higher Priority and restart.
Other packs reusing character IDs or name IDs can still conflict.


Version 1.3.0 validation: ten importer regressions pass, including a synthetic
unrelated character pack with document snippets, unrelated-entry rejection and
real-table precedence. The packaged importer registers all eight characters,
eight 10,000-stud shop entries and eight names from the original Mandalorian ZIP;
781 supplied assets are compared byte-for-byte. Cache reuse, removal and unchanged
baseline DAT bytes are verified in an isolated fixture. Native regressions pass
with two runtime tests skipped when their synthetic memory layout is unavailable.
Actual in-game shop presentation, purchase flow and character visuals remain
unverified for this pack.


## Inferred paths, variants and larger overhauls (1.4.0)

Loose character files no longer need a CHARS prefix. The resolver uses the game's
CHARS.TXT, registries supplied by the archive, exact character filenames, source
folder matches, and neighboring character configs/models to determine the target.
A new character folder containing a name_id config plus its matching model can
identify itself. An anchored character folder can also place neighboring animation
and config files. Unrelated prose documents are not treated as character configs.
Explicit game paths always win. Every inferred mapping and its reason is recorded
in the generation report. Ambiguous loose binary paths cause an actionable import
error rather than being silently dropped or assigned an arbitrary destination.

Sibling option folders replacing the same inferred files become complete archive
variants. Select them under Pause > Mod Settings > the asset package > the named
folder (restart). The first alphabetically sorted option is the default. Selection
is saved per package and becomes active on restart. Repeated explicit destinations
and inconsistent/ambiguous option groups still fail. No archive/character name is
used to select a special adapter.

Clones uses this generic resolver: seven configs and six selected models map into
the native character directories, and seven icons map into STUFF/ICONS. Its three
model choices are 1 normal (default), 2 antenna and 3 visor. These are replacements
for existing clone characters, not a new roster/shop expansion. Unlock requirements
for those existing characters are retained.

Modern Overhaul 2.2.2 now fits the general 8 GiB extracted-package / 512 MiB file
limits. MOVIES/ and BIK, FNT and CSV assets are recognized as well. Fonts/CSV files
are binary/pass-through resources, not numeric language TXT snippets. The native
import timeout is ten minutes to allow larger packages. All supported original
file bytes are preserved in the overlay; only the PDF documentation is ignored.
Actual gameplay, every asset decoder and external movie playback require in-game
verification; file import alone does not establish those behaviors.

When combining these downloads, set explicit priorities: Modern Overhaul 100,
Additional Levels (Modern Overhaul variant) 200, Clones 300, and extra character
packs such as Mandalorian 400. Higher wins. Do not also enable the standard
Additional Levels variant. A base overhaul loaded after its addons can replace
their registries or models; filename order alone does not express dependencies.


Version 1.4.0 verification: thirteen Python tests pass, covering general unrelated
folder layouts, neighbor inference, ambiguity, variant switching, font passthrough,
shared tables and archive path validation. Native tests pass with one runtime test
skipped; native variant setting registration is exercised by the asset test.
The packaged importer checks all 1,961 supported Modern Overhaul files byte-for-byte
and all 20 selected Clones files independently for each of its three variants.
Caching/removal checks pass. A combined fixture with Modern Overhaul (100), its
Additional Levels addon (200), Clones antenna (300) and Mandalorian (400) preserves
the expansion registries, all selected clone assets and all eight shop entries.
These are isolated import/routing tests, not gameplay or purchase verification.


## Startup conflict and priority editor (1.5.0)

The native loader launches the bundled Python/Tk editor automatically when enabled
asset ZIP/RAR packages overlap. Review happens before staging/publishing the new
overlay. Cached imports are reviewed too. No overlap means no popup. Use
Manage Mod Priorities.cmd with the game closed to open it even without conflicts.
The packaged executable includes Python, Tcl and Tk.

The left list is the actual load order. Drag rows, use Load earlier/Load later,
or set a numeric priority (0-1000); higher loads later. Moving rows assigns distinct
priorities across the list. Enabled and archive variants can be changed too.
The right list shows the selected mod's overlapping files and the current last
provider, with the complete provider order available by selecting a file. Shared
character TXT tables are labeled merge-aware; two mods touching such a table can
coexist if their character entries differ. An overlap is not proof of incompatibility.
File replacements and full level-pack tables use the last file provider.

Save & start game writes the existing Mods/Config/asset_*.ini settings, preserving
other setting values, then rebuilds and activates the overlay before startup
continues. A failed multi-file save attempts to restore the original configs.
Use saved settings or close the editor to discard pending edits and continue.
Unchanged saved settings reuse the cached generation. Time spent reviewing is
excluded from the native ten-minute extraction watchdog.

This detects asset-path overlaps, including inferred/generated entries, for the
selected variants and enabled packages. It cannot infer DLL hook conflicts,
missing dependencies or semantic incompatibilities between unrelated files.
The overlay report includes the overlap/provider model for later inspection.

Developer checks: the default Python suite tests overlap winners, disabled mods,
merge-aware labels, load-order changes, INI preservation, rollback and review of
cached overlays. Run tests/gui_load_order_smoke.py for actual Tk widget actions,
and TCSAssetManager.exe --check-gui to verify bundled Tcl/Tk without changing mods.


Version 1.5.0 verification: all 17 Python regressions and 12 native tests pass.
The real Tk widget smoke test checks both drag-and-drop and arrow-button order,
manual priorities, disabling, conflict details, layout bounds and saving. The
packaged executable passes its bundled-Tk check. Automatic invocation is wired
into the native startup helper; the full game-launch/dialog sequence has not
been verified in a live game session for this release.


## Equal-priority prompts, Auto-sort and Launch game (1.6.0)

The automatic editor now opens only for overlapping enabled asset providers with
equal priorities. Disabled mods do not trigger it. Unrelated files sharing a
priority do not trigger it. Distinct priorities across overlapping providers
skip the editor, for both fresh and cached imports. The manual launcher always
opens it. The full overlap list remains visible while editing.

Auto-sort orders broad level/world packages before focused replacements, using
profile and file coverage, then assigns unique priorities. This is a deterministic
suggestion rather than a dependency solver. It does not recognize special mod
names or execute archive instructions. Save or adjust the suggested order.

Save settings persists changes and keeps the editor open. Launch game saves and
closes it. During startup that resumes the existing game without creating a
second process. From the manual editor, the helper finishes the overlay build
before starting the game executable in its installation directory. A child-only
review token is bound to the archive/config fingerprint, preventing the same
manual review from immediately reopening at startup. Any changed input invalidates
that approval. The manual launcher should be used with the game closed.

Verification covers tied/untied/disabled/non-overlapping providers, deterministic
auto-sort, GUI button actions, launch ordering and process arguments, prevention
of duplicate startup launches, and review-token invalidation. Process launch is
mocked in unit tests; a full live-game launch remains unverified.


## Responsive load-order layout (1.6.1)

The loader version is visible in the title bar and heading. A fixed footer grid
row reserves space for Launch game and Save settings, while only the lists absorb
height changes. Toolbars wrap when scaled controls are too wide, long captions
wrap with the window, and the minimum height accounts for scaled controls within
screen bounds. Regression checks use nine mods and a variant selector at three
window sizes and 100%, 150%, and 200% Tk scaling, asserting that version, Auto-sort
and Launch game remain visible and both lists retain a usable viewport.


## Responsive Steam startup (1.6.2)

The importer previously blocked the game thread in a process wait without pumping
window messages. It now waits on both importer completion and the Windows message
queue, dispatches window messages, preserves quit requests, and always retains a
bounded processing timeout even if the review event cannot be created. Human
review time is still excluded. A required priority editor raises itself briefly
and takes focus, then releases topmost status. Human-review signaling begins only
after the actual editor window has been created.

Unchanged archives reuse their SHA-256 from Mods/Assets/archive-hashes.json when
size, modification time and change/creation time match. Config contents are still
read every launch. Missing/corrupt caches are regenerated; changed archive metadata
triggers rehashing. Manually replacing bytes while preserving all metadata can
require clearing this cache. Asset generation reuse still depends on the resulting
fingerprint. Cold imports can take longer than cached launches.

TCSModernOptions.log identifies importer start, completion, error or interruption;
Mods/Assets/import.log contains importer details. Update both native DLL and helper.
The native regression checks message dispatch during waits, timeout without a review
event, pause during actual review, and quit preservation. Python checks unchanged
archives are not reread and modified archives/corrupt caches are handled. This fixes
confirmed source-level startup risks; the user's intermittent Steam failure has
not been reproduced in a live game session.


## Native-plugin filtering (1.6.3)

Live inspection confirmed a delayed launch after asset import had completed:
the plugin loader was expanding Modern Overhaul and other asset ZIPs again into
Mods/Loaded. Its former 15-second wait did not terminate extraction workers.
The same game process later created a window and reached the Present hook.

Plugin scanning now reads the ZIP central directory to check for a root-level
manifest.ini before running extraction. Asset-only ZIPs are skipped without
PowerShell or decompression. Native DLL plugins use standard non-ZIP64 ZIPs with
a root manifest. Plugin extraction uses a responsive, bounded two-minute wait and
terminates a timed-out worker. Tests classify actual Modern Overhaul/Mandalorian
archives as assets and First Person/Skip Splash archives as plugins, in addition
to synthetic root/nested/case/malformed-directory cases. All 13 native tests pass.

This prevents the second asset extraction but does not automatically remove old
Mods/Loaded asset copies or older Mods/Assets generations. Those generated folders
can be cleared with the game and importer closed; keep Mods/Config and mod archives.
Do not infer every byte of an installed game folder is disposable cache.


## Visible preparation and cache reuse (1.6.4)

A native preparation window appears before the Python helper starts, stays responsive
while it runs, hides for load-order review, and closes when preparation finishes.
Large changed packages still require an import; the window does not report a percent.

The overlay fingerprint now uses effective enabled state, bounded priority and
non-default variant selections per installed archive. INI formatting, newly written
defaults and stale configs for removed archives no longer invalidate the overlay.
Archive hashes retain the metadata cache described above. Version changes trigger
one rebuild. Twenty-five Python and thirteen native regression tests pass.
