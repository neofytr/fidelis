// SPDX-License-Identifier: GPL-3.0-or-later
//
// Session persistence: the queue, the current index, and the playback offset
// survive a daemon restart. Playback never auto-resumes — we come back paused.
// All I/O is atomic (write-and-rename) so a kill during the save can never
// leave half-written state on disk; the previous snapshot stays intact.

#include <fidelis/session/session.hpp>

#include <nlohmann/json.hpp>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <unistd.h>

namespace fidelis::session {

namespace {
using json = nlohmann::json;
}

std::string serialize(const Snapshot& s) {
    json arr = json::array();
    for (const auto& p : s.tracks) {
        arr.push_back(p.string());
    }
    json j = {
        {"version", s.version},
        {"tracks", arr},
        {"current_index", s.current_index},
        {"position_frames", s.position_frames},
    };
    return j.dump(2);
}

std::optional<Snapshot> deserialize(std::string_view json_text) {
    json j = json::parse(json_text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) {
        return std::nullopt;
    }
    Snapshot s;
    if (auto it = j.find("version"); it != j.end() && it->is_number_integer()) {
        s.version = it->get<int>();
    }
    if (auto it = j.find("tracks"); it != j.end() && it->is_array()) {
        s.tracks.reserve(it->size());
        for (const auto& e : *it) {
            if (!e.is_string()) {
                return std::nullopt;
            }
            s.tracks.emplace_back(e.get<std::string>());
        }
    }
    if (auto it = j.find("current_index"); it != j.end() && it->is_number_integer()) {
        s.current_index = it->get<int>();
    }
    if (auto it = j.find("position_frames"); it != j.end() &&
        it->is_number_unsigned()) {
        s.position_frames = it->get<std::uint64_t>();
    }
    return s;
}

bool save(const std::filesystem::path& path, const Snapshot& s) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    // Atomic write: dump to a sibling tmp file and rename. A crash mid-write
    // leaves the old session intact; a successful rename is a single inode
    // swap so readers never see a torn file.
    const std::filesystem::path tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc | std::ios::binary);
        if (!out) {
            return false;
        }
        out << serialize(s);
        out.flush();
        if (!out) {
            return false;
        }
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

std::optional<Snapshot> load(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::ostringstream os;
    os << in.rdbuf();
    return deserialize(os.str());
}

std::filesystem::path default_path() {
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        return std::filesystem::path(xdg) / "fidelis" / "session.json";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / ".local" / "share" / "fidelis" /
               "session.json";
    }
    return std::filesystem::path(".local/share/fidelis/session.json");
}

} // namespace fidelis::session
