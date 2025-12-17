#!/bin/bash

# External Build Script for Common-Library
# Used when common-library is a dependency of another component
# This script builds all dependencies and the component itself

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_CONFIG="${SCRIPT_DIR}/build_config.json"
HEADER_PREFIX=${HEADER_PREFIX:-$HOME/usr/include/rdkb}
INSTALL_PREFIX=${INSTALL_PREFIX:-$HOME/usr/local}

# Color output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }

echo "=========================================="
log_info "External Build: Common-Library"
echo "=========================================="

# Step 1: Setup dependencies
log_info "Step 1: Setting up dependencies..."
cd "$SCRIPT_DIR"
./setup_dependencies.sh

# Step 2: Build component
log_info "Step 2: Building common-library..."
cd "$SCRIPT_DIR"
./build_native.sh

# Step 3: Install headers
log_info "Step 3: Installing headers..."
header_count=$(jq '.component_header_dirs | length' "$BUILD_CONFIG")
dest_subdir=$(jq -r '.component_install_subdir' "$BUILD_CONFIG")

for ((i=0; i<header_count; i++)); do
    header_dir=$(jq -r ".component_header_dirs[$i]" "$BUILD_CONFIG")
    
    if [ -d "$SCRIPT_DIR/$header_dir" ]; then
        mkdir -p "$HEADER_PREFIX/$dest_subdir"
        cp -r "$SCRIPT_DIR/$header_dir"/*.h "$HEADER_PREFIX/$dest_subdir/" 2>/dev/null || true
        log_info "  ✓ Installed headers from: $header_dir"
    fi
done

# Step 4: Install libraries
log_info "Step 4: Installing libraries..."
lib_count=$(jq '.component_libraries | length' "$BUILD_CONFIG")

for ((i=0; i<lib_count; i++)); do
    pattern=$(jq -r ".component_libraries[$i]" "$BUILD_CONFIG")
    
    for lib_file in $SCRIPT_DIR/$pattern; do
        if [ -f "$lib_file" ]; then
            cp "$lib_file" "$INSTALL_PREFIX/lib/"
            log_info "  ✓ Installed: $(basename $lib_file)"
        fi
    done
done

echo ""
echo "=========================================="
log_info "External Build Complete!"
echo "=========================================="
log_info "Headers: ${HEADER_PREFIX}/${dest_subdir}"
log_info "Libraries: ${INSTALL_PREFIX}/lib"
echo "=========================================="

exit 0
