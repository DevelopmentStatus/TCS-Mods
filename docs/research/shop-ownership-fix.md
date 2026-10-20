# Shop Ownership Fix 1.0.0

Read-only inspection of the user's running supported PC game found five owned
characters with transient shop purchased=0: Super Battle Droid, Kit Fisto,
Tusken Raider, Boba Fett and The Emperor. Collection flags were 0x03, while the
corresponding shop entries still offered a price. A ship purchase succeeded.

PC instruction inspection established the two separate ownership paths:

- SelectShopItem at 0x4c2730 reads shopitem+0x63, then calls AddToCollection
  (0x594a90) after unlock and stud checks. AddToCollection rejects an already
  owned character; that path does not charge studs or update shop presentation.
- UpdateCharacterIDs at 0x4c15e0 refreshes item IDs/prices, not purchased flags.
  Native shop initialization (call at 0x4c19c1) and shop update (call at 0x4c74e1)
  both use this function. It is a safe point to access the active shop allocation.
- PC item size 0x74, character ID ushort +0x60, type byte +0x62, purchased byte
  +0x63, price uint +0x64. Native shop capacity is 100 entries.
- The collection ownership/available bit is bit 0, read from the character-save
  array addressed by 0x93b288. Bit 1 alone indicates unlocked, not owned, and is
  insufficient to mark a shop entry purchased.

The plugin hooks the native updater with a 10-byte, position-independent prologue:
`57 33 ff 66 39 3d fe 5e 85 00`. It checks supported image identity and refuses a
conflicting prologue. It runs the original updater before correcting only type-1
entries whose valid character ID already has ownership bit 0. No purchase
function is called, and no collection/save/purchase-bit writes are made.

The option defaults on. When disabled, only temporary flags raised by this plugin
are restored on the next shop update; genuine native purchases take precedence.
Pointers are only accessed inside the native updater, after bounds/memory checks.
Shutdown does not access cached shop pointers. Removing the plugin requires restart.

Validation: the synchronization test covers owned versus merely unlocked versus
locked IDs, other shop item types, invalid IDs, repeated updates, disabling,
legitimate purchase preservation and untouched nonpresentation bytes. The native
runtime test loads the built production DLL in a separate synthetic host, executes
the installed detour at the real PC address, tests API/build-conflict rejection,
setting changes and hook restoration, and verifies stud/save bytes stay identical.
All 12 native CTest cases passed. No live-game injection or save edits were made.
On 2026-10-20, the user confirmed the installed mod works in their shop.

Initial asset mods preserved the original shop collection order and definitions
and appended two entries. Evidence does not establish that these mods caused the
preexisting collection/shop mismatch. Missing icons from texture replacements
remain a separate asset compatibility question.
