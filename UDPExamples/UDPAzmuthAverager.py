#!/usr/bin/env python3
# This script listens to WSJT-X UDP packets and computes a rolling average of the azimuth
# from your home grid to the grids of stations that you decode.
# example output: heading= 105.3° | smoothed= 102.6° | dir=E | s=27 | p=75 | d=75
# average each burst | average each window | cardinal direction | sample count | packets seen | packets decoded
# DE K7MHI

import datetime
import math
import re
import socket
import struct
import sys
from collections import deque

# ---- configuration ----
WSJT_X_UDP_HOST = "127.0.0.1"  # default UDP host for WSJT-X
WSJT_X_UDP_PORT = 2237  # default UDP port for WSJT-X
MY_GRID = "CN88ab"      # Will be updated from Status message de_grid
WINDOW_SECONDS = 180    # 3 minutes of history for the smoothed value
BURST_SECONDS = 20      # FT8/4/2 cycle length, WSPR burst length, etc. for the live heading
DEBUG = False           # Set to True to log malformed packets
RESULTANT_THRESHOLD = 0.3  # Minimum resultant vector length for stable heading

# Rolling buffers for the live heading and the smoothed heading
cycle_samples = deque() # current burst samples used by the live heading
heading_history = deque()  # heading values used for the smoothed window average
latest_station_grid = None
packets_seen = 0
packets_decoded = 0
home_grid = MY_GRID  # Will be updated from Status message de_grid

# ---- helpers ----


def cardinal_direction(deg):
    """Return a cardinal direction label for a heading in degrees."""
    if deg is None:
        return "--"
    dirs = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
    idx = int((deg / 45.0) + 0.5) % 8
    return dirs[idx]


def render_status(packets_seen, packets_decoded, sample_count, heading=None, smoothed_heading=None, message=None):
    """Write a single-line status update with heading and packet activity."""
    if heading is None:
        heading_text = "heading=--.--°"
    else:
        heading_text = f"heading={heading:5.1f}°"

    if smoothed_heading is None:
        smoothed_text = "smoothed=--.--°"
        direction_text = "dir=--"
    else:
        smoothed_text = f"smoothed={smoothed_heading:5.1f}°"
        direction_text = f"dir={cardinal_direction(smoothed_heading)}"

    if message:
        message_text = f" | {message}"
    else:
        message_text = ""

    line = (
        f"{heading_text} | {smoothed_text} | {direction_text} | "
        f"s={sample_count}{message_text} | p={packets_seen} | d={packets_decoded}"
    )
    sys.stdout.write("\r" + line)
    sys.stdout.flush()


def extract_grid_from_text(text):
    """Try to pull a Maidenhead locator from decoded text.
    Excludes QSO-ending tokens RR73, RRR, and 73.
    """
    if not text:
        return None
    text = text.upper()
    # Exclude RR73, RRR, 73 tokens which are QSO terminators, not grids
    text_without_terminators = re.sub(r"\b(RR73|RRR|73)\b", "", text)
    m = re.search(r"\b([A-R]{2}[0-9]{2}(?:[A-X]{2})?)", text_without_terminators)
    return m.group(1) if m else None

def grid_to_latlon(grid):
    """
    Convert a Maidenhead locator to approximate lat/lon.
    Good enough for antenna heading, not geodesy grade.
    4-char grid: center of 2°×1° square
    6-char grid: center of subsquare within that square
    """
    g = (grid or "").strip().upper()
    if len(g) < 4:
        raise ValueError(f"bad grid: {grid}")

    lon = (ord(g[0]) - ord("A")) * 20.0
    lat = (ord(g[1]) - ord("A")) * 10.0
    lon += int(g[2]) * 2.0
    lat += int(g[3]) * 1.0

    if len(g) >= 6:
        # 6-char subsquare: smaller offset
        lon += (ord(g[4]) - ord("A")) * (5.0 / 60.0)
        lat += (ord(g[5]) - ord("A")) * (2.5 / 60.0)
    else:
        # 4-char square: center offset
        lon += 1.0
        lat += 0.5

    lon -= 180.0
    lat -= 90.0

    return lat, lon

def bearing_deg(lat1, lon1, lat2, lon2):
    """Great-circle bearing from point 1 to point 2."""
    lat1 = math.radians(lat1)
    lon1 = math.radians(lon1)
    lat2 = math.radians(lat2)
    lon2 = math.radians(lon2)

    dlon = lon2 - lon1
    y = math.sin(dlon) * math.cos(lat2)
    x = math.cos(lat1) * math.sin(lat2) - math.sin(lat1) * math.cos(lat2) * math.cos(dlon)
    brng = math.degrees(math.atan2(y, x))
    return (brng + 360.0) % 360.0

def azimuth_from_grid(home_grid, station_grid):
    home_lat, home_lon = grid_to_latlon(home_grid)
    st_lat, st_lon = grid_to_latlon(station_grid)
    return bearing_deg(home_lat, home_lon, st_lat, st_lon)

def heading_from_window(items):
    """Average heading over the current rolling window of samples.
    Returns None if samples are scattered (resultant vector too short).
    """
    if not items:
        return None

    x = 0.0
    y = 0.0
    n = len(items)

    for _, angle_deg in items:
        angle_rad = math.radians(angle_deg)
        x += math.cos(angle_rad)
        y += math.sin(angle_rad)

    # Check resultant vector length to detect scattered samples
    r = math.hypot(x, y) / n
    if r < 0.3:  # Threshold for stable heading
        return None  # Samples too scattered

    heading = math.degrees(math.atan2(y, x))
    return (heading + 360.0) % 360.0


def average_headings(headings):
    """Average a sequence of heading values using vector math."""
    if not headings:
        return None

    x = 0.0
    y = 0.0

    for _, heading_deg in headings:
        angle_rad = math.radians(heading_deg)
        x += math.cos(angle_rad)
        y += math.sin(angle_rad)

    if x == 0.0 and y == 0.0:
        return None

    heading = math.degrees(math.atan2(y, x))
    return (heading + 360.0) % 360.0


def smooth_heading(heading_history, timestamp, last_update, min_samples=6):
    """Sample the smoothed heading once per window from recent heading values."""
    if last_update is not None and timestamp - last_update < datetime.timedelta(seconds=WINDOW_SECONDS):
        return None, last_update

    cutoff = timestamp - datetime.timedelta(seconds=WINDOW_SECONDS)
    while heading_history and heading_history[0][0] < cutoff:
        heading_history.popleft()

    if len(heading_history) < min_samples:
        return None, last_update

    return average_headings(heading_history), timestamp

def add_sample(grid, timestamp):
    """Add a gridded decode to the rolling sample buffer."""
    if not grid:
        return

    try:
        angle = azimuth_from_grid(home_grid, grid)
    except Exception:
        return

    cycle_samples.append((timestamp, angle))

    # Keep only the latest burst for the current heading
    cutoff_burst = timestamp - datetime.timedelta(seconds=BURST_SECONDS)
    while cycle_samples and cycle_samples[0][0] < cutoff_burst:
        cycle_samples.popleft()

# ---- WSJT-X UDP decoding ----

class QtDataStreamReader:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def _read(self, fmt):
        size = struct.calcsize(fmt)
        if self.pos + size > len(self.data):
            raise ValueError("unexpected end of data")
        value = struct.unpack(">" + fmt, self.data[self.pos:self.pos + size])
        self.pos += size
        return value[0] if len(value) == 1 else value

    def read_uint32(self):
        return self._read("I")

    def read_int32(self):
        return self._read("i")

    def read_uint64(self):
        return self._read("Q")

    def read_uint8(self):
        return self._read("B")

    def read_double(self):
        return self._read("d")

    def read_bool(self):
        return bool(self._read("B"))

    def read_bytes(self, length):
        if self.pos + length > len(self.data):
            raise ValueError("unexpected end of data")
        value = self.data[self.pos:self.pos + length]
        self.pos += length
        return value

    def read_qbytearray(self):
        size = self.read_uint32()
        if size == 0xFFFFFFFF:
            return None
        return self.read_bytes(size)

    def read_utf8(self):
        raw = self.read_qbytearray()
        return None if raw is None else raw.decode("utf-8", errors="replace")

    def read_qtime(self):
        ms = self.read_uint32()
        if ms == 0xFFFFFFFF:
            return None
        hour = ms // 3600000
        minute = (ms % 3600000) // 60000
        second = (ms % 60000) // 1000
        msec = ms % 1000
        return datetime.time(hour, minute, second, msec * 1000)

def decode_message(data):
    reader = QtDataStreamReader(data)
    magic = reader.read_uint32()
    if magic != 0xADBCCBDA:
        raise ValueError("bad magic number")

    schema = reader.read_uint32()
    if schema not in (2, 3):
        raise ValueError(f"unsupported schema {schema}")

    message_type = reader.read_uint32()
    message_id = reader.read_utf8()
    return schema, message_type, message_id, reader

# ---- main listener ----
print("WSJT-X Azimuth Averager")
print(f"Listening on {WSJT_X_UDP_HOST}:{WSJT_X_UDP_PORT}")
print(f"Initial home grid: {MY_GRID}")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # Allows reuse of port if script crashes
sock.settimeout(1.0)  # 1 second timeout for Windows Ctrl+C handling
sock.bind((WSJT_X_UDP_HOST, WSJT_X_UDP_PORT))
smoothed_heading = None
display_heading = None
display_smoothed_heading = None
last_smoothed_update = None

try:
    while True:
        try:
            data, _ = sock.recvfrom(10240)  # Match buffer size with decode_WSJT-UDP.py
        except socket.timeout:
            continue  # No packet received, try again
        
        now = datetime.datetime.now()
        packets_seen += 1

        try:
            _, message_type, _, reader = decode_message(data)
            packets_decoded += 1

            if message_type == 1:
                dial_freq = reader.read_uint64()
                mode = reader.read_utf8()
                dx_call = reader.read_utf8()
                report = reader.read_utf8()
                tx_mode = reader.read_utf8()
                tx_enabled = reader.read_bool()
                transmitting = reader.read_bool()
                decoding = reader.read_bool()
                rx_df = reader.read_uint32()
                tx_df = reader.read_uint32()
                de_call = reader.read_utf8()
                de_grid = reader.read_utf8()
                dx_grid = reader.read_utf8()
                tx_watchdog = reader.read_bool()
                submode = reader.read_utf8()
                fast_mode = reader.read_bool()
                special_op_mode = reader.read_uint8()
                freq_tolerance = reader.read_uint32()
                tr_period = reader.read_uint32()
                config_name = reader.read_utf8()
                tx_message = reader.read_utf8()

                # Seed home_grid from de_grid (most reliable source)
                if de_grid:
                    home_grid = de_grid
                
                if dx_grid:
                    latest_station_grid = dx_grid

            elif message_type == 2:
                # decode message
                is_new = reader.read_bool()
                _time = reader.read_qtime()
                _snr = reader.read_int32()
                _delta_time = reader.read_double()
                _delta_frequency = reader.read_uint32()
                _mode = reader.read_utf8()
                message = reader.read_utf8()
                _low_confidence = reader.read_bool()
                _off_air = reader.read_bool()

                grid = extract_grid_from_text(message)
                if grid:  # Only add gridded decodes
                    add_sample(grid, now)

            elif message_type == 10:
                # wspr decode
                _is_new = reader.read_bool()
                _time = reader.read_qtime()
                _snr = reader.read_int32()
                _delta_time = reader.read_double()
                _frequency = reader.read_uint64()
                _drift = reader.read_int32()
                _callsign = reader.read_utf8()
                grid = reader.read_utf8()
                _power = reader.read_int32()
                _off_air = reader.read_bool()

                if grid:  # Only add gridded decodes
                    add_sample(grid, now)

        except Exception as e:
            if DEBUG:
                print(f"Malformed packet: {e}")
            # Ignore malformed packets.

        current_heading = heading_from_window(cycle_samples)

        if current_heading is not None:
            display_heading = current_heading
            heading_history.append((now, current_heading))
            new_smoothed, last_smoothed_update = smooth_heading(
                heading_history,
                now,
                last_smoothed_update,
            )
            if new_smoothed is not None:
                smoothed_heading = new_smoothed
            display_smoothed_heading = smoothed_heading
        elif display_heading is None:
            display_heading = None
            display_smoothed_heading = None

        render_status(
            packets_seen,
            packets_decoded,
            len(cycle_samples),
            heading=display_heading,
            smoothed_heading=display_smoothed_heading,
        )

except KeyboardInterrupt:
    print("\nStopped.")
finally:
    sock.close()
