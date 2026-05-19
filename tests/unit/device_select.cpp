// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit tests for select_preferred_device — the device-preference resolver.
// Synthetic DeviceInfo lists; no ALSA. Covers the regression where a USB DAC
// re-enumerating under a new ALSA card name (SE -> UAC1) made the saved
// hw-string preference resolve to the wrong device or nothing.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <fidelis/engine/device.hpp>

#include <string>
#include <vector>

namespace tp = fidelis::engine;

namespace {

tp::DeviceInfo mk(std::string hw, std::string id, bool usb) {
    tp::DeviceInfo d;
    d.alsa_hw_string = std::move(hw);
    d.id = std::move(id);
    d.fingerprint.is_usb = usb;
    return d;
}

// HDMI/onboard first (typical enumeration order), USB DAC second.
std::vector<tp::DeviceInfo> devs() {
    return {
        mk("hw:CARD=NVidia,DEV=7", "card:HDA_NVidia", false),
        mk("hw:CARD=SE,DEV=0", "usb:2fc6:f082:iFi_USB_Audio_SE", true),
    };
}

} // namespace

TEST_CASE("exact hw-string preference is honoured") {
    CHECK(tp::select_preferred_device("hw:CARD=SE,DEV=0", devs()) ==
          "hw:CARD=SE,DEV=0");
}

TEST_CASE("stable id resolves to the device's current hw string") {
    // Saved id, but the card renamed SE -> UAC1 since.
    auto d = devs();
    d[1].alsa_hw_string = "hw:CARD=UAC1,DEV=0";
    CHECK(tp::select_preferred_device("usb:2fc6:f082:iFi_USB_Audio_SE", d) ==
          "hw:CARD=UAC1,DEV=0");
}

TEST_CASE("absent preference falls back to the USB DAC, not HDMI") {
    CHECK(tp::select_preferred_device("usb:dead:beef:gone", devs()) ==
          "hw:CARD=SE,DEV=0");
    CHECK(tp::select_preferred_device("hw:CARD=Nope,DEV=9", devs()) ==
          "hw:CARD=SE,DEV=0");
}

TEST_CASE("empty preference picks the first device") {
    CHECK(tp::select_preferred_device("", devs()) == "hw:CARD=NVidia,DEV=7");
}

TEST_CASE("no devices yields empty") {
    CHECK(tp::select_preferred_device("hw:CARD=SE,DEV=0", {}).empty());
    CHECK(tp::select_preferred_device("", {}).empty());
}

TEST_CASE("fallback with no USB device uses the first device") {
    std::vector<tp::DeviceInfo> only_hdmi = {
        mk("hw:CARD=NVidia,DEV=7", "card:HDA_NVidia", false),
        mk("hw:CARD=NVidia,DEV=8", "card:HDA_NVidia", false),
    };
    CHECK(tp::select_preferred_device("usb:x:y:z", only_hdmi) ==
          "hw:CARD=NVidia,DEV=7");
}

TEST_CASE("usb_only drops every non-USB device (hard refusal)") {
    auto out = tp::usb_only(devs());  // HDMI + USB iFi
    REQUIRE(out.size() == 1);
    CHECK(out[0].alsa_hw_string == "hw:CARD=SE,DEV=0");
    CHECK(out[0].fingerprint.is_usb);
}

TEST_CASE("usb_only on an all-onboard system yields nothing") {
    std::vector<tp::DeviceInfo> only_hdmi = {
        mk("hw:CARD=NVidia,DEV=7", "card:HDA_NVidia", false),
        mk("hw:CARD=sofhdadsp,DEV=0", "card:sof", false),
    };
    CHECK(tp::usb_only(only_hdmi).empty());
}

TEST_CASE("usb_only keeps multiple USB DACs") {
    std::vector<tp::DeviceInfo> two_usb = {
        mk("hw:CARD=SE,DEV=0", "usb:2fc6:f082:ifi", true),
        mk("hw:CARD=E70,DEV=0", "usb:152a:8750:topping", true),
    };
    CHECK(tp::usb_only(two_usb).size() == 2);
}
