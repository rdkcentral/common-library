#!/bin/bash

################################################################################
# Generic Dependency Setup Script
# 
# Purpose: Setup all dependencies for native component builds
#   - Clone and copy header-only dependencies
#   - Clone, build, and install shared library dependencies
#   - Clean up all build artifacts after installation
#
# Usage: ./setup_dependencies.sh
#
# This script is fully generic and driven by component_config.json.
# To use with any component:
#   1. Copy this script and component_config.json to your component's folder
#   2. Update component_config.json with your component's dependencies
#   3. Run this script from within the folder containing component_config.json
################################################################################

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPONENT_CONFIG="${SCRIPT_DIR}/component_config.json"

# Installation paths
HEADER_PREFIX="${HEADER_PREFIX:-$HOME/usr/include/rdkb}"
INSTALL_PREFIX="${INSTALL_PREFIX:-$HOME/usr/local}"
BUILD_DIR="${BUILD_DIR:-$HOME/build}"

# Build settings
BUILD_JOBS="${BUILD_JOBS:-$(nproc)}"

# Color output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_step() { echo -e "${BLUE}[STEP]${NC} $1"; }

################################################################################
# Validate prerequisites
################################################################################
validate_prerequisites() {
    log_step "Validating prerequisites..."
    
    # Check for jq
    if ! command -v jq &> /dev/null; then
        log_error "jq is not installed. Please install it: sudo apt-get install jq"
        exit 1
    fi
    
    # Check for git
    if ! command -v git &> /dev/null; then
        log_error "git is not installed. Please install it: sudo apt-get install git"
        exit 1
    fi
    
    # Check config file exists
    if [ ! -f "$COMPONENT_CONFIG" ]; then
        log_error "Component config not found: $COMPONENT_CONFIG"
        log_error "Please ensure component_config.json exists in the script directory"
        exit 1
    fi
    
    # Validate JSON syntax
    if ! jq empty "$COMPONENT_CONFIG" 2>/dev/null; then
        log_error "Invalid JSON syntax in: $COMPONENT_CONFIG"
        exit 1
    fi
    
    # Validate required JSON fields
    local required_fields=("build_options" "header_dependencies" "library_dependencies")
    for field in "${required_fields[@]}"; do
        if ! jq -e ".${field}" "$COMPONENT_CONFIG" &>/dev/null; then
            log_error "Missing required field in JSON: ${field}"
            exit 1
        fi
    done
    
    log_info "✓ Prerequisites validated"
}

################################################################################
# Setup environment
################################################################################
setup_environment() {
    log_step "Setting up environment..."
    
    # Export paths for dependency builds
    export PKG_CONFIG_PATH="$INSTALL_PREFIX/lib/pkgconfig:$INSTALL_PREFIX/lib64/pkgconfig:$PKG_CONFIG_PATH"
    export LD_LIBRARY_PATH="$INSTALL_PREFIX/lib:$INSTALL_PREFIX/lib64:$LD_LIBRARY_PATH"
    export CFLAGS="-I$HEADER_PREFIX"
    export LDFLAGS="-L$INSTALL_PREFIX/lib"
    
    # Clean previous installations (if enabled)
    local clean_before=$(jq -r '.build_options.clean_before_build // true' "$COMPONENT_CONFIG")
    if [ "$clean_before" = "true" ]; then
        log_info "Cleaning previous installations..."
        
        if [ -d "$HOME/usr" ]; then
            log_info "  Removing: $HOME/usr"
            chmod -R u+w "$HOME/usr" 2>/dev/null || true
            rm -rf "$HOME/usr" || {
                log_warn "Failed to remove $HOME/usr, trying with sudo..."
                sudo rm -rf "$HOME/usr"
            }
        fi
        
        if [ -d "$BUILD_DIR" ]; then
            log_info "  Removing: $BUILD_DIR"
            chmod -R u+w "$BUILD_DIR" 2>/dev/null || true
            rm -rf "$BUILD_DIR" || {
                log_warn "Failed to remove $BUILD_DIR, trying with sudo..."
                sudo rm -rf "$BUILD_DIR"
            }
        fi
    else
        log_info "Skipping cleanup (clean_before_build=false)"
    fi
    
    # Create directory structure
    mkdir -p "$HEADER_PREFIX"
    mkdir -p "$INSTALL_PREFIX/lib"
    mkdir -p "$INSTALL_PREFIX/lib64"
    mkdir -p "$BUILD_DIR"
    
    log_info "✓ Environment ready"
}

################################################################################
# Process header-only dependencies
################################################################################
process_header_dependencies() {
    # Check if we should skip header dependencies
    local skip_headers=$(jq -r '.build_options.skip_header_dependencies // false' "$COMPONENT_CONFIG")
    if [ "$skip_headers" = "true" ]; then
        log_info "Skipping header dependencies (skip_header_dependencies=true)"
        return
    fi
    
    log_step "Processing header-only dependencies..."
    
    local count=$(jq '.header_dependencies | length' "$COMPONENT_CONFIG")
    
    if [ "$count" -eq 0 ]; then
        log_warn "No header dependencies found"
        return
    fi
    
    for ((i=0; i<count; i++)); do
        local name=$(jq -r ".header_dependencies[$i].name" "$COMPONENT_CONFIG")
        local repo=$(jq -r ".header_dependencies[$i].repository" "$COMPONENT_CONFIG")
        local branch=$(jq -r ".header_dependencies[$i].branch" "$COMPONENT_CONFIG")
        
        echo ""
        log_info "[$((i+1))/$count] Processing: $name"
        
        # Clone repository
        cd "$BUILD_DIR"
        log_info "  Cloning $name..."
        if ! git clone --depth 1 "$repo" -b "$branch" "$name" > /dev/null 2>&1; then
            log_error "Failed to clone $name from $repo (branch: $branch)"
            exit 1
        fi
        
        # Copy headers
        local header_count=$(jq ".header_dependencies[$i].header_paths | length" "$COMPONENT_CONFIG")
        for ((j=0; j<header_count; j++)); do
            local src=$(jq -r ".header_dependencies[$i].header_paths[$j].source" "$COMPONENT_CONFIG")
            local dest=$(jq -r ".header_dependencies[$i].header_paths[$j].destination" "$COMPONENT_CONFIG")
            
            local full_src="$BUILD_DIR/$name/$src"
            local full_dest="$HEADER_PREFIX/$dest"
            
            if [ -d "$full_src" ]; then
                mkdir -p "$full_dest"
                # Copy header files
                find "$full_src" -maxdepth 1 -name "*.h" -exec cp {} "$full_dest/" \; 2>/dev/null || true
                # Also copy Python files for code generation tools
                find "$full_src" -maxdepth 1 -name "*.py" -exec cp {} "$full_dest/" \; 2>/dev/null || true
                log_info "  ✓ Copied: $src → $dest"
            else
                log_warn "  Source path not found: $src"
            fi
        done
        
        # Cleanup cloned repository
        rm -rf "$BUILD_DIR/$name"
        log_info "  ✓ Cleaned up $name"
    done
    
    # Apply critical header patches needed by library dependencies
    log_info "Applying header patches..."
    local ccsp_msg_bus_header="$HEADER_PREFIX/ccsp_message_bus.h"
    if [ -f "$ccsp_msg_bus_header" ]; then
        if ! grep -q "typedef struct DBusLoop DBusLoop" "$ccsp_msg_bus_header"; then
            sed -i '/typedef struct _CCSP_MESSAGE_BUS_CONNECTION/i typedef struct DBusLoop DBusLoop;' "$ccsp_msg_bus_header"
            log_info "  ✓ Patched ccsp_message_bus.h with DBusLoop typedef"
        else
            log_info "  ✓ ccsp_message_bus.h already patched"
        fi
    fi
    
    log_info "✓ Header dependencies completed"
}

################################################################################
# Apply source patches to a library dependency
################################################################################
apply_library_patches() {
    local dep_index=$1
    local build_dir=$2
    
    local patch_count=$(jq ".library_dependencies[$dep_index].source_patches // [] | length" "$COMPONENT_CONFIG")
    
    if [ "$patch_count" -eq 0 ]; then
        return
    fi
    
    log_info "  Applying source patches..."
    
    for ((i=0; i<patch_count; i++)); do
        local file=$(jq -r ".library_dependencies[$dep_index].source_patches[$i].file" "$COMPONENT_CONFIG")
        local patch_type=$(jq -r ".library_dependencies[$dep_index].source_patches[$i].type // \"insert\"" "$COMPONENT_CONFIG")
        
        local target_file="$build_dir/$file"
        
        if [ ! -f "$target_file" ]; then
            log_warn "    Patch target not found: $file"
            continue
        fi
        
        if [ "$patch_type" == "replace" ]; then
            # Search/Replace patch
            local search=$(jq -r ".library_dependencies[$dep_index].source_patches[$i].search" "$COMPONENT_CONFIG")
            local replace=$(jq -r ".library_dependencies[$dep_index].source_patches[$i].replace" "$COMPONENT_CONFIG")
            
            if grep -qF -- "$replace" "$target_file"; then
                log_info "    ✓ Patch already applied: $file"
                continue
            fi
            
            if grep -qF -- "$search" "$target_file"; then
                # Escape special sed regex chars (using | as delimiter)
                local search_escaped=$(printf '%s\n' "$search" | sed 's/[[\.*^$|]/\\&/g')
                local replace_escaped=$(printf '%s\n' "$replace" | sed 's/[|&]/\\&/g')
                
                sed -i "s|$search_escaped|$replace_escaped|g" "$target_file"
                log_info "    ✓ Applied replacement patch: $file"
            else
                log_warn "    Search pattern not found in $file"
            fi
        elif [ "$patch_type" == "insert" ]; then
            # Insert before patch
            local patch_line=$(jq -r ".library_dependencies[$dep_index].source_patches[$i].patch_line" "$COMPONENT_CONFIG")
            local insert_before=$(jq -r ".library_dependencies[$dep_index].source_patches[$i].insert_before" "$COMPONENT_CONFIG")
            
            if grep -qF "$insert_before" "$target_file"; then
                log_info "    ✓ Patch already applied: $file"
                continue
            fi
            
            if grep -qF "$patch_line" "$target_file"; then
                sed -i "/$patch_line/i $insert_before" "$target_file"
                log_info "    ✓ Applied insert patch: $file"
            else
                log_warn "    Patch line not found in $file"
            fi
        else
            log_warn "    Invalid patch type: $patch_type"
        fi
    done
}

################################################################################
# Build shared library dependency
################################################################################
build_library_dependency() {
    local dep_index=$1
    local name=$2
    local build_sys=$3
    local cmake_opts=$4
    local meson_opts=$5
    local autotools_opts=$6
    local source_subdir=$7
    
    # Apply source patches if any
    apply_library_patches "$dep_index" "$BUILD_DIR/$name"
    
    # Change to source subdirectory if specified
    local build_path="$BUILD_DIR/$name"
    if [ -n "$source_subdir" ] && [ "$source_subdir" != "null" ]; then
        build_path="$BUILD_DIR/$name/$source_subdir"
        log_info "  Using source subdirectory: $source_subdir"
    fi
    
    case "$build_sys" in
        cmake)
            log_info "  Building with CMake..."
            
            # Clean build directory if requested
            local clean_before=$(jq -r '.build_options.clean_before_build // false' "$COMPONENT_CONFIG")
            if [ "$clean_before" = "true" ] && [ -d "$BUILD_DIR/${name}_build" ]; then
                log_info "  Cleaning previous CMake build..."
                rm -rf "$BUILD_DIR/${name}_build"
            fi
            
            local cmake_args="-DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX"
            if [ -n "$cmake_opts" ] && [ "$cmake_opts" != "null" ]; then
                local opts_count=$(echo "$cmake_opts" | jq 'length')
                for ((k=0; k<opts_count; k++)); do
                    local opt=$(echo "$cmake_opts" | jq -r ".[$k]")
                    cmake_args="$cmake_args $opt"
                done
            fi
            
            cmake -H"$build_path" -B"$BUILD_DIR/${name}_build" $cmake_args
            make -j"$BUILD_JOBS" -C "$BUILD_DIR/${name}_build"
            make -C "$BUILD_DIR/${name}_build" install
            ;;
            
        meson)
            log_info "  Building with Meson..."
            
            cd "$build_path"
            
            # Clean build directory if requested
            local clean_before=$(jq -r '.build_options.clean_before_build // false' "$COMPONENT_CONFIG")
            if [ "$clean_before" = "true" ] && [ -d "build" ]; then
                log_info "  Cleaning previous Meson build..."
                rm -rf build
            fi
            
            local meson_args="--prefix=$INSTALL_PREFIX"
            if [ -n "$meson_opts" ] && [ "$meson_opts" != "null" ]; then
                local opts_count=$(echo "$meson_opts" | jq 'length')
                for ((k=0; k<opts_count; k++)); do
                    local opt=$(echo "$meson_opts" | jq -r ".[$k]")
                    meson_args="$meson_args $opt"
                done
            fi
            
            meson build $meson_args
            cd build
            ninja all test || ninja all
            ninja install
            ;;
            
        autotools)
            log_info "  Building with Autotools..."
            
            cd "$build_path"
            
            # Clean if requested and Makefile exists
            local clean_before=$(jq -r '.build_options.clean_before_build // false' "$COMPONENT_CONFIG")
            if [ "$clean_before" = "true" ] && [ -f "Makefile" ]; then
                log_info "  Running make clean..."
                make clean || true
            fi
            
            # Check if this directory has configure.ac/configure.in
            if [ -f "configure.ac" ] || [ -f "configure.in" ]; then
                # Copy autogen.sh from repository root if not present
                if [ ! -f "autogen.sh" ] && [ -f "$BUILD_DIR/$name/autogen.sh" ]; then
                    log_info "  Copying autogen.sh from repository root..."
                    cp "$BUILD_DIR/$name/autogen.sh" .
                fi
                
                # Run autogen.sh if it exists, otherwise run autoreconf
                if [ -f "autogen.sh" ]; then
                    log_info "  Running autogen.sh..."
                    ./autogen.sh
                else
                    log_info "  Running autoreconf..."
                    autoreconf -fi
                fi
            elif [ -n "$source_subdir" ] && [ "$source_subdir" != "null" ]; then
                # No configure.ac in subdirectory, run autogen at root then return to subdir
                log_info "  Running autogen.sh at repository root..."
                cd "$BUILD_DIR/$name"
                if [ -f "autogen.sh" ]; then
                    ./autogen.sh
                elif [ -f "configure.ac" ] || [ -f "configure.in" ]; then
                    autoreconf -fi
                fi
                cd "$build_path"
            fi
            
            # Process configure options - separate env vars from regular options
            local configure_args="--prefix=$INSTALL_PREFIX"
            if [ -n "$autotools_opts" ] && [ "$autotools_opts" != "null" ]; then
                local opts_count=$(echo "$autotools_opts" | jq 'length')
                for ((k=0; k<opts_count; k++)); do
                    local opt=$(echo "$autotools_opts" | jq -r ".[$k]")
                    # Expand environment variables in the option
                    opt=$(eval echo "$opt")
                    
                    # Check if this is an environment variable assignment (VAR=value)
                    if [[ "$opt" =~ ^([A-Z_]+)=(.*)$ ]]; then
                        local var_name="${BASH_REMATCH[1]}"
                        local var_value="${BASH_REMATCH[2]}"
                        export "${var_name}=${var_value}"
                        log_info "  Exported ${var_name}=${var_value}"
                    else
                        # Regular configure option (like --enable-foo)
                        configure_args="$configure_args $opt"
                    fi
                done
            fi
            
            # Run configure if it exists
            if [ -f "configure" ]; then
                ./configure $configure_args
            fi
            
            make -j"$BUILD_JOBS"
            make install
            ;;
            
        custom)
            log_info "  Using custom build system (dependency_script will handle build)..."
            # For custom build systems, the dependency_script handles everything
            # Libraries will be copied from the build directory later
            ;;
            
        *)
            log_error "Unknown build system: $build_sys"
            exit 1
            ;;
    esac
}

################################################################################
# Process shared library dependencies
################################################################################
process_library_dependencies() {
    # Check if we should skip library dependencies
    local skip_libraries=$(jq -r '.build_options.skip_library_dependencies // false' "$COMPONENT_CONFIG")
    if [ "$skip_libraries" = "true" ]; then
        log_info "Skipping library dependencies (skip_library_dependencies=true)"
        return
    fi
    
    log_step "Processing shared library dependencies..."
    
    local count=$(jq '.library_dependencies | length' "$COMPONENT_CONFIG")
    
    if [ "$count" -eq 0 ]; then
        log_warn "No library dependencies found"
        return
    fi
    
    for ((i=0; i<count; i++)); do
        local name=$(jq -r ".library_dependencies[$i].name" "$COMPONENT_CONFIG")
        local repo=$(jq -r ".library_dependencies[$i].repository" "$COMPONENT_CONFIG")
        local branch=$(jq -r ".library_dependencies[$i].branch" "$COMPONENT_CONFIG")
        local build_sys=$(jq -r ".library_dependencies[$i].build_system" "$COMPONENT_CONFIG")
        local dep_script=$(jq -r ".library_dependencies[$i].dependency_script" "$COMPONENT_CONFIG")
        
        echo ""
        log_info "[$((i+1))/$count] Processing: $name"
        
        # Check if already installed
        if [ -f "$INSTALL_PREFIX/.installed_$name" ]; then
            log_info "  ✓ $name already installed (skipping)"
            continue
        fi
        
        # Clone repository if not already present
        cd "$BUILD_DIR"
        if [ -d "$name" ]; then
            log_info "  Repository already cloned: $name"
        else
            log_info "  Cloning $name..."
            if ! git clone --depth 1 "$repo" -b "$branch" "$name" > /dev/null 2>&1; then
                log_error "Failed to clone $name from $repo (branch: $branch)"
                exit 1
            fi
        fi
        
        # Run dependency script if specified
        if [ -n "$dep_script" ] && [ "$dep_script" != "null" ]; then
            log_info "  Running dependency script: $dep_script"
            cd "$BUILD_DIR/$name"
            if [ -f "$dep_script" ]; then
                chmod +x "$dep_script"
                # Set a different BUILD_DIR for nested builds to avoid self-deletion
                export BUILD_DIR="$BUILD_DIR/${name}_deps"
                ./"$dep_script"
                log_info "  ✓ Dependency script completed"
            else
                log_error "Dependency script not found: $dep_script"
                exit 1
            fi
        else
            # Get build options
            local cmake_opts=$(jq -c ".library_dependencies[$i].cmake_options // null" "$COMPONENT_CONFIG")
            local meson_opts=$(jq -c ".library_dependencies[$i].meson_options // null" "$COMPONENT_CONFIG")
            local autotools_opts=$(jq -c ".library_dependencies[$i].configure_options // null" "$COMPONENT_CONFIG")
            local source_subdir=$(jq -r ".library_dependencies[$i].source_subdir // null" "$COMPONENT_CONFIG")
            
            # Build the dependency using standard build system
            build_library_dependency "$i" "$name" "$build_sys" "$cmake_opts" "$meson_opts" "$autotools_opts" "$source_subdir"
        fi
        
        # Create installation marker
        touch "$INSTALL_PREFIX/.installed_$name"
        
        log_info "  ✓ $name built and installed"
    done
    
    log_info "✓ Library dependencies completed"
}

################################################################################
# Cleanup build artifacts
################################################################################
cleanup_build_artifacts() {
    log_step "Cleaning up build artifacts..."
    
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        log_info "✓ Build directory removed"
    fi
}

################################################################################
# Display summary
################################################################################
display_summary() {
    echo ""
    echo "=========================================="
    log_info "Dependency Setup Complete!"
    echo "=========================================="
    log_info "Headers:   $HEADER_PREFIX"
    log_info "Libraries: $INSTALL_PREFIX/lib"
    echo "=========================================="
}

################################################################################
# Main execution
################################################################################
main() {
    echo "=========================================="
    log_step "Dependency Setup Script"
    echo "=========================================="
    
    validate_prerequisites
    setup_environment
    process_header_dependencies
    process_library_dependencies
    cleanup_build_artifacts
    display_summary
    
    exit 0
}

main "$@"
