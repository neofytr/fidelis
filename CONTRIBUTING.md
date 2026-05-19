# Contributing to fidelis

Thanks for the interest. fidelis is a small, principled project; the bar for changes is correspondingly principled.

## Before you open an issue

Search closed issues first. For bugs, please include:

- distro + kernel version (`uname -a`),
- ALSA version (`aplay --version`),
- USB DAC vendor/product, and whether PipeWire / PulseAudio is running,
- the relevant section of `journalctl --user -u fidelis` if running via the unit, otherwise the daemon's stderr,
- a screenshot or text dump of the Pipeline page when the bug reproduces — the verdict + per-condition list is usually decisive.

For feature requests, please be honest about whether the feature fits the locked design (`docs/spec/locked.md`). Anything that would break bit-perfect by default — software volume, EQ, system-wide routing, DSP — is out of scope, no matter how convenient.

## Building locally

See [`BUILDING.md`](BUILDING.md). The short version:

```
( cd web && npm ci && npm run build )
meson setup build
ninja -C build
meson test -C build
```

## Coding conventions

Source style is described in [`docs/architecture.md`](docs/architecture.md). The non-negotiable points:

- **C++23.** `std::expected`, `std::span`, concepts. Headers `.hpp`, sources `.cpp`. SPDX `GPL-3.0-or-later` on every source file.
- **Audio-thread invariants.** No allocation, no locks, no syscalls outside `snd_pcm_*`, no exceptions. If you touch `src/engine/alsa/`, double-check.
- **Comments terse and technical.** Why, not what. Present tense, no first-person, no apology / preamble.
- **Tests.** New unit tests use the vendored doctest single-header (`#include <doctest.h>`). Plain-`main()` per-binary tests pre-date doctest and are not being migrated. Pull requests must keep `meson test -C build` green.

## Pull requests

- Small + focused beats big + sprawling. A PR that does one thing reviews fast.
- The first commit message line is a sentence in imperative voice, ≤ 72 chars. Body wraps at 72. Reference issues with `#NNN` when relevant.
- CI must pass on every commit pushed to the PR branch.
- Please don't add any AI-generated commentary, attribution, or trailers — the project's commit history is meant to read like the work of human technical authors.

## Security

If you find a security-affecting bug — a way to bypass the bearer token, drive the daemon into a state that lets a network peer alter audio, etc. — please email the maintainer privately before opening a public issue.
