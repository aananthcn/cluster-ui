set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# ── Sysroot ───────────────────────────────────────────────────────────────────
# Inside Docker the sysroot is always mounted at /sdk/root
# On the pc: $HOME/sdk/rpi/root
if(EXISTS "/sdk/root")
    set(RPI_SYSROOT "/sdk/root")
else()
    # Fallback for running cmake outside Docker (native pc builds)
    get_filename_component(_HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
    get_filename_component(RPI_SYSROOT "${_HERE}/../root" ABSOLUTE)
endif()

message(STATUS "RPi sysroot: ${RPI_SYSROOT}")

# ── Compilers ─────────────────────────────────────────────────────────────────
set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc-14)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++-14)

set(CMAKE_SYSROOT      ${RPI_SYSROOT})
set(CMAKE_FIND_ROOT_PATH ${RPI_SYSROOT})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ── Qt6 location inside sysroot ───────────────────────────────────────────────
set(Qt6_DIR "${RPI_SYSROOT}/usr/lib/aarch64-linux-gnu/cmake/Qt6"
    CACHE PATH "Qt6 cmake dir in sysroot")

# ── Compiler flags ────────────────────────────────────────────────────────────
set(CMAKE_C_FLAGS_INIT   "--sysroot=${RPI_SYSROOT}")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${RPI_SYSROOT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "--sysroot=${RPI_SYSROOT} -Wl,-rpath-link,${RPI_SYSROOT}/usr/lib/aarch64-linux-gnu")
