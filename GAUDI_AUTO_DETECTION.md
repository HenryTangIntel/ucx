# Gaudi Automatic Detection Configuration

## Summary

The UCX build system has been successfully configured to automatically detect and build Gaudi support without requiring manual environment variables. This eliminates the need to set CPPFLAGS and LDFLAGS manually before running configure.

## Changes Made

### 1. Updated config/m4/gaudi.m4

- **Enhanced GAUDI_CPPFLAGS**: Added all necessary flags including:
  - `-I/usr/include/habanalabs` - Gaudi headers
  - `-I/usr/include/drm` - DRM headers 
  - `-DHAVE_GAUDI=1` - Enable Gaudi support
  - `-DHAVE_HLTHUNK_H=1` - Enable hlthunk header support

- **Changed default**: Modified default from "guess" to "yes" for automatic detection

- **Enhanced GAUDI_LDFLAGS**: Set to `-L/usr/lib/habanalabs` for system library path

### 2. Updated src/tools/perf/gaudi/configure.m4

- **Fixed header detection**: Changed from `hlthunk.h` to `habanalabs/hlthunk.h`
- **Changed default**: Modified default from "check" to "yes" for automatic detection

### 3. Updated configure.ac

- **Added standalone module**: Gaudi now appears as a separate module category
- **Module display**: Shows "GAUDI modules: < gaudi >" instead of being under IB modules

### 4. Updated src/uct/gaudi/configure.m4

- **Module naming**: Changed from "gaudi_copy" to "gaudi" for cleaner display

### 5. Cleaned up Makefiles

- **src/uct/gaudi/Makefile.am**: Removed hardcoded paths, uses only configured variables
- **src/ucm/gaudi/Makefile.am**: Already properly configured
- **src/tools/perf/gaudi/Makefile.am**: Already properly configured

## Usage

### Before (manual environment setup required):
```bash
CPPFLAGS="-I/usr/include/habanalabs -I/usr/include/drm -DHAVE_GAUDI=1 -DHAVE_HLTHUNK_H=1" \
LDFLAGS="-L/usr/lib/habanalabs" \
./configure --prefix=/path/to/install
```

### After (automatic detection):
```bash
./configure --prefix=/path/to/install
```

## Configuration Output

The build configuration now shows Gaudi as a standalone module:

```
configure: UCX build configuration:
configure:          UCT modules:   < ib rdmacm cma >
configure:         CUDA modules:   < >
configure:        GAUDI modules:   < gaudi >
configure:         ROCM modules:   < >
configure:           IB modules:   < mlx5 >
configure:          UCM modules:   < gaudi >
configure:         Perf modules:   < gaudi >
```

## Benefits

1. **Automatic Detection**: No manual environment setup required
2. **Standalone Module**: Gaudi appears as its own module category like CUDA/ROCM  
3. **Clean Configuration**: All paths and flags are handled by the build system
4. **Consistent Behavior**: Same detection logic across UCT, UCM, and Performance tools
5. **Future-Proof**: Easy to maintain and extend for new Gaudi features

## Files Modified

- `config/m4/gaudi.m4`
- `src/tools/perf/gaudi/configure.m4` 
- `src/uct/gaudi/configure.m4`
- `configure.ac`
- `src/uct/gaudi/Makefile.am`

## Verification

The configuration automatically detects Gaudi when the system has:
- Headers: `/usr/include/habanalabs/hlthunk.h`
- Libraries: `/usr/lib/habanalabs/libhl-thunk.so`

No manual intervention required!
