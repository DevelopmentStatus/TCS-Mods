#include "pause_menu.h"
// Native dispatch must keep every action on either side of the inserted row.
static_assert(tcs::modSettingsRow==5);
static_assert(tcs::originalPauseRow(0)==0); // Resume
static_assert(tcs::originalPauseRow(4)==4); // Effects Settings
static_assert(tcs::originalPauseRow(6)==5); // Extras (or Quit when Extras is absent)
static_assert(tcs::originalPauseRow(7)==6); // Quit or Drop Out
static_assert(tcs::originalPauseRow(8)==7); // Quit with two players
int main() { return 0; }
