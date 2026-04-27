<p align="center">
  <img src="docs/wsjtz_icon.png" alt="WSJT-Z" width="160">
</p>

<h1 align="center">WSJT-Z</h1>

<p align="center">
  A modified WSJT-X with extended automation, advanced filtering, and an enhanced decoder pipeline.
</p>

<p align="center">
  <a href="https://github.com/sq9fve/wsjt-z/releases"><img src="https://img.shields.io/github/v/release/sq9fve/wsjt-z?include_prereleases&label=release" alt="Latest release"></a>
  <a href="https://www.gnu.org/licenses/gpl-3.0.txt"><img src="https://img.shields.io/badge/license-GPL--3.0-blue" alt="License: GPL-3.0"></a>
  <a href="https://groups.io/g/WSJT-Z/topics"><img src="https://img.shields.io/badge/discuss-groups.io-orange" alt="groups.io"></a>
</p>

---

WSJT-Z is a fork of [WSJT-X](https://wsjt.sourceforge.io/wsjtx.html) by **Joe Taylor K1JT** and the WSJT Development Group. Initially developed as an automation project, WSJT-Z now focuses on extending the operating workflow with deeper filtering, smarter automation, and a more responsive UI — while keeping full compatibility with the upstream WSJT-X protocols.

WSJT-Z supports **FT8**, **FT4**, **FT2**, **JT4 / JT9 / JT65**, **Q65**, **MSK144**, **WSPR**, **Echo** and **FreqCal**.

> ⚠️ **Always monitor your transceiver while using WSJT-Z**, unless unattended/automated operation is explicitly permitted by the regulations in your country.

---

## Discussion & support

- **Groups.io (questions, comments, bug reports):** https://groups.io/g/WSJT-Z/topics
- **Issue tracker:** https://github.com/sq9fve/wsjt-z/issues
- **Changelog:** see [`changelog.txt`](changelog.txt) and the GitHub [Releases](https://github.com/sq9fve/wsjt-z/releases) page

## Download

Pre-built Windows installers and release notes are published on the GitHub **Releases** page:

  → **https://github.com/sq9fve/wsjt-z/releases**

(Builds for other platforms are not currently provided; build from source — see below.)

## Features

### Operating workflow
- **Auto CQ** — call CQ unattended with configurable repeat count and band-rotation rules
- **Auto Call** — automatically respond to filtered stations until a daily/QSO limit is reached
- **Pounce mode** — lock onto a specific call until the QSO completes
- **Priority call queue** — promote callsigns of interest to the front of the call list
- **Auto Call Next** — chain QSOs without operator intervention
- **Band-hopper** — schedule automatic band changes by time-of-day or activity

### Filtering & highlighting
- **Ignored stations list** — silence specific calls or prefixes
- **Prefix / state / continent / CQ-target filters** — include or exclude by call prefix, US state, continent, or directed-CQ target
- **DXCC / Continent / CQ Zone / ITU Zone** worked-before alerts (per-band variants supported)
- **New on band / new in mode / new for the year** highlighting
- **LoTW user filter** — restrict to stations active on Logbook of the World
- **Signal-strength threshold** filter

### Decoder pipeline
- **Multi-threaded FT8 decoder** (selectable: Auto, 1–12 threads) — substantial speed-ups on multi-core systems
- **JTDX-derived FT8 enhancements** — additional decoding passes, OSD on `ndepth=2`, lowered sync thresholds for weak signals
- **Early-decode dedup** — eliminates duplicate decodes within a single FT8 cycle in multi-threaded mode
- **Performance work in the C++ hot path** — cached filter lists, reduced regex compilation, gated debug logging, tuned Fortran release flags

### Integration & UX
- **QRZ.com lookup panel** — name, address, grid, biography
- **Audio alerts** — configurable per-event sounds
- **PSK Reporter integration**
- **Larger Band Activity window** with improved column layout
- **Custom alert rules**
- **NA_VHF / EU_VHF contest flows** with associated UI tweaks
- **Modernized highlighting model** with per-band overrides

## Tools

The `tools/` directory contains helper scripts for project maintenance:

- `tools/rebuild_usstate_db.py` — rebuilds `USState.db` (callsign → US state mapping used for state filtering) from the weekly FCC ULS amateur licensee dump. Stdlib-only Python; run with `--help` for options.

## License

Licensed under the **GNU General Public License v3** — see [`COPYING`](COPYING) and https://www.gnu.org/licenses/gpl-3.0.txt.

The vast majority of the code is created and copyrighted by the WSJT Development Group, led by Joe Taylor K1JT, and licensed under the same terms. WSJT-Z extensions follow the same license.

## Credits

- **WSJT Development Group** — Joe Taylor K1JT and contributors — for WSJT-X, the upstream project this fork is built on
- **JTDX team** — for the multi-threaded FT8 decoder enhancements that informed the WSJT-Z decoder pipeline
- **JTSDK** — for the Windows build environment
- **WSJT-Z contributors** — see the commit log
