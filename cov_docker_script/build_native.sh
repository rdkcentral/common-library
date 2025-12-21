#!/bin/bash

################################################################################
# Generic Native Component Build Script
# 
# Purpose: Build any native component based on component_config.json
#   - Reads build configuration from component_config.json
#   - Applies source patches if needed
#   - Supports autotools, CMake, Meson build systems
#   - Produces shared libraries or binaries
#
# Usage: ./build_native.sh
#
# This script is fully generic and driven by component_config.json.
# To use with any component:
#   1. Copy this script and component_config.json to your component's folder
#   2. Update component_config.json with your component's build settings
#   3. Run setup_dependencies.sh first to install dependencies
#   4. Run this script to build your component
################################################################################

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPONENT_ROOT="$(dirname "$SCRIPT_DIR")"
COMPONENT_CONFIG="${SCRIPT_DIR}/component_config.json"

# Installation paths
HEADER_PREFIX="${HEADER_PREFIX:-$HOME/usr/include/rdkb}"
INSTALL_PREFIX="${INSTALL_PREFIX:-$HOME/usr/local}"
COMPONENT_INSTALL_DIR="${COMPONENT_INSTALL_DIR:-$INSTALL_PREFIX}"

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
    
    # Validate required JSON fields for native component
    local required_fields=("native_component" "native_component.name" "native_component.build_system")
    for field in "${required_fields[@]}"; do
        if ! jq -e ".${field}" "$COMPONENT_CONFIG" &>/dev/null; then
            log_error "Missing required field in JSON: ${field}"
            exit 1
        fi
    done
    
    # Check if component root directory exists
    if [ ! -d "$COMPONENT_ROOT" ]; then
        log_error "Component root directory not found: $COMPONENT_ROOT"
        exit 1
    fi
    
    # Validate build system availability
    local build_sys=$(jq -r '.native_component.build_system' "$COMPONENT_CONFIG")
    case "$build_sys" in
        autotools)
            if [ ! -f "$COMPONENT_ROOT/configure" ] && [ ! -f "$COMPONENT_ROOT/configure.ac" ] && [ ! -f "$COMPONENT_ROOT/autogen.sh" ]; then
                log_warn "Autotools files not found in component root. Build may fail."
            fi
            ;;
        cmake)
            if ! command -v cmake &> /dev/null; then
                log_error "cmake is not installed but build_system is set to 'cmake'"
                exit 1
            fi
            ;;
        meson)
            if ! command -v meson &> /dev/null; then
                log_error "meson is not installed but build_system is set to 'meson'"
                exit 1
            fi
            ;;
    esac
    
    log_info "✓ Prerequisites validated"
}

################################################################################
# Setup build environment
################################################################################
setup_build_environment() {
    log_step "Setting up build environment..."
    
    # Read configuration
    local cflags=$(jq -r '.native_component.cflags // [] | join(" ")' "$COMPONENT_CONFIG")
    local system_includes=$(jq -r '.native_component.system_includes // []' "$COMPONENT_CONFIG")
    local ldflags=$(jq -r '.native_component.ldflags // [] | join(" ")' "$COMPONENT_CONFIG")
    
    # Process dependency include directories - supports both absolute and relative paths
    local dependency_includes=""
    local dep_count=$(jq '.native_component.dependency_include_dirs // [] | length' "$COMPONENT_CONFIG")
    for ((i=0; i<dep_count; i++)); do
        local dep_dir=$(jq -r ".native_component.dependency_include_dirs[$i]" "$COMPONENT_CONFIG")
        # Check if it's an absolute path or contains variables
        if [[ "$dep_dir" == /* ]] || [[ "$dep_dir" == \$* ]]; then
            # Absolute path or variable - expand and use as-is
            dep_dir=$(eval echo "$dep_dir")
            dependency_includes="$dependency_includes -I$dep_dir"
        elif [ "$dep_dir" == "." ]; then
            # Current directory - use HEADER_PREFIX
            dependency_includes="$dependency_includes -I$HEADER_PREFIX"
        else
            # Relative path - prefix with HEADER_PREFIX
            dependency_includes="$dependency_includes -I$HEADER_PREFIX/$dep_dir"
        fi
    done
    
    # Build additional library search paths from configuration
    local additional_lib_paths=""
    local lib_path_count=$(jq '.build_options.additional_lib_paths // [] | length' "$COMPONENT_CONFIG")
    for ((i=0; i<lib_path_count; i++)); do
        local lib_path=$(jq -r ".build_options.additional_lib_paths[$i]" "$COMPONENT_CONFIG")
        # Expand variables in path
        lib_path=$(eval echo "$lib_path")
        additional_lib_paths="$additional_lib_paths -L$lib_path"
    done
    
    # Expand system includes with wildcard support
    local expanded_includes=""
    local count=$(echo "$system_includes" | jq 'length')
    for ((i=0; i<count; i++)); do
        local inc=$(echo "$system_includes" | jq -r ".[$i]")
        # Expand wildcards in path
        for expanded_path in $inc; do
            if [ -d "$expanded_path" ]; then
                expanded_includes="$expanded_includes -I$expanded_path"
            fi
        done
    done
    
    # Export build flags
    export CFLAGS="$cflags -I$HEADER_PREFIX $expanded_includes $dependency_includes"
    export LDFLAGS="$ldflags -L$INSTALL_PREFIX/lib $additional_lib_paths"
    export PKG_CONFIG_PATH="$INSTALL_PREFIX/lib/pkgconfig:$INSTALL_PREFIX/lib64/pkgconfig:$PKG_CONFIG_PATH"
    export LD_LIBRARY_PATH="$INSTALL_PREFIX/lib:$INSTALL_PREFIX/lib64:$LD_LIBRARY_PATH"
    
    # Add additional library paths to LD_LIBRARY_PATH
    for ((i=0; i<lib_path_count; i++)); do
        local lib_path=$(jq -r ".build_options.additional_lib_paths[$i]" "$COMPONENT_CONFIG")
        lib_path=$(eval echo "$lib_path")
        export LD_LIBRARY_PATH="$lib_path:$LD_LIBRARY_PATH"
    done
    
    log_info "CFLAGS: $CFLAGS"
    log_info "LDFLAGS: $LDFLAGS"
    log_info "✓ Build environment ready"
}

################################################################################
# Apply source patches
################################################################################
apply_source_patches() {
    log_step "Applying source patches..."
    
    local patch_count=$(jq '.native_component.source_header_patches // [] | length' "$COMPONENT_CONFIG")
    
    if [ "$patch_count" -eq 0 ]; then
        log_info "No patches to apply"
        return
    fi
    
    for ((i=0; i<patch_count; i++)); do
        local source=$(jq -r ".native_component.source_header_patches[$i].source" "$COMPONENT_CONFIG")
        
        # Support both absolute paths and relative paths
        local target_file
        if [[ "$source" == /* ]] || [[ "$source" == \$HOME* ]]; then
            # Expand variables in path
            target_file=$(eval echo "$source")
        else
            target_file="$COMPONENT_ROOT/$source"
        fi
        
        if [ ! -f "$target_file" ]; then
            log_warn "Patch target not found: $target_file"
            continue
        fi
        
        # Check patch type: sed_command, insert_before or search/replace
        local patch_line=$(jq -r ".native_component.source_header_patches[$i].patch_line // \"null\"" "$COMPONENT_CONFIG")
        local insert_before=$(jq -r ".native_component.source_header_patches[$i].insert_before // \"null\"" "$COMPONENT_CONFIG")
        local search=$(jq -r ".native_component.source_header_patches[$i].search // \"null\"" "$COMPONENT_CONFIG")
        local replace=$(jq -r ".native_component.source_header_patches[$i].replace // \"null\"" "$COMPONENT_CONFIG")
        local sed_command=$(jq -r ".native_component.source_header_patches[$i].sed_command // \"null\"" "$COMPONENT_CONFIG")
        
        if [ "$sed_command" != "null" ]; then
            # Direct sed command for complex replacements
            eval "sed -i '$sed_command' '$target_file'" && log_info "  ✓ Applied sed patch: $source" || log_warn "  Failed to apply sed patch: $source"
        elif [ "$search" != "null" ] && [ "$replace" != "null" ]; then
            # Search/Replace patch
            if grep -qF "$replace" "$target_file"; then
                log_info "  ✓ Patch already applied: $source"
                continue
            fi
            
            if grep -qF "$search" "$target_file"; then
                # Use awk for safer string replacement
                awk -v search="$search" -v replace="$replace" '{
                    if (index($0, search) > 0) {
                        sub(search, replace);
                    }
                    print;
                }' "$target_file" > "$target_file.tmp" && mv "$target_file.tmp" "$target_file"
                log_info "  ✓ Applied replacement patch: $source"
            else
                log_warn "  Search pattern not found in $source: $search"
            fi
        elif [ "$patch_line" != "null" ] && [ "$insert_before" != "null" ]; then
            # Insert before patch
            if grep -q "$insert_before" "$target_file"; then
                log_info "  ✓ Patch already applied: $source"
                continue
            fi
            
            if grep -q "$patch_line" "$target_file"; then
                sed -i "/$patch_line/i $insert_before" "$target_file"
                log_info "  ✓ Applied insert patch: $source"
            else
                log_warn "  Patch line not found in $source: $patch_line"
            fi
        else
            log_warn "  Invalid patch format at index $i"
        fi
    done
    
    log_info "✓ Patches applied"
}

################################################################################
# Run pre-build commands
################################################################################
run_pre_build_commands() {
    log_step "Running pre-build commands..."
    
    local cmd_count=$(jq '.native_component.pre_build_commands // [] | length' "$COMPONENT_CONFIG")
    
    if [ "$cmd_count" -eq 0 ]; then
        log_info "No pre-build commands to run"
        return
    fi
    
    cd "$COMPONENT_ROOT"
    
    for ((i=0; i<cmd_count; i++)); do
        local description=$(jq -r ".native_component.pre_build_commands[$i].description" "$COMPONENT_CONFIG")
        local command=$(jq -r ".native_component.pre_build_commands[$i].command" "$COMPONENT_CONFIG")
        
        # Expand variables in command
        command=$(eval echo "$command")
        
        log_info "  [$((i+1))/$cmd_count] $description"
        
        if eval "$command"; then
            log_info "  ✓ Success: $description"
        else
            log_error "  ✗ Failed: $description"
            return 1
        fi
    done
    
    log_info "✓ Pre-build commands completed"
}

################################################################################
# Configure component
################################################################################
configure_component() {
    log_step "Configuring component..."
    
    cd "$COMPONENT_ROOT"
    
    # Run autogen if exists
    if [ -f "autogen.sh" ]; then
        log_info "Running autogen.sh..."
        ./autogen.sh
    fi
    
    # Read configure options
    local configure_opts=$(jq -r '.native_component.configure_options // [] | join(" ")' "$COMPONENT_CONFIG")
    
    # Configure
    log_info "Running configure..."
    ./configure --prefix="$COMPONENT_INSTALL_DIR" $configure_opts
    
    log_info "✓ Configuration complete"
}

################################################################################
# Build component
################################################################################
build_component() {
    log_step "Building component..."
    
    cd "$COMPONENT_ROOT"
    
    # Clean previous build artifacts
    if [ -f "Makefile" ]; then
        log_info "Running make clean..."
        make clean > /dev/null 2>&1 || true
    fi
    
    log_info "Running make..."
    make -j"$BUILD_JOBS"
    
    log_info "✓ Build complete"
}

################################################################################
# Install component libraries
################################################################################
install_component_libraries() {
    log_step "Installing component libraries..."
    
    local lib_patterns=$(jq -r '.native_component.component_libraries // [] | join(" ")' "$COMPONENT_CONFIG")
    local install_subdir=$(jq -r '.native_component.component_install_subdir // ""' "$COMPONENT_CONFIG")
    local auto_discover=$(jq -r '.build_options.auto_discover_libraries // false' "$COMPONENT_CONFIG")
    
    # Determine installation directory
    local install_dir="$COMPONENT_INSTALL_DIR/lib"
    if [ -n "$install_subdir" ] && [ "$install_subdir" != "null" ]; then
        install_dir="$COMPONENT_INSTALL_DIR/lib/$install_subdir"
    fi
    
    mkdir -p "$install_dir"
    
    # Copy libraries
    cd "$COMPONENT_ROOT"
    local libs_found=0
    
    # If patterns are specified, use them
    if [ -n "$lib_patterns" ] && [ "$lib_patterns" != "null" ]; then
        for pattern in $lib_patterns; do
            for lib_file in $pattern; do
                if [ -f "$lib_file" ]; then
                    cp -P "$lib_file" "$install_dir/"
                    log_info "  ✓ Installed: $(basename $lib_file)"
                    libs_found=1
                fi
            done
        done
    fi
    
    # If auto_discover is enabled and no libraries found, search dynamically
    if [ "$auto_discover" = "true" ] && [ $libs_found -eq 0 ]; then
        log_info "Searching for shared libraries dynamically..."
        
        # Find all shared/dynamic libraries: .so, .so.*, .a, .la
        while IFS= read -r lib_file; do
            if [ -f "$lib_file" ]; then
                cp -P "$lib_file" "$install_dir/"
                log_info "  ✓ Installed: $(basename $lib_file)"
                libs_found=1
            fi
        done < <(find . -type f \( -name "*.so" -o -name "*.so.*" -o -name "*.a" -o -name "*.la" \) 2>/dev/null)
        
        # Also find symlinks to shared libraries
        while IFS= read -r lib_link; do
            if [ -L "$lib_link" ]; then
                cp -P "$lib_link" "$install_dir/"
                log_info "  ✓ Installed: $(basename $lib_link) (symlink)"
                libs_found=1
            fi
        done < <(find . -type l \( -name "*.so" -o -name "*.so.*" \) 2>/dev/null)
    fi
    
    if [ $libs_found -eq 0 ]; then
        log_warn "No libraries found (patterns: $lib_patterns)"
    else
        log_info "✓ Libraries installed to: $install_dir"
    fi
}

################################################################################
# Install component headers
################################################################################
install_component_headers() {
    log_step "Installing component headers..."
    
    local header_count=$(jq '.native_component.component_header_dirs // [] | length' "$COMPONENT_CONFIG")
    
    if [ "$header_count" -eq 0 ]; then
        log_info "No component headers to install"
        return
    fi
    
    for ((i=0; i<header_count; i++)); do
        local src=$(jq -r ".native_component.component_header_dirs[$i].source" "$COMPONENT_CONFIG")
        local dest=$(jq -r ".native_component.component_header_dirs[$i].destination" "$COMPONENT_CONFIG")
        
        local full_src="$COMPONENT_ROOT/$src"
        local full_dest="$HEADER_PREFIX/$dest"
        
        if [ -d "$full_src" ]; then
            mkdir -p "$full_dest"
            cp -r "$full_src"/*.h "$full_dest/" 2>/dev/null || true
            log_info "  ✓ Installed headers: $src → $dest"
        else
            log_warn "  Header source not found: $src"
        fi
    done
    
    log_info "✓ Headers installed"
}

################################################################################
# Display summary
################################################################################
display_summary() {
    local component_name=$(jq -r '.native_component.name // "unknown"' "$COMPONENT_CONFIG")
    
    echo ""
    echo "=========================================="
    log_info "Native Component Build Complete!"
    echo "=========================================="
    log_info "Component:  $component_name"
    log_info "Install:    $COMPONENT_INSTALL_DIR"
    echo "=========================================="
}

################################################################################
# Main execution
################################################################################
main() {
    local component_name=$(jq -r '.native_component.name // "Component"' "$COMPONENT_CONFIG")
    
    echo "=========================================="
    log_step "Building Native Component: $component_name"
    echo "=========================================="
    
    validate_prerequisites
    setup_build_environment
    apply_source_patches
    run_pre_build_commands
    configure_component
    build_component
    install_component_libraries
    install_component_headers
    display_summary
    
    exit 0
}

main "$@"
