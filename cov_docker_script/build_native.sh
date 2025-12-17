#!/bin/bash

# Native Build Script for Common-Library Component
# Assumes dependencies are already set up via setup_dependencies.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_CONFIG="${SCRIPT_DIR}/build_config.json"
HEADER_PREFIX=${HEADER_PREFIX:-$HOME/usr/include/rdkb}
INSTALL_PREFIX=${INSTALL_PREFIX:-$HOME/usr/local}
BUILD_JOBS=${BUILD_JOBS:-$(nproc)}

# Color output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }

echo "=========================================="
log_info "Building Common-Library Component"
echo "=========================================="

# Verify dependencies exist
if [ ! -d "$HEADER_PREFIX" ]; then
    log_error "Headers not found at: $HEADER_PREFIX"
    log_error "Please run setup_dependencies.sh first"
    exit 1
fi

if [ ! -d "$INSTALL_PREFIX/lib" ]; then
    log_error "Libraries not found at: $INSTALL_PREFIX/lib"
    log_error "Please run setup_dependencies.sh first"
    exit 1
fi

# Build CFLAGS from JSON
CFLAGS_BASE=$(jq -r '.cflags[]' "$BUILD_CONFIG" | tr '\n' ' ')

# Add dependency include directories
header_subdirs=$(jq -r '.dependency_include_dirs[]' "$BUILD_CONFIG")
for subdir in $header_subdirs; do
    if [ "$subdir" == "." ]; then
        CFLAGS_BASE="$CFLAGS_BASE -I$HEADER_PREFIX"
    else
        CFLAGS_BASE="$CFLAGS_BASE -I$HEADER_PREFIX/$subdir"
    fi
done

# Build LDFLAGS from JSON
LDFLAGS_BASE=$(jq -r '.ldflags[]' "$BUILD_CONFIG" 2>/dev/null | tr '\n' ' ') || true
LDFLAGS_BASE="-L$INSTALL_PREFIX/lib $LDFLAGS_BASE"

# Build configure options
configure_opts=$(jq -r '.configure_options[]' "$BUILD_CONFIG" 2>/dev/null | tr '\n' ' ') || true

# Process source header patches
patch_count=$(jq '.source_header_patches | length' "$BUILD_CONFIG" 2>/dev/null) || patch_count=0
if [ "$patch_count" -gt 0 ]; then
    log_info "Applying source header patches..."
    for ((i=0; i<patch_count; i++)); do
        src=$(jq -r ".source_header_patches[$i].source" "$BUILD_CONFIG")
        patch_line=$(jq -r ".source_header_patches[$i].patch_line" "$BUILD_CONFIG")
        insert_before=$(jq -r ".source_header_patches[$i].insert_before" "$BUILD_CONFIG")
        
        if [ -f "$SCRIPT_DIR/$src" ]; then
            # Check if patch already applied
            if ! grep -q "$insert_before" "$SCRIPT_DIR/$src"; then
                # Patch the actual source file
                sed -i "/$patch_line/i $insert_before" "$SCRIPT_DIR/$src"
                log_info "  ✓ Patched source file: $src"
            else
                log_info "  ✓ Patch already applied: $src"
            fi
        fi
    done
fi

# Set build environment
export PKG_CONFIG_PATH="$INSTALL_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="$INSTALL_PREFIX/lib:$LD_LIBRARY_PATH"
export CFLAGS="$CFLAGS_BASE"
export LDFLAGS="$LDFLAGS_BASE"

log_info "Build Environment:"
log_info "  Headers: $HEADER_PREFIX"
log_info "  Libraries: $INSTALL_PREFIX/lib"
log_info "  Jobs: $BUILD_JOBS"

# Navigate to component directory
cd "$SCRIPT_DIR"

# Clean previous build
log_info "Cleaning previous build..."
make clean 2>/dev/null || true
rm -rf autom4te.cache config.log config.status Makefile

# Run autogen if it exists
if [ -f "autogen.sh" ]; then
    log_info "Running autogen.sh..."
    ./autogen.sh
fi

# Configure
log_info "Configuring..."
./configure --prefix="$INSTALL_PREFIX" $configure_opts

# Build
log_info "Building..."
make -j"$BUILD_JOBS"

# Install (optional)
if [ "$1" == "install" ]; then
    log_info "Installing..."
    make install
    log_info "✓ Installation complete"
fi

echo ""
echo "=========================================="
log_info "Build Complete!"
echo "=========================================="
log_info "Build artifacts in: $SCRIPT_DIR"
if [ "$1" == "install" ]; then
    log_info "Installed to: $INSTALL_PREFIX"
fi
echo "=========================================="

exit 0
