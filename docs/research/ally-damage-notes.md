# Ally damage investigation and implementation

Target: LEGO Star Wars: The Complete Saga, supported x86 Steam build.
Runtime analysis image SHA256: e4666017907099507a2939ef77a7bfaaabdb51586b6cf3491323e5c1574014d6.
PE timestamp 0x4ac4a20b; image size 0x268e000; preferred/required image base 0x400000.

REA open_binary identified the same PE and hash. REA procedure_pseudo_code reported
provider_unavailable (Ghidra not configured; Hopper unsupported; IDA unconfigured).
No REA decompilation or runtime observation is claimed. Used local pefile/Capstone
instruction inspection and the reconstructed Android source only as a structural
reference. PC offsets and calling conventions were checked in the actual x86 image.

Observed instruction evidence:
- 0x58b720 ObjHitObj_Flags: flags at owner+0x1fc, bit 0x80 means player-controlled.
  Nonplayer base 2 vs player base 0x804; state at +0x1f8 adds 0x10, 0x20, or 8.
- 0x438230 ObjHitObj: six cdecl arguments (attacker, target, damage, flags,
  probe, context), also called at 0x58e6c3 (melee) and 0x5815ed (bolt).
- 0x4387d6 onward: flags bit 2 enters nonplayer-target damage suppression.
  0x43885b onward: flags bit 4 preserves native friendly-NPC filtering based
  on target state 0x400 and absence of 0x10005. Other damage rules remain native.
- 0x582762 generates bolt hit flags, stored at bolt+0xf8 with projectile bit 0x80.
- 0x5815a3 loads owner from bolt+4. 0x5815ad..0x5815d2 rejects a live,
  uncontrolled owner whose state bit 1 equals the target's bit 1. The extra
  mid-function hook bypasses this rejection only for enabled AI allies.
- owner+0x280 is signed party slot (-1 for nonparty), verified in PC flag/bolt
  routines and correlated with creature.cpp party creation (up to eight slots).

Implementation:
- Ally: live character (flags 0x1001), not human-controlled (0x80 clear), and
  either party slot 0..7 or native friendly state ((state & 0x10405)==0x400).
- Convert outgoing flags: clear 2; set 0x804. Preserve weapon/context flags.
- Normalize explicit and cached flags at ObjHitObj as well as the flag factory.
  Disabling restores native allegiance bits even on previously created bolts.
- No persistent player-control flag writes, HP edits, enemy AI changes or fake
  damage events. Native weapons, target defenses and attack scheduling remain.
- Detours copy complete position-independent instructions (6,8,6 bytes).
  Validate PE identity and all prologues before installation. Restore on shutdown.

Verification:
- Native routine replay executes the original PC machine code with synthetic
  objects, including faction variants and party boundaries.
- Production DLL synthetic-host test checks the actual exported initialization,
  three installed hooks, native register/stack ABI at the bolt mid-hook,
  damage/probe/context forwarding, HP reduction, enemy isolation, live toggle,
  human takeover and byte-for-byte restoration. Its native damage consumer is
  simulated, not the complete game routine. Only the host-module query is mocked.
- Windows can reserve one of the fixed addresses before tests start; that case
  returns 77 (CTest skip). Successful standalone run observed after two attempts.
- No actual in-game combat session has been observed. AI aggression, mission
  behavior, bosses, friendly-fire interactions and level transitions need playtesting.

## Active combat AI, v1.1.0 (REA-backed analysis)

REA 6.4.0 / Ghidra 12.1.3 completed analyze_function for 0x43e530.
Evidence: ev_592cd39934b15c8a99eb6139268ebfdcac9f9987a1280d2c579a36ba7f9dd10a.
Saved full record: ally-combat-rea-function.json; reusable REA snapshot:
ally-combat-rea-snapshot.json. Focused readable views are saved as
ally-combat-rea-pseudocode.txt and ally-combat-rea-assembly.txt.
The binary hash matches the original target above. Ghidra reports the complete
function body 0x43e530..0x43fd94 (6245 bytes), 1511 assembly instructions and
36396 characters of pseudocode. Native value-flow is truncated; conclusions
here use returned assembly and pseudocode, not an assumed complete value graph.

Connection recovery: the MCP server lacked GHIDRA_INSTALL_DIR. Used the existing
public REA CLI launcher with process-local GHIDRA_INSTALL_DIR pointing to the
already-installed C:/Tools/ghidra_12.1.3_PUBLIC, and workspace-local TEMP/TMP.
A sandboxed attempt completed initial analysis but localhost connection was
blocked (Winsock 10013). Cancelled that attempt and ran the same read-only REA
command through approved execution outside the sandbox. The successful CLI
session closed its ephemeral project automatically. No persistent REA/client
configuration edits or engine installations were made.

Observed in this REA evidence:
- Per-character GameAIProcess loop uses ESI for the current object.
- +0x3ac is the native opponent API pointer and +0x3b0 its metric. The native
  firing path writes **(object+0x3ac) to object+0xea0 at 0x43f619..0x43f621.
- +0xca4 is the pad packet, +8 receives pressed input. Attack global is
  0x80227c and special is 0x802280.
- 0x43f65b..0x43f725 maintains defense based on +0xeec bit 2 and resets the
  defense timer +0xecc while a native opponent is present.
- 0x43f788..0x43f7f5 selects draw/sheath-related action/special edges using
  weapon-ready bit +0xe22 bit 1.
- At 0x43f7f8 and 0x43f7fb the two three-byte instructions are exactly
  MOV EDX,[ESI+0x54]; MOV EAX,[EDX+0x24]. These position-independent instructions
  follow native combat decision merging and form the six-byte hook site.
- The function continues normal movement-specific and interaction processing;
  the mod does not replace GameAIProcess or its route/opponent selection.

Implementation/design choices (not claimed as original game behavior):
- Independent bool setting ActiveCombatAI, default On, label Active combat AI.
- Applies to live, grounded, uncontrolled allies with a live native nonparty,
  nonhuman opponent, valid private pad and supported saber/blaster model flags.
- Ready contexts only: idle, native held guard, active combo or shooting. Other
  contexts, vehicles, context targets, shared human pads and paused dt are untouched.
- Melee reach 0.85 game units, blaster range 8; melee also checks relative height.
- Aim toward the current native opponent with bounded angular speed. Use normal
  attack presses to draw weapons and attack, paced at 0.38s melee / 0.48s ranged.
- Facing close hostile attacks can trigger 0.20s held guard, at most once per
  1.10s, allowing counterattacks even against a sustained threat. Only native
  held guard is released directly; active block/hit/interaction animations remain.
- Preserve all general registers, flags and x87/SSE state around the callback.
  FXSAVE/FNINIT/FXRSTOR avoids corrupting a live native x87 stack.

Validation: all nine CTest checks passed. The production DLL synthetic host now
executes all four hooks, including the real naked combat callback. It verifies
held-guard release into offense, cooldown, bounded defense/counterattack, ranged
attacks, enemy/human/party exclusion, dead targets, shared human input exclusion,
paused frames, independent setting callbacks, x87 stack preservation and exact
hook restoration. Real in-game playtesting remains outstanding. Opponent
acquisition, pathfinding and boss encounter behavior are not simulated in this test.

## Companion-only correction, v1.1.1

The v1.0/v1.1 classification was too broad: the fallback native friendly-state
mask (state & 0x10405)==0x400 also admitted friendly ambient NPCs. Removed that
fallback entirely. A live uncontrolled character now needs both a valid party
slot 0..7 and exact pointer identity with the object registered in that slot at
0x93d7f0 + 4*slot. All four hooks use this shared eligibility function.

The authoritative party table is established by retained REA Evidence
 ev_592cd39934b15c8a99eb6139268ebfdcac9f9987a1280d2c579a36ba7f9dd10a:
0x43e874 reads [ESI*4+0x93d7f0] in the eight-slot party loop;
0x43fca4..0x43fd12 enumerates all eight pointers through 0x93d80c.
No new binary assumptions or changes to the hook sites are introduced.

Regression validation runs the actual production DLL: a friendly-state NPC
outside the party table is excluded by damage-flag, damage-consumer, bolt-filter
and combat-decision hooks, even with a stale slot value of 1. A real companion
with the same faction flags remains eligible. Existing checks pass; the fixed-
address synthetic-host test skipped in CTest due to address collision and then
passed standalone on attempt four. Actual in-game behavior needs confirmation.
