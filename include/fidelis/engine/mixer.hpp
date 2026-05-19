// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FIDELIS_ENGINE_MIXER_HPP
#define FIDELIS_ENGINE_MIXER_HPP

#include <string>
#include <vector>

namespace fidelis::engine {

// One simple-mixer element as exposed by alsamixer. A control may carry a
// volume, a mute switch, and/or an enumerated selector simultaneously
// (rare, but the ALSA model allows it), so the flags are independent.
struct MixerControl {
    std::string name;          // selem name, e.g. "PCM"
    unsigned int index = 0;    // selem index; >0 disambiguates duplicates

    bool has_volume = false;
    long volume_min = 0;       // raw scale
    long volume_max = 0;
    bool has_db = false;
    long db_min_x100 = 0;      // hundredths of a dB
    long db_max_x100 = 0;
    // Per-channel current volume as 0..100 percent of the raw range. One
    // entry per active playback channel (mono => single entry).
    std::vector<int> channel_pct;

    bool has_switch = false;
    // Per-channel switch: 1 = on, 0 = muted. ALSA semantics.
    std::vector<int> channel_switch;

    bool is_enum = false;
    std::vector<std::string> enum_items;
    int enum_current = -1;
};

// Enumerate every playback-relevant simple-mixer control on the card that
// backs alsa_hw_string ("hw:CARD=<id>,DEV=<n>"). The control device is
// separate from the PCM, so this is safe while the PCM is held exclusively.
// Returns an empty vector when the card has no mixer or cannot be opened.
std::vector<MixerControl> list_mixer_controls(const std::string& alsa_hw_string);

// Mutators. index selects among duplicate-named elements. pct is clamped to
// [0,100] and applied to every channel. Return false on lookup/open failure.
bool set_mixer_volume_pct(const std::string& alsa_hw_string,
                          const std::string& name, unsigned int index,
                          int pct);
bool set_mixer_switch(const std::string& alsa_hw_string,
                      const std::string& name, unsigned int index, bool on);
bool set_mixer_enum(const std::string& alsa_hw_string, const std::string& name,
                    unsigned int index, int item);

} // namespace fidelis::engine

#endif
