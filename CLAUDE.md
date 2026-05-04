# CLAUDE.md — ClusterUI Instrument Cluster App

## Project Overview

ClusterUI is a Qt6-based instrument cluster UI application for automotive use. It renders gauges (Tachometer, Speedometer, and ancillary indicators) sourced from vehicle property data.

The project is being migrated from a raw CMake build to a **Conan + CMake** build, and its data source is being migrated from a UDP bridge / synthetic sine-wave generator to a **gRPC client** that reads vehicle properties from **vhal-core** — a Linux/QNX port of Android's Vehicle HAL (VHAL).

---

## Build System Migration: CMake → Conan + CMake

### Goal
Replace any manual `find_package` / `FetchContent` dependency management with Conan. The existing `CMakeLists.txt` should be preserved in structure but updated to consume dependencies injected by Conan.

### What to do
- Add a `conanfile.py` at the project root.
- Required Conan dependencies: `qt/6.x`, `grpc`, `protobuf` (let gRPC resolve protobuf transitively — do **not** add an explicit `protobuf` requirement).
- Use `cmake_layout()` or equivalent manual `self.folders` layout (avoid nested `build/` quirks from `cmake_layout()`).
- Conan generators: `CMakeToolchain`, `CMakeDeps`.
- The build workflow should be:
  ```
  conan install . --output-folder=build/Release --build=missing -pr <profile>
  cmake -B build/Release -DCMAKE_TOOLCHAIN_FILE=build/Release/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
  cmake --build build/Release
  ```
- Cross-compilation profiles exist for: `linux-x86` (Ubuntu 22.04, GCC 11), `rpi` (Raspberry Pi OS, aarch64), `qnx-aarch64`.

### What NOT to do
- Do not add `protobuf` as a direct Conan dependency — gRPC pulls it in.
- Do not duplicate `CMakeToolchain` in a manual `generate()` method.
- Do not assume toolchain files land in a `generators/` subfolder — they are flat in the output folder.

---

## Data Source Migration: UDP/Synthetic → gRPC Client

### Background
- The app previously supported two bridge modes controlled by CMake flags:
  - `USE_UDP_BRIDGE` — network data via UDP.
  - `USE_SYNTHETIC_BRIDGE` — locally generated sine-wave data simulating Tacho/Speed/etc.
- Both are being **replaced** by a gRPC client mode.

### Goal
ClusterUI becomes a **gRPC client** that connects to `vhal-core` (the server) and subscribes to / polls vehicle properties needed for the cluster UI.

### vhal-core context
- `vhal-core` is a port of Android 16's VHAL (Vehicle HAL) to Linux and QNX.
- It exposes vehicle properties over gRPC (replacing Android's Binder/AIDL transport).
- Proto definitions live in `vhal-core`'s source tree (under the ported `hardware/interfaces/automotive/vehicle/aidl/` paths).
- The relevant gRPC service and proto stubs are compiled as part of `vhal-core`'s build; ClusterUI links against them.

### Vehicle properties needed by ClusterUI (minimum set)
Map these VHAL property IDs to UI gauges:

| Gauge          | VHAL Property          |
|----------------|------------------------|
| Speedometer    | `PERF_VEHICLE_SPEED`   |
| Tachometer     | `ENGINE_RPM`           |
| Fuel level     | `FUEL_LEVEL`           |
| Gear indicator | `GEAR_SELECTION`       |
| Engine temp    | `ENGINE_COOLANT_TEMP`  |
| Turn indicators| `TURN_SIGNAL_STATE`    |

Add others if the existing UDP/synthetic bridge supplied them.

### What to do
- Remove `USE_UDP_BRIDGE` and `USE_SYNTHETIC_BRIDGE` CMake options and all code paths guarded by them.
- Introduce a `VhalGrpcClient` class (or equivalent name) that:
  - Takes the vhal-core gRPC server address (default: `localhost:50051`) as a constructor argument, configurable via a config file or command-line flag.
  - Implements `getProperty(int32_t propId)` returning a `VehiclePropValue`.
  - Optionally supports a subscribe/callback model if the vhal-core server exposes a streaming RPC.
- Wire `VhalGrpcClient` into the existing gauge data pipeline wherever `UDPBridge` or `SyntheticBridge` were consumed.
- The gRPC proto/stub headers and the compiled stub library come from `vhal-core`. Link against it via CMake target (e.g., `vhal_core::grpc_stub` or however it is exported — confirm from `vhal-core`'s `CMakeLists.txt`).

### CMakeLists.txt changes
- Add `find_package(vhal-core)` or `add_subdirectory(../vhal-core)` depending on how vhal-core is consumed (as an installed package via Conan, or as a sibling directory).
- Link ClusterUI executable against the vhal-core gRPC stub target.
- Remove the `USE_UDP_BRIDGE` / `USE_SYNTHETIC_BRIDGE` option blocks.

---

## Rear View Camera (RVC) Stream

### Intent
When the driver engages Reverse gear the instrument cluster must display a live
rear view camera feed in a Picture-in-Picture overlay on the right side of the
screen.  The camera runs on the Android partition of the RPi and transmits
H.264-encoded video over RTP/UDP.  ClusterUI receives and decodes the stream
using GStreamer and renders it via Qt Multimedia's `VideoOutput` QML type.

### Trigger
`GEAR_SELECTION == GEAR_REVERSE` (bitmask `0x0002`) causes `VehicleBridge` to
set `DriveMode::REVERSE`.  `main.cpp` connects `VehicleBridge::stateChanged` to
`RvcStreamHandler::start()` / `stop()`.  No changes to `VehicleBridge` itself
are required — the existing `driveMode` property is sufficient.

### GStreamer pipeline
The following pipeline was verified end-to-end on the RPi.  It is constructed
programmatically in `RvcStreamHandler::start()` via `gst_parse_launch()`:

```
udpsrc port=5004
  caps=application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96
  ! rtph264depay
  ! video/x-h264,stream-format=byte-stream,alignment=au
  ! h264parse
  ! avdec_h264          ← software decode; replace with v4l2h264dec for RPi HW
  ! videoconvert
  ! video/x-raw,format=RGBA
  ! appsink name=rvc_appsink sync=false max-buffers=2 drop=true
```

`autovideosink` from the test command is replaced by `appsink` so frames can be
pulled into Qt.  `drop=true` and `max-buffers=2` prevent backpressure from
stalling the pipeline when the Qt main thread is briefly busy.

### Thread model
- GStreamer runs its streaming threads internally.
- `GstAppSinkCallbacks::new_sample` fires on a GStreamer thread.
- The callback deep-copies the decoded RGBA frame into a `QByteArray`, then
  calls `QMetaObject::invokeMethod(this, lambda, Qt::QueuedConnection)` to push
  the `QVideoFrame` to `QVideoSink::setVideoFrame()` on the Qt main thread.
- `RvcStreamHandler::checkBus()` polls the GStreamer message bus once per second
  from a `QTimer` (Qt main thread) — no GLib main loop dependency.

### Qt Multimedia integration
- `VideoOutput` (QML, `import QtMultimedia`) creates and owns a `QVideoSink`.
- `PipOverlay.qml` passes that sink to C++ via
  `Component.onCompleted: rvcStream.setVideoSink(videoSink)`.
- `RvcStreamHandler` stores the pointer and writes frames into it.
- `VideoOutput` handles all scaling and aspect-ratio fitting inside the PiP
  rectangle automatically.

### What NOT to do
- Do not call `QVideoSink::setVideoFrame()` from a non-Qt-main thread — always
  marshal via `Qt::QueuedConnection`.
- Do not use `emit-signals=true` on the appsink when using `GstAppSinkCallbacks`
  — they are separate mechanisms; enabling both wastes CPU on GLib signal emission.
- Do not add `gst_deinit()` at app exit — Qt's shutdown order is unpredictable
  and GStreamer's internal threads may still be active.
- Do not manage GStreamer element lifetime with `std::unique_ptr` — always use
  `gst_object_unref()` as GStreamer has its own reference-counting (`GObject`).

### CMakeLists.txt requirements
- `Qt6::Multimedia` must be linked (provides `QVideoSink`, `QVideoFrame`, etc.).
  Do **not** add `Qt6::MultimediaQuick` as a required component — it is not
  present in all sysroots (e.g. the RPi cross-compile SDK).  The `VideoOutput`
  QML type it backs is loaded at runtime as a QML plugin; no link-time dependency
  is needed.
- GStreamer is a **system dependency** found via `pkg-config`, not Conan:
  ```cmake
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
      gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
  ```
  Link with `PkgConfig::GSTREAMER`.
- On Ubuntu pc: `sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev`
- On RPi: same package names; `gstreamer1.0-plugins-bad` also needed for
  `avdec_h264` (provided by `gstreamer1.0-libav`).

---

## Coding Conventions

- C++17.
- Follow Qt6 signal/slot conventions for UI updates from the gRPC polling/callback thread.
- gRPC calls must happen off the Qt main thread; use `QMetaObject::invokeMethod` or a `QTimer`-driven poll on a worker thread to push data to the UI.
- GStreamer callbacks must never touch Qt objects directly — always marshal to the Qt main thread via `Qt::QueuedConnection`.
- Do not block the Qt event loop.

---

## Folder Structure (expected)

```
ClusterUI/
├── CLAUDE.md
├── conanfile.py
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── VhalGrpcClient.h / .cpp
│   ├── RvcStreamHandler.h / .cpp   <- RTP camera stream (GStreamer + Qt Multimedia)
│   ├── vehiclebridge.h / .cpp
│   ├── vehiclestate.h
│   └── ...
├── qml/
│   ├── ClusterRoot.qml
│   ├── PipOverlay.qml              <- VideoOutput replaces animated placeholder
│   ├── Speedometer.qml
│   ├── Tachometer.qml
│   └── themes/
├── profiles/
│   ├── linux-x86
│   ├── rpi
│   └── qnx-aarch64
└── build/                          <- generated, not in source control
```

---

## Key Constraints and Pitfalls

- GCC version on the dev machine is **11** (Ubuntu 22.04) — the `linux-x86` Conan profile must specify `compiler.version=11`.
- Protobuf version must **not** be pinned explicitly in `conanfile.py`; let gRPC own it.
- Qt6 and gRPC both bring in their own CMake integration; ensure `CMakeDeps` generated files do not conflict with Qt's own `find_package` machinery.
- vhal-core's gRPC server address should be runtime-configurable, not hardcoded, to support both local dev (localhost) and embedded targets where vhal-core runs as a separate process (possibly on a different IP/port).
- `VideoOutput.videoSink` (the read-only `QVideoSink*` property) is only available from Qt 6.5 onwards — the project already requires `REQUIRES 6.5` in `qt_standard_project_setup()`.
- On RPi with `eglfs`, ensure the GStreamer `videoconvert` element can output `RGBA`; if a GL-based sink is later added, the context must be shared with Qt's OpenGL context.
