#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=========================================================="
echo " Energy Crisis - Google Play App Bundle (.aab) Builder    "
echo "=========================================================="

if ! command -v java &> /dev/null && [ -z "$JAVA_HOME" ]; then
    echo ""
    echo "Note: Java / JDK (17 or higher) was not found in PATH."
    echo ""
    echo "You have two easy options to produce the .aab bundle:"
    echo "1. Android Studio (Easiest GUI):"
    echo "   - Open Android Studio -> Open project -> choose the '$ROOT_DIR/android' folder."
    echo "   - Click menu: Build > Generate Signed Bundle / APK > Android App Bundle."
    echo "   - Select keystore: '$ROOT_DIR/android/release.keystore' (pass: N@v!-energycrisis-GameDev-1967)."
    echo ""
    echo "2. Install JDK & Android SDK from package manager:"
    echo "   - On Arch/CachyOS: sudo pacman -S jdk17-openjdk android-tools"
    echo "   - Then re-run: ./scripts/build-android-bundle.sh"
    exit 1
fi

if [ -d "$HOME/Android/Sdk" ]; then
    export ANDROID_HOME="$HOME/Android/Sdk"
    if [ ! -f "$ROOT_DIR/android/local.properties" ]; then
        echo "sdk.dir=$HOME/Android/Sdk" > "$ROOT_DIR/android/local.properties"
    fi
fi

if [ ! -f "$ROOT_DIR/android/local.properties" ] && [ -z "$ANDROID_HOME" ]; then
    echo ""
    echo "Android SDK location was not found."
    echo "Please run the setup script to download the SDK & NDK automatically:"
    echo "  ./scripts/setup-android-sdk.sh"
    echo ""
    echo "Or open the '$ROOT_DIR/android' folder in Android Studio."
    exit 1
fi

cd "$ROOT_DIR/android"
chmod +x gradlew
./gradlew bundleRelease

BUNDLE_PATH="$ROOT_DIR/android/app/build/outputs/bundle/release/app-release.aab"

if [ -f "$BUNDLE_PATH" ]; then
    echo ""
    echo "SUCCESS! Android App Bundle (.aab) built successfully:"
    echo "-> $BUNDLE_PATH"
    echo ""
    echo "You can upload this file directly to the Google Play Console!"
fi

