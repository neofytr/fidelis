# Open questions

The design is largely settled. What's listed below is what remains.

## Resolved (kept for context)

- **Bit-perfect indicator UX.** Three-state `PERFECT / QUALIFIED / NOT BIT-PERFECT` with per-condition breakdown. Lives on the Pipeline page.
- **DAC capability cache.** Re-probe on every device-open. No on-disk cache.
- **First-run UX.** Empty page with prominent "Select DAC" / "Add library root" prompts.
- **Configuration scope.** Minimal — only keys users want to override.
- **UI surface.** Web app served from the daemon on `localhost:7800`.
- **Test framework.** doctest (single-header, vendored at `third_party/doctest/`).
- **DBus library.** sdbus-c++.

## Open

- **Period-count scaling across rates.** Currently fixed at 4 × 12 ms regardless of rate. "Constant wall-clock latency" suggests scaling the period frames per rate; exact formula (round to power-of-two, snap to DAC's preferred period, …) to be decided when xrun stress tests land in CI.
- **Logging / diagnostics format.** Plain text vs structured (JSON). Probably structured once a CLI ships.
- **CUE-sheet matcher.** Per-file `.cue` vs `cuesheet=` in FLAC tags vs both. Decided in P4.
