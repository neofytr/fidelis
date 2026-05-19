// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FIDELIS_SRC_DBUS_SNAPSHOT_SERIALIZE_HPP
#define FIDELIS_SRC_DBUS_SNAPSHOT_SERIALIZE_HPP

#include <fidelis/engine/device.hpp>
#include <fidelis/engine/telemetry.hpp>

#include <sdbus-c++/sdbus-c++.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace fidelis::dbus_svc {

constexpr std::string_view bit_perfect_level_name(
    fidelis::engine::BitPerfectVerdict::Level lv) noexcept {
    using L = fidelis::engine::BitPerfectVerdict::Level;
    switch (lv) {
    case L::Yes:       return "YES";
    case L::Qualified: return "QUALIFIED";
    case L::No:        return "NO";
    }
    return "NO";
}

// "vid:pid:serial" or empty when not USB.
std::string usb_vid_pid_serial(
    const fidelis::engine::DeviceFingerprint& fp);

// Union of (format, rate) pairs as ["S16_LE@44100", ...].
std::vector<std::string> device_format_strings(
    const fidelis::engine::DeviceCapabilities& caps);

// Render the full snapshot as a dict of stage-name to dict of field-name to
// variant. Schema documented in fidelis_iface.cpp.
std::map<std::string, std::map<std::string, sdbus::Variant>>
snapshot_to_dict(const fidelis::engine::PipelineSnapshot& snap);

} // namespace fidelis::dbus_svc

#endif
