#pragma once

// Called on the render thread after the unpacked Steam image is available.
bool InstallNativePauseMenu(void (*openSettings)(void*), bool (*settingsOpen)(),
    void (*drawSettings)(void*), void (*updateSettings)(void*), void (*log)(const char*));
