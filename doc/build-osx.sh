#!/usr/bin/env bash
# Build script for macOS. Homebrew is used to manage dependencies.
# last tested: May5th2026 OSX 26 Apple Silicon (arm64)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# 0) Base tools
xcode-select -p >/dev/null 2>&1 || xcode-select --install

# 1) Homebrew (skip if already installed)
if ! command -v brew >/dev/null 2>&1; then
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

if [ -x /opt/homebrew/bin/brew ]; then
  eval "$(/opt/homebrew/bin/brew shellenv)"
elif [ -x /usr/local/bin/brew ]; then
  eval "$(/usr/local/bin/brew shellenv)"
else
  echo "brew not found in /opt/homebrew/bin or /usr/local/bin"
  exit 1
fi
brew update

# 2) Build dependencies from Homebrew
# IMPORTANT: use Homebrew Qt5 on Apple Silicon so all binaries are arm64.
brew install cmake ninja pkg-config gcc boost fftw hamlib libusb portaudio asciidoctor ccache qt@5

# 3) Resolve Qt5 path from Homebrew
QT5_PREFIX="$(brew --prefix qt@5)"
test -x "$QT5_PREFIX/bin/qmake" || {
  echo "Homebrew Qt5 not found at $QT5_PREFIX"
  exit 1
}
echo "Qt5 found at $QT5_PREFIX"

# 4) Find gfortran (prefer unversioned, fallback to highest versioned)
if command -v gfortran >/dev/null 2>&1; then
  GFORTRAN_BIN="$(command -v gfortran)"
else
  GFORTRAN_BIN="$(find "$(brew --prefix)/bin" -maxdepth 1 -name 'gfortran-*' -print 2>/dev/null | sort | tail -n1 || true)"
fi

if [ -z "$GFORTRAN_BIN" ]; then
  echo "gfortran not found. Check brew gcc installation."
  exit 1
fi

echo "gfortran found at $GFORTRAN_BIN"
"$GFORTRAN_BIN" --version | head -n 1

# 5) Detect host architecture and choose matching build settings.
HOST_ARCH="$(uname -m)"
case "$HOST_ARCH" in
  arm64)
    CMAKE_ARCH="arm64"
    CMAKE_DEPLOY_TARGET="11.0"
    ;;
  x86_64)
    CMAKE_ARCH="x86_64"
    CMAKE_DEPLOY_TARGET="11.0"
    ;;
  *)
    echo "Unsupported host architecture: $HOST_ARCH"
    exit 1
    ;;
esac
echo "Configuring for host architecture: $CMAKE_ARCH"

# 7) Use current repository checkout
cd "$PROJECT_ROOT"

# 8) Configure with CMake
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="$CMAKE_ARCH" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$CMAKE_DEPLOY_TARGET" \
  -DCMAKE_Fortran_COMPILER="$GFORTRAN_BIN" \
  -DCMAKE_PREFIX_PATH="$QT5_PREFIX;$(brew --prefix)" \
  -DQt5_DIR="$QT5_PREFIX/lib/cmake/Qt5" \
  -DFFTW3_ROOT_DIR="$(brew --prefix fftw)" \
  -DHamlib_ROOT="$(brew --prefix hamlib)" \
  -DUsb_ROOT="$(brew --prefix libusb)" \
  -DWSJT_SKIP_MANPAGES=ON \
  -DWSJT_GENERATE_DOCS=OFF \
  -DCMAKE_INSTALL_PREFIX="$PWD/stage"

# 9) Build + install
cmake --build build -j"$(sysctl -n hw.ncpu)"
cmake --install build

echo "Done. App bundle should be under: $PWD/stage/wsjtx.app"

# 9a) Preflight check for SysV shared memory limits.
# WSJT-X/WSJT-Z uses a large shared memory segment; VM low memory can cause
# runtime failure: "QSharedMemory::handle: system-imposed size restrictions".
REQUIRED_SHMMAX=134217728
REQUIRED_SHMALL=32768
CURRENT_SHMMAX="$(sysctl -n kern.sysv.shmmax 2>/dev/null || echo 0)"
CURRENT_SHMALL="$(sysctl -n kern.sysv.shmall 2>/dev/null || echo 0)"

if [ "$CURRENT_SHMMAX" -lt "$REQUIRED_SHMMAX" ] || [ "$CURRENT_SHMALL" -lt "$REQUIRED_SHMALL" ]; then
  echo
  echo "WARNING: system memory sysctl limits look too low for wsjtx runtime."
  echo "  Current:  kern.sysv.shmmax=$CURRENT_SHMMAX  kern.sysv.shmall=$CURRENT_SHMALL"
  echo "  Required: kern.sysv.shmmax>=$REQUIRED_SHMMAX  kern.sysv.shmall>=$REQUIRED_SHMALL"
  echo
  echo "Run these commands, if application has memory errors at runtime:"
  echo "  sudo sysctl -w kern.sysv.shmmax=$REQUIRED_SHMMAX"
  echo "  sudo sysctl -w kern.sysv.shmall=$REQUIRED_SHMALL"
  echo
fi