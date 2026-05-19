// SPDX-License-Identifier: GPL-3.0-or-later
//
// Test-only seam. Lets unit tests construct an Engine over a mock IDevice
// without exposing IDevice on the public API. Not installed.

#ifndef FIDELIS_ENGINE_TEST_ACCESS_HPP
#define FIDELIS_ENGINE_TEST_ACCESS_HPP

#include "output_iface.hpp"

#include <fidelis/engine/device.hpp>
#include <fidelis/engine/engine.hpp>
#include <fidelis/engine/error.hpp>

#include <expected>
#include <memory>

namespace fidelis::engine {

struct EngineTestHooks {
    static std::expected<std::unique_ptr<Engine>, Error>
    create_with_device(EngineConfig cfg, std::unique_ptr<detail::IDevice> dev);

    // Variant that also pins the active fingerprint so hotplug tests can
    // drive Disconnected/return cycles without a real ALSA card.
    static std::expected<std::unique_ptr<Engine>, Error>
    create_with_device_fp(EngineConfig cfg,
                          std::unique_ptr<detail::IDevice> dev,
                          DeviceFingerprint active_fp);
};

} // namespace fidelis::engine

#endif
