// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pure-function tests for the ReplayGain helpers.
//   db_to_linear:                round-trip + NaN passthrough.
//   compute_replaygain_linear:   mode selection, track-falls-back-to-album,
//                                missing tags -> 1.0, peak-aware clipping.
//   apply_gain:                  per-format scaling + saturation.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <fidelis/engine/replaygain.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace eng = fidelis::engine;

namespace {
constexpr float kNan = std::numeric_limits<float>::quiet_NaN();

bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}
} // namespace

TEST_CASE("db_to_linear: known values + NaN passthrough") {
    CHECK(approx(eng::db_to_linear(0.0f), 1.0f));
    CHECK(approx(eng::db_to_linear(6.0f), 1.9953f, 1e-3f));
    CHECK(approx(eng::db_to_linear(-6.0f), 0.50119f, 1e-3f));
    CHECK(approx(eng::db_to_linear(20.0f), 10.0f, 1e-3f));
    CHECK(approx(eng::db_to_linear(-20.0f), 0.1f, 1e-3f));
    CHECK(approx(eng::db_to_linear(kNan), 1.0f));
}

TEST_CASE("compute_replaygain_linear: Off mode always returns 1.0") {
    eng::Tags t;
    t.rg_album_gain = -10.0f;
    t.rg_album_peak = 0.5f;
    t.rg_track_gain = -5.0f;
    eng::RgConfig cfg{};  // defaults: Off, prevent_clipping=true
    CHECK(approx(eng::compute_replaygain_linear(t, cfg), 1.0f));
}

TEST_CASE("Album mode applies the album gain") {
    eng::Tags t;
    t.rg_album_gain = -6.0f;
    t.rg_album_peak = 0.5f;
    eng::RgConfig cfg{eng::RgMode::Album, /*prevent_clipping=*/false};
    CHECK(approx(eng::compute_replaygain_linear(t, cfg), 0.50119f, 1e-3f));
}

TEST_CASE("Track mode applies the track gain") {
    eng::Tags t;
    t.rg_track_gain = -3.0f;
    t.rg_track_peak = 0.5f;
    eng::RgConfig cfg{eng::RgMode::Track, false};
    CHECK(approx(eng::compute_replaygain_linear(t, cfg), 0.7079f, 1e-3f));
}

TEST_CASE("Track mode falls back to album when track tags are missing") {
    eng::Tags t;
    t.rg_album_gain = -6.0f;
    t.rg_album_peak = 0.5f;
    // No track tags -> still uses album.
    eng::RgConfig cfg{eng::RgMode::Track, false};
    CHECK(approx(eng::compute_replaygain_linear(t, cfg), 0.50119f, 1e-3f));
}

TEST_CASE("Album mode does NOT fall back to track gain") {
    eng::Tags t;
    t.rg_track_gain = -6.0f;
    t.rg_track_peak = 0.5f;
    // No album tag — must stay bit-perfect (returns 1.0), not silently
    // mix modes with track gain.
    eng::RgConfig cfg{eng::RgMode::Album, false};
    CHECK(approx(eng::compute_replaygain_linear(t, cfg), 1.0f));
}

TEST_CASE("Missing tags -> 1.0, even with RG enabled") {
    eng::Tags t;  // every tag NaN
    eng::RgConfig cfg{eng::RgMode::Album, true};
    CHECK(approx(eng::compute_replaygain_linear(t, cfg), 1.0f));
}

TEST_CASE("prevent_clipping caps gain so linear * peak < 1.0") {
    eng::Tags t;
    t.rg_album_gain = +12.0f;  // boost would amplify peak past full-scale
    t.rg_album_peak = 0.5f;    // raw linear = ~3.98, capped to ~2.0 (1/0.5)
    eng::RgConfig cfg{eng::RgMode::Album, true};
    const float linear = eng::compute_replaygain_linear(t, cfg);
    CHECK(linear * t.rg_album_peak < 1.0f);
    CHECK(linear <= (1.0f / t.rg_album_peak) + 1e-3f);
}

TEST_CASE("prevent_clipping leaves attenuation alone (no headroom op)") {
    eng::Tags t;
    t.rg_album_gain = -6.0f;
    t.rg_album_peak = 0.5f;
    eng::RgConfig cfg_on{eng::RgMode::Album, true};
    eng::RgConfig cfg_off{eng::RgMode::Album, false};
    CHECK(approx(eng::compute_replaygain_linear(t, cfg_on),
                 eng::compute_replaygain_linear(t, cfg_off)));
}

TEST_CASE("apply_gain S16_LE: 1.0 is a no-op") {
    std::vector<std::int16_t> v = {-32000, -1000, 0, 1000, 32000};
    std::vector<std::int16_t> orig = v;
    eng::apply_gain({reinterpret_cast<std::byte*>(v.data()),
                     v.size() * sizeof(std::int16_t)},
                    eng::SampleFormat::S16_LE, 1.0f);
    CHECK(v == orig);
}

TEST_CASE("apply_gain S16_LE: 0.5 halves samples, with rounding") {
    std::vector<std::int16_t> v = {-32000, -1000, 0, 1000, 32000};
    eng::apply_gain({reinterpret_cast<std::byte*>(v.data()),
                     v.size() * sizeof(std::int16_t)},
                    eng::SampleFormat::S16_LE, 0.5f);
    CHECK(v[0] == -16000);
    CHECK(v[1] == -500);
    CHECK(v[2] == 0);
    CHECK(v[3] == 500);
    CHECK(v[4] == 16000);
}

TEST_CASE("apply_gain S16_LE: saturates at full scale") {
    std::vector<std::int16_t> v = {-32000, 32000};
    eng::apply_gain({reinterpret_cast<std::byte*>(v.data()),
                     v.size() * sizeof(std::int16_t)},
                    eng::SampleFormat::S16_LE, 4.0f);
    CHECK(v[0] == std::numeric_limits<std::int16_t>::min());
    CHECK(v[1] == std::numeric_limits<std::int16_t>::max());
}

TEST_CASE("apply_gain S32_LE: halves a known sample") {
    std::vector<std::int32_t> v = {1 << 28, -(1 << 28)};
    eng::apply_gain({reinterpret_cast<std::byte*>(v.data()),
                     v.size() * sizeof(std::int32_t)},
                    eng::SampleFormat::S32_LE, 0.5f);
    CHECK(v[0] == (1 << 27));
    CHECK(v[1] == -(1 << 27));
}

TEST_CASE("apply_gain S24_3LE: round-trip on a known sample") {
    // 24-bit sample value = 0x10_0000 = 1048576; halve to 524288.
    std::array<std::byte, 3> buf{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x10}};
    eng::apply_gain({buf.data(), buf.size()},
                    eng::SampleFormat::S24_3LE, 0.5f);
    // 524288 = 0x080000 -> bytes 00 00 08
    CHECK(static_cast<unsigned>(buf[0]) == 0x00);
    CHECK(static_cast<unsigned>(buf[1]) == 0x00);
    CHECK(static_cast<unsigned>(buf[2]) == 0x08);
}

TEST_CASE("apply_gain S24_3LE: negative sample sign-extension is preserved") {
    // Bytes 00 00 80 = -8388608 (24-bit min). Apply 0.5 -> -4194304.
    // -4194304 = 0xFFC00000 in 32-bit, low 24 bits = 0xC00000.
    std::array<std::byte, 3> buf{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x80}};
    eng::apply_gain({buf.data(), buf.size()},
                    eng::SampleFormat::S24_3LE, 0.5f);
    CHECK(static_cast<unsigned>(buf[0]) == 0x00);
    CHECK(static_cast<unsigned>(buf[1]) == 0x00);
    CHECK(static_cast<unsigned>(buf[2]) == 0xC0);
}

TEST_CASE("apply_gain FLOAT_LE: float keeps native range (no saturation)") {
    std::vector<float> v = {-2.0f, -0.5f, 0.0f, 0.5f, 2.0f};
    eng::apply_gain({reinterpret_cast<std::byte*>(v.data()),
                     v.size() * sizeof(float)},
                    eng::SampleFormat::FLOAT_LE, 2.0f);
    CHECK(approx(v[0], -4.0f));
    CHECK(approx(v[1], -1.0f));
    CHECK(approx(v[2],  0.0f));
    CHECK(approx(v[3],  1.0f));
    CHECK(approx(v[4],  4.0f));
}

TEST_CASE("replaygain_qualification mentions the dB amount and mode") {
    const std::string q = eng::replaygain_qualification(
        eng::db_to_linear(-6.0f), eng::RgMode::Album);
    CHECK(q.find("album") != std::string::npos);
    CHECK(q.find("ReplayGain") != std::string::npos);
    CHECK(q.find("-6.") != std::string::npos);
}

TEST_CASE("replaygain_qualification is empty when no gain is applied") {
    CHECK(eng::replaygain_qualification(1.0f, eng::RgMode::Album).empty());
}
