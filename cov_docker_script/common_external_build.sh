#!/bin/bash

################################################################################
# Generic External Build Script
# 
# Purpose: Entry point for building any component when used as a dependency
#   - Called by other components that depend on this component
#   - Sets up all dependencies using setup_dependencies.sh
#   - Builds native component using build_native.sh
#   - Provides complete build for external consumers
#   - Fully generic - works with any component via component_config.json
#
# Usage: ./common_external_build.sh
################################################################################

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPONENT_ROOT="$(dirname "$SCRIPT_DIR")"
CONFIG_FILE="$SCRIPT_DIR/component_config.json"

# Color output
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_step() { echo -e "${BLUE}[STEP]${NC} $1"; }

################################################################################
# Main execution
################################################################################
main() {
    # Read component name from config
    COMPONENT_NAME=$(jq -r '.native_component.name' "$CONFIG_FILE")
    
    echo ""
    echo "=========================================="
    log_step "External Build Script for $COMPONENT_NAME"
    echo "=========================================="
    echo ""
    
    # Step 1: Setup dependencies
    log_step "Step 1: Setting up dependencies..."
    if [ -f "$SCRIPT_DIR/setup_dependencies.sh" ]; then
        chmod +x "$SCRIPT_DIR/setup_dependencies.sh"
        "$SCRIPT_DIR/setup_dependencies.sh"
    else
        log_error "setup_dependencies.sh not found"
        exit 1
    fi
    
    echo ""
    
    # Step 2: Build native component
    log_step "Step 2: Building native component..."
    if [ -f "$SCRIPT_DIR/build_native.sh" ]; then
        chmod +x "$SCRIPT_DIR/build_native.sh"
        "$SCRIPT_DIR/build_native.sh"
    else
        log_error "build_native.sh not found"
        exit 1
    fi
    
    echo ""
    echo "=========================================="
    log_info "External Build Complete!"
    echo "=========================================="
    log_info "Component: $COMPONENT_NAME"
    log_info "Location:  $COMPONENT_ROOT"
    echo "=========================================="
    
    exit 0
}

main "$@"
