// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FIDELIS_ENGINE_REPLAYGAIN_HPP
#define FIDELIS_ENGINE_REPLAYGAIN_HPP

#include <fidelis/engine/decoder.hpp>
#include <fidelis/engine/format.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace fidelis::engine {

// Which RG tag to use as the gain source. Track mode loudness-matches every
// track to a reference; Album mode preserves intra-album dynamics.
enum class RgMode : std::uint8_t { Off, Track, Album };

// Per-track settings the engine consults each time a decoder is loaded. Off
// means no gain is applied and the bit-perfect verdict stays PERFECT. Any
// other mode applies a linear scale on the decoder thread before samples
// enter the ring; the verdict drops to QUALIFIED with the dB amount listed.
struct RgConfig {
    RgMode mode = RgMode::Off;
    bool prevent_clipping = true;   // pre-attenuate when the sample-peak tag
                                    // would push samples past full-scale
};

// Pure: compute the linear scale factor implied by an RG config + the tags
// on the currently-loaded decoder. Returns 1.0f when no usable tag is
// present (so an album with no RG metadata stays bit-perfect, even if RG
// is enabled — there is nothing to apply). NaN inputs are treated as
// "missing". When prevent_clipping is set, the linear factor is reduced
// just enough that `linear * peak < 1.0` (one-bit headroom from full-scale)
// to avoid hard-clipping the integer outputs.
//
// Output range: [0.0, ~3.16] for typical RG values (-10 to +10 dB). Always
// positive and finite.
float compute_replaygain_linear(const Tags& tags, const RgConfig& cfg);

// Pure: convert a dB value (the kind ReplayGain stores) to a linear gain.
// 0 dB -> 1.0; +6 dB -> ~2.0; -6 dB -> ~0.501. NaN passes through as 1.0.
float db_to_linear(float db);

// Apply a linear gain factor to one block of decoded PCM, in place. Output
// is rounded to nearest and saturated to the format's full-scale range —
// the only safe behaviour for integer formats. Floating-point output keeps
// its native range. Caller guarantees `buf.size()` is a multiple of the
// per-sample byte count for `fmt`. linear==1.0f is a no-op fast path.
void apply_gain(std::span<std::byte> buf, SampleFormat fmt, float linear);

// Human-readable note appended to BitPerfectVerdict.qualifications while
// ReplayGain is in effect. Empty when linear == 1.0.
std::string replaygain_qualification(float linear, RgMode mode);

} // namespace fidelis::engine

#endif
