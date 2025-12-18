#!/bin/bash

# Dependency Setup Script for Common-Library
# Installs system packages and sets up all dependencies

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HEADERS_CONFIG="${SCRIPT_DIR}/common_library_external_dependencies_headers.json"
BUILD_DEPS_CONFIG="${SCRIPT_DIR}/common_library_external_dependencies_build.json"
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
log_info "Common-Library Dependency Setup"
echo "=========================================="

# Verify required tools are available
if ! command -v jq &> /dev/null; then
    log_error "jq is not installed in the Docker container"
    exit 1
fi

# Clean previous installation
log_info "Cleaning previous installations..."
rm -rf "$HOME/usr"
rm -rf "$HOME/build"
mkdir -p "$HEADER_PREFIX"
mkdir -p "$INSTALL_PREFIX/lib"
log_info "✓ Clean environment ready"

# Clone and setup header-only dependencies
log_info "Setting up header-only dependencies..."

header_count=$(jq '.dependencies | length' "$HEADERS_CONFIG")
for ((i=0; i<header_count; i++)); do
    name=$(jq -r ".dependencies[$i].name" "$HEADERS_CONFIG")
    repo=$(jq -r ".dependencies[$i].repository" "$HEADERS_CONFIG")
    branch=$(jq -r ".dependencies[$i].branch" "$HEADERS_CONFIG")
    
    log_info "Cloning ${name}..."
    cd "$HOME"
    [ -d "$name" ] && rm -rf "$name"
    git clone "$repo" -b "$branch" "$name"
    
    # Copy headers
    header_path_count=$(jq ".dependencies[$i].header_paths | length" "$HEADERS_CONFIG")
    for ((j=0; j<header_path_count; j++)); do
        src=$(jq -r ".dependencies[$i].header_paths[$j].source" "$HEADERS_CONFIG")
        dest=$(jq -r ".dependencies[$i].header_paths[$j].destination" "$HEADERS_CONFIG")
        
        full_src="$HOME/$name/$src"
        full_dest="$HEADER_PREFIX/$dest"
        
        if [ -d "$full_src" ]; then
            mkdir -p "$full_dest"
            cp "$full_src"/*.h "$full_dest/" 2>/dev/null || true
            log_info "  ✓ Copied headers: $src -> $dest"
        fi
    done
done

# Clone and build dependencies
log_info "Building dependencies..."

build_count=$(jq '.dependencies | length' "$BUILD_DEPS_CONFIG")
for ((i=0; i<build_count; i++)); do
    name=$(jq -r ".dependencies[$i].name" "$BUILD_DEPS_CONFIG")
    repo=$(jq -r ".dependencies[$i].repository" "$BUILD_DEPS_CONFIG")
    branch=$(jq -r ".dependencies[$i].branch" "$BUILD_DEPS_CONFIG")
    build_sys=$(jq -r ".dependencies[$i].build_system" "$BUILD_DEPS_CONFIG")
    
    echo ""
    log_info "Processing: ${name}"
    
    # Clone
    cd $HOME
    [ -d "$name" ] && rm -rf "$name"
    git clone "$repo" -b "$branch" "$name"
    
    # Check if external build script is specified
    external_script=$(jq -r ".dependencies[$i].external_build_script // empty" "$BUILD_DEPS_CONFIG")
    
    if [ -n "$external_script" ]; then
        # Use external build script if specified
        log_info "Building with external script: $external_script"
        if [ -f "$HOME/$name/$external_script" ]; then
            cd "$HOME/$name"
            ./"$external_script"
        else
            log_error "External build script not found: $external_script"
            exit 1
        fi
    else
        # Use standard build system
        case "$build_sys" in
            meson)
                log_info "Building with Meson..."
                cd "$HOME/$name"
                meson build --prefix="$INSTALL_PREFIX"
                cd build
                ninja all test || ninja all
                ;;
            cmake)
                log_info "Building with CMake..."
                cmake -H"$HOME/$name" -B"$HOME/build/$name" \
                      -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
                      -DBUILD_FOR_DESKTOP=ON \
                      -DCMAKE_BUILD_TYPE=Debug
                make -j"$BUILD_JOBS" -C "$HOME/build/$name"
                ;;
            autotools)
                log_info "Building with Autotools..."
                cd "$HOME/$name"
                [ -f "autogen.sh" ] && ./autogen.sh
                ./configure --prefix="$INSTALL_PREFIX"
                make -j"$BUILD_JOBS"
                make install
                ;;
            *)
                log_error "Unknown build system: $build_sys"
                exit 1
                ;;
        esac
    fi
    
    # Copy libraries (skip if external script was used, as it handles installation)
    if [ -z "$external_script" ]; then
        lib_count=$(jq ".dependencies[$i].library_patterns | length" "$BUILD_DEPS_CONFIG")
        mkdir -p "$INSTALL_PREFIX/lib"
        for ((j=0; j<lib_count; j++)); do
            pattern=$(jq -r ".dependencies[$i].library_patterns[$j]" "$BUILD_DEPS_CONFIG")
            for lib_file in $HOME/$name/$pattern; do
                if [ -f "$lib_file" ]; then
                    cp "$lib_file" "$INSTALL_PREFIX/lib/"
                    log_info "  ✓ Installed: $(basename $lib_file)"
                fi
            done
        done
    fi
    
    log_info "✓ ${name} completed"
done

# Clean up cloned repositories and build directories
log_info "Cleaning up build artifacts..."
cd $HOME
for ((i=0; i<header_count; i++)); do
    name=$(jq -r ".dependencies[$i].name" "$HEADERS_CONFIG")
    [ -d "$name" ] && rm -rf "$name"
    log_info "  ✓ Removed: $name"
done

for ((i=0; i<build_count; i++)); do
    name=$(jq -r ".dependencies[$i].name" "$BUILD_DEPS_CONFIG")
    [ -d "$name" ] && rm -rf "$name"
    log_info "  ✓ Removed: $name"
done

# Remove build directory
if [ -d "$HOME/build" ]; then
    rm -rf "$HOME/build"
    log_info "  ✓ Removed: build directory"
fi

echo ""
echo "=========================================="
log_info "Dependency Setup Complete!"
echo "=========================================="
log_info "Headers: ${HEADER_PREFIX}"
log_info "Libraries: ${INSTALL_PREFIX}/lib"
log_info "All build artifacts cleaned"
echo "=========================================="

exit 0
