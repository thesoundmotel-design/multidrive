#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="$PROJECT_ROOT/build/MotelVox_artefacts/Release"
PACKAGE_ROOT="$(mktemp -d)"
OUTPUT_DIR="$PROJECT_ROOT/build/installer"
OUTPUT_PACKAGE="$OUTPUT_DIR/MotelVox-macOS-1.0.0.pkg"

cleanup() {
    rm -rf "$PACKAGE_ROOT"
}
trap cleanup EXIT

VST3_BUNDLE="$BUILD_ROOT/VST3/Motel Vox.vst3"
AU_BUNDLE="$BUILD_ROOT/AU/Motel Vox.component"

for bundle in "$VST3_BUNDLE" "$AU_BUNDLE"; do
    if [[ ! -d "$bundle" ]]; then
        printf 'Missing plugin bundle: %s\n' "$bundle" >&2
        exit 1
    fi
done

mkdir -p \
    "$PACKAGE_ROOT/Library/Audio/Plug-Ins/VST3" \
    "$PACKAGE_ROOT/Library/Audio/Plug-Ins/Components" \
    "$OUTPUT_DIR"
cp -R "$VST3_BUNDLE" "$PACKAGE_ROOT/Library/Audio/Plug-Ins/VST3/"
cp -R "$AU_BUNDLE" "$PACKAGE_ROOT/Library/Audio/Plug-Ins/Components/"

pkgbuild \
    --root "$PACKAGE_ROOT" \
    --identifier com.thesoundmotel.motelvox \
    --version 1.0.0 \
    --install-location / \
    "$OUTPUT_PACKAGE"

printf 'Created installer: %s\n' "$OUTPUT_PACKAGE"