// SPDX-License-Identifier: GPL-3.0-or-later
//
// ReplayGain helpers. Pure dSP: dB-to-linear conversion, gain selection
// (track / album) with peak-aware clip prevention, and a per-format sample
// scaler that runs on the decoder thread before samples reach the ring.
// No allocation, no locks, no syscalls — just a multiply + saturate.

#include <fidelis/engine/replaygain.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

namespace fidelis::engine {

namespace {

bool is_nan(float v) noexcept { return std::isnan(v); }

// Pick the (gain, peak) pair for this mode. Track mode falls back to album
// when track tags are missing (common for albums tagged only at the album
// level); Album mode never falls back to track gain (would mix modes).
void pick_pair(const Tags& t, RgMode mode, float& gain_db, float& peak) {
    gain_db = std::numeric_limits<float>::quiet_NaN();
    peak = std::numeric_limits<float>::quiet_NaN();
    if (mode == RgMode::Track) {
        gain_db = t.rg_track_gain;
        peak    = t.rg_track_peak;
        if (is_nan(gain_db)) {
            gain_db = t.rg_album_gain;
            peak    = t.rg_album_peak;
        }
    } else if (mode == RgMode::Album) {
        gain_db = t.rg_album_gain;
        peak    = t.rg_album_peak;
    }
}

// Read a signed 24-bit little-endian sample from three bytes. Sign-extends
// to int32 so the multiply below has the right sign.
inline std::int32_t read_s24_3le(const std::byte* p) noexcept {
    const auto b0 = static_cast<std::uint8_t>(p[0]);
    const auto b1 = static_cast<std::uint8_t>(p[1]);
    const auto b2 = static_cast<std::uint8_t>(p[2]);
    std::uint32_t u = static_cast<std::uint32_t>(b0) |
                      (static_cast<std::uint32_t>(b1) << 8) |
                      (static_cast<std::uint32_t>(b2) << 16);
    // Sign-extend from 24 to 32.
    if (u & 0x00800000u) {
        u |= 0xFF000000u;
    }
    return static_cast<std::int32_t>(u);
}

inline void write_s24_3le(std::byte* p, std::int32_t v) noexcept {
    p[0] = static_cast<std::byte>(v & 0xFF);
    p[1] = static_cast<std::byte>((v >> 8) & 0xFF);
    p[2] = static_cast<std::byte>((v >> 16) & 0xFF);
}

template <typename Int>
inline Int sat_round_to_int(double x) noexcept {
    constexpr double lo = static_cast<double>(std::numeric_limits<Int>::min());
    constexpr double hi = static_cast<double>(std::numeric_limits<Int>::max());
    if (x >= hi) return std::numeric_limits<Int>::max();
    if (x <= lo) return std::numeric_limits<Int>::min();
    return static_cast<Int>(std::llround(x));
}

// 24-bit saturate. The 24-bit signed range is [-8388608, 8388607].
inline std::int32_t sat_round_s24(double x) noexcept {
    constexpr double lo = -8388608.0;
    constexpr double hi =  8388607.0;
    if (x >= hi) return  8388607;
    if (x <= lo) return -8388608;
    return static_cast<std::int32_t>(std::llround(x));
}

} // namespace

float db_to_linear(float db) {
    if (is_nan(db)) {
        return 1.0f;
    }
    // 10^(db/20). std::pow is fine on the decoder thread (not RT).
    return std::pow(10.0f, db / 20.0f);
}

float compute_replaygain_linear(const Tags& tags, const RgConfig& cfg) {
    if (cfg.mode == RgMode::Off) {
        return 1.0f;
    }
    float gain_db = 0.0f;
    float peak = 0.0f;
    pick_pair(tags, cfg.mode, gain_db, peak);
    if (is_nan(gain_db)) {
        return 1.0f;  // no usable tag — stay bit-perfect
    }
    float linear = db_to_linear(gain_db);
    if (cfg.prevent_clipping && !is_nan(peak) && peak > 0.0f) {
        // Headroom: keep linear*peak strictly below 1.0 (one LSB shy of
        // full-scale) to avoid integer saturation on the loudest samples.
        constexpr float kFs = 0.999969f;  // -0.0003 dB; effectively full-scale
        const float ceiling = kFs / peak;
        if (linear > ceiling) {
            linear = ceiling;
        }
    }
    if (linear < 0.0f || !std::isfinite(linear)) {
        linear = 1.0f;
    }
    return linear;
}

void apply_gain(std::span<std::byte> buf, SampleFormat fmt, float linear) {
    if (linear == 1.0f || buf.empty()) {
        return;
    }
    const double g = static_cast<double>(linear);

    switch (fmt) {
    case SampleFormat::S16_LE: {
        auto* p = reinterpret_cast<std::int16_t*>(buf.data());
        const std::size_t n = buf.size() / sizeof(std::int16_t);
        for (std::size_t i = 0; i < n; ++i) {
            p[i] = sat_round_to_int<std::int16_t>(
                static_cast<double>(p[i]) * g);
        }
        break;
    }
    case SampleFormat::S24_LE: {
        // 24-bit in a 32-bit container; low 3 bytes used, top byte unused
        // by the kernel. We treat the full int32 value as the sample value
        // shifted up — multiplying preserves that, and we saturate to the
        // 24-bit signed range stored in the low 3 bytes (with sign-fill).
        auto* p = reinterpret_cast<std::int32_t*>(buf.data());
        const std::size_t n = buf.size() / sizeof(std::int32_t);
        for (std::size_t i = 0; i < n; ++i) {
            p[i] = sat_round_s24(static_cast<double>(p[i]) * g);
        }
        break;
    }
    case SampleFormat::S24_3LE: {
        auto* p = buf.data();
        const std::size_t n = buf.size() / 3;
        for (std::size_t i = 0; i < n; ++i) {
            const std::int32_t s = read_s24_3le(p + i * 3);
            write_s24_3le(p + i * 3,
                          sat_round_s24(static_cast<double>(s) * g));
        }
        break;
    }
    case SampleFormat::S32_LE: {
        auto* p = reinterpret_cast<std::int32_t*>(buf.data());
        const std::size_t n = buf.size() / sizeof(std::int32_t);
        for (std::size_t i = 0; i < n; ++i) {
            p[i] = sat_round_to_int<std::int32_t>(
                static_cast<double>(p[i]) * g);
        }
        break;
    }
    case SampleFormat::FLOAT_LE: {
        auto* p = reinterpret_cast<float*>(buf.data());
        const std::size_t n = buf.size() / sizeof(float);
        for (std::size_t i = 0; i < n; ++i) {
            // Float keeps its native range; no saturation.
            p[i] = static_cast<float>(static_cast<double>(p[i]) * g);
        }
        break;
    }
    }
}

std::string replaygain_qualification(float linear, RgMode mode) {
    if (linear == 1.0f) {
        return {};
    }
    const float db = 20.0f * std::log10(linear);
    char buf[96];
    const char* m = (mode == RgMode::Track) ? "track"
                  : (mode == RgMode::Album) ? "album" : "off";
    std::snprintf(buf, sizeof(buf),
                  "ReplayGain (%s) %+.2f dB applied — digital scaling, "
                  "bit-perfect path broken by design",
                  m, static_cast<double>(db));
    return buf;
}

} // namespace fidelis::engine
