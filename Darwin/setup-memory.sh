#!/bin/bash
#
# WSJT-Z macOS Shared Memory Setup Script
# This script checks and optionally increases system shared memory limits
# required for WSJT-Z to run without "QSharedMemory::handle" errors.
#
# Usage: ./setup-memory.sh [--auto]
#   --auto: automatically apply fixes without prompting

set -e

REQUIRED_SHMMAX=134217728
REQUIRED_SHMALL=32768

CURRENT_SHMMAX="$(sysctl -n kern.sysv.shmmax 2>/dev/null || echo 0)"
CURRENT_SHMALL="$(sysctl -n kern.sysv.shmall 2>/dev/null || echo 0)"

echo "========================================="
echo "WSJT-Z macOS Memory Configuration Check"
echo "========================================="
echo
echo "Current settings:"
echo "  kern.sysv.shmmax = $CURRENT_SHMMAX"
echo "  kern.sysv.shmall = $CURRENT_SHMALL"
echo
echo "Required for WSJT-Z:"
echo "  kern.sysv.shmmax >= $REQUIRED_SHMMAX"
echo "  kern.sysv.shmall >= $REQUIRED_SHMALL"
echo

if [ "$CURRENT_SHMMAX" -ge "$REQUIRED_SHMMAX" ] && [ "$CURRENT_SHMALL" -ge "$REQUIRED_SHMALL" ]; then
  echo "✓ Memory settings are OK. WSJT-Z should run without issues."
  exit 0
fi

echo "⚠ Your system memory limits are too low for WSJT-Z."
echo

if [ "$1" = "--auto" ]; then
  FIX=1
else
  echo "Would you like to increase these limits? (requires administrator password)"
  read -p "Enter 'yes' to continue, or anything else to skip: " response
  if [ "$response" != "yes" ]; then
    echo "Skipped. WSJT-Z may experience memory errors. Run this script again if needed."
    exit 0
  fi
  FIX=1
fi

if [ $FIX -eq 1 ]; then
  echo
  echo "Applying memory configuration..."
  echo
  
  sudo sysctl -w kern.sysv.shmmax=$REQUIRED_SHMMAX
  sudo sysctl -w kern.sysv.shmall=$REQUIRED_SHMALL
  
  echo
  echo "✓ Memory limits updated successfully!"
  echo
  echo "These settings will persist until reboot."
  echo "To make them permanent, you can edit /etc/sysctl.conf:"
  echo "  sudo nano /etc/sysctl.conf"
  echo
  echo "Then add these lines:"
  echo "  kern.sysv.shmmax=$REQUIRED_SHMMAX"
  echo "  kern.sysv.shmall=$REQUIRED_SHMALL"
  echo
fi
