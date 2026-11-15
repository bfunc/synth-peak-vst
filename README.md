# Peak Control

An editor for the Novation Peak (firmware 2.1): a VST3 plugin plus a standalone app for Windows.
For Novation Peak; not affiliated with or endorsed by Novation / Focusrite.

Status: **core only.** The parameter table, CC / NRPN coding, patch SysEx and the send queue are written and tested.
No plugin or UI yet. Plan: [docs/PLAN.md](docs/PLAN.md). Research notes: [docs/RESEARCH.md](docs/RESEARCH.md).
UI preview: [docs/panel-mockup.html](docs/panel-mockup.html).

## Layout

| Path | What |
|---|---|
| `src/core/ParamTable.*` | Every patch parameter: CC / NRPN, range, Init default, value names, byte in the patch |
| `src/core/Midi.*` | CC, 0–255 CC pairs, NRPN: encode, and decode of what the synth sends |
| `src/core/Sysex.*` | Requests, the 527-byte patch message, name, values in the 510 data bytes |
| `src/core/SendQueue.*` | Throttles live sends (per parameter and in total) |
| `tests/CoreTests.cpp` | Core tests, incl. decoding a real dump from `docs/dumps` |

## Tests

No local compiler needed:

```sh
sh tools/test-docker.sh      # gcc in Docker
```

Or with CMake: `cmake -B build && cmake --build build && ctest --test-dir build`. GitHub Actions runs them on Windows.

## What is still provisional

Read off the synth in the listening session (PLAN.md step 1):

- Patch byte offsets: only name, category / genre, Voice (32–39), the mod and FX matrices and the NRPN-MSB-25
  block are mapped. Everything else has no offset yet, so it can be sent live but not read from a patch.
- The 0–255 CC pair coding (MSB = value >> 1, LSB = (value & 1) << 6) is taken from the patch format, not tested live.
- Value list order for mod sources / destinations, FX matrix, wavetables, category / genre, delay sync.

## Licence

AGPL-3.0, see [LICENSE](LICENSE).
