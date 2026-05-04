#!/usr/bin/env bash
set -euo pipefail

# Arguments to this script
RPI_TARGET="${1:-aananth@192.168.10.10}"   # user@pc
VHAL_SERVER="${2:-localhost:50051}"

RPI_USER="${RPI_TARGET%%@*}"
RPI_IP="${RPI_TARGET##*@}"

PROJECT_DIR="$(pwd)"
BUILD_DIR="${PROJECT_DIR}/build/rpi"
REMOTE_DIR="/home/${RPI_USER}/cluster"

# Working directory check
if [ ! -d "$PROJECT_DIR/src" ]; then
    echo "Error: You must invoke this script from ClusterUI folder!" >&2
    exit 1
fi
echo "ClusterUI working folder: ${PROJECT_DIR}"
echo "Target:                   ${RPI_USER}@${RPI_IP}:${REMOTE_DIR}"
echo "vhal-core server:         ${VHAL_SERVER}"
echo ""

QT_RPI_PATH="${QT_RPI_PATH:-${HOME}/Qt/6.8.3/gcc_64}"

echo "==> Conan install + CMake build..."
conan install . --output-folder=build/rpi --build=missing -pr profiles/rpi
# shellcheck source=/dev/null
source build/rpi/conanbuild.sh
cmake -B build/rpi \
      -DCMAKE_TOOLCHAIN_FILE=build/rpi/conan_toolchain.cmake \
      -DCMAKE_PREFIX_PATH="${QT_RPI_PATH}" \
      -DQT_HOST_PATH="${QT_RPI_PATH}" \
      -DCMAKE_BUILD_TYPE=Release \
      -GNinja
cmake --build build/rpi -j"$(nproc)"

echo "==> Deploying to ${RPI_USER}@${RPI_IP}:${REMOTE_DIR}..."
ssh "${RPI_USER}@${RPI_IP}" "mkdir -p ${REMOTE_DIR}"
rsync -avz --delete \
    "${BUILD_DIR}/cluster-ui" \
    "${RPI_USER}@${RPI_IP}:${REMOTE_DIR}/"

echo "==> Launching on RPi..."
ssh "${RPI_USER}@${RPI_IP}" bash <<REMOTE
    export QT_QPA_PLATFORM=eglfs
    export QSG_RHI_BACKEND=opengl
    export QT_QPA_EGLFS_HIDECURSOR=1
    export QT_QPA_EGLFS_KMS_CONFIG=/home/${RPI_USER}/cluster/eglfs.json

    pkill -f cluster-ui || true
    sleep 0.2
    nohup ${REMOTE_DIR}/cluster-ui --vhal-server ${VHAL_SERVER} > /tmp/cluster.log 2>&1 &
    echo "Started cluster-ui. Logs: /tmp/cluster.log"
REMOTE

echo "==> Done."
