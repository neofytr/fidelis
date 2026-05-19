# Roadmap

`fidelis` is pre-release. The engine, decoders, library, MPRIS, REST/WebSocket surface, and the web UI are in place. The roadmap below is the path from the current tree to a packaged v1.0.0.

## Locked release scope

- **Targets:** external USB DACs only. Onboard codecs / HDMI outputs are hard-refused at enumeration.
- **Format scope (v1.0):** PCM only. DSD (`.dsf` / `.dff`) returns an explicit "unsupported in this release" error; native DSD + DoP land in 1.1.
- **License:** GPL-3.0-or-later. The vendored sources under `third_party/` are GPL-compatible.

## v1.0.0

| Phase | Goal | Status |
|-------|------|--------|
| P0 | Land in-flight engine/queue/web fixes (gapless advance, stable-id device pinning, web ↔ engine decouple) with regression tests | Done |
| P1 | Rename project / binary / namespace / config path to `fidelis`; one-time migration from the legacy config path | Done |
| P2 | USB-only enumeration enforced at source; updated UI copy | Done |
| P3 | Bearer-token auth: random token on first run, fail-closed when bound non-loopback without one; UI token gate wired | Pending |
| P4 | Audiophile-core features: session/queue persistence, hardened natural-file gapless, opt-in ReplayGain (off; QUALIFIED when on), robust scanner + folder browse + graceful unmounted paths, DSD hard-refuse. CUE deferred to 1.1 (see below). | In progress |
| P5 | Control surfaces: `systemd --user` unit, REST-backed CLI subcommands, MPRIS / media-key polish, mobile-responsive UI | Pending |
| P6 | Quality and release gates: CI build/test matrix, automated bit-perfect loopback test as a CI gate, full docs set, public-repo audit | Pending |
| P7 | Package + release: AUR PKGBUILD + signed source tarball, prebuilt binary tarball + systemd unit installer, Debian `.deb` + Fedora COPR, tag v1.0.0 | Pending |

## Beyond v1.0

- **1.1** — Native ALSA DSD (`DSD_U16/U32_LE/BE`) and DoP fallback; DSD format negotiation in `format_match`; DSD-aware Pipeline view. CUE sheets: sidecar `.cue` parser, virtual queue entries with `(path, start_frame, end_frame)`, and an engine `stop_frame` mechanism so playback is gapless *across* virtual tracks within a single audio file (Dark-Side-of-the-Moon style continuous albums).
- **1.2** — M3U / M3U8 playlists, Last.fm scrobble (opt-in), exportable session bit-perfect log.
- **Backlog (post-1.x)** — Optional libsoxr resampler with QUALIFIED verdict (per-device opt-in), AirPlay / DLNA renderer mode, ALSA loopback as a transparent capture endpoint for offline null tests.

## Quality bar (release-gate)

A release is not cut until:

- `meson test -C build` is green on the CI matrix (no skips other than the documented `needs-alsa` / `needs-loopback` suites).
- The bit-perfect loopback integration test runs at least once against a real loopback DAC.
- Public-repo audit passes — no AI artifacts, full SPDX header coverage, `.gitignore` does not name local-only files.
- README + man page + CHANGELOG describe what is actually in the binary, with no aspirational features.
