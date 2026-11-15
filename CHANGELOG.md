# v1.1.2
## New in 1.1.2

- Coalesce object-load notifications instead of posting a task for every loaded object.
- Scan one attached cell per update, including initial discovery. Schedule incremental fallback discovery every 60 seconds rather than scanning all loaded references every 10 seconds.
- Spread plant checks into up to 32 checks and two accepted regrowth requests every 250ms, preserving the old nominal maximum throughput. Milk retains its two-second interval.
- Suppress duplicate pending regrowth requests, release rejected/completed requests, and expire missing completions after ten seconds.
- Discard tasks from previous save sessions and reduce routine per-plant logging. Existing timer deadlines, RNG rules, ESP/scripts, INI format and co-save formats are unchanged.

Validation: native build and loader checks plus 3,897 timer boundary checks passed. Visual regrowth, runtime/save-load behavior and measured frame-time improvement still require gameplay testing. Discovery is gradual after loading; unusually dense individual cells can still be costly.

Update the existing mod in Vortex, retain your existing INI settings, and restart Skyrim. No new save or save cleaner is needed.

# v1.1.1
- Default Crimson Nirnroot regrowth to off, including MCM reset-to-default.
- Regular Nirnroot remains on; existing saved preferences are preserved.

# v1.1.0
- Optional Immersive Interactions 1.78 cow-milking adapter, automatically detected without a hard dependency.
- Uses universal random cooldown, preserves its animations/rewards and clears only cow entries from the shared used list.
- One-time per-save restoration for already-milked cows; bootstrap state and pending milk timers survive saves.
- Explicit scripted vanilla Nirnroot and Crimson Nirnroot support, including disabled picked references, spent plant/glow cleanup and vanilla script state restoration.
- Separate regrowth toggles and custom 1-30-day sliders per Nirnroot type; disabled custom timing uses the universal range.
- Organized MCM pages: General, Nirnroot, Compatibility.

# v1.0.1
- Retry loaded-grid discovery and track attached-cell plants without requiring a finished model.
- Replace player Update hook with bounded native task scheduling.
- Add live plant inspection and timer/dispatch/heartbeat diagnostics.

# v1.0.0
- Random 3–5-day flora regrowth, MCM 1–30-day bounds, loaded picked-plant discovery and SKSE co-save persistence.
- Light ESP and minimal gameplay-only install archive.
