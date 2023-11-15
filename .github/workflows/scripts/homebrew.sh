#!/bin/bash

set -e

# This script cannot run concurrently in the same CI builder as the Homebrew
# packages are installed globally, and the different architectures would
# overwrite each other.

# Order important as each dependency must have fully satisfied transitive
# dependencies to avoid installing Intel versions in the cross-compilation
# case.
dependencies=(
    pcre2
    freetype
    jansson
    libpng
    jpeg-turbo
    libmodplug
    minizip
    lame
    mpg123
    speex
    speexdsp
    opus
    libvorbis
    libogg
    flac
    libsndfile
    sdl2
    sdl2_net
    sdl2_gfx
    sdl2_image
    sdl2_mixer
)


function install_arm64() {
    export CPU="arm64"
    # Ignore pre-installed Intel packages, always install what we say.
    export HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK="true"
    export HOMEBREW_NO_INSTALL_CLEANUP="true"
    export HOMEBREW_DIR="/Users/runner/Library/Caches/Homebrew/downloads"

    brew reinstall --quiet pkg-config
    brew fetch --force --bottle-tag=arm64_sonoma "${dependencies[@]}"

    for dependency in "${dependencies[@]}"; do
        echo "Installing $dependency"
        brew reinstall --quiet "${HOMEBREW_DIR}"/*"${dependency}"-*.arm64_sonoma.bottle*.tar.gz
    done
}

function install_intel() {
    export CPU="x86_64"
    export HOMEBREW_NO_INSTALL_CLEANUP="true"
    brew reinstall --quiet pkg-config
    brew install --quiet "${dependencies[@]}"
}

function create_bundle() {
    ARCH=$1
    if [ "$ARCH" == "intel" ];then
        ARCH="x86_64"
    fi
    sh misc/install/create_osx_bundle.sh $ARCH

    echo
    echo "Bundled content types:"
    find ezQuake.app/Contents -type f -exec file {} \;

    zip -9 ezQuake.zip ezQuake.app
}

function build_intel() {
    export CPU="x86_64"
    make strip
}

function build_arm64() {
    export CPU="arm64"
    make strip DARWIN_TARGET=arm64-apple-macos14
}

case $1 in
    "install-intel")
        install_intel
        ;;
    "install-arm64")
        install_arm64
        ;;
    "build-intel")
        build_intel
        ;;
    "build-arm64")
        build_arm64
        ;;
    "create-bundle")
        create_bundle $2
        ;;
    *)
        echo "Unknown arg $1"
        exit 1
        ;;
esac
