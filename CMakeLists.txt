cmake_minimum_required(VERSION 3.22)
project(ClusterUI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── Dependencies injected by Conan ────────────────────────────────────────────
# Run:
#   conan install . --output-folder=build/Release --build=missing -pr <profile>
#   cmake -B build/Release \
#         -DCMAKE_TOOLCHAIN_FILE=build/Release/conan_toolchain.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/Release

find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick QuickControls2 Multimedia)

# When cross-compiling, the host Qt cmake state (loaded via QT_HOST_PATH) can
# leave QT6_IS_SHARED_LIBS_BUILD unset or FALSE, causing qt6_import_qml_plugins
# to try to statically link QML plugins that don't exist in the cross scope.
# RPi Qt is a shared-library build so plugins are runtime-loaded; skip static
# linking by asserting the correct value here.
if(CMAKE_CROSSCOMPILING)
    set(QT6_IS_SHARED_LIBS_BUILD TRUE)
endif()
find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)   # pulled in by gRPC; found via CMakeDeps

# GStreamer — detected via pkg-config (available on both pc Ubuntu and RPi)
find_package(PkgConfig REQUIRED)

# When cross-compiling, redirect pkg-config to the sysroot so it finds the
# target's GStreamer packages instead of the pc's.  Without this, pkg-config
# returns HOST (x86_64) include paths such as /usr/include/x86_64-linux-gnu
# and /usr/lib/x86_64-linux-gnu/glib-2.0/include.  Because -isystem flags from
# pkg-config are passed as absolute paths they are NOT prefixed with the sysroot
# by the compiler, so they bypass the sysroot mechanism entirely and inject x86_64
# type-definition headers into the AArch64 build.  This corrupts fundamental types:
# protobuf and abseil cast pointers through uintptr_t which then resolves to
# 'unsigned int' (32-bit) instead of 'unsigned long' (64-bit), causing dozens of
# [-fpermissive] precision-loss errors across all translation units.
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSROOT)
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
    set(ENV{PKG_CONFIG_LIBDIR}
        "${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/pkgconfig:${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
    set(ENV{PKG_CONFIG_PATH} "")
    message(STATUS "GStreamer: pkg-config redirected to sysroot (cross-build)")
endif()

pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-video-1.0
)

# ── vhal-core packages ────────────────────────────────────────────────────────
# ClusterUI depends only on vhal-types (AIDL headers, stubs) and
# vhal-ipc-grpc (VehicleServer::Stub). vhal-server is not needed at build time.
add_subdirectory(../vhal-core/packages/vhal-types    ${CMAKE_BINARY_DIR}/vhal-types    EXCLUDE_FROM_ALL)
add_subdirectory(../vhal-core/packages/vhal-ipc-grpc ${CMAKE_BINARY_DIR}/vhal-ipc-grpc EXCLUDE_FROM_ALL)

# ── Qt setup ──────────────────────────────────────────────────────────────────
if(Qt6_VERSION VERSION_GREATER_EQUAL "6.8")
    qt_policy(SET QTP0004 NEW)
endif()
qt_standard_project_setup(REQUIRES 6.5)

# ── Executable ────────────────────────────────────────────────────────────────
qt_add_executable(cluster-ui
    src/main.cpp
    src/vehiclebridge.cpp
    src/vehiclebridge.h
    src/vehiclestate.h
    src/VhalGrpcClient.cpp
    src/VhalGrpcClient.h
    src/RvcStreamHandler.cpp
    src/RvcStreamHandler.h
)

qt_add_qml_module(cluster-ui
    URI ClusterUI
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES
        qml/ClusterRoot.qml
        qml/Speedometer.qml
        qml/Tachometer.qml
        qml/PipOverlay.qml
        qml/themes/Sport.qml
        qml/themes/Normal.qml
    RESOURCES
        assets/fonts/cluster.ttf
        assets/images/warning_icons.svg
)

set_target_properties(cluster-ui PROPERTIES
    QT_RESOURCE_PREFIX "/"
)

target_link_libraries(cluster-ui PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    gRPC::grpc++
    protobuf::libprotobuf
    VehicleServerProtoStub
    AidlPropertyHeaders
    AndroidStubs
    PkgConfig::GSTREAMER
)

# ── Install target — used by deploy.sh ────────────────────────────────────────
install(TARGETS cluster-ui
    RUNTIME DESTINATION bin
)
install(DIRECTORY qml/    DESTINATION qml)
install(DIRECTORY assets/ DESTINATION assets)
