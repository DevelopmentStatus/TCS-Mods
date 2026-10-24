# TCS Mod Loader 1.6.5

The priority editor now opens independently of asset extraction. A broken or
unsupported ZIP/RAR appears in the list with its error, so it can be disabled
without removing every mod. Disabled archives are not inspected or extracted.
Save settings and Close do not rebuild assets; Launch game prepares the enabled
selection before starting the game. Import failures show the archive and reason,
plus the full report.json path.

During startup, the native wait handles the preparation window's messages and
leaves queued game messages for the game after graphics initialization resumes.
This addresses a startup ordering risk that could leave the first launch without
a usable window after priority review. Startup logging now distinguishes creation
of the Direct3D interface, the game device and the first presented frame. Games
started from the manual editor request a fresh bundled-helper environment.

Close the game and extract TCSModLoader-v1.6.5.zip beside LEGOStarWarsSaga.exe.
Replace both d3d9.dll and TCSAssetManager.exe. Keep Mods archives and Mods/Config.
The updated helper includes Python and Tcl/Tk.

For an import error, send the report.json named in the message. A screenshot of
the old generic failure cannot identify the archive or underlying error. This
release makes the error recoverable and visible; it does not make unsupported
archive layouts compatible automatically.

Validation: 29 Python regression tests passed; 12 native tests passed and one
unrelated shop-ownership runtime test skipped in the sandbox. A real Tk smoke
test verified error display, disabling/saving, rebuilding the healthy selection,
and rechecking a re-enabled broken archive. The bundled helper's --check-gui
probe passed. Full gameplay with the user's complete mod selection is unverified.

An isolated copy of the real game also passed cold-import and cached-startup
checks with the packaged helper and updated DLL: both launches rendered, the
native asset-read probe checked the active replacement with zero failures, and
the probe closed the game normally. This fixture used saved distinct priorities;
the user's exact priority-review hang still needs confirmation with their stack.
