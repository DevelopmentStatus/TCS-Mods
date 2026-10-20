# Asset overrides, loader 1.1.0

Supported PC runtime image SHA-256:
`e4666017907099507a2939ef77a7bfaaabdb51586b6cf3491323e5c1574014d6`.
Image base 0x400000, timestamp 0x4ac4a20b, image size 0x268e000.

REA identified the input PE; its decompilation provider was unavailable. PC
instructions were inspected locally with pefile/Capstone. The reconstructed
Android nufile source was used only as a structural reference. PC instructions
and an isolated copy of the PC game established the paths below.

| PC routine | Observation |
| --- | --- |
| 0x4fa3f0 NuFileOpenDF | cdecl(path, mode, DAT header, quiet); DAT lookup at 0x4fa43d, disk fallback at 0x4fa4db; prologue 81 ec 04 01 00 00 |
| 0x4fa610 NuFileExistQuiet | cdecl(path), direct DAT-tree lookup; prologue 57 8b 7c 24 08 |
| 0x4fa690 NuFileSize | cdecl(path), 64-bit EDX:EAX result; prologue 51 a1 90 ee 91 00 |
| 0x4fd5d0 NuFileLoadBuffer | cdecl(path, buffer, capacity); direct DAT bulk lookup; prologue a1 88 ee 91 00 |
| 0x4ff130 disk formatter | Colon in a path selects the supplied absolute filename |
| 0x4fb550 / 0x4f99e0 | Native read / close used by integration probe |

Detoured prologues end on instruction boundaries, without relative instructions.
Build identity and all four signatures are checked before changes. Failure rolls
back installed hooks. Writes/appends use the original route. Replacement opens
use native disk handles with a null DAT header; bulk loads use absolute paths to
miss the packed tree while retaining native buffer/decode rules.

The importer reads uncompressed shared text from the PC -3 DAT index using its
uppercase/backslash path hash. Original DATs are opened read-only. Archive names,
bytes, asset settings and baseline DAT metadata form the cache fingerprint.
Removing an archive invalidates the cache. Replacements stay in Mods/Assets.

Validation on 2026-10-19:

- Seven Python tests passed: merging, priority, disable, removal, source DAT
  invariance, failed-generation publication and cache reuse.
- Ten CTest cases passed, including production native routing/fallback tests.
- Standalone packaged helper imported all three supplied archives: 145 files.
- A fully copied PC game launched with these archives and a developer-only probe.
  It checked all 145 overlay files byte-for-byte via native open/read/size and
  bulk loading, with zero failures. The game reached its render loop.
- After moving the three archive copies out of Mods, a second launch produced an
  empty overlay. Native size/bulk loading returned the vanilla roster without
  DARTHNIHILUS, with zero failures. Previous generated folders still existed.

The shared-link test proposal was rejected by automatic approval review; no links
were created. Tests used independent game copies and left the installation alone.
Logs remain under the ignored build/asset-game-smoke folder. These checks prove
file routing/removal, not every character's visual/animation correctness or
compatibility with unrelated Nexus packages.
