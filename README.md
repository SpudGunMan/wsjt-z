# WSJT-Z

WSJT-X fork with JTDX decoder enhancements and WSJT-Z extensions by SQ9FVE.

Based on WSJT-X 3.0.0 + JTDX FT8 multi-thread decoder + WSJT-Z UI/filter additions.

## Features beyond upstream WSJT-X
- Multi-thread FT8 decoder (Decode → Number of FT8 threads, Auto or 1–12)
- Auto CQ / Auto Call, pounce mode, priority call queue
- Advanced filters: ignored stations, prefix/state/continent/CQ-target, new-on-band
- Before-worked alerts (CQ Zone / ITU Zone / grid / continent / country, per-band variants)
- QRZ callsign lookup panel
- Band-hopper, custom alerts, NA_VHF/EU_VHF contest flows

## Build
Windows via JTSDK:
- **64-bit:** `E:\JTSDK64-Tools` → `jtsdk64.cmd` → `jtbuild rinstall` (or `jtbuild package`)
- **32-bit:** `E:\JTSDK-Tools` → `jtsdk-env.cmd` → `jtbuild package`

Source path is read from `tmp/build.txt` (`SRCD`). Build artifacts land in `E:\JTSDK-Build\output\_\build{32,64}`.

## Version
`3.0.0-2.0.1` — WSJT-X 3.0.0 base, WSJT-Z mod v2.0.1.

See `CLAUDE.md` for development notes and the `NEWS` file for upstream changelog.

## License
GPL-3 (inherited from WSJT-X).
