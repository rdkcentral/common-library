# Common-Library JSON-Driven Build System

Complete documentation for the JSON-based configuration system for building common-library and using it as a dependency in other components.

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Architecture](#architecture)
4. [Configuration Files](#configuration-files)
5. [Build Scripts](#build-scripts)
6. [Building Common-Library](#building-common-library)
7. [Adding New Dependencies](#adding-new-dependencies)
8. [Using Common-Library as a Dependency](#using-common-library-as-a-dependency)
9. [External Build Script Support](#external-build-script-support)
10. [Installation Paths](#installation-paths)
11. [Troubleshooting](#troubleshooting)

---

## Overview

This system provides a completely JSON-driven approach to building native components with automatic dependency management. Key features:

- ✓ All configuration in human-readable JSON files
- ✓ Automatic dependency resolution and building
- ✓ Support for multiple build systems (Meson, CMake, Autotools)
- ✓ External build script support for recursive dependency builds
- ✓ Standardized installation paths
- ✓ Source header patching capability
- ✓ Automatic cleanup of build artifacts

---

## Quick Start

### Building Common-Library (Standalone)

```bash
cd /path/to/common-library

# Step 1: Setup all dependencies (clones, builds, installs trower-base64, rbus, rdk_logger)
./setup_dependencies.sh

# Step 2: Build common-library
./build_native.sh

# Optional: Build and install common-library
./build_native.sh install
```

### Using Common-Library in Another Component

```bash
cd /path/to/component-x

# Step 1: Add common-library to X_build_deps.json with external_build_script
# (See example in example_component_x/X_build_deps.json)

# Step 2: Setup dependencies (this will build common-library and all its dependencies)
./setup_dependencies.sh

# Step 3: Build Component X
./build_native.sh
```

### Script Usage Reference

| Script | Purpose | Arguments | When to Use |
|--------|---------|-----------|-------------|
| `setup_dependencies.sh` | Install all dependencies | None | Before building component |
| `build_native.sh` | Build the component | `install` (optional) | After dependencies are set up |
| `common_external_build.sh` | External build handler | None | Automatically called when used as dependency |

---

## Architecture

### Component Structure

```
common-library/
├── common_library_headers.json      # Header-only dependencies
├── common_library_build_deps.json   # Build dependencies
├── build_config.json                # Component build configuration
├── setup_dependencies.sh            # Dependency setup script
├── build_native.sh                  # Component build script
└── common_external_build.sh         # External build script (for use as dependency)
```

### Build Flow

```
1. setup_dependencies.sh
   ├── Reads common_library_headers.json
   ├── Clones and copies header-only dependencies
   ├── Reads common_library_build_deps.json
   ├── For each dependency:
   │   ├── Check if external_build_script is specified
   │   ├── If YES: Run external build script (recursive build)
   │   └── If NO: Build with standard build system
   └── Clean up cloned repositories

2. build_native.sh
   ├── Reads build_config.json
   ├── Applies source header patches
   ├── Builds CFLAGS and LDFLAGS dynamically
   ├── Configures and builds component
   └── Optionally installs

3. common_external_build.sh (when used as dependency)
   ├── Runs setup_dependencies.sh
   ├── Runs build_native.sh
   └── Installs headers and libraries
```

---

## Configuration Files

### 1. common_library_headers.json

Defines header-only dependencies (no compilation required).

```json
{
  "dependencies": [
    {
      "name": "rdk_logger",
      "repository": "https://github.com/rdkcentral/rdk_logger.git",
      "branch": "main",
      "header_paths": [
        {
          "source": "include",
          "destination": "rdk_logger"
        }
      ]
    }
  ]
}
```

**Fields:**
- `name`: Dependency name
- `repository`: Git repository URL
- `branch`: Git branch to clone
- `header_paths`: Array of source/destination header paths
  - `source`: Path to headers within the cloned repo
  - `destination`: Subdirectory under `$HOME/usr/include/rdkb/`

### 2. common_library_build_deps.json

Defines dependencies that require compilation.

```json
{
  "dependencies": [
    {
      "name": "trower-base64",
      "repository": "https://github.com/xmidt-org/trower-base64.git",
      "branch": "main",
      "build_system": "meson",
      "external_build_script": "",
      "library_patterns": [
        "build/libtrower-base64.so*"
      ]
    }
  ]
}
```

**Fields:**
- `name`: Dependency name
- `repository`: Git repository URL
- `branch`: Git branch to clone
- `build_system`: Build system type (`meson`, `cmake`, `autotools`)
- `external_build_script`: Path to external build script (empty if not used)
- `library_patterns`: Glob patterns to find built libraries

**External Build Script:**
- If `external_build_script` is **empty** (`""`): Use standard `build_system`
- If `external_build_script` has **value** (`"script.sh"`): Run that script for recursive builds

### 3. build_config.json

Component build and installation configuration.

```json
{
  "component_name": "common-library",
  "build_config": {
    "cflags": [
      "-DSAFEC_DUMMY_API",
      "-D_ANSC_USE_OPENSSL_"
    ],
    "header_subdirs": [
      ".",
      "rdk_logger",
      "rbus"
    ],
    "ldflags": [],
    "configure_options": [],
    "source_header_patches": [
      {
        "source": "source/ccsp/include/ccsp_message_bus.h",
        "patch_line": "typedef struct _CCSP_MESSAGE_BUS_CONNECTION",
        "insert_before": "typedef struct DBusLoop DBusLoop;"
      }
    ]
  },
  "installation": {
    "header_dirs": [
      "source/ccsp/include"
    ],
    "library_patterns": [
      "source/.libs/libccsp_common.so*"
    ],
    "destination_subdir": "common-library"
  }
}
```

**Build Config Fields:**
- `cflags`: Array of compiler flags
- `header_subdirs`: Subdirectories under `$HEADER_PREFIX` to add to include path
  - `"."` adds the base `$HEADER_PREFIX` path
- `ldflags`: Array of linker flags
- `configure_options`: Additional autotools configure options
- `source_header_patches`: Patches to apply to source headers before build
  - `source`: Path to source header file
  - `patch_line`: Line to find for insertion point
  - `insert_before`: Text to insert before the patch_line

**Installation Fields:**
- `header_dirs`: Directories containing headers to install
- `library_patterns`: Glob patterns for libraries to install
- `destination_subdir`: Subdirectory under `$HEADER_PREFIX/` for installed headers

---

## Build Scripts

### 1. setup_dependencies.sh

Sets up all dependencies (headers and builds).

**Environment Variables:**
- `HEADER_PREFIX`: Header installation path (default: `$HOME/usr/include/rdkb`)
- `INSTALL_PREFIX`: Library installation path (default: `$HOME/usr/local`)
- `BUILD_JOBS`: Number of parallel build jobs (default: `nproc`)

**What it does:**
1. Verifies `jq` is installed
2. Cleans `$HOME/usr` and `$HOME/build`
3. Clones and copies header-only dependencies
4. Clones and builds compilation dependencies
5. Handles external build scripts if specified
6. Copies built libraries to `$INSTALL_PREFIX/lib`
7. Cleans up all cloned repositories and build directories

### 2. build_native.sh

Builds the component using JSON configuration.

**What it does:**
1. Reads `build_config.json`
2. Applies source header patches to actual source files
3. Dynamically builds CFLAGS with all header subdirectories
4. Dynamically builds LDFLAGS
5. Runs autogen.sh if present
6. Configures with all options from JSON
7. Builds the component
8. Optionally installs (pass `install` argument)

**Usage:**
```bash
./build_native.sh           # Build only
./build_native.sh install   # Build and install
```

### 3. common_external_build.sh

Complete external build script for when common-library is used as a dependency.

**What it does:**
1. Runs `setup_dependencies.sh` (builds all common-library dependencies)
2. Runs `build_native.sh` (builds common-library)
3. Installs headers to `$HEADER_PREFIX/common-library/`
4. Installs libraries to `$INSTALL_PREFIX/lib/`

---

## Building Common-Library

### Standalone Build

```bash
cd common-library

# Step 1: Install all dependencies
./setup_dependencies.sh

# Step 2: Build common-library
./build_native.sh

# Optional: Build and install
./build_native.sh install
```

### What Gets Built

**Dependencies:**
- rdk_logger (headers only)
- trower-base64 (Meson build)
- rbus (CMake build)

**Output:**
- Headers: `$HOME/usr/include/rdkb/`
  - `rdk_logger/`
  - `trower-base64/`
  - `rbus/`
- Libraries: `$HOME/usr/local/lib/`
  - `libtrower-base64.so*`
  - `librbus.so*`
- Component: Built in `common-library/source/.libs/`

---

## Adding New Dependencies

### Adding a Header-Only Dependency

Edit `common_library_headers.json`:

```json
{
  "dependencies": [
    {
      "name": "new-header-lib",
      "repository": "https://github.com/org/new-header-lib.git",
      "branch": "main",
      "header_paths": [
        {
          "source": "include",
          "destination": "new-header-lib"
        }
      ]
    }
  ]
}
```

**Then run:**
```bash
./setup_dependencies.sh  # Headers will be copied to $HOME/usr/include/rdkb/new-header-lib/
```

### Adding a Build Dependency (Meson)

Edit `common_library_build_deps.json`:

```json
{
  "dependencies": [
    {
      "name": "new-meson-lib",
      "repository": "https://github.com/org/new-meson-lib.git",
      "branch": "main",
      "build_system": "meson",
      "external_build_script": "",
      "library_patterns": [
        "build/lib*.so*"
      ]
    }
  ]
}
```

**Then run:**
```bash
./setup_dependencies.sh  # Builds with meson and installs libraries
```

### Adding a Build Dependency (CMake)

```json
{
  "name": "new-cmake-lib",
  "repository": "https://github.com/org/new-cmake-lib.git",
  "branch": "main",
  "build_system": "cmake",
  "external_build_script": "",
  "library_patterns": [
    "../build/new-cmake-lib/lib*.so*"
  ]
}
```

### Adding a Build Dependency (Autotools)

```json
{
  "name": "new-autotools-lib",
  "repository": "https://github.com/org/new-autotools-lib.git",
  "branch": "main",
  "build_system": "autotools",
  "external_build_script": "",
  "library_patterns": [
    "src/.libs/lib*.so*"
  ]
}
```

### Adding a Dependency with External Build Script

```json
{
  "name": "complex-component",
  "repository": "https://github.com/org/complex-component.git",
  "branch": "main",
  "build_system": "autotools",
  "external_build_script": "external_build.sh",
  "library_patterns": [
    "**/*.so*"
  ]
}
```

**Note:** When `external_build_script` is specified (not empty), the script will:
- Clone the repository
- Execute the external build script
- Skip using `build_system` 
- Skip library copying (external script handles installation)

### Updating Component Build Configuration

To use new dependencies in your component, edit `build_config.json`:

```json
{
  "build_config": {
    "cflags": [
      "-DUSE_NEW_LIB"
    ],
    "header_subdirs": [
      ".",
      "new-header-lib",
      "new-meson-lib"
    ],
    "ldflags": [
      "-lnew-meson-lib"
    ]
  }
}
```

**Then rebuild:**
```bash
./build_native.sh
```

---

## Using Common-Library as a Dependency

### Component X Example

When Component X needs common-library as a dependency:

#### 1. Create X_build_deps.json

```json
{
  "dependencies": [
    {
      "name": "common-library",
      "repository": "https://github.com/rdkcentral/common-library.git",
      "branch": "main",
      "build_system": "autotools",
      "external_build_script": "common_external_build.sh",
      "library_patterns": [
        "source/.libs/libccsp_common.so*"
      ]
    }
  ]
}
```

#### 2. Copy setup_dependencies.sh

Copy `setup_dependencies.sh` from common-library and update:

```bash
BUILD_DEPS_CONFIG="${SCRIPT_DIR}/X_build_deps.json"
```

#### 3. Create X_build_config.json

```json
{
  "component_name": "component-x",
  "build_config": {
    "cflags": ["-DFEATURE_X"],
    "header_subdirs": [".", "common-library"],
    "ldflags": ["-lccsp_common"],
    "configure_options": [],
    "source_header_patches": []
  }
}
```

#### 4. Build Component X

```bash
cd component-x

# Builds common-library and all its dependencies automatically
./setup_dependencies.sh

# Builds Component X
./build_native.sh
```

---

## External Build Script Support

### When to Use

Use `external_build_script` when a dependency:
- Has its own complex build process
- Has nested dependencies that need to be built first
- Provides its own build script that handles everything

### How It Works

**In X_build_deps.json:**
```json
{
  "name": "common-library",
  "external_build_script": "common_external_build.sh"
}
```

**Build Flow:**
1. Component X runs `setup_dependencies.sh`
2. Script detects `external_build_script` is not empty
3. Clones common-library repository
4. Executes `common-library/common_external_build.sh`
5. External script builds all nested dependencies
6. Installs everything to standard paths
7. Component X can now build

### Standard Build vs External Build

**Standard Build** (`external_build_script: ""`):
- Script uses `build_system` field (meson/cmake/autotools)
- Builds the component directly
- Copies libraries using `library_patterns`

**External Build** (`external_build_script: "script.sh"`):
- Ignores `build_system` field
- Runs the specified external script
- External script handles all installation
- No library copying by setup script (external script does it)

---

## Installation Paths

All components follow these standardized paths:

### Headers
```
$HOME/usr/include/rdkb/
├── rdk_logger/           (header-only dependency)
├── trower-base64/        (header-only + build dependency)
├── rbus/                 (build dependency headers)
├── rtmessage/            (build dependency headers)
└── common-library/       (when installed as dependency)
    ├── ccsp/
    ├── cosa/
    └── util_api/
```

### Libraries
```
$HOME/usr/local/lib/
├── libtrower-base64.so*
├── librbus.so*
└── libccsp_common.so*
```

### Build Artifacts (cleaned after build)
```
$HOME/
├── trower-base64/        (removed after build)
├── rbus/                 (removed after build)
└── build/                (removed after build)
```

---

## Troubleshooting

### jq not installed
```
[ERROR] jq is not installed in the Docker container
```
**Solution:** Install jq in your Docker image or system

### External build script not found
```
[ERROR] External build script not found: common_external_build.sh
```
**Solution:** Ensure the external build script exists in the cloned repository

### Headers not found
```
[ERROR] Headers not found at: /home/user/usr/include/rdkb
```
**Solution:** Run `setup_dependencies.sh` before `build_native.sh`

### Unknown build system
```
[ERROR] Unknown build system: xyz
```
**Solution:** Use only supported build systems: `meson`, `cmake`, `autotools`, or provide `external_build_script`

### DBusLoop typedef error
```
error: unknown type name 'DBusLoop'
```
**Solution:** The source header patch is configured in `build_config.json` and applied automatically. If this error occurs, verify:
- `source_header_patches` is configured correctly
- `build_native.sh` is applying the patch before configure

### Library patterns not matching
```
No libraries found for pattern: build/*.so*
```
**Solution:** Check the actual build output directory and update `library_patterns` in JSON config

---

## Summary

This JSON-driven build system provides:

1. **Separation of Concerns**: Configuration separate from scripts
2. **Reusability**: Same scripts work for any component
3. **Flexibility**: Support for multiple build systems and external scripts
4. **Automatic Dependency Resolution**: Recursive builds with external_build_script
5. **Clean Builds**: Automatic cleanup of artifacts
6. **Standardization**: Consistent paths and structure across all components

**Key Files:**
- `common_library_headers.json` - Header-only dependencies
- `common_library_build_deps.json` - Build dependencies (with optional external_build_script)
- `build_config.json` - Component configuration
- `setup_dependencies.sh` - Dependency installer
- `build_native.sh` - Component builder
- `common_external_build.sh` - External build handler

For examples, see the `example_component_x/` directory.
