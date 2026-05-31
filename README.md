# obs-nozzle

> This codebase is currently in its AI-slob prototyping phase: the code runs on momentum, vibes, and plausible intent.
> Proper debugging will be introduced once demand graduates from hypothetical to measurable.

OBS Studio plugin providing [nozzle](https://github.com/nozzle-io/nozzle) GPU texture sharing as both source and output.

## Disclaimer / Notice

This library is currently a work in progress and contains many incomplete features and unverified implementations.
Although it may appear usable at first glance, it may not function correctly.

Please use it with the understanding that no guarantees are made regarding its behavior, and perform debugging, validation, and review as needed.
If you encounter problems, please do not become angry; instead, contributions in the form of Issues or Pull Requests would be greatly appreciated.

## What It Does

- **Nozzle Receiver (Source)** — Receive GPU textures from any nozzle sender and display them in OBS
- **Nozzle Sender (Output)** — Send OBS rendered frames to any nozzle receiver

## Features

- Cross-platform: macOS (Metal/IOSurface), Windows (D3D11), Linux (DMA-BUF)
- Named sender/receiver model with automatic discovery
- No extra dependencies beyond OBS Studio and nozzle

## Building

### Prerequisites

- CMake 3.20+
- C++17 compiler
- OBS Studio development files
- nozzle submodule (initialized automatically)

### Build Steps

```bash
git clone --recurse-submodules git@github.com:nozzle-io/obs-nozzle.git
cd obs-nozzle
cmake -B build \
    -DOBS_INCLUDE_DIR=/path/to/obs-studio/libobs \
    -DOBS_LIBRARY=/path/to/libobs.so
cmake --build build
```

### Platform Notes

**macOS:**
```bash
# If OBS installed via homebrew
cmake -B build -DOBS_INCLUDE_DIR=/opt/homebrew/include/obs -DOBS_LIBRARY=/opt/homebrew/lib/libobs.0.dylib
```

**Linux:**
```bash
cmake -B build -DOBS_INCLUDE_DIR=/usr/include/obs -DOBS_LIBRARY=/usr/lib/x86_64-linux-gnu/libobs.so.0
```

**Windows:**
```bash
cmake -B build -DOBS_INCLUDE_DIR="C:/path/to/obs-studio/libobs" -DOBS_LIBRARY="C:/path/to/libobs.lib"
```

### Environment Variable

Alternatively, set `OBS_STUDIO_DIR` before running cmake:
```bash
export OBS_STUDIO_DIR=/path/to/obs-studio
cmake -B build
```

## Release packages

CI builds real loadable OBS plugin modules for the release targets instead of the
old header/static check path. Release packages are produced for:

- `obs-nozzle-latest-<short_sha>-macos.zip`
- `obs-nozzle-latest-<short_sha>-windows.zip`
- `obs-nozzle-vX.Y.Z-macos.zip`
- `obs-nozzle-vX.Y.Z-windows.zip`

The CI dependency source is OBS Studio `32.1.2` official release artifacts:

- macOS links against a universal `libobs.framework` made from the official OBS
  Apple and Intel macOS dmgs. The plugin output is `obs-nozzle.so`
  (`Mach-O ... bundle`) and CI verifies both `arm64` and `x86_64` slices.
- Windows links against an import library generated from `obs.dll` in the
  official OBS Windows x64 zip. The plugin output is `obs-nozzle.dll`
  (`PE/MZ` DLL).
- Both platforms use `libobs` headers from the official
  `OBS-Studio-32.1.2-Sources.tar.gz` source release.

Package layout is a prefix-style OBS plugin layout with one top-level package
folder:

```text
obs-nozzle-<channel>-<platform>/
  README.md
  LICENSE
  lib/obs-plugins/obs-nozzle/obs-nozzle.so        # macOS
  bin/obs-plugins/obs-nozzle/obs-nozzle.dll       # Windows
  share/obs-plugins/obs-nozzle/locale/en-US.ini
```

Copy the platform-specific plugin binary and `share/obs-plugins/obs-nozzle`
locale directory into the matching OBS plugin prefix for your installation.

## Installation from local build

Build with `NOZZLE_OBS_LINK_LIBRARY=ON` and a real `OBS_LIBRARY` path. Then copy
the built module and `data/locale/en-US.ini` into the same layout shown above.

## Usage

### Nozzle Receiver (Source)

1. Add a new source in OBS
2. Select "Nozzle Receiver"
3. Choose a sender from the dropdown (or type a sender name)
4. Adjust timeout if needed (default: 100ms)

### Nozzle Sender (Output)

1. Go to Settings → Output
2. Add a new output of type "Nozzle Sender"
3. Set the sender name and application name
4. Start the output

## License

MIT

Third-party dependencies:

- [nozzle](https://github.com/nozzle-io/nozzle) — MIT
