# TCS Mod Loader 1.8.0 and Character & Shop Extender 1.0.0

Character/shop expansion is now a separate optional plugin. The loader no longer
links the capacity patch tables, extended character/text/portrait registries or
companion-save hooks. The optional extender retains those features and the
existing companion-save format. No save migration is required; keep native saves
and their `.tcs-characters` files together.

## Downloads and setup

- [Loader 1.8.0](../../dist/TCSModLoader-v1.8.0.zip): extract beside the game EXE,
  including the complete `TCSAssetRuntime` directory.
- [Character & Shop Extender 1.0.0](../../dist/TCSCharacterShopExtender-v1.0.0.zip):
  keep compressed in `Mods` when using character packs requiring extended tables.
- Run `Prepare Mods.cmd` before starting normally, or use
  `Manage Mod Priorities.cmd > Launch game`. Repeat after changing archives.

The manager prepares plugin caches outside the game process. The loader verifies
archive receipts and loads the prepared DLLs without launching the helper.
The helper uses a directory runtime rather than unpacking an embedded runtime
into a temporary directory. Saga Tweaks / ASI integration is removed.

The extender initializes before ordinary plugins. The manager detects missing
extender dependencies, and the loader rejects dependent overlays if extender
initialization fails. Cosmetic assets without that dependency remain supported.
The extender is restart-only: native tables and callbacks reference its module
until the process exits.

## Antivirus evidence recorded before publication

Results observed on October 31, 2026. At the user's request, further VirusTotal
checks were stopped. Only the following completed reports were verified:

| Exact artifact | SHA-256 / report | Completed result |
| --- | --- | --- |
| Loader `d3d9.dll` | [5e73a7fe09f4fb6138ff4a148fe46a32486dc5a9a583e2658821a45dfaa45d5d](https://www.virustotal.com/gui/file/5e73a7fe09f4fb6138ff4a148fe46a32486dc5a9a583e2658821a45dfaa45d5d) | **0/70** |
| Extender ZIP | [d590fe080063c6a825774b2a45bfc53ca4301836681af453d2c838a46e6a2e44](https://www.virustotal.com/gui/file/d590fe080063c6a825774b2a45bfc53ca4301836681af453d2c838a46e6a2e44) | **0/67** |

The exact final loader ZIP has SHA-256
`6257c71f7dedb0ecba2e433d3589aabf6a8b03f8b790a19ad3f7460c34f0f0af`.
It was submitted, but its completed report was not verified before checks stopped.
Its antivirus status is **unverified**, as are the separately submitted helper
EXE and extender DLL. The helper SHA-256 is
`af4ad4b02c434c918b43bf7db7e7126fa927c2c588106cd3aeab65046f485399`;
the extender DLL SHA-256 is
`c764ee38aff73ad801c24667d0a241507430ebd48fbb3184e77e9bf9df4c491f`.
VirusTotal warned that it may not inspect every member of the loader ZIP. The DLL
result does not establish a result for the entire ZIP or its bundled runtime.

Earlier controlled diagnostic scans associated the `.672` detection with the
combined capacity/save/registry code group: removing that group cleared `.672`,
while removing portrait hooks alone did not. Narrower exclusions did not identify
a single function or signature. That exact signature attribution remains
unresolved. This release implements the requested architectural separation; it
does not claim to prove a vendor signature is a false positive. No API renaming,
byte padding, packing or hash-only modification was used.

## Validation

- 22 native CTest tests passed, including loader/plugin cache integration,
  unsupported-host rejection, save-format/native-hook fixtures, asset overlay
  dependency rejection, settings/menu behavior and existing plugin contracts.
- 66 Python tests ran: 65 passed and one Windows junction test was skipped.
- The actual packaged helper passed its Tk runtime check and prepared an empty
  game directory successfully.
- ZIP integrity and packaged DLL byte equality were verified. The core ZIP has
  no extender DLL. The core DLL has no companion-save magic/path marker; those
  remain in the separate extender DLL.
- The user reported it working. No new full gameplay session was exercised by
  the automated checks; real-game positive initialization, a populated shop and
  an existing extended save still require gameplay validation.

Source and build scripts accompany the packages. Diagnostic variants, audit
binaries and research executables are excluded. See
[SHA256SUMS](../../dist/SHA256SUMS.txt) for package checksums.
