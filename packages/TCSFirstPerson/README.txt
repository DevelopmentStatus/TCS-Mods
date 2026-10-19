TCS First Person v0.4.0 - TCS ModLoader API 1

INSTALL
Close the game. Place TCSFirstPerson-v0.4.0.zip directly in the game's Mods folder. Leave it zipped. Replace the previous First Person plugin ZIP if present. Keep Mods/Config to preserve saved settings. Requires the TCS ModLoader for the supported 32-bit Steam game build.

SETTINGS
Pause > Mod Settings > TCS First Person
Aim-bot (native targeting): Off fires bolts from the muzzle along the camera view. On restores native game targeting.
First person style: Normal or Animated (smoothed skeleton attachment, no camera roll).
Horizontal field of view: 60 to 120 degrees, all camera modes.
Head hiding, crosshair, sensitivity, orbit distance, PC controls and bindings are also adjustable.

CONTROLS
C cycles First Person > Third Person > Orbit.
Third Person uses the normal game camera. Orbit follows mouse movement.
WASD move; Shift walk; Mouse1 attack; Mouse2 block or ranged zoom.
Space jump; E/F ability/interact; Q switch; Tab/R previous/next where allowed.
F5 toggles custom camera; F6 releases/resumes mouse look; F7 recenters.

Configuration: Mods/Config/tcs.firstperson.ini
The FirstPersonStyle setting applies to the single First Person slot. Animated retains smoothed positional skeleton following. Aim-bot does not add a new target-search system; it restores the original game targeting. Third Person and Orbit always retain native targeting. Headless characters use orbit fallback.

Validation: Win32 Release build and all five automated checks passed. Camera switching, setting persistence and aiming still require live gameplay verification after restart.
