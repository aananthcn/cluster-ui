# ClusterUI

A Qt6-based instrument cluster UI for automotive use. It renders Speedometer, Tachometer, and ancillary gauges driven by live vehicle property data received from **vhal-core** over gRPC.

---

## Host Setup

### 1. System packages

```bash
sudo apt update && sudo apt install -y \
  build-essential cmake ninja-build git \
  gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
  libgl1-mesa-dev \
  python3 python3-pip ssh rsync

pip install conan
```

### 2. Qt installation

Qt is not managed by Conan — install it using the [Qt Online Installer](https://www.qt.io/download-qt-installer).

**For local (x86) development — Qt 6.11.0:**
- In the installer, select **Qt 6.11.0 → Desktop gcc 64-bit**
- Default install path: `~/Qt/6.11.0/gcc_64`

**For RPi cross-compilation — Qt 6.8.3:**
- In the installer, select **Qt 6.8.3 → Desktop gcc 64-bit** (pc tools, always required)
- Also select **Qt 6.8.3 → Raspberry Pi** (aarch64 target libraries)
- Default install path: `~/Qt/6.8.3/gcc_64`

Conan manages only gRPC and jsoncpp. All other dependencies are provided by Qt and the system.

---

## Build

### Local (x86 desktop)

```bash
conan install . --output-folder=build/pc --build=missing -pr profiles/linux-x86
source build/pc/conanbuild.sh
cmake -B build/pc \
  -DCMAKE_TOOLCHAIN_FILE=build/pc/conan_toolchain.cmake \
  -DCMAKE_PREFIX_PATH=$HOME/Qt/6.11.0/gcc_64 \
  -DCMAKE_BUILD_TYPE=Release -GNinja
cmake --build build/pc -j$(nproc)
```

> **Important:** Always pass `-DCMAKE_PREFIX_PATH` pointing to your local Qt installation. CMake does not expand `~` — use `$HOME` or an absolute path. If you previously ran a build without this flag, clean first (`rm -rf build/pc`) before re-running, otherwise stale CMake files will be picked up instead of the local Qt.

### Cross-compile for RPi (aarch64)

```bash
conan install . --output-folder=build/rpi --build=missing -pr profiles/rpi
source build/rpi/conanbuild.sh
cmake -B build/rpi \
  -DCMAKE_TOOLCHAIN_FILE=build/rpi/conan_toolchain.cmake \
  -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64 \
  -DQT_HOST_PATH=$HOME/Qt/6.8.3/gcc_64 \
  -DCMAKE_BUILD_TYPE=Release -GNinja
cmake --build build/rpi -j$(nproc)
```

> **Note:** The first Conan build compiles gRPC from source. Subsequent builds use the local cache (`~/.conan2/p/`) and are fast.

---

## Run Locally

ClusterUI connects to a running **vhal-core** gRPC server. Start the server first, then:

```bash
./build/pc/cluster-ui --vhal-server localhost:50051
```

The `--vhal-server` flag accepts any `pc:port`. It defaults to `localhost:50051` if omitted.

---

## Deploy to RPi

The deploy script cross-compiles the app on the pc, copies it to the RPi over rsync, and launches it:

```bash
./scripts/deploy.sh [rpi-user@rpi-ip] [vhal-server-address]
```

Defaults: `aananth@192.168.10.10` and `localhost:50051`. Examples:

```bash
# Use defaults
./scripts/deploy.sh

# Override target only
./scripts/deploy.sh pi@192.168.1.42

# Override both target and vhal-core server
./scripts/deploy.sh pi@192.168.1.42 192.168.1.10:50051
```

Logs on the RPi are written to `/tmp/cluster.log`.

---

## Clean

```bash
rm -rf build/pc
rm -rf build/rpi
```
