#!/usr/bin/env python3
# WSJT-Z Control via UDP
# Schema 3+ support for remote control of WSJT-Z features
# DE K7MHI

import socket
import struct
import sys
import argparse
import time
from enum import IntEnum
from datetime import datetime


class MessageType(IntEnum):
    """WSJT-Z UDP Message Types"""
    Configure = 15


class QtDataStreamWriter:
    """Writes Qt-compatible binary data streams (big-endian)"""
    
    def __init__(self):
        self.data = bytearray()
    
    def write_uint32(self, value):
        self.data.extend(struct.pack('>I', value))
    
    def write_int32(self, value):
        self.data.extend(struct.pack('>i', value))
    
    def write_uint16(self, value):
        self.data.extend(struct.pack('>H', value))
    
    def write_uint8(self, value):
        self.data.extend(struct.pack('>B', value))
    
    def write_bool(self, value):
        self.data.extend(struct.pack('>B', 1 if value else 0))
    
    def write_double(self, value):
        self.data.extend(struct.pack('>d', value))
    
    def write_float(self, value):
        self.data.extend(struct.pack('>f', value))
    
    def write_qtime(self, qtime_obj):
        """Write QTime as milliseconds since midnight"""
        if isinstance(qtime_obj, str):
            # Parse "HH:MM:SS" or "HH:MM:SS.mmm"
            parts = qtime_obj.split(':')
            if len(parts) >= 2:
                h = int(parts[0])
                m = int(parts[1])
                s = int(parts[2].split('.')[0]) if len(parts) > 2 else 0
                ms = int(parts[2].split('.')[1]) if '.' in parts[2] else 0
                ms_total = (h * 3600 + m * 60 + s) * 1000 + ms
                self.write_uint32(ms_total)
            else:
                self.write_uint32(0)
        else:
            self.write_uint32(qtime_obj)
    
    def write_qcolor(self, color_str):
        """Write QColor as 0xAARRGGBB"""
        if color_str.startswith('0x') or color_str.startswith('0X'):
            color_val = int(color_str, 16)
        else:
            # Try to parse as decimal
            color_val = int(color_str)
        self.write_uint32(color_val)
    
    def write_utf8(self, text):
        """Write a UTF-8 string as QByteArray (size-prefixed)"""
        if text is None or text == '':
            self.write_uint32(0xffffffff)  # null string
        else:
            encoded = text.encode('utf-8')
            self.write_uint32(len(encoded))
            self.data.extend(encoded)
    
    def get_bytes(self):
        return bytes(self.data)


def send_udp_message(server_address, server_port, message):
    """Send UDP message to WSJT-X"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(message, (server_address, server_port))
        sock.close()
        return True
    except Exception as e:
        print(f"Error sending message: {e}", file=sys.stderr)
        return False


# ============================================================================
# Message Builders
# ============================================================================



def build_configure(message_id, mode='', frequency_tolerance=0xffffffff, 
                   submode='', fast_mode=False, tr_period=0xffffffff,
                   rx_df=0xffffffff, dx_call='', dx_grid='', 
                   generate_messages=False, auto_cq_enabled=False, auto_call_enabled=False,
                   schema=None):
    """Build Configure message"""
    writer = QtDataStreamWriter()
    
    # Auto-detect schema: use 3 only if AutoCQ/AutoCall fields are being set
    if schema is None:
        schema = 3 if (auto_cq_enabled or auto_call_enabled) else 2
    
    writer.write_uint32(0xadbccbda)  # magic
    writer.write_uint32(schema)
    writer.write_uint32(MessageType.Configure)
    writer.write_utf8(message_id)
    
    # Schema 2 fields
    writer.write_utf8(mode)
    writer.write_uint32(frequency_tolerance)
    writer.write_utf8(submode)
    writer.write_bool(fast_mode)
    writer.write_uint32(tr_period)
    writer.write_uint32(rx_df)
    writer.write_utf8(dx_call)
    writer.write_utf8(dx_grid)
    writer.write_bool(generate_messages)
    
    # Schema 3+ fields
    if schema >= 3:
        writer.write_bool(auto_cq_enabled)
        writer.write_bool(auto_call_enabled)
    
    return writer.get_bytes()





# ============================================================================
# Subcommand Handlers
# ============================================================================

def cmd_configure(args):
    """Handle 'configure' subcommand"""
    kwargs = {
        'mode': args.mode or '',
        'frequency_tolerance': args.freq_tol if args.freq_tol else 0xffffffff,
        'submode': args.submode or '',
        'fast_mode': args.fast_mode,
        'tr_period': args.tr_period if args.tr_period else 0xffffffff,
        'rx_df': args.rx_df if args.rx_df else 0xffffffff,
        'dx_call': args.dx_call or '',
        'dx_grid': args.dx_grid or '',
        'generate_messages': args.generate_messages,
        'schema': args.schema,
    }
    
    if args.auto_cq is not None:
        kwargs['auto_cq_enabled'] = args.auto_cq
    if args.auto_call is not None:
        kwargs['auto_call_enabled'] = args.auto_call
    
    message = build_configure(args.id, **kwargs)
    
    if args.verbose:
        print("=" * 60)
        print("Configure Message")
        print("=" * 60)
        print(f"Target: {args.host}:{args.port}")
        print(f"Client ID: {args.id}")
        actual_schema = args.schema if args.schema else (3 if (args.auto_cq or args.auto_call) else 2)
        print(f"Schema: {actual_schema}")
        print()
        if args.mode:
            print(f"Mode:     {args.mode}")
        if args.submode:
            print(f"Submode:  {args.submode}")
        if args.fast_mode:
            print(f"Fast Mode: enabled")
        if args.freq_tol:
            print(f"Freq Tol: {args.freq_tol} Hz")
        if args.tr_period:
            print(f"T/R Period: {args.tr_period}s")
        if args.rx_df:
            print(f"RX DF: {args.rx_df} Hz")
        if args.dx_call:
            print(f"DX Call: {args.dx_call}")
        if args.dx_grid:
            print(f"DX Grid: {args.dx_grid}")
        if args.auto_cq is not None:
            print(f"AutoCQ:   {'ENABLE' if args.auto_cq else 'DISABLE'}")
        if args.auto_call is not None:
            print(f"AutoCall: {'ENABLE' if args.auto_call else 'DISABLE'}")
        print()
    
    if send_udp_message(args.host, args.port, message):
        if args.verbose:
            print("✓ Message sent successfully")
        else:
            print("OK")
        return 0
    else:
        print("✗ Failed to send message", file=sys.stderr)
        return 1





# ============================================================================
# Main
# ============================================================================

if __name__ == "__main__":
    DEFAULT_SERVER = 'localhost'
    DEFAULT_PORT = 2237
    DEFAULT_ID = 'ControlClient'
    
    parser = argparse.ArgumentParser(
        description='Control WSJT-X via UDP - Configure command',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Configure WSJT-X for FT8
  %(prog)s configure --mode FT8
  
  # Enable AutoCQ mode
  %(prog)s configure --auto-cq --mode FT8
  
  # Set frequency tolerance and T/R period
  %(prog)s configure --freq-tol 2000 --tr-period 15
  
  # Disable AutoCall
  %(prog)s configure --no-auto-call
        ''')
    
    parser.add_argument('--host', default=DEFAULT_SERVER,
                       help=f'WSJT-X server address (default: {DEFAULT_SERVER})')
    parser.add_argument('--port', type=int, default=DEFAULT_PORT,
                       help=f'WSJT-X UDP port (default: {DEFAULT_PORT})')
    parser.add_argument('--id', default=DEFAULT_ID,
                       help=f'Client identifier (default: {DEFAULT_ID})')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Show message details')
    
    subparsers = parser.add_subparsers(dest='command', help='Command to execute')
    
    # ========== Configure ==========
    config_parser = subparsers.add_parser('configure', help='Configure WSJT-X settings')
    config_parser.add_argument('--mode', help='Mode (FT8, FT4, JT65, etc.)')
    config_parser.add_argument('--submode', help='Submode character')
    config_parser.add_argument('--fast-mode', action='store_true', help='Enable fast mode')
    config_parser.add_argument('--freq-tol', type=int, help='Frequency tolerance in Hz')
    config_parser.add_argument('--tr-period', type=int, help='T/R period in seconds')
    config_parser.add_argument('--rx-df', type=int, help='RX audio frequency offset in Hz')
    config_parser.add_argument('--dx-call', help='DX call to work')
    config_parser.add_argument('--dx-grid', help='DX grid locator')
    config_parser.add_argument('--generate-messages', action='store_true',
                              help='Enable automatic message generation')
    config_parser.add_argument('--auto-cq', action='store_true', dest='auto_cq',
                              help='Enable AutoCQ mode')
    config_parser.add_argument('--no-auto-cq', action='store_false', dest='auto_cq',
                              help='Disable AutoCQ mode')
    config_parser.add_argument('--auto-call', action='store_true', dest='auto_call',
                              help='Enable AutoCall mode')
    config_parser.add_argument('--no-auto-call', action='store_false', dest='auto_call',
                              help='Disable AutoCall mode')
    config_parser.add_argument('--schema', type=int, choices=[2, 3],
                              help='Schema version (default: auto-detect)')
    config_parser.set_defaults(func=cmd_configure, auto_cq=None, auto_call=None)
    
    args = parser.parse_args()
    
    if not hasattr(args, 'func'):
        parser.print_help()
        sys.exit(1)
    
    sys.exit(args.func(args))
