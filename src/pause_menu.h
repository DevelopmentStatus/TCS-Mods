#pragma once

namespace tcs {
constexpr int modSettingsRow = 5;
// Resume, Options, Control Setup, Video Settings, Effects Settings, Mod Settings.
constexpr int originalPauseRow(int row) { return row > modSettingsRow ? row - 1 : row; }
}
