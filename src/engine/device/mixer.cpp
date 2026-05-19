// SPDX-License-Identifier: GPL-3.0-or-later
//
// Full simple-mixer enumeration. Unlike probe.cpp's single preferred volume
// element, this walks every selem on the card so the UI can present the same
// set of knobs alsamixer does. Operates on the card-level control device,
// independent of the exclusively-held PCM.

#include <fidelis/engine/mixer.hpp>

#include "device_internal.hpp"

#include <alsa/asoundlib.h>

#include <array>
#include <string>
#include <vector>

namespace fidelis::engine {

namespace {

using detail::MixerHandle;

// Playback channel ids in render order. SND_MIXER_SCHN_MONO aliases
// FRONT_LEFT in ALSA, so a mono element surfaces as a single entry.
constexpr std::array<snd_mixer_selem_channel_id_t, 8> kChannels = {
    SND_MIXER_SCHN_FRONT_LEFT,   SND_MIXER_SCHN_FRONT_RIGHT,
    SND_MIXER_SCHN_REAR_LEFT,    SND_MIXER_SCHN_REAR_RIGHT,
    SND_MIXER_SCHN_FRONT_CENTER, SND_MIXER_SCHN_WOOFER,
    SND_MIXER_SCHN_SIDE_LEFT,    SND_MIXER_SCHN_SIDE_RIGHT,
};

int card_index_from_hw(const std::string& alsa_hw_string) {
    auto parsed = detail::parse_hw_string(alsa_hw_string);
    return parsed ? parsed->card_index : -1;
}

// Open + load the simple mixer for "hw:N". Caller keeps the handle alive
// while touching any element pointer obtained from it.
MixerHandle open_card_mixer(int card_index) {
    snd_mixer_t* raw = nullptr;
    if (snd_mixer_open(&raw, 0) < 0) {
        return {};
    }
    MixerHandle mix{raw};
    const std::string ctl = "hw:" + std::to_string(card_index);
    if (snd_mixer_attach(mix.get(), ctl.c_str()) < 0) {
        return {};
    }
    if (snd_mixer_selem_register(mix.get(), nullptr, nullptr) < 0) {
        return {};
    }
    if (snd_mixer_load(mix.get()) < 0) {
        return {};
    }
    return mix;
}

snd_mixer_elem_t* find_selem(snd_mixer_t* mix, const std::string& name,
                             unsigned int index) {
    snd_mixer_selem_id_t* sid = nullptr;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, index);
    snd_mixer_selem_id_set_name(sid, name.c_str());
    return snd_mixer_find_selem(mix, sid);
}

int raw_to_pct(long v, long lo, long hi) {
    if (hi <= lo) {
        return 0;
    }
    const long p = 100L * (v - lo) / (hi - lo);
    return p < 0 ? 0 : (p > 100 ? 100 : static_cast<int>(p));
}

} // namespace

std::vector<MixerControl>
list_mixer_controls(const std::string& alsa_hw_string) {
    std::vector<MixerControl> out;
    const int ci = card_index_from_hw(alsa_hw_string);
    if (ci < 0) {
        return out;
    }
    MixerHandle mix = open_card_mixer(ci);
    if (!mix) {
        return out;
    }

    for (snd_mixer_elem_t* elem = snd_mixer_first_elem(mix.get());
         elem != nullptr; elem = snd_mixer_elem_next(elem)) {
        if (snd_mixer_selem_is_active(elem) == 0) {
            continue;
        }
        const bool has_vol = snd_mixer_selem_has_playback_volume(elem) != 0;
        const bool has_sw  = snd_mixer_selem_has_playback_switch(elem) != 0;
        const bool is_enum = snd_mixer_selem_is_enumerated(elem) != 0;
        if (!has_vol && !has_sw && !is_enum) {
            continue;  // capture-only or otherwise irrelevant to playback
        }

        MixerControl mc;
        snd_mixer_selem_id_t* sid = nullptr;
        snd_mixer_selem_id_alloca(&sid);
        snd_mixer_selem_get_id(elem, sid);
        if (const char* nm = snd_mixer_selem_id_get_name(sid)) {
            mc.name = nm;
        }
        mc.index = snd_mixer_selem_id_get_index(sid);

        if (has_vol) {
            mc.has_volume = true;
            long lo = 0, hi = 0;
            snd_mixer_selem_get_playback_volume_range(elem, &lo, &hi);
            mc.volume_min = lo;
            mc.volume_max = hi;
            long dlo = 0, dhi = 0;
            if (snd_mixer_selem_get_playback_dB_range(elem, &dlo, &dhi) == 0) {
                mc.has_db = true;
                mc.db_min_x100 = dlo;
                mc.db_max_x100 = dhi;
            }
            for (auto ch : kChannels) {
                if (snd_mixer_selem_has_playback_channel(elem, ch) != 1) {
                    continue;
                }
                long cur = lo;
                snd_mixer_selem_get_playback_volume(elem, ch, &cur);
                mc.channel_pct.push_back(raw_to_pct(cur, lo, hi));
            }
        }

        if (has_sw) {
            mc.has_switch = true;
            for (auto ch : kChannels) {
                if (snd_mixer_selem_has_playback_channel(elem, ch) != 1) {
                    continue;
                }
                int sw = 1;
                snd_mixer_selem_get_playback_switch(elem, ch, &sw);
                mc.channel_switch.push_back(sw);
            }
        }

        if (is_enum) {
            mc.is_enum = true;
            const int n = snd_mixer_selem_get_enum_items(elem);
            for (int i = 0; i < n; ++i) {
                char buf[64] = {0};
                if (snd_mixer_selem_get_enum_item_name(
                        elem, static_cast<unsigned int>(i), sizeof(buf) - 1,
                        buf) == 0) {
                    mc.enum_items.emplace_back(buf);
                }
            }
            unsigned int idx = 0;
            if (snd_mixer_selem_get_enum_item(elem, SND_MIXER_SCHN_FRONT_LEFT,
                                              &idx) == 0) {
                mc.enum_current = static_cast<int>(idx);
            }
        }

        out.push_back(std::move(mc));
    }

    return out;
}

bool set_mixer_volume_pct(const std::string& alsa_hw_string,
                          const std::string& name, unsigned int index,
                          int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    const int ci = card_index_from_hw(alsa_hw_string);
    if (ci < 0) {
        return false;
    }
    MixerHandle mix = open_card_mixer(ci);
    if (!mix) {
        return false;
    }
    snd_mixer_elem_t* elem = find_selem(mix.get(), name, index);
    if (elem == nullptr ||
        snd_mixer_selem_has_playback_volume(elem) == 0) {
        return false;
    }
    long lo = 0, hi = 0;
    snd_mixer_selem_get_playback_volume_range(elem, &lo, &hi);
    const long target =
        lo + static_cast<long>(pct) * (hi - lo) / 100L;
    return snd_mixer_selem_set_playback_volume_all(elem, target) == 0;
}

bool set_mixer_switch(const std::string& alsa_hw_string,
                      const std::string& name, unsigned int index, bool on) {
    const int ci = card_index_from_hw(alsa_hw_string);
    if (ci < 0) {
        return false;
    }
    MixerHandle mix = open_card_mixer(ci);
    if (!mix) {
        return false;
    }
    snd_mixer_elem_t* elem = find_selem(mix.get(), name, index);
    if (elem == nullptr ||
        snd_mixer_selem_has_playback_switch(elem) == 0) {
        return false;
    }
    return snd_mixer_selem_set_playback_switch_all(elem, on ? 1 : 0) == 0;
}

bool set_mixer_enum(const std::string& alsa_hw_string, const std::string& name,
                    unsigned int index, int item) {
    if (item < 0) {
        return false;
    }
    const int ci = card_index_from_hw(alsa_hw_string);
    if (ci < 0) {
        return false;
    }
    MixerHandle mix = open_card_mixer(ci);
    if (!mix) {
        return false;
    }
    snd_mixer_elem_t* elem = find_selem(mix.get(), name, index);
    if (elem == nullptr || snd_mixer_selem_is_enumerated(elem) == 0) {
        return false;
    }
    return snd_mixer_selem_set_enum_item(elem, SND_MIXER_SCHN_FRONT_LEFT,
                                         static_cast<unsigned int>(item)) == 0;
}

} // namespace fidelis::engine
