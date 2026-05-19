// SPDX-License-Identifier: GPL-3.0-or-later
//
// DSD is out of scope for v1.0. open_decoder must refuse .dsf / .dff with a
// FormatNotSupported error and a message that names DSD, before any I/O —
// so the gate is independent of whether a real DSD file is on disk.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <fidelis/engine/decoder_factory.hpp>
#include <fidelis/engine/error.hpp>

namespace eng = fidelis::engine;

TEST_CASE("DSD files are hard-refused (no .dsf)") {
    auto r = eng::open_decoder("/nonexistent/track.dsf");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code == eng::ErrorCode::FormatNotSupported);
    CHECK(r.error().message.find("DSD") != std::string::npos);
}

TEST_CASE("DSD files are hard-refused (no .dff)") {
    auto r = eng::open_decoder("/nonexistent/track.dff");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code == eng::ErrorCode::FormatNotSupported);
    CHECK(r.error().message.find("DSD") != std::string::npos);
}

TEST_CASE("Refusal is case-insensitive on the extension") {
    auto r = eng::open_decoder("/nonexistent/TRACK.DSF");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code == eng::ErrorCode::FormatNotSupported);
    CHECK(r.error().message.find("DSD") != std::string::npos);
}
