// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FIDELIS_LIBRARY_LIBRARY_HPP
#define FIDELIS_LIBRARY_LIBRARY_HPP

#include <fidelis/engine/error.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fidelis::library {

// One row in the library, denormalized for the GUI list view. `bit_depth`
// is 0 for lossy formats (MP3 / Vorbis / Opus / lossy ALAC), set for PCM /
// FLAC / lossless ALAC. `total_frames` is 0 if the decoder couldn't tell
// (rare; some streams lack a seek table). `mtime_unix_ns` is filesystem
// modification time at scan time.
struct Track {
    std::int64_t id;
    std::filesystem::path path;
    std::string title;
    std::string artist;
    std::string album;
    std::string album_artist;
    std::string track_no;
    std::string disc_no;
    std::string date;
    std::string codec;
    std::uint32_t sample_rate_hz;
    std::uint16_t channels;
    std::uint16_t bit_depth;
    std::uint64_t total_frames;
    std::chrono::milliseconds duration;
    std::int64_t file_size_bytes;
    std::int64_t mtime_unix_ns;
};

struct Album {
    std::int64_t id;
    std::string title;
    std::string album_artist;
    std::string date;
    std::int64_t track_count;
};

struct Artist {
    std::int64_t id;
    std::string name;
    std::int64_t album_count;
    std::int64_t track_count;
};

enum class SortBy : std::uint8_t {
    Artist,
    Album,
    Title,
    DateAdded,
};

// Free-text query is matched via FTS5 against (title, artist, album,
// album_artist). `artist` / `album` filter rows to an exact match on the
// corresponding column. limit/offset are 0-based; limit=0 means unbounded.
struct SearchFilter {
    std::string query;
    std::string artist;
    std::string album;
    SortBy      sort_by = SortBy::Artist;
    std::size_t limit  = 0;
    std::size_t offset = 0;
};

enum class ScanState : std::uint8_t {
    Idle,
    Scanning,
    Indexing,
    Cleaning,
};

struct ScanProgress {
    ScanState state;
    std::size_t files_seen;
    std::size_t files_indexed;
    std::size_t files_deleted;
    std::filesystem::path current_path;
    std::chrono::steady_clock::time_point started_at;
};

// Delivered on the scanner thread. Callbacks must return promptly. ScanStarted
// fires before the directory walk; ScanFinished after the deletion sweep.
struct DeltaEvent {
    enum class Kind : std::uint8_t {
        Added,
        Updated,
        Removed,
        ScanStarted,
        ScanFinished,
    };
    Kind kind;
    std::int64_t track_id;
    std::filesystem::path path;
};

using DeltaCallback = std::function<void(const DeltaEvent&)>;

struct Config {
    std::filesystem::path db_path;
    std::vector<std::filesystem::path> roots;
    std::vector<std::string> ignore_patterns;
};

// Owns the SQLite database and the scanner thread. Open() initializes the
// schema (running any pending migrations) and starts the scanner thread idle;
// rescan_async() kicks it off. Queries run on the calling thread against a
// shared, mutex-guarded read connection; the scanner has its own write
// connection. Reads do not block writes thanks to WAL.
//
// All public methods are thread-safe.
class Library {
public:
    static std::expected<std::unique_ptr<Library>, fidelis::engine::Error>
    open(Config cfg);

    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;
    Library(Library&&) = delete;
    Library& operator=(Library&&) = delete;
    ~Library();

    void set_delta_callback(DeltaCallback cb);

    // Update watched roots and/or ignore patterns; effective on next rescan.
    void set_roots(std::vector<std::filesystem::path> roots);
    void set_ignore_patterns(std::vector<std::string> patterns);

    void rescan_async();

    ScanProgress progress() const;

    std::expected<std::vector<Track>, fidelis::engine::Error>
    search(const SearchFilter& filter) const;

    std::expected<std::vector<Album>, fidelis::engine::Error>
    albums(const std::string& artist) const;

    std::expected<std::vector<Artist>, fidelis::engine::Error>
    artists() const;

    std::expected<std::vector<Track>, fidelis::engine::Error>
    tracks_in_album(std::int64_t album_id) const;

    std::expected<Track, fidelis::engine::Error>
    track_by_id(std::int64_t id) const;

    std::expected<Track, fidelis::engine::Error>
    track_by_path(const std::filesystem::path& path) const;

private:
    Library();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// True for path extensions we treat as audio. Lowercase / case-insensitive.
// The same prefilter the scanner uses, exposed for the web folder-browse and
// for tests. Magic-byte confirmation still happens later in open_decoder().
bool has_audio_extension(const std::filesystem::path& p);

struct FsEntry {
    std::string name;
    std::filesystem::path path;
    bool is_dir = false;
    std::uint64_t size = 0;
};

struct FsListing {
    std::filesystem::path path;     // resolved (lexically_normal) listed dir
    std::filesystem::path parent;   // empty when at "/" or unresolved
    std::vector<FsEntry> entries;   // dirs first then audio files; both sorted
};

// Read a directory for the folder-browse REST endpoint: directories plus
// audio files only (anything else is hidden). Dirs are listed first, then
// audio files, each group alphabetical case-insensitive. Hidden entries
// ("." prefix) are skipped. Returns nullopt when the path is not a readable
// directory.
std::optional<FsListing> list_audio_dir(const std::filesystem::path& dir);

// Walk a directory recursively and collect every audio file path in stable
// (lexicographic) order. Used by /api/queue/append-folder so a single REST
// call queues a whole album. Returns nullopt when `dir` is not a readable
// directory.
std::optional<std::vector<std::filesystem::path>>
collect_audio_files(const std::filesystem::path& dir);

} // namespace fidelis::library

#endif
