// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FIDELIS_CONFIG_CONFIG_HPP
#define FIDELIS_CONFIG_CONFIG_HPP

#include <fidelis/engine/error.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace fidelis::config {

enum class RtPolicy : std::uint8_t { Auto, Fifo, Other };

enum class DefaultView : std::uint8_t { Main, Library, Pipeline };

struct DeviceSection {
    std::string preferred;            // "hw:CARD=...,DEV=N" or empty
};

struct AudioSection {
    std::uint32_t period_ms = 12;
    std::uint32_t period_count = 4;
    RtPolicy rt_policy = RtPolicy::Auto;
};

struct LibrarySection {
    std::vector<std::filesystem::path> roots;
    std::vector<std::string> ignore_patterns;
};

struct ThemeSection {
    bool follow_hyprland = true;
    std::filesystem::path override_file;
};

struct UiSection {
    DefaultView default_view = DefaultView::Main;
};

struct DbusSection {
    bool enabled = true;
};

enum class RgConfigMode : std::uint8_t { Off, Track, Album };

struct ReplayGainSection {
    // Default off. Enabling drops the bit-perfect verdict to QUALIFIED,
    // visibly. Album mode preserves intra-album dynamics; the loudness is
    // matched per album, not per track.
    RgConfigMode mode = RgConfigMode::Off;
    bool prevent_clipping = true;
};

struct WebSection {
    // Empty host means "loopback" (127.0.0.1). 0.0.0.0 binds every interface.
    std::string host = "127.0.0.1";
    std::uint16_t port = 7800;
    // Bearer token required on every /api/ route except /api/art/*. Empty
    // means auth disabled — refused at startup unless host is loopback so a
    // shared-LAN bind is never silently unauthenticated.
    std::string token;
};

struct Config {
    DeviceSection device;
    AudioSection audio;
    LibrarySection library;
    ThemeSection theme;
    UiSection ui;
    DbusSection dbus;
    WebSection web;
    ReplayGainSection replaygain;
};

// Default location: $XDG_CONFIG_HOME/fidelis/config.toml, falling back to
// ~/.config/fidelis/config.toml. Returned path may not exist.
std::filesystem::path default_config_path();

// Default DB location for the library: $XDG_DATA_HOME/fidelis/library.db,
// falling back to ~/.local/share/fidelis/library.db.
std::filesystem::path default_library_db_path();

// Expand a leading "~" / "~/" in a path to $HOME. No effect if HOME is unset
// or the input does not start with "~".
std::filesystem::path expand_user(std::filesystem::path p);

// Parse TOML text into a Config. Missing keys take their default values.
// Returns InvalidArgument on parse error or wrong-typed key.
std::expected<Config, fidelis::engine::Error>
parse(std::string_view toml_text);

// Convenience: read the file at `path` and parse. NotFound when the file
// does not exist (caller decides whether that is fatal).
std::expected<Config, fidelis::engine::Error>
load_file(const std::filesystem::path& path);

// Update [device].preferred in the config file at `path`. Creates the file
// and parent directories if they do not exist. Preserves existing keys.
void save_device_preferred(const std::filesystem::path& path,
                           const std::string& hw_string);

// Write/replace the [web].token value in the config file at `path`. Creates
// the file and parent dirs if needed. Preserves existing keys.
void save_web_token(const std::filesystem::path& path,
                    const std::string& token);

// Persist the [replaygain] section in the config file at `path`. Preserves
// every other key.
void save_replaygain(const std::filesystem::path& path,
                     RgConfigMode mode, bool prevent_clipping);

// Generate a cryptographically-strong random bearer token (base64url, 32
// bytes of entropy). Reads /dev/urandom; returns an empty string on failure.
std::string generate_token();

// True when `host` is bound only to the local machine ("", "127.0.0.1",
// "::1", "localhost"). Anything else is treated as LAN-reachable and is
// refused at startup with an empty token.
bool is_loopback_host(std::string_view host);

} // namespace fidelis::config

#endif
