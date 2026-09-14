#pragma once
#include <cstddef>
#include <optional>

#include "setup/registry_state.h"

namespace setup {

enum class InstallMode {
    Fresh,      // nothing installed
    Update,     // keep the device node, swap the driver on it
    Reinstall,  // uninstall everything, then install fresh
};

// An update keeps the root device node, so Windows keeps each virtual gun's ParentIdPrefix and
// the Raw Input device paths games bind to survive. Only a complete install with exactly one
// live node qualifies; anything irregular is repaired by a full reinstall.
inline InstallMode ChooseInstallMode(const std::optional<InstallState>& state, size_t devices,
                                     size_t present_devices) {
    if (!state) return InstallMode::Fresh;
    if (state->version.empty() || state->cert_thumbprint.empty() || state->driver_inf.empty()) {
        return InstallMode::Reinstall;
    }
    if (!state->pending_cert_thumbprint.empty() || !state->pending_driver_inf.empty() ||
        !state->previous_cert_thumbprint.empty() || !state->previous_driver_inf.empty()) {
        return InstallMode::Reinstall;
    }
    if (devices != 1 || present_devices != 1) return InstallMode::Reinstall;
    return InstallMode::Update;
}

}  // namespace setup
