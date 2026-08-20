#!/usr/bin/env bash
#
# Build and run the dyncom interpreter differential test harness on the host.
#
# Configures a lean host build (no tools/tests/scripting/Qt frontend), builds the
# dyncom_difftest target, and runs it. dynarmic is built too: the harness uses it
# as an independent oracle for whole-program cases.
# Exits non-zero on the first divergence. This is the verification gate for the
# harness-gated dyncom optimizations.
#
# Usage:
#   scripts/cpu_difftest.sh                 # default seed/count
#   scripts/cpu_difftest.sh <seed> <count>  # forwarded to the harness
#
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

BUILD_DIR="build/host-difftest"

cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DEKA2L1_BUILD_TOOLS=OFF \
    -DEKA2L1_BUILD_TESTS=OFF \
    -DEKA2L1_ENABLE_SCRIPTING_ABILITY=OFF \
    -DEKA2L1_ENABLE_DISCORD_RICH_PRESENCE=OFF \
    -DEKA2L1_BUILD_DYNCOM_DIFFTEST=ON \
    -DEKA2L1_CPU_DYNCOM_ONLY=OFF \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    >/dev/null

JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
cmake --build "${BUILD_DIR}" --target dyncom_difftest -j"${JOBS}"

BIN="$(find "${BUILD_DIR}" -name dyncom_difftest -type f -perm -111 | head -1)"
if [ -z "${BIN}" ]; then
    echo "dyncom_difftest binary not found" >&2
    exit 1
fi

echo "==> running ${BIN} $*"
"${BIN}" "$@"
