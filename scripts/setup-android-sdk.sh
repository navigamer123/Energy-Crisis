#!/bin/bash
set -e

SDK_DIR="$HOME/Android/Sdk"
mkdir -p "$SDK_DIR/cmdline-tools"

echo "=========================================================="
echo " Setting up Android SDK, NDK, and Build Tools             "
echo "=========================================================="

# Check if cmdline-tools is already downloaded
if [ ! -d "$SDK_DIR/cmdline-tools/latest/bin" ]; then
    echo "1. Downloading Google Android command-line tools..."
    curl -o /tmp/cmdline-tools.zip -L https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
    
    rm -rf /tmp/cmdline-extracted
    mkdir -p /tmp/cmdline-extracted
    unzip -q -o /tmp/cmdline-tools.zip -d /tmp/cmdline-extracted
    
    rm -rf "$SDK_DIR/cmdline-tools/latest"
    mv /tmp/cmdline-extracted/cmdline-tools "$SDK_DIR/cmdline-tools/latest"
    rm -rf /tmp/cmdline-extracted /tmp/cmdline-tools.zip
fi

export ANDROID_HOME="$SDK_DIR"
export PATH="$SDK_DIR/cmdline-tools/latest/bin:$PATH"

echo "2. Accepting Android SDK licenses..."
yes | sdkmanager --licenses > /dev/null 2>&1 || true

echo "3. Installing Android Platform 34, Build Tools, NDK, and CMake..."
sdkmanager "platforms;android-34" "build-tools;34.0.0" "ndk;26.3.11579264" "cmake;3.22.1"

echo "4. Generating android/local.properties..."
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "sdk.dir=$SDK_DIR" > "$PROJECT_DIR/android/local.properties"

echo "=========================================================="
echo " Android SDK & NDK setup complete!                        "
echo " You can now run: ./scripts/build-android-bundle.sh       "
echo "=========================================================="

