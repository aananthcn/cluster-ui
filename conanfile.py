import os
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps


class ClusterUIConan(ConanFile):
    name = "ClusterUI"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("vhal-ipc-grpc/1.0")  # brings vhal-types/1.0 + grpc/1.69.0 transitively
        # Qt is NOT managed by Conan — located via _find_qt_prefix() at generate time.
        # Do NOT add protobuf here — gRPC pulls it in transitively.

    def generate(self):
        tc = CMakeToolchain(self)
        qt_prefix = self._find_qt_prefix()
        if qt_prefix:
            # Pass the path as a dedicated variable; CMakeLists.txt prepends it
            # to CMAKE_PREFIX_PATH before find_package(Qt6).
            tc.variables["QT_PREFIX_PATH"] = qt_prefix
            self.output.info(f"Qt prefix: {qt_prefix}")
        else:
            self.output.warning("Qt Online Installer not found — CMake will rely on system Qt")
        tc.generate()
        CMakeDeps(self).generate()

    # ── Qt path detection ─────────────────────────────────────────────────────
    def _find_qt_prefix(self):
        home = os.path.expanduser("~")
        arch = str(self.settings.arch)

        if arch == "x86_64":
            # Qt Online Installer: ~/Qt/<version>/gcc_64
            return self._scan_qt_dir(os.path.join(home, "Qt"), "gcc_64")
        else:
            # RPi cross-compile SDK: ~/sdk/rpi/**/lib/cmake/Qt6/Qt6Config.cmake
            return self._scan_sdk_dir(os.path.join(home, "sdk", "rpi"))

    def _scan_qt_dir(self, qt_base, arch_dir):
        """Pick the highest Qt version under qt_base/<version>/<arch_dir>."""
        if not os.path.isdir(qt_base):
            return None
        versions = sorted(
            [v for v in os.listdir(qt_base)
             if os.path.isfile(os.path.join(qt_base, v, arch_dir,
                                            "lib", "cmake", "Qt6", "Qt6Config.cmake"))],
            reverse=True
        )
        return os.path.join(qt_base, versions[0], arch_dir) if versions else None

    def _scan_sdk_dir(self, sdk_root):
        """Walk sdk_root looking for any directory containing lib/cmake/Qt6/Qt6Config.cmake."""
        if not os.path.isdir(sdk_root):
            return None
        for dirpath, _dirs, files in os.walk(sdk_root):
            if "Qt6Config.cmake" in files and dirpath.endswith(os.path.join("lib", "cmake", "Qt6")):
                # dirpath is .../lib/cmake/Qt6 — return the prefix 3 levels up
                return os.path.normpath(os.path.join(dirpath, "..", "..", ".."))
        return None
