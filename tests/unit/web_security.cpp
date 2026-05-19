// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pure-function tests for the web-security helpers. is_loopback_host decides
// whether the daemon may run without a bearer token; generate_token must
// return a high-entropy URL-safe string when /dev/urandom is reachable.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <fidelis/config/config.hpp>

#include <set>
#include <string>

namespace cfg = fidelis::config;

TEST_CASE("is_loopback_host matches every loopback form") {
    CHECK(cfg::is_loopback_host(""));
    CHECK(cfg::is_loopback_host("127.0.0.1"));
    CHECK(cfg::is_loopback_host("::1"));
    CHECK(cfg::is_loopback_host("localhost"));
    CHECK(cfg::is_loopback_host("0:0:0:0:0:0:0:1"));
}

TEST_CASE("is_loopback_host rejects routable addresses") {
    CHECK_FALSE(cfg::is_loopback_host("0.0.0.0"));
    CHECK_FALSE(cfg::is_loopback_host("192.168.1.10"));
    CHECK_FALSE(cfg::is_loopback_host("10.0.0.1"));
    CHECK_FALSE(cfg::is_loopback_host("::"));
    CHECK_FALSE(cfg::is_loopback_host("fe80::1"));
}

TEST_CASE("generate_token returns a non-empty URL-safe base64 string") {
    const std::string t = cfg::generate_token();
    REQUIRE_FALSE(t.empty());
    CHECK(t.size() >= 32);
    for (char c : t) {
        const bool ok = (c >= 'A' && c <= 'Z') ||
                        (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') ||
                        c == '-' || c == '_';
        CHECK(ok);
    }
}

TEST_CASE("generate_token yields a different value on every call") {
    // 8 draws of >= 32 bytes of entropy each. A collision implies broken RNG.
    std::set<std::string> seen;
    for (int i = 0; i < 8; ++i) {
        seen.insert(cfg::generate_token());
    }
    CHECK(seen.size() == 8);
}
