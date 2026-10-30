#pragma once

// Saga keeps its native Tweaks/Drop Out/Quit handling. Loader settings append
// after those rows when a validated Saga handler is available.
void ConfigureSagaPauseMenu(void (__cdecl *update)(void*),int* tweaksRow=nullptr);
int NativeModSettingsRow();
void MaintainNativePauseMenu();

// Called on the render thread after the unpacked Steam image is available.
bool InstallNativePauseMenu(void (*openSettings)(void*), bool (*settingsOpen)(),
    void (*drawSettings)(void*), void (*updateSettings)(void*), void (*log)(const char*));
