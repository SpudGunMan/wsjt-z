#!/usr/bin/env python3
"""
rebuild_usstate_db.py — Rebuild WSJT-Z's USState.db from FCC ULS data.

USState.db maps US amateur callsigns to their licensee state. WSJT-Z reads it
via logbook/usstatedb.cpp::getState() to drive the state-based filters and
highlighting in the Band Activity / Rx Frequency windows.

The authoritative source is the FCC Universal Licensing System (ULS) Amateur
Radio licensee database, refreshed weekly:

    https://data.fcc.gov/download/pub/uls/complete/l_amat.zip   (~200 MB)

This script does the full refresh end-to-end:
  1. downloads l_amat.zip (unless --skip-download)
  2. unzips HD.dat + EN.dat into the work directory
  3. parses HD.dat to find ACTIVE, unexpired licenses
  4. parses EN.dat for those licensees and emits (callsign, state) rows
     keeping only valid 2-letter USPS codes (50 states + DC + PR/VI/GU/AS/MP
     + AA/AE/AP military APO/FPO)
  5. writes a fresh SQLite database with the schema WSJT-Z expects:
         CREATE TABLE USState ( call TEXT, state TEXT )
         CREATE INDEX idx_call ON USState(call)
  6. (optional) installs it over the repo's USState.db, keeping a .bak

The default work directory is a sibling of the repo so the multi-hundred-MB
download doesn't clutter the source tree:

    <repo>/..  (e.g. E:\\JTSDK-Build\\)
        wsjtz/                <-- the repo
        _uls_scratch/         <-- created automatically; safe to delete

Typical use (from anywhere):

    python3 tools/rebuild_usstate_db.py            # download + rebuild + install
    python3 tools/rebuild_usstate_db.py --dry-run  # build but don't replace USState.db
    python3 tools/rebuild_usstate_db.py --skip-download  # reuse existing l_amat.zip

Requires: Python 3.7+ (stdlib only — no pip dependencies).

License: GPL-3 (matches WSJT-Z).
"""
import argparse
import datetime
import os
import shutil
import sqlite3
import sys
import urllib.request
import zipfile

ULS_URL = "https://data.fcc.gov/download/pub/uls/complete/l_amat.zip"

VALID_STATES = {
    # 50 states
    "AL","AK","AZ","AR","CA","CO","CT","DE","FL","GA","HI","ID","IL","IN","IA",
    "KS","KY","LA","ME","MD","MA","MI","MN","MS","MO","MT","NE","NV","NH","NJ",
    "NM","NY","NC","ND","OH","OK","OR","PA","RI","SC","SD","TN","TX","UT","VT",
    "VA","WA","WV","WI","WY",
    # DC + territories
    "DC","PR","VI","GU","AS","MP",
    # Military APO/FPO
    "AA","AE","AP",
}

# FCC ULS pipe-delimited record formats (1-based field numbers shown):
#   HD.dat  ("Application/License Header")
#       1  record_type ("HD")
#       2  unique_system_identifier
#       5  call_sign
#       6  license_status         "A" = Active
#       8  grant_date             MM/DD/YYYY
#       9  expired_date           MM/DD/YYYY
#   EN.dat  ("Entity")
#       1  record_type ("EN")
#       2  unique_system_identifier
#       5  call_sign
#      18  state                  2-letter USPS code
#
# Reference: https://www.fcc.gov/sites/default/files/pubacc_tbl_descriptions_8.pdf


def script_dir() -> str:
    return os.path.dirname(os.path.abspath(__file__))


def repo_root() -> str:
    return os.path.dirname(script_dir())


def default_work_dir() -> str:
    # Sibling of the repo, e.g. E:\JTSDK-Build\_uls_scratch
    return os.path.join(os.path.dirname(repo_root()), "_uls_scratch")


def download(url: str, dst: str) -> None:
    print(f"Downloading {url}")
    print(f"  -> {dst}")
    with urllib.request.urlopen(url) as resp, open(dst, "wb") as out:
        total = int(resp.headers.get("Content-Length") or 0)
        read = 0
        chunk = 1 << 20  # 1 MB
        while True:
            buf = resp.read(chunk)
            if not buf:
                break
            out.write(buf)
            read += len(buf)
            if total:
                pct = read * 100 // total
                print(f"  {read/1e6:.1f} / {total/1e6:.1f} MB ({pct}%)", end="\r")
            else:
                print(f"  {read/1e6:.1f} MB", end="\r")
    print()


def unzip(zip_path: str, dst_dir: str) -> None:
    print(f"Unzipping {zip_path}")
    with zipfile.ZipFile(zip_path) as zf:
        for name in ("HD.dat", "EN.dat"):
            try:
                zf.extract(name, path=dst_dir)
            except KeyError:
                print(f"ERROR: {name} not found in {zip_path}", file=sys.stderr)
                sys.exit(2)
    print(f"  HD.dat + EN.dat extracted to {dst_dir}")


def parse_hd_active(hd_path: str) -> set:
    """Return USIs whose license is Active and not expired today."""
    today = datetime.date.today()
    active: set = set()
    total = 0
    with open(hd_path, "r", encoding="latin-1", errors="replace") as f:
        for line in f:
            total += 1
            parts = line.rstrip("\r\n").split("|")
            if len(parts) < 9 or parts[0] != "HD":
                continue
            usi = parts[1]
            status = parts[5]
            expired = parts[8]
            if status != "A":
                continue
            if expired:
                try:
                    m, d, y = expired.split("/")
                    if datetime.date(int(y), int(m), int(d)) < today:
                        continue
                except ValueError:
                    pass  # malformed date — keep the row
            active.add(usi)
    print(f"HD.dat: {total} rows scanned; {len(active)} active+unexpired licensees")
    return active


def parse_en_rows(en_path: str, active: set):
    """Yield (call, state) for active licensees with a valid USPS state code."""
    total = yielded = no_state = bad_state = 0
    with open(en_path, "r", encoding="latin-1", errors="replace") as f:
        for line in f:
            total += 1
            parts = line.rstrip("\r\n").split("|")
            if len(parts) < 18 or parts[0] != "EN":
                continue
            usi = parts[1]
            if usi not in active:
                continue
            call = parts[4].strip().upper()
            state = parts[17].strip().upper()
            if not call:
                continue
            if not state:
                no_state += 1
                continue
            if state not in VALID_STATES:
                bad_state += 1
                continue
            yielded += 1
            yield (call, state)
    print(
        f"EN.dat: {total} rows scanned; emitted {yielded}, "
        f"skipped {no_state} no-state, {bad_state} invalid-state"
    )


def build_db(out_path: str, rows_iter) -> None:
    if os.path.exists(out_path):
        os.remove(out_path)
    conn = sqlite3.connect(out_path)
    cur = conn.cursor()
    cur.execute("CREATE TABLE USState ( call TEXT, state TEXT )")
    cur.execute("BEGIN")
    seen: set = set()
    inserted = dups = 0
    for call, state in rows_iter:
        if call in seen:
            dups += 1
            continue
        seen.add(call)
        cur.execute("INSERT INTO USState (call, state) VALUES (?, ?)", (call, state))
        inserted += 1
    cur.execute("CREATE INDEX idx_call ON USState(call)")
    conn.commit()
    cur.execute("VACUUM")
    conn.close()
    sz = os.path.getsize(out_path)
    print(f"DB: inserted {inserted} unique callsigns ({dups} duplicate calls dropped); {sz/1e6:.1f} MB")


def install(new_db: str, target_db: str) -> None:
    if os.path.exists(target_db):
        bak = target_db + ".bak"
        if os.path.exists(bak):
            os.remove(bak)
        shutil.copy2(target_db, bak)
        print(f"Backup: {bak}")
    shutil.copy2(new_db, target_db)
    print(f"Installed: {target_db}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--work-dir", default=default_work_dir(),
                    help="scratch dir for download + extract (default: sibling of repo)")
    ap.add_argument("--target", default=os.path.join(repo_root(), "USState.db"),
                    help="path to install the new DB into (default: <repo>/USState.db)")
    ap.add_argument("--skip-download", action="store_true",
                    help="reuse existing l_amat.zip in work dir")
    ap.add_argument("--skip-unzip", action="store_true",
                    help="reuse existing HD.dat/EN.dat in work dir")
    ap.add_argument("--dry-run", action="store_true",
                    help="build the DB but do not replace --target")
    args = ap.parse_args()

    os.makedirs(args.work_dir, exist_ok=True)
    zip_path = os.path.join(args.work_dir, "l_amat.zip")
    hd_path = os.path.join(args.work_dir, "HD.dat")
    en_path = os.path.join(args.work_dir, "EN.dat")
    new_db = os.path.join(args.work_dir, "USState.db")

    if not args.skip_download:
        download(ULS_URL, zip_path)
    elif not os.path.exists(zip_path):
        print(f"ERROR: --skip-download set but {zip_path} not found", file=sys.stderr)
        return 1

    if not args.skip_unzip:
        unzip(zip_path, args.work_dir)
    else:
        for p in (hd_path, en_path):
            if not os.path.exists(p):
                print(f"ERROR: --skip-unzip set but {p} not found", file=sys.stderr)
                return 1

    active = parse_hd_active(hd_path)
    build_db(new_db, parse_en_rows(en_path, active))

    if args.dry_run:
        print(f"--dry-run: leaving new DB at {new_db}; {args.target} unchanged")
    else:
        install(new_db, args.target)

    return 0


if __name__ == "__main__":
    sys.exit(main())
