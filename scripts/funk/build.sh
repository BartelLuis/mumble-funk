#!/usr/bin/env bash
# Run as your normal login user, from any directory.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
[[ $(git -C "$repo" branch --show-current) == funk-v0.1 ]] || {
    echo 'Bitte zuerst auf den Zweig funk-v0.1 wechseln.' >&2; exit 1;
}
git -C "$repo" merge-base --is-ancestor dfbac8d3d9c967003e74b4ffc5610f7d45e8f666 HEAD
cmake -S "$repo" -B "$repo/build/funk" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_NUMBER=939 \
    -Dclient=OFF -Dserver=ON -Dtests=ON -Dplugins=OFF -Doverlay=OFF \
    -Dlto=OFF -DCMAKE_UNITY_BUILD=OFF
cmake --build "$repo/build/funk" --parallel "${FUNK_BUILD_JOBS:-2}"
ctest --test-dir "$repo/build/funk" --output-on-failure
echo "Build und Tests erfolgreich. Installation: sudo bash scripts/funk/install.sh"
