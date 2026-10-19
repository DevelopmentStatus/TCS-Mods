# Pause-menu Mod Settings implementation (2026-10-19)

Target: validated 32-bit Steam TCS, timestamp `0x4ac4a20b`, image base
`0x400000`, image size `0x268e000`. Analysis image SHA-256:
`e4666017907099507a2939ef77a7bfaaabdb51586b6cf3491323e5c1574014d6`.

## Evidence and boundaries

- REA / Ghidra 12.1.3 string search found `Effects Settings` at `0x776c04`.
  Evidence: `ev_8f6bdf8f786b5c299c7fcda3e3f1733dbade4cef51d61ef017858386c3288ad8`
  in `pause-search.json`; analysis binding in `pause-analysis.json`.
- REA did not define a procedure at `0x495900`; its decompilation request
  failed with `No procedure exists`. The following code findings instead
  come from Capstone disassembly of the existing runtime dump, inspected
  with `pause_inspect.py`, and are not attributed to Ghidra pseudocode.
- The localized Effects pointer is `0x297adf4`. Pause drawing at `0x495b50`
  calls native `GameDrawMenuEntry` (`0x574de0`) for Effects at `0x495bfb`.
  An extra call immediately afterward inserts the new row and increments
  the game's draw count and vertical position normally.
- Pause update is `0x495900`. The template draw/update slots are
  `0x7fad30` / `0x7fad34`; the initialized table slots are
  `0x800e4c` / `0x800e50`. Both copies are wrapped by the loader.
- PC menu structures have stride `0xe0`, unlike the larger mobile structure
  in the nearby SAGA source tree. Selection row is short `+0x0a`, last row
  short `+0x12`, selected item int `+0x34`, confirm int `+0x64`, cancel int
  `+0x68`, draw count int `+0x8c`, and text scale float `+0xd0`.
- `check_pause_runtime.py` performs read-only process inspection. The saved
  `pause-runtime-native.json` records both live callback slots pointing into
  the loader and a native settings page with three rows in the game process.
  Hook logs record the native pause draw, update, selection, and panel open.
- The first implementation reused a GDI child-window overlay. User testing
  observed flashing, blocked menu actions and a crash; logs recorded failed
  display recovery. It was replaced with native game drawing and input.
  Opening the current panel does not create windows or request device resets.

## Behavior

Row 5 is Mod Settings. Native dispatch after row 5 is shifted back by one
only while calling the original updater, preserving Extras/Drop Out/Quit.
The settings panel retains pause state, supports category and page rows,
generates controls from registered descriptors, and restores selection to
Mod Settings on Back/Escape. F10 and the legacy MenuKey setting are unused.

The existing D3D9 Reset/Present/EndScene hooks were also corrected from
default C calling convention lambdas to named `STDMETHODCALLTYPE` functions.
The rebuilt loader reached a stable render loop. Window style `0x14cf0000`
confirmed a decorated window during the first live verification.

## Verification

Win32 Release build and all four CTest cases passed. Runtime hook and native
panel opening were observed in logs and live memory. The user subsequently
confirmed that the panel opens and returns to the pause menu correctly.
Full display-mode
changes, all third-party settings, and complete controller coverage remain
outside this verification.

## Per-mod menus and native video lifecycle follow-up

The root list now contains one button per registered mod. Selecting its name
opens only that mod's settings. Back/Escape first returns to the mod list.
Rows use explicit -0.14 vertical pitch, a smaller uniform native font, six
settings per page and concise labels without repeated mod-name prefixes.

Capstone inspection traced the game's Video Settings Apply action at
0x50ee70 to the renderer method 0x6e3940 (`thiscall`, one stack argument,
ret 4). That method uses the native mode table at renderer+0x61c, releases
game resources through 0x6df0e0, resets the device, updates global dimensions
0x973900/0x973904 and reconstructs resources through 0x6dfe20. The loader now
calls this lifecycle instead of calling D3D9 Reset directly. Its reset hook
can recover the previous presentation before the native resource rebuild.
Unconfirmed Modern Options display values are not written to the INI.

An opt-in, non-shipping `TCSVideoRuntimeProbe` plugin exercised the public
host video API in the real game. Hook logs verified 1366x768, then a return
to 1280x720 with a surviving render loop. See video-runtime-after.json and
video-runtime-probe.log. This is runtime evidence of those windowed modes;
exclusive fullscreen and every possible graphics setting are not covered.

The final follow-up build passes five CTest cases, including actual per-mod
menu navigation, setting ownership/persistence and hierarchical Back/Escape.
