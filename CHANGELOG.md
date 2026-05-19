# Changelog

All notable changes are recorded here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] — 2026-05-19

First public release.

#### Added

- **Web UI on `localhost:7800`.** Svelte 5 + Vite + Tailwind, embedded HTTP server (cpp-httplib). Now Playing, Library (album grid + FTS search), Queue (drag-reorder), Pipeline (live telemetry + three-state bit-perfect verdict), Mixer (every alsamixer control on the active DAC), DAC picker, folder browse.
- **Bit-perfect verdict** with per-condition breakdown on the Pipeline page: format match, no resample, no DSP, RT scheduling, hardware mixer state. PERFECT / QUALIFIED / NOT BIT-PERFECT.
- **USB DAC enumeration** with stable fingerprint id; the saved preference survives ALSA card renaming (an iFi that flips between SE / UAC1 when its USB descriptor reads garbled is correctly resolved to its current hw string).
- **Web ↔ engine decouple.** The HTTP server stays up even when no DAC can be opened so the device picker is always reachable.
- **Session / queue persistence.** Tracks + index + position survive a daemon restart; playback never auto-resumes — the daemon comes back paused.
- **Folder browse REST** (`/api/fs`, `/api/queue/append-folder`) — append from any directory; dot-dirs hidden; audio-extension prefilter.
- **Bearer-token auth.** Random 256-bit token generated on first non-loopback bind, persisted to config, refused to start when an empty token is paired with a routable bind address.
- **ReplayGain (opt-in)** — album / track, peak-aware clip prevention; applied on the decoder thread before the ring; drops the verdict to QUALIFIED with the dB amount surfaced.
- **systemd user unit** so the daemon survives terminal exits.
- **`fidelis ctl <verb>`** CLI client (status / play / pause / toggle / next / prev / enqueue / clear) talking to the running daemon over REST.

#### Changed

- **Renamed** from `transporter` to `fidelis`. Binary, namespace, MPRIS name (`org.mpris.MediaPlayer2.fidelis`), config dir (`~/.config/fidelis/`), include path (`<fidelis/...>`). One-time migration from the old `~/.config/transporter` config preserves device + library preferences.
- **USB-only device policy.** Onboard codecs and HDMI outputs are hard-refused at enumeration. `pick_device` resolves the saved preference against the filtered list; if absent, the first USB DAC wins and the swap is disclosed on stderr.
- **DSD files** (`.dsf` / `.dff`) are explicitly refused with `FormatNotSupported`. Native DSD + DoP are planned for 1.1.
- **CUE sheets** deferred to 1.1 (single-file albums play as one long track in v1.0; the engine work for sample-accurate gapless across virtual tracks within a CUE-split album lands with DSD).
- **Library scanner** no longer wipes tracks under a temporarily-unmounted root. The deletion sweep only considers paths under roots that the current scan actually walked.

#### Fixed

- Queue gapless-advance regression (the second song repeated forever because the queue never caught up to the engine's staged-track swap).
- `kill(getpid(), SIGTERM)` instead of `raise(SIGTERM)` so the main thread's `sigsuspend` actually wakes on shutdown.
- Cover art on Now Playing resolves via `/api/art/current` off the engine's live source path; works for files outside the library.

#### Quality

- 34 unit + integration tests covering the bit-perfect, queue, device-pinning, session-persistence, scanner-protection, folder-browse, replaygain, web-security, DSD-refusal, and format-match paths. CI runs the hardware-free subset on every push.

[1.0.0]: https://github.com/neofytr/fidelis/releases/tag/v1.0.0
