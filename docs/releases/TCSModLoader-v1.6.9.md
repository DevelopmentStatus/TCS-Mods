# TCS Mod Loader 1.6.9

Fixes two regressions from 1.6.8 when Saga Tweaks is enabled.

Mod Settings is drawn inline below Effects Settings again. The previous appended
row depended on a draw wrapper that could be replaced during menu initialization.
The loader now reattaches recognized native/Saga menu slots during presentation
and the Effects draw call. Dispatch translates both cursor fields and Saga's
drawn Tweaks-row index, then restores them, keeping Tweaks/Extras/Drop Out/Quit
on their intended actions. Unknown third-party menu handlers are left alone.

TCS Modern Options v0.4.0 is no longer skipped. Window mode maps to Saga's
convention (Modern Windowed 0 -> Saga 1; Borderless 1 -> 2; Fullscreen 2 -> 0).
VSync and the frame cap also use Saga's exported state/functions. Saved startup
preferences apply after the native device exists. Display changes use Saga's
native reset path, and the loader avoids overwriting its scaled backbuffer.
Only Skip Splash remains suppressed because its branch patch blocks Saga's
startup readiness check. No replacement Modern Options plugin ZIP is required.

Close the game and replace both d3d9.dll and TCSAssetManager.exe with this release.
Keep Saga Tweaks' archive and TCSModernOptions-v0.4.0.zip in Mods, compressed.

Validation: all 43 Python tests passed. Native regression coverage includes the
actual existing Modern Options DLL initialized through the real loader host
with mock Saga exports, default Windowed mapping, mode/cap changes, preserving
backbuffer parameters, inline button drawing without the late wrapper, both
cursor fields, Saga row translation, and repair of recognized runtime slots.
Full in-game display transitions and the rendered menu remain unverified.
