# Architecture

Runtime design of `fidelis`. Complements the locked spec under `docs/spec/locked.md`.

## High-level shape

`fidelis` is a single binary built from a small set of cohesive modules. The audio engine has zero dependency on any UI module.

```
                          ┌──────────────┐
                          │    main()    │
                          │  arg parse,  │
                          │  wire-up     │
                          └──────┬───────┘
                  ┌──────────────┼──────────────┬─────────────┐
                  ▼              ▼              ▼             ▼
            ┌──────────┐   ┌──────────┐   ┌──────────┐  ┌────────┐
            │  config  │   │ library  │   │   web    │  │  dbus  │
            │  (toml)  │   │ (sqlite) │   │ (httplib │  │ (MPRIS)│
            └────┬─────┘   └────┬─────┘   │  + WS)   │  └────┬───┘
                 │              │         └────┬─────┘       │
                 │              └──────┬───────┘             │
                 │                     ▼                     │
                 │              ┌─────────────┐              │
                 └─────────────►│    queue    │◄─────────────┘
                                └──────┬──────┘
                                       ▼
                                ┌─────────────┐
                                │   engine    │
                                │  decoder →  │
                                │  format     │
                                │  match →    │
                                │  ring →     │
                                │  ALSA hw:   │
                                └──────┬──────┘
                                       │
                                       ▼
                                  ┌─────────┐
                                  │ USB DAC │
                                  └─────────┘
```

`engine/` is the only module on the audio path. Everything else is control or presentation.

## Thread model

| Thread          | Schedule                          | Count | Role                                                              |
|-----------------|-----------------------------------|-------|-------------------------------------------------------------------|
| Main            | `SCHED_OTHER`                     | 1     | Boot, signal wait (`sigsuspend`), teardown.                       |
| ALSA writer     | `SCHED_FIFO p80` + `mlockall`     | 1     | Pulls from the lock-free ring, writes to `snd_pcm`. No alloc, no locks, no stdio. |
| Decoder         | `SCHED_OTHER`                     | 1     | Decodes the current source, pushes into the ring.                 |
| Hotplug monitor | `SCHED_OTHER`                     | 1     | libudev netlink reader; emits `Disconnected` / `Returned` events. |
| Web listener    | `SCHED_OTHER`                     | 1     | cpp-httplib `listen()`.                                           |
| Web workers     | `SCHED_OTHER`                     | per-request | One per accepted HTTP / WS handler (httplib).               |
| Telemetry push  | `SCHED_OTHER`                     | 1     | 10 Hz fan-out of `pipeline_snapshot()` to WebSocket clients.      |
| Scanner         | `SCHED_OTHER`, default nice       | 1     | Library walk → SQLite insert.                                     |
| DBus            | `SCHED_OTHER`                     | 1+    | sdbus-c++ event loop for MPRIS.                                   |

Audio thread invariants: no allocation, no locks, no syscalls outside `snd_pcm_*`, no exceptions.

## Engine API

Public headers under `include/fidelis/engine/`. Consumers see the engine through this surface only.

- `Engine::create(EngineConfig)` — open the DAC, claim it exclusive, start the writer thread.
- `Engine::load(path)` / `Engine::preload(path)` — decoder swap. `preload()` stages the next source so a same-rate transition completes without closing the PCM.
- `Engine::play()` / `pause()` / `stop()` / `seek(frame)`.
- `Engine::state()` — `Idle | Loading | Playing | Paused | Stopped | Error | Disconnected`.
- `Engine::set_event_callback(fn)` — `TrackLoaded`, `TrackEnded`, `Disconnected`, `Returned`, `StateChanged`, `Error`.
- `Engine::pipeline_snapshot()` — read-only audit struct: source / decoder / format-match / ring / output / device / realtime + a three-state `BitPerfectVerdict` with per-condition reasons. Drives the Pipeline page.

## REST + WebSocket surface

Served by the embedded cpp-httplib server (`src/web/`). Bind is `0.0.0.0:7800` by default; auth is bearer token from config.

| Method | Path                                | Purpose                                         |
|--------|-------------------------------------|-------------------------------------------------|
| GET    | `/api/state`                        | Player state + current track summary            |
| POST   | `/api/play` `/api/pause` `/api/seek`| Transport                                       |
| POST   | `/api/load`                         | Load a path (jumps if already queued)           |
| GET    | `/api/queue`                        | Queue + current index                           |
| POST   | `/api/queue/{append,remove,reorder,clear,jump}` | Queue mutation                      |
| GET    | `/api/library/albums`               | Album list                                      |
| GET    | `/api/library/tracks?album_id=N`    | Album tracks                                    |
| GET    | `/api/library/search?q=...`         | FTS5 search                                     |
| GET    | `/api/art/{track_id}`               | Cover art for a library track                   |
| GET    | `/api/art/album/{album_id}`         | Cover art for an album                          |
| GET    | `/api/art/current`                  | Cover art for whatever the engine has loaded    |
| GET    | `/api/devices`                      | USB DACs only — non-USB devices are filtered    |
| POST   | `/api/devices/select`               | Persist a DAC preference and exit (manager restarts) |
| GET    | `/api/mixer`                        | Every ALSA simple-mixer control on the active card |
| POST   | `/api/mixer/set`                    | Drive volume / switch / enum on a control       |
| WS     | `/api/snapshot`                     | `pipeline_snapshot()` JSON, pushed at 10 Hz     |

The web server takes nullable `engine*` / `queue*` pointers: when the daemon comes up with no usable DAC (absent, busy), the engine is null but the REST surface still serves — the device picker stays reachable so the user can recover.

## Module map

| Path                  | Role                                                                 |
|-----------------------|----------------------------------------------------------------------|
| `src/engine/`         | FSM, ALSA output, decoders, format match, RT scheduling, hotplug, telemetry |
| `src/queue/`          | Playback queue. Drives `engine.preload()` for gapless                |
| `src/library/`        | SQLite schema, scanner, FTS5 search                                  |
| `src/dbus/`           | MPRIS 2 + a small custom interface                                   |
| `src/hotplug/`        | libudev monitor (compiled into the engine static lib)                |
| `src/config/`         | TOML config loader                                                   |
| `src/web/`            | cpp-httplib HTTP + WebSocket; static asset serving                   |
| `web/`                | Svelte 5 + Vite + TypeScript + Tailwind frontend                     |
| `include/fidelis/`    | Public headers                                                       |
| `third_party/`        | Vendored: doctest, toml++, cpp-httplib, nlohmann/json, Apple ALAC    |

## USB-only device policy

Enumeration filters every non-USB device at the source (`engine::usb_only`, applied inside `list_playback_devices`). Onboard codecs / HDMI outputs are hard-refused project-wide. The startup picker resolves the saved preference (an `hw:` string or a stable fingerprint id) against the filtered list; if the pinned device is absent, the first USB DAC wins and the swap is disclosed on stderr.

## Rate switching and gapless

The DAC is opened at the source rate. On a same-rate transition the engine swaps decoders without closing the PCM (`preload()` path) — sample-accurate gapless. On a different rate, the PCM is drained, closed, and reopened at the new rate; this is a soft gap of a few buffer drains.

## Bit-perfect verdict

The Pipeline page renders a three-state pill from `pipeline_snapshot().bit_perfect`:

- `PERFECT` — file format matched, no resample, no DSP, no software-volume scaling, ALSA `hw:` opened with our requested format.
- `QUALIFIED` — playing, but a known-and-disclosed condition applies (e.g. ReplayGain enabled, hardware volume below max). Each condition is listed.
- `NOT BIT-PERFECT` — verdict failure (format mismatch fallback path; should not occur with default settings).

## Public source-tree layout

```
include/fidelis/        # Public headers
src/                    # Implementation, by module
web/                    # Svelte UI (web/dist is committed; binary serves it)
tests/{unit,integration,support}
docs/                   # This file, phases.md, spec/
third_party/            # Vendored sources only
packaging/              # systemd unit, limits files
fixtures/               # Generated test audio (regenerable via _make_*.sh)
```

## Conventions

C++23 (`std::expected`, `std::span`, concepts). Public headers `.hpp`, sources `.cpp`. SPDX `GPL-3.0-or-later` in every source file. Comments terse and technical: WHY, not WHAT.

New unit tests use the vendored doctest single-header. Run the full suite with `meson test -C build`; pre-PR work should keep it green. A subset is gated on real hardware (`--suite needs-alsa`, `--suite needs-loopback`).
