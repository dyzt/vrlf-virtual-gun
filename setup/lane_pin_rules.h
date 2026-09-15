#pragma once
#include <string>
#include <vector>

#include "shared/gun_device.h"

namespace setup {

// ConfigFlags bit PnP sets on a devnode whose driver install failed.
inline constexpr unsigned long CONFIGFLAG_FAILED_INSTALL = 0x40;

// One lane's VHF devnode key as setup found it.
struct LaneKey {
    bool exists = false;
    bool installed = false;  // PnP finished installing it (see IsInstalledLaneKey)
    std::wstring prefix;     // its ParentIdPrefix, empty when absent
};

// Any key under Enum\VHF\HID_DEVICE_SYSTEM_VHF.
struct VhfKey {
    std::wstring name;
    std::wstring prefix;
};

enum class PinAction {
    AlreadyPinned,
    Write,             // overwrite ParentIdPrefix with the pinned value
    CreateFirst,       // no key yet: create the lane once so PnP writes it
    RemoveThenCreate,  // a key PnP never finished installing: remove the devnode, then create
    Collision,         // another device holds the pinned value: refuse
};

struct PinSummary {
    bool driver_unavailable = false;
    int failed = 0;
};

// A key written before PnP ever installed the lane has no Driver value and is flagged failed;
// PnP never binds a driver to it, so it must be removed rather than pinned.
inline bool IsInstalledLaneKey(bool has_driver_value, unsigned long config_flags) {
    return has_driver_value && (config_flags & CONFIGFLAG_FAILED_INSTALL) == 0;
}

inline PinAction DecidePin(const LaneKey& key, const std::wstring& pinned, bool collides) {
    if (!key.exists) return PinAction::CreateFirst;
    if (!key.installed) return PinAction::RemoveThenCreate;
    if (vgun::same_id(key.prefix, pinned)) return PinAction::AlreadyPinned;
    if (collides) return PinAction::Collision;
    return PinAction::Write;
}

// Does any key other than the target hold the pinned value? The same lane's key from an earlier
// install (a different root prefix) is ours, not a collision.
inline bool IsCollision(const std::vector<VhfKey>& keys, const std::wstring& target_name, unsigned lane,
                        const std::wstring& pinned) {
    for (const VhfKey& k : keys) {
        if (vgun::same_id(k.name, target_name)) continue;
        if (!vgun::same_id(k.prefix, pinned)) continue;
        if (vgun::is_lane_key_of(k.name, lane)) continue;
        return true;
    }
    return false;
}

// The install already succeeded with `install_code` (0 or 3010); pins only ever make it worse.
inline int PinExitCode(int install_code, const PinSummary& pins) {
    if (pins.driver_unavailable) return install_code == 3010 ? 3010 : 1;
    if (pins.failed > 0) return 1;
    return install_code;
}

}  // namespace setup
