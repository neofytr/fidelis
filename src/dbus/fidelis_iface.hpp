// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FIDELIS_SRC_DBUS_FIDELIS_IFACE_HPP
#define FIDELIS_SRC_DBUS_FIDELIS_IFACE_HPP

#include <fidelis/engine/engine.hpp>
#include <fidelis/library/library.hpp>

#include <sdbus-c++/sdbus-c++.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace fidelis::dbus_svc {

// Adaptor for org.mpris.MediaPlayer2.fidelis at
// /org/mpris/MediaPlayer2/fidelis. Exposes the bit-perfect verdict, the
// device fingerprint + capabilities, the RT mode, the ring fill, plus the
// flattened pipeline snapshot via GetSnapshot(). Methods ReloadConfig,
// RescanLibrary, SelectDevice are also published.
class FidelisIface {
public:
    using ReloadHook = std::function<bool()>;          // returns true on apply
    using RescanHook = std::function<void()>;

    FidelisIface(sdbus::IObject& obj,
                     fidelis::engine::Engine* engine,
                     fidelis::library::Library* library,
                     ReloadHook reload,
                     RescanHook rescan);

    FidelisIface(const FidelisIface&) = delete;
    FidelisIface& operator=(const FidelisIface&) = delete;

    // Emit BitPerfectChanged when the level transitioned.
    void emit_bitperfect_changed(const std::string& level,
                                 const std::vector<std::string>& qualifications);

private:
    void register_vtable();

    sdbus::IObject& obj_;
    fidelis::engine::Engine* engine_;
    fidelis::library::Library* library_;
    ReloadHook reload_;
    RescanHook rescan_;
};

} // namespace fidelis::dbus_svc

#endif
