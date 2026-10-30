# TCS Mod Loader 1.7.1

Fixes the remaining reason later character packs were absent: the global native
character registry stopped at 340 entries. Increasing shop capacity alone in
1.7.0 did not register characters beyond that point.

The loader now replaces the startup registry parser with a bounded 1,000-entry
implementation using the game's existing lexer. It avoids the original parser's
separate 500-entry offset arrays and 10 KB string pool. Character and runtime
records retain native layouts/defaults and use checked permanent-arena space.
Five model/spawn checks accept the expanded character range. Hub background
candidate storage and the Free Play ID list expand with the limit; security-cookie
and stack-argument offsets move together. An unused vehicle ID-array output is
disabled while retaining the native first-ID result.

The 1,000-entry shop, extended ownership and per-save .tcs-characters companions
from 1.7.0 remain included. Keep companions alongside native saves when backing
up or transferring them. Normal story requirements and prices remain intact;
registration does not automatically grant Free Play ownership.

Live read-only verification found CHARCOUNT=340 before this fix and CHARCOUNT=373
afterward. All 13 reported Clone Trooper, Anakin and Obi-Wan variants now have
native IDs 360–372 and loaded character configs. All 13 resolve into the native
132-entry shop collection, retaining 15,000–50,000-stud prices and Kamino/Dooku/
Vader area requirements. This is live table verification; visual selection and
purchasing were not automated because computer-use access to the game was denied.

Validation: the 1,000-entry parser test covers long string pools, native defaults,
runtime pointers and arena bounds. Expanded hook/save tests pass. Final CTest:
18 passed, two preexisting fixed-address runtime probes skipped, no failures.
The unchanged Python importer suite previously passed all 50 tests. Original
instruction signatures were checked against the supported PC analysis dump.

The installed loader and saves were backed up before replacement. This native
update retains importer/cache version 1.7.0; no asset re-extraction is needed.
