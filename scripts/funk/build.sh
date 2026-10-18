#!/usr/bin/env bash
# Run as your normal login user, from any directory.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
# shellcheck source=scripts/funk/source.sh
source "$repo/scripts/funk/source.sh"
funk_require_supported_source "$repo"
cmake -S "$repo" -B "$repo/build/funk" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_NUMBER=939 \
    -Dclient=OFF -Dserver=ON -Dtests=ON -Dplugins=OFF -Doverlay=OFF \
    -Dlto=OFF -DCMAKE_UNITY_BUILD=OFF
cmake --build "$repo/build/funk" --parallel "${FUNK_BUILD_JOBS:-2}"
ctest --test-dir "$repo/build/funk" --output-on-failure
echo "Build und Tests erfolgreich. Installation: sudo bash scripts/funk/install.sh"
