#pragma once
#include <string>
#include "mod_api.h"
// Runs before plugins/game asset initialization. False leaves native lookup intact.
bool PrepareAssetOverlay(const std::wstring& root, const TCSModHost& host, void (*log)(const char*));
