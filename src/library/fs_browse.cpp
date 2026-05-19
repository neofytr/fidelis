// SPDX-License-Identifier: GPL-3.0-or-later
//
// Folder-browse helpers backing /api/fs and /api/queue/append-folder.
// Filesystem-only — no SQLite, no engine; pure enough for unit testing
// against a temp directory.

#include <fidelis/library/library.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

namespace fidelis::library {

namespace fs = std::filesystem;

namespace {

constexpr std::array<std::string_view, 13> kAudioExt = {
    ".wav",  ".wave", ".aif",  ".aiff", ".aifc",
    ".flac", ".m4a",  ".mp4",  ".alac",
    ".mp3",  ".ogg",  ".oga",  ".opus",
};

std::string lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c + ('a' - 'A'));
        }
    }
    return s;
}

bool icmp_lt(const std::string& a, const std::string& b) {
    // Case-insensitive ASCII compare; matches what file managers do.
    const std::size_t n = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto ca = static_cast<unsigned char>(std::tolower(a[i]));
        const auto cb = static_cast<unsigned char>(std::tolower(b[i]));
        if (ca != cb) return ca < cb;
    }
    return a.size() < b.size();
}

} // namespace

bool has_audio_extension(const fs::path& p) {
    const std::string ext = lower(p.extension().string());
    for (auto e : kAudioExt) {
        if (ext == e) return true;
    }
    return false;
}

std::optional<FsListing> list_audio_dir(const fs::path& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return std::nullopt;
    }

    FsListing out;
    out.path = dir.lexically_normal();
    if (out.path.has_parent_path() && out.path != out.path.root_path()) {
        out.parent = out.path.parent_path();
    }

    for (const auto& e : fs::directory_iterator(
             dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        const fs::path p = e.path();
        const std::string name = p.filename().string();
        if (name.empty() || name.front() == '.') {
            continue;  // hide dotfiles / dotdirs
        }
        FsEntry fe;
        fe.name = name;
        fe.path = p;
        std::error_code ec2;
        fe.is_dir = e.is_directory(ec2);
        if (fe.is_dir) {
            out.entries.push_back(std::move(fe));
            continue;
        }
        if (!has_audio_extension(p)) {
            continue;
        }
        fe.size = static_cast<std::uint64_t>(e.file_size(ec2));
        out.entries.push_back(std::move(fe));
    }

    // Dirs first (alphabetical), then files (alphabetical) — file-manager
    // convention; lets users drill in by clicking the top of the list.
    std::sort(out.entries.begin(), out.entries.end(),
              [](const FsEntry& a, const FsEntry& b) {
                  if (a.is_dir != b.is_dir) return a.is_dir;
                  return icmp_lt(a.name, b.name);
              });
    return out;
}

std::optional<std::vector<fs::path>>
collect_audio_files(const fs::path& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return std::nullopt;
    }
    std::vector<fs::path> out;
    fs::recursive_directory_iterator it(
        dir, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    for (; !ec && it != end; ++it) {
        const fs::path p = it->path();
        const std::string name = p.filename().string();
        // Skip dot-files AND dot-directories so a folder-append never
        // descends into ~/.cache, .config, .Trash-1000 and friends.
        if (!name.empty() && name.front() == '.') {
            std::error_code ec2;
            if (it->is_directory(ec2)) {
                it.disable_recursion_pending();
            }
            continue;
        }
        std::error_code ec2;
        if (it->is_regular_file(ec2) && has_audio_extension(p)) {
            out.push_back(p);
        }
    }
    std::sort(out.begin(), out.end(),
              [](const fs::path& a, const fs::path& b) {
                  return icmp_lt(a.string(), b.string());
              });
    return out;
}

} // namespace fidelis::library
