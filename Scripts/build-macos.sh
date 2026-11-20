#!/bin/bash
set -euo pipefail

case "${1:-}" in
    "") install=false ;;
    --install) install=true ;;
    *) printf 'Usage: bash Scripts/build-macos.sh [--install]\n' >&2; exit 2 ;;
esac
if [[ $# -gt 1 ]]; then
    printf 'Usage: bash Scripts/build-macos.sh [--install]\n' >&2
    exit 2
fi
if [[ "$(uname -s)" != Darwin ]]; then
    printf 'Building for Logic Pro requires macOS and Xcode.\n' >&2
    exit 1
fi

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="$root/Build/macos"
xcodebuild -version
command -v cmake >/dev/null || { printf 'CMake 3.22+ is required.\n' >&2; exit 1; }

cmake -S "$root" -B "$build" -G Xcode \
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
    -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
    -DCHEFS_TOUCH_MAC_VST3=OFF \
    -DCHEFS_TOUCH_BUILD_TESTS=ON
cmake --build "$build" --config Release --target ChefsTouchValidation --parallel

bundle="$build/ChefsTouch_artefacts/Release/AU/Chefs Touch.component"
test_host="$build/ChefsTouchValidation_artefacts/Release/ChefsTouchValidation"
plist="$bundle/Contents/Info.plist"
executable="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$plist")"
version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$plist")"
xcrun lipo "$bundle/Contents/MacOS/$executable" -verify_arch arm64 x86_64
/usr/bin/codesign --force --sign - "$bundle"
/usr/bin/codesign --verify --strict "$bundle"

if $install; then
    components="$HOME/Library/Audio/Plug-Ins/Components"
    destination="$components/Chefs Touch.component"
    mkdir -p "$components"
    staging="$(mktemp -d "$components/../.chefs-touch.XXXXXX")"
    /usr/bin/ditto "$bundle" "$staging/Chefs Touch.component"
    previous="$staging/Previous.component"
    if [[ -e "$destination" ]]; then
        mv "$destination" "$previous"
    fi
    rollback() {
        status=$?
        if [[ "$status" -ne 0 ]]; then
            if [[ -e "$destination" ]]; then
                mv "$destination" "$staging/Rejected.component"
            fi
            if [[ -e "$previous" ]]; then
                mv "$previous" "$destination"
            fi
            printf 'Installation validation failed. See %s/auval.log\n' "$build" >&2
        fi
    }
    trap rollback EXIT
    mv "$staging/Chefs Touch.component" "$destination"

    validated=false
    for attempt in 1 2 3; do
        if /usr/bin/auval -v aufx CfTc ChTi >"$build/auval.log" 2>&1 \
            && grep -q 'AU VALIDATION SUCCEEDED' "$build/auval.log"; then
            validated=true
            break
        fi
        if [[ "$attempt" -lt 3 ]]; then sleep 2; fi
    done
    cat "$build/auval.log"
    "$validated"
    "$test_host" "$destination"
    trap - EXIT
    if [[ -e "$previous" ]]; then
        printf 'Previous component backed up at %s\n' "$previous"
    else
        rmdir "$staging"
    fi
    printf 'Installed and validated %s\n' "$destination"
fi

archive="$build/ChefsTouch-$version-macOS-Universal-AU.zip"
/usr/bin/ditto -c -k --keepParent "$bundle" "$archive.tmp"
mv -f "$archive.tmp" "$archive"
printf 'Audio Unit bundle: %s\nZIP: %s\n' "$bundle" "$archive"

