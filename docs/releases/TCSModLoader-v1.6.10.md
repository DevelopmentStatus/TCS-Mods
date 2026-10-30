# TCS Mod Loader 1.6.10

Fixes the forced fullscreen-to-windowed reset introduced in 1.6.9. The loader
queued a complete Saga display reset immediately after device creation and ran
it inside the first Present hook. The reported Windows crash was an access
violation at game address 0x605bdb: renderer cleanup dereferenced the resource
table from 0x2854bc8 before it was initialized. The log ended before Present
completed. This is consistent with the premature reset path.

The loader now selects the initial windowed/fullscreen presentation mode before
creating the device and does not queue an immediate startup reset. Subsequent
Saga display changes are posted to the window-message loop; Present only queues
them and never invokes Saga's resource teardown/reset directly. Backbuffer sizes
already computed by Saga remain unchanged. Modern Options and the inline Mod
Settings repairs from 1.6.9 remain enabled.

Install with the game closed by replacing d3d9.dll and TCSAssetManager.exe from
this ZIP. The importer is unchanged from 1.6.9, so its cache version intentionally
remains 1.6.9 and existing prepared assets are reused. No mod archives, save files
or setting files need to be removed.

Regression tests exercise the actual Modern Options DLL through the loader
host, initial Windowed parameters, first-Present behavior with a device present,
preservation of backbuffer dimensions, and existing pause-menu dispatch. These
tests verify that Present does not execute a Saga reset even when one is pending.

Live startup verification: the hotfix was installed with a backup and the game
was launched through Steam (app 32440) with the user's installed mod stack.
The game reached the main menu and remained running. Read-only diagnostics
confirmed a bordered 1280x720 client window, native windowed flag 1, a live D3D
device, and an initialized renderer resource table. The log passed the first
Present call. Later manual display transitions and gameplay were not tested.
