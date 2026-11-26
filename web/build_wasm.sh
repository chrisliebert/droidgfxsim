#!/usr/bin/env bash
set -euo pipefail

# Script directory and repository root resolution
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${REPO_ROOT}/build-wasm"
DIST_DIR="${REPO_ROOT}/dist"

echo "=== DroidGfxSim WebAssembly Build Pipeline ==="
echo "Repo Root:  ${REPO_ROOT}"
echo "Build Dir:  ${BUILD_DIR}"
echo "Dist Dir:   ${DIST_DIR}"

# Check Emscripten environment
if ! command -v emcmake &> /dev/null; then
    echo "ERROR: 'emcmake' not found in PATH."
    echo "Please activate Emscripten SDK first:"
    echo "  source /path/to/emsdk/emsdk_env.sh"
    exit 1
fi

echo "--- Configuring WebAssembly CMake target ---"
emcmake cmake -S "${REPO_ROOT}/web" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release

echo "--- Building WebAssembly artifacts ---"
PARALLEL_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
cmake --build "${BUILD_DIR}" --config Release -j"${PARALLEL_JOBS}"

echo "--- Staging web deployment in ${DIST_DIR} ---"
mkdir -p "${DIST_DIR}"

# Copy generated files to dist/
cp -f "${BUILD_DIR}/droidgfxsim.html" "${DIST_DIR}/index.html"
cp -f "${BUILD_DIR}/droidgfxsim.js" "${DIST_DIR}/"
cp -f "${BUILD_DIR}/droidgfxsim.wasm" "${DIST_DIR}/"

if [ -f "${BUILD_DIR}/droidgfxsim.data" ]; then
    cp -f "${BUILD_DIR}/droidgfxsim.data" "${DIST_DIR}/"
fi

echo "=== Build Complete! ==="
echo "Web assets successfully staged in: ${DIST_DIR}"
echo "To test locally, run:"
echo "  python3 -m http.server 8080 --directory \"${DIST_DIR}\""
echo "and navigate to http://localhost:8080"
