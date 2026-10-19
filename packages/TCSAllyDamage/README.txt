TCS ALLY DAMAGE 1.1.1

COMPANION SCOPE FIX
Only AI-controlled characters in the game's registered party list qualify.
Friendly bystanders, ambient NPCs and enemies are excluded, including NPCs with
a stale party-slot value. This restriction covers both damage and combat AI.

SETTINGS
Pause > Mod Settings > TCS Ally Damage

AI allies deal damage: existing party-companion attacks use player-style damage eligibility.
Active combat AI: lightsaber/blaster party companions with a native combat opponent choose
paced attacks, aim toward their opponent, draw their weapon when needed, and use
short defensive windows before counterattacking. Both settings default to On
and can be changed independently. Turning Active combat AI Off restores the
original AI decisions while keeping the damage setting available.

The combat logic uses the game's existing opponent selection and navigation.
It does not make idle allies search the map for enemies. Native jumping,
interactions, Force actions, vehicles, player control, damage amounts, shields
and boss defenses remain under the game. Allies outside attack range continue
their original navigation. Melee attacks require close range; blasters can fire
at a native opponent within eight game units when aimed toward it.

INSTALL OR UPDATE
Close the game. Remove older TCS Ally Damage ZIPs from the root Mods folder
(or move them into Mods/Backups). Put TCSAllyDamage-v1.1.1.zip directly in Mods;
keep it zipped. Requires TCS Mod Loader and the supported 32-bit Steam PC
version of LEGO Star Wars: The Complete Saga. Compatible with TCS First Person.
Existing settings stay in Mods/Config/tcs.allydamage.ini.

REMOVE
Close the game and remove TCSAllyDamage-v1.1.1.zip from Mods.

VALIDATION
Native x86 flag replay, actual production-DLL hooks in a synthetic game host,
combat decisions, attack/defense/counterattack pacing, human/enemy isolation,
live settings, x87 preservation and shutdown restoration are tested.
The synthetic host may skip on a fixed-address allocation collision.
Actual in-game combat still needs playtesting.
