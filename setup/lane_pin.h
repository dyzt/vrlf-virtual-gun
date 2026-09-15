#pragma once
#include "setup/lane_pin_rules.h"

namespace setup {

// Pins every lane's VHF devnode ParentIdPrefix under the live root devnode, creating a lane
// once first where Windows has not written its key yet. Logs each lane. Needs admin.
PinSummary PinLanes();

}  // namespace setup
