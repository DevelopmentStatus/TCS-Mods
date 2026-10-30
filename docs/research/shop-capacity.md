# Shop capacity and extended character ownership

## Portrait object registry (1.7.3)

Live inspection of the installed373-character roster found the global object
count at its750-entry capacity. Revan used icon ID749; later character configs
kept icon ID-1, so their supplied individual scenes were never loaded. Their
filenames/internal names and the183-icon preload metadata were correct.

Registry initialization call0x4ca16f invokes0x579e70. The wrapper copies the
original sentinel-terminated8-byte table into4,096 loader-owned records and
provides a256KiB loader-owned name pool. Globals951bb8/951bbc/951bc0 hold table,
count and capacity;951bc8/951bcc/951bd0 hold name pool, size and used length.
Original count/name bounds checks in0x579f60 remain active. No extra world or
permanent arena capacity is consumed for these buffers.

Direct background lookups0x419f21/0x419f41 and entry0 access0x4c9e2f/0x4ca77c
follow the new table. Initialization still receives the original baseline table
at0x4ca160. A full text-operand scan of7f44f8..7f5c68 found no other references.
The optional early demo/save-icon name override is preserved when copying.

Runtime world tables atworld+0x2ac0 allocate actualCount*16 bytes. Kind3 scene
resolution at0x57a0d7 scans individual cached icon scenes before shared-scene
fallback. It fills the special handles/active byte used by both shop and roster
drawing. No new renderer fallback or scene-file modification is needed.

Native regression execution includes the actual registry initializer and
add-extra code, object750+ normal/locked names, full4,096 capacity and name-pool
bounds. The user confirmed the installed icon fix works in-game.

Static source: `research/TCS.runtime-analysis.exe`, SHA-256
`e4666017907099507a2939ef77a7bfaaabdb51586b6cf3491323e5c1574014d6`.
Supported original image base 0x400000, timestamp 0x4ac4a20b, image size 0x268e000.
Android reconstruction corroborates structure but is not used as PC address evidence.

`src/shop_capacity.h` contains the complete exact-byte patch manifest. Its
instructions were checked against that dump. Allocation call 0x4c164d now returns
loader storage; loop 0x4c1880 and collect-all 0x4c6e07 expand together. Character
items are 0x74 bytes. ShopCollection 0x855ef8 has list+0 and ushort count+6;
collection records are 0x1c bytes. SHOPCHARCOUNT is 0x8800d8; 0x87c56c is the
character cheat count, not the UI character count. Cheat storage/caps at
0x8801d0/144 move to 1,044 entries. Purchase-bit uses at 0x4c18f4, 0x4c26ea and
0x4c6e22 move to the extended vector.

The two calls to UpdateCharacterIDs (0x4c19c1 and 0x4c74e1) are wrapped without
replacing entry 0x4c15e0, preserving the separate ownership plugin. Native calls
are bounded to 1,000 items. The master collection count is restored afterward.

Game save begins 0x8668c8 and is 0x7e4c bytes. Character flags at 0x86e5c0 have
exactly 340 bytes before the next live global at 0x86e714. Pointer initializer
0x428c43 and four direct NewGame accesses at 0x42af0b/20/3b/64 move to external
ownership. Other observed ownership accesses use pointer 0x93b288. CData stride
0x4c contains directory+8 and file+0xc. Stable companion identities use file.

Accepted load copy 0x50bcd0 and save snapshot copy 0x50bdfb call void cdecl
copy 0x4ef660 with (dest,source,size). Save synchronization occurs before the
checksum, load synchronization after acceptance. PC save writer 0x5439d0 accepts
(slot,buffer,size,hash); main payload size 0x7e50 includes the checksum. Its first
five bytes contain only `mov eax,0x2470`, so the detour bridge does not relocate
the following stack-probe call. NewGame 0x42abf0 similarly has a verified
position-independent 10-byte prologue. Full save filename comes from 0x5436e0;
active memcard slot is 0x921dc0. The native writer closes the file but reports
success without checking fwrite, hence the explicit on-disk payload verification.

Companion format: eight bytes `TCSCHR12`, 32-byte native-body SHA-256, uint32
record count, then (uint16 key length, uint8 flags, ASCII file identity) records.
Native little endian; bounded sizes, unique keys, no trailing bytes. Writes use
a sibling temporary file and MoveFileEx replace/write-through. Native save
files are never opened for writing by the companion code.

## Global registration and model creation (1.7.1)

Live PID27592 had CHARCOUNT340, initialized shop102. Its generated roster
contained later packs that never entered CData. Startup call0x4ca303 originally
invoked ConfigureCharacterList0x6307c0 with maximum340. Native parser further
caps at500 and has two500-short offset arrays and10KB text pool. The replacement
uses native lexer Create0x4fc860, GetLine0x4fc5b0, GetWord0x4f7fb0,
Destroy0x4fa300 (word pointerNUFPAR+0x510), preserving comments/quoted words.
Layouts match native76-byte CData defaults and caller-supplied runtime stride.
Checked arena allocation supports1000 records with dynamically sized names.

Spawn bounds at0x41c7db,0x41f8f5,0x41faa3,0x41ff18,0x43d8da move from340/339
to1000/999. Scene/icon/player-model lookup allocations already follow CHARCOUNT.
Hub candidate frame0x419fb0/0x41a0d3 grows from1360 to4000 bytes (no arguments
or security cookie). Free Play frame0x489150 grows0x2b4 to0x7e0; cookie and three
incoming arguments move by0x52c, with one common epilogue at0x489768. Room for
1000 short IDs plus terminator is required. Hub door selection0x47e9b3 passes
NULL for an unused IDs array;0x47e9d9 reads its existing first-ID output instead.

Live PID9268 after installation had373 registered characters. All13 reported
variants had IDs360–372, valid inherited configuration flags/name IDs, and
resolved purchase definitions in the132-entry ShopCollection. No gameplay/save
writes were made by the read-only verification script. Visual purchase/selection
was not automated because computer-use access to the game window was denied.

## Save-load switching crash (1.7.2)

Crash39240: exception0xc0000409, return address0x491a8c. Recorded CHARCOUNT at
0x93b278=373; cookie at ESP0x1af970+0x2cc=0x1afc3c contained0xffffffff, while
native security cookie0x829570 was0x6fcf8a2f. This establishes the overwrite
by missing model-ID shorts rather than a native save checksum failure.

Helper0x491620 writes CHARCOUNT shorts through ECX. Its only callers are
0x491817 and0x49195f. The enclosing frame at0x491800 expands0x2ac->0x7d4,
moving its cookie0x2a8->0x7d0. The crashing function0x491840 expands0x2d0->0x7f8;
cookie0x2cc->0x7f4, argument0x2d8->0x800, second cookie read0x2d4->0x7fc.
Both epilogues adjust the new frame. Candidate picker0x491760 expands0x550->0xfa0
on entry and both exits. All remain below the4KiB stack-probe threshold.

The native-code regression extracts these bodies plus native RNG0x5b9e30 and
security check0x6e693a from the owned PC dump, rebasing their absolute globals
into the isolated fixture. It never patches or injects into a live process.
External animation/camera calls are inert fixture stubs; actual CHARCOUNT
copies, stack frames, cookie checks and candidate selection execute unchanged
except for the production capacity patches. Counts340/373/1000 exercise both
crashing-function return paths and pick the last candidate with native RNG.
