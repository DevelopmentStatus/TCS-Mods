# TCS Mod Loader 1.7.2

Fixes the save-load crash introduced when 1.7.1 enabled more than 340 registered
characters. The hub's character-switching helpers still copied CHARCOUNT model
IDs into 340-entry stack arrays. With the installed 373-character roster this
overwrote a security cookie and the game terminated with 0xc0000409 at0x491a8c.

The reported crash dump confirmed CHARCOUNT373 and a cookie overwritten with
0xffffffff (unloaded-character IDs). The two short-ID buffers and associated
integer candidate buffer now support1,000 entries. Cookie offsets, the world
argument and every return path move consistently with the enlarged frames.
The native security-cookie checks remain active. Save format and companions
are unchanged; the update does not rewrite or restore the user's saves.

Validation executes the actual PC instructions for the failing function and
its two switching helpers in an isolated mirrored layout. Cases340,373,1000
preserve native cookies, exercise both return paths, and use the native RNG
to select the final candidate339,372,999. A separate static audit reviewed
CHARCOUNT references and native ID-list producers for other unbounded fixed
arrays. All20 native CTest checks passed. This regression does not claim full
live save-load verification.

Replace d3d9.dll while the game is closed. The asset helper/cache is unchanged.
