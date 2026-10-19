# First-person projectile aiming, v0.3.6

Target: TCS.runtime-analysis.exe, SHA256 e4666017907099507a2939ef77a7bfaaabdb51586b6cf3491323e5c1574014d6 (existing read-only runtime analysis image).

PC instruction observations with pefile/Capstone:
- 0x4b1d27 calls 0x583af0 with owner/type/fire flags, caller stack cleanup 12 bytes.
- 0x583af0 matches Bolt_Shoot structure in Android reference source, verified against PC offsets rather than assuming Android layout.
- PC target flag: owner+0xe20 bit 0x800; script target pointer owner+0xea0; target mode bits owner+0xeec mask 0x38000. The end-of-shot multi-bolt lifetime branch also tests 0x800.
- 0x5841e0 calls 0x582e70 with owner, origin, rotation, type, flags; caller cleanup 20 bytes.
- 0x582e70 serializes origin (12 bytes) and rotation (64 bytes) then invokes 0x582470.
- 0x58299b-0x5829fb constructs positive-Z velocity, rotates it through the supplied matrix, normalizes into bolt+0xac. Camera forward is the same matrix positive-Z row.

Changes install only through the proxy on restart. Current game process was neither injected nor patched by tools. No gameplay execution of the new build has been observed yet.
REA open_binary succeeded for the identified target. MCP deep analysis unavailable because provider was not configured; local instruction analysis used instead. Source reference: research/saga/src/legoapi/items/collect/bolts.cpp, Bolt_Shoot/Bolt_Add.
