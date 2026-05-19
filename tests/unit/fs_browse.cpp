// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the folder-browse helpers used by /api/fs and
// /api/queue/append-folder. Builds a small temp directory tree, exercises
// extension filtering, sort order (dirs first then files), hidden-entry
// suppression, and recursive collection.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <fidelis/library/library.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace lib = fidelis::library;
namespace fs = std::filesystem;

namespace {

struct Tree {
    fs::path root;
    Tree() {
        root = fs::temp_directory_path() /
               (std::string("fidelis_fsb_") +
                std::to_string(static_cast<unsigned long long>(
                    reinterpret_cast<std::uintptr_t>(this))));
        std::error_code ec;
        fs::remove_all(root, ec);
        fs::create_directories(root, ec);
    }
    ~Tree() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    void touch(const std::string& rel) {
        const auto p = root / rel;
        fs::create_directories(p.parent_path());
        std::ofstream o(p);
        o << "x";
    }
    void mkdir(const std::string& rel) {
        fs::create_directories(root / rel);
    }
};

} // namespace

TEST_CASE("has_audio_extension is case-insensitive and only audio") {
    CHECK(lib::has_audio_extension("/a/song.flac"));
    CHECK(lib::has_audio_extension("/a/song.FLAC"));
    CHECK(lib::has_audio_extension("/a/song.Mp3"));
    CHECK(lib::has_audio_extension("/a/song.opus"));
    CHECK_FALSE(lib::has_audio_extension("/a/cover.jpg"));
    CHECK_FALSE(lib::has_audio_extension("/a/notes.txt"));
    CHECK_FALSE(lib::has_audio_extension("/a/song"));
}

TEST_CASE("list_audio_dir filters to audio + dirs, hides dotfiles") {
    Tree t;
    t.mkdir("Albums");
    t.mkdir("Albums/.hidden_dir");
    t.touch("song.flac");
    t.touch("song.mp3");
    t.touch("README.md");
    t.touch(".hidden_song.flac");
    t.touch("cover.jpg");

    auto r = lib::list_audio_dir(t.root);
    REQUIRE(r.has_value());

    // Expect: Albums (dir), song.flac, song.mp3. README/.hidden/cover excluded.
    REQUIRE(r->entries.size() == 3);
    CHECK(r->entries[0].name == "Albums");
    CHECK(r->entries[0].is_dir);
    CHECK(r->entries[1].name == "song.flac");
    CHECK_FALSE(r->entries[1].is_dir);
    CHECK(r->entries[2].name == "song.mp3");
}

TEST_CASE("list_audio_dir: dirs first, then case-insensitive name order") {
    Tree t;
    t.mkdir("z_dir");
    t.mkdir("A_dir");
    t.touch("zeta.flac");
    t.touch("alpha.flac");
    t.touch("Beta.flac");

    auto r = lib::list_audio_dir(t.root);
    REQUIRE(r.has_value());
    REQUIRE(r->entries.size() == 5);
    CHECK(r->entries[0].name == "A_dir");
    CHECK(r->entries[1].name == "z_dir");
    CHECK(r->entries[2].name == "alpha.flac");
    CHECK(r->entries[3].name == "Beta.flac"); // case-insensitive: B before z
    CHECK(r->entries[4].name == "zeta.flac");
}

TEST_CASE("list_audio_dir returns nullopt for a missing path") {
    CHECK_FALSE(lib::list_audio_dir("/no/such/dir/here").has_value());
}

TEST_CASE("collect_audio_files walks recursively and sorts stably") {
    Tree t;
    t.touch("a.flac");
    t.touch("disc1/01.flac");
    t.touch("disc1/02.flac");
    t.touch("disc1/cover.jpg");
    t.touch("disc2/01.flac");
    t.touch(".dot/skipme.flac");  // dotfile guard

    auto r = lib::collect_audio_files(t.root);
    REQUIRE(r.has_value());
    REQUIRE(r->size() == 4);
    // Lexicographic order (case-insensitive). a.flac comes first since it
    // sits at the top, then disc1/*.flac, then disc2/01.flac.
    CHECK(r->at(0).filename() == "a.flac");
    CHECK(r->at(1).filename() == "01.flac");
    CHECK(r->at(2).filename() == "02.flac");
    CHECK(r->at(3).filename() == "01.flac");
}

TEST_CASE("collect_audio_files returns nullopt for a missing path") {
    CHECK_FALSE(lib::collect_audio_files("/no/such/dir/here").has_value());
}
