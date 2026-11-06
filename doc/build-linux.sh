#!/usr/bin/env bash
# Build script for Ubuntu/Debian Linux
# last tested: May 2026, Ubuntu 24.04
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# 0) Base tools
sudo apt-get update
sudo apt install -y libhamlib4t64 ccache libhamlib-dev libhamlib-utils
sudo apt-get install -y build-essential cmake ninja-build pkg-config g++ gfortran libboost-all-dev libfftw3-dev libusb-1.0-0-dev libudev-dev portaudio19-dev qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools libqt5serialport5-dev qtmultimedia5-dev qttools5-dev libqt5websockets5-dev asciidoctor ccache libqt5multimedia5-plugins 

# 1) Find Qt5 qmake
QT5_QMAKE="$(command -v qmake || command -v qmake-qt5 || true)"
if [ -z "$QT5_QMAKE" ]; then
  echo "Qt5 qmake not found. Please ensure qtbase5-dev is installed."
  exit 1
fi
QT5_PREFIX="$(dirname $(dirname "$QT5_QMAKE"))"
echo "Qt5 found at $QT5_PREFIX"

# 2) Find gfortran
GFORTRAN_BIN="$(command -v gfortran)"
if [ -z "$GFORTRAN_BIN" ]; then
  echo "gfortran not found. Please ensure gfortran is installed."
  exit 1
fi
echo "gfortran found at $GFORTRAN_BIN"
"$GFORTRAN_BIN" --version | head -n 1


# 3) Use current repository checkout
cd "$PROJECT_ROOT"

# 3a) Find rigctl for Hamlib
RIGCTL_BIN="$(command -v rigctl || true)"
if [ -z "$RIGCTL_BIN" ]; then
  echo "rigctl not found. Please ensure hamlib-utils is installed."
  exit 1
fi
echo "rigctl found at $RIGCTL_BIN"

# 4) Configure with CMake
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_Fortran_COMPILER="$GFORTRAN_BIN" \
  -DCMAKE_PREFIX_PATH="$QT5_PREFIX" \
  -DQt5_DIR="$QT5_PREFIX/lib/cmake/Qt5" \
  -DFFTW3_ROOT_DIR="/usr" \
  -DHamlib_ROOT="/usr" \
  -DUsb_ROOT="/usr" \
  -DWSJT_SKIP_MANPAGES=ON \
  -DWSJT_GENERATE_DOCS=OFF \
  -DRIGCTL_EXE="$RIGCTL_BIN" \
  -DCMAKE_INSTALL_PREFIX="$PWD/stage"

# 5) Build + install
cmake --build build -j"$(nproc)"
cmake --install build

echo "Done. Binaries should be under: $PWD/stage/"
echo "To install system-wide, run: sudo cmake --install build --prefix=/usr/local"
