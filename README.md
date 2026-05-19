# fidelis

A minimal, bit-perfect Linux music player for **external USB DACs**.

The decoder writes the file's samples to ALSA directly. No sound server in the audio path, no resampling, no DSP, no software volume. The DAC is held exclusive while playing; the rate is switched per track. Every stage of the pipeline is visible in the UI so you can audit, in real time, that what you hear is what's on disk.

> Status: pre-release. Working toward a v1.0 with packaging for Arch (AUR), Debian, and Fedora. The protocol on the audio path is locked; the surface is still moving.

## Why

Most Linux audio stacks default to convenience — automatic resampling, software volume, shared mixers. Convenient, but no longer the file. `fidelis` defaults the other way: **bit-exact or refuse**. If the DAC cannot accept the file's native format, playback errors out — it does not silently degrade.

## Design (non-negotiable)

- **Decoder → ALSA `hw:` → DAC.** No PipeWire / PulseAudio / JACK on the audio path.
- **Hard exclusive.** While playing, the active DAC is held outright (kernel `-EBUSY` for any other opener).
- **Auto rate-switch.** The DAC follows the track's native sample rate. Same-rate transitions go through a gapless decoder swap with no PCM close/reopen.
- **Refuse or disclose — never silently degrade.** Format mismatch → hard error. ReplayGain (planned, opt-in) marks the verdict QUALIFIED while applied.
- **Audit-grade transparent.** The Pipeline page shows the live source / decoder / format-match / ring / output / device / realtime state, plus a three-state bit-perfect verdict (`PERFECT` / `QUALIFIED` / `NOT BIT-PERFECT`) with per-condition breakdown.
- **External USB DACs only.** Onboard codecs and HDMI outputs are hard-refused at enumeration — they are not the target.
- **Linux-native.** No Windows, no macOS, no X11/Wayland GUI. The interface is a local web app (Svelte) served by an embedded HTTP server on `localhost:7800`.

## What works today

- ALSA-direct playback on USB DACs with hard-exclusive ownership and per-track rate switching.
- Decoders: WAV, AIFF, FLAC, ALAC (M4A), MP3, Ogg Vorbis, Opus.
- Gapless across same-rate tracks via background preload.
- SCHED_FIFO realtime audio thread (with explicit fallback + disclosure to `SCHED_OTHER` when capabilities are not granted).
- SQLite library + background scanner + FTS5 search.
- MPRIS 2 (`org.mpris.MediaPlayer2.fidelis`) — `playerctl` and most media-key daemons work.
- Web UI on `http://localhost:7800`: Now Playing, Library (album grid + search), Queue (drag to reorder), Pipeline (live telemetry + bit-perfect verdict), Mixer (every alsamixer control on the active DAC), DAC picker.
- USB hotplug — playback pauses on disconnect and resumes on the same fingerprint.

## What is explicitly out of scope (for v1.0)

DSP, EQ, software volume, system-wide routing, multi-zone, plugin systems, DSD (planned for 1.1), playlists (M3U), and any non-USB output.

## Building from source

Dependencies are GPL-3-compatible system libraries. The vendored sources under `third_party/` (Apple ALAC, doctest, cpp-httplib, nlohmann/json, toml++) are not external requirements.

### Arch

```
sudo pacman -S meson ninja pkg-config gcc \
  alsa-lib flac mpg123 libvorbis opus \
  sqlite sdbus-cpp \
  systemd-libs \
  nodejs npm
```

### Debian / Ubuntu

```
sudo apt install meson ninja-build pkg-config g++ \
  libasound2-dev libflac-dev libmpg123-dev libvorbis-dev libopus-dev \
  libsqlite3-dev libsdbus-c++-dev libudev-dev \
  nodejs npm
```

### Build

```
git clone https://github.com/neofytr/fidelis.git
cd fidelis

# Web UI (committed under web/dist/; rebuild only if you edit the frontend)
( cd web && npm ci && npm run build )

# Daemon
meson setup build
ninja -C build
```

Binary at `build/fidelis`.

## Running

```
./build/fidelis
```

then open `http://localhost:7800` in any browser. The first connected USB DAC is picked automatically; switch via the **DAC** button. Configuration lives in `~/.config/fidelis/config.toml`:

```toml
[device]
# Either an explicit hw: string or a stable fingerprint id; the DAC picker
# saves the fingerprint id so the saved preference survives ALSA card-name
# renaming.
preferred = "hw:CARD=SE,DEV=0"

[audio]
# Total ALSA buffer = period_ms * period_count.
period_ms    = 12
period_count = 4
rt_policy    = "auto"      # auto | fifo | other

[library]
roots           = ["/home/user/Music"]
ignore_patterns = [".*", "Samples"]
```

### Keyboard shortcuts (web UI)

| Key       | Action                  |
|-----------|-------------------------|
| Space     | Play / Pause            |
| ← / →     | Seek −10s / +10s        |
| m         | Mute toggle (hw mixer)  |
| − / =     | Volume −3% / +3% (hw)   |

Volume keys drive the DAC's own playback control (e.g. PCM,0 on iFi) via ALSA. There is no software volume.

### Realtime audio

The audio thread requests `SCHED_FIFO` priority 80 with `mlockall`. Grant it without root:

```
sudo gpasswd -a "$USER" audio
echo '@audio  -  rtprio   95'        | sudo tee /etc/security/limits.d/99-fidelis.conf
echo '@audio  -  memlock  unlimited' | sudo tee -a /etc/security/limits.d/99-fidelis.conf
```

Log out and back in. If the request fails, `fidelis` falls back to `SCHED_OTHER` and surfaces that on the Pipeline page.

### Exclusive access vs. PipeWire

`fidelis` opens the DAC with `O_EXCL` semantics — PipeWire / PulseAudio cannot share. To stop PipeWire from claiming your DAC, add a udev hint (replace VID/PID for your device):

```
# /etc/udev/rules.d/90-fidelis-exclusive.rules
SUBSYSTEM=="sound", ATTRS{idVendor}=="2fc6", ATTRS{idProduct}=="f082", \
    ENV{ACP_IGNORE}="1", ENV{PULSE_IGNORE}="1"
```

```
sudo udevadm control --reload && sudo udevadm trigger
```

## Tests

```
meson test -C build
```

A subset is gated on `--suite needs-alsa` (hardware) and `--suite needs-loopback` (a loopback-capable DAC for bit-exact verification).

## License

GPL-3.0-or-later. See [`LICENSE`](LICENSE).
