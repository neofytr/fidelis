// SPDX-License-Identifier: GPL-3.0-or-later
//
// Preference resolution, factored out of main so it is unit-testable without
// ALSA. The volatile part of a device identity is its ALSA card name (a USB
// DAC whose product descriptor reads garbled re-enumerates under a generic
// id); the stable part is the fingerprint id. Preferences are persisted as
// the id and resolved back to whatever hw string the device has now.

#include <fidelis/engine/device.hpp>

#include <algorithm>

namespace fidelis::engine {

std::vector<DeviceInfo> usb_only(std::vector<DeviceInfo> devices) {
    std::erase_if(devices, [](const DeviceInfo& d) {
        return !d.fingerprint.is_usb;
    });
    return devices;
}

std::string select_preferred_device(const std::string& preferred,
                                    const std::vector<DeviceInfo>& devices) {
    if (devices.empty()) {
        return {};
    }
    if (!preferred.empty()) {
        for (const auto& d : devices) {
            if (d.alsa_hw_string == preferred) {
                return d.alsa_hw_string;
            }
        }
        for (const auto& d : devices) {
            if (!d.id.empty() && d.id == preferred) {
                return d.alsa_hw_string;
            }
        }
        // Pinned device absent: keep the engine alive on a present device so
        // the UI/device-picker stays usable; prefer a USB DAC.
        for (const auto& d : devices) {
            if (d.fingerprint.is_usb) {
                return d.alsa_hw_string;
            }
        }
        return devices.front().alsa_hw_string;
    }
    return devices.front().alsa_hw_string;
}

} // namespace fidelis::engine
