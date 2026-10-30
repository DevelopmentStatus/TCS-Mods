# TCS Mod Loader 1.7.0

The character shop now supports 1,000 purchasable entries. Previously the native
shop initialized only the first 100 entries, even when the merged collection
contained more. The installed stack had 131 purchasable definitions; the four
reported Clone Trooper, Anakin and Obi-Wan expansions occupied entries 118–130.
Their collection rules remain intact: completing the required story areas and
paying their stud prices is still required before Free Play ownership.

Character shop items use a loader-owned buffer rather than consuming another
104,400 bytes of the hub's fixed arena. The initialization and collect-all bounds,
purchase-bit vector and all character/extra cheat storage move together. There
are 1,044 cheat slots (1,000 character entries plus the original 44 extras).
The existing shelf scrolls through the larger list; it does not draw a
1,000-portrait Free Play grid simultaneously.

The native save has only 340 character ownership bytes. Runtime ownership now
uses separate bounded storage for up to 4,096 registered character IDs, so added
IDs cannot overwrite adjacent game state. Native saves retain their original
size, checksum and first 340 ownership bytes. Additional progress is stored in
a `.tcs-characters` companion beside each native save. Keep both files when
backing up or transferring progress.

Companions contain character file identities rather than roster indices. They
are matched to the native snapshot using SHA-256, loaded only after the game
accepts its native checksum, and committed atomically after the saved native
payload has been verified on disk. Missing/malformed/mismatched companions
retain native ownership and leave additional characters unowned. New Game and
loading another slot reset additional progress. Entries for temporarily removed
mods are retained. Old saves cannot recover added progress that the previous
loader never successfully stored.

All patches verify the supported Steam PC executable and exact original
instructions before modification. Installation rolls back on patch failure.
The optional Shop Ownership Fix remains compatible; its native updater hook is
preserved. Previous roster inference, portrait, Saga Tweaks, Mod Settings and
window-mode fixes remain included.

Validation: 50 Python tests and all 19 native CTest checks passed. New tests cover
1,000 records; the 100/128/340 storage boundaries; the 1,000th shop entry; clamping
an oversized collection; adjacent-native-state preservation; successful, failed
and short-write saves; native-snapshot mismatch; slot isolation; New Game;
character reordering; malformed companions; and patch-conflict rejection. Native
hook logic executes in an isolated, relocated copy of the PC memory layout;
the real instruction signatures are separately checked against the PC dump.
This does not constitute an in-game purchase or a 1,000-character visual test.

Replace both `d3d9.dll` and `TCSAssetManager.exe` while the game is closed.
