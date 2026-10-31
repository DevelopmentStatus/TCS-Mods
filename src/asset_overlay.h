#pragma once
#include <string>
#include "mod_api.h"
// Runs after plugin initialization, before game asset loading.
// False leaves native lookup intact, including when a required plugin is absent.
bool PrepareAssetOverlay(const std::wstring& root, const TCSModHost& host, void (*log)(const char*),
                         bool (*pluginAvailable)(const char*)=nullptr);
