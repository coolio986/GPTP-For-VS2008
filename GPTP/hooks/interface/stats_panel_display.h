#pragma once
#include "../../SCBW/structures.h"
#include "../../SCBW/structures/CUnit.h"

namespace hooks {

void stats_panel_display(BinDlg* dialog);  // 00426C60

void injectStatsPanelDisplayHook();

}  // namespace hooks