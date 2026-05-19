// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FIDELIS_SESSION_SESSION_HPP
#define FIDELIS_SESSION_SESSION_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fidelis::session {

// Persisted session snapshot. Restored at startup so the user comes back to
// the queue they left behind; playback is NEVER auto-resumed — the daemon
// always comes up paused, the user explicitly hits play.
struct Snapshot {
    std::vector<std::filesystem::path> tracks;
    int current_index = -1;            // -1 = nothing loaded
    std::uint64_t position_frames = 0; // offset into the current track
    // Schema version. Older snapshots from prior releases are tolerated as
    // long as the JSON parses; missing fields take their default.
    int version = 1;
};

// Serialise to JSON text. Stable schema; reversible.
std::string serialize(const Snapshot& s);

// Parse JSON text. Returns nullopt on any parse error or wrong-shaped data.
// A schema-version skew is not an error here — extra/missing keys take
// defaults so the daemon never refuses to start because of a stale snapshot.
std::optional<Snapshot> deserialize(std::string_view json_text);

// File I/O. Writes are atomic (write-and-rename). load() returns nullopt
// when the file does not exist or cannot be parsed.
bool save(const std::filesystem::path& path, const Snapshot& s);
std::optional<Snapshot> load(const std::filesystem::path& path);

// Default location: $XDG_DATA_HOME/fidelis/session.json, falling back to
// ~/.local/share/fidelis/session.json. The returned path may not exist.
std::filesystem::path default_path();

} // namespace fidelis::session

#endif
