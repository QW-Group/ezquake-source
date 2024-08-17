#!/bin/sh

show_error() {
    echo "Error: $1"
    exit 1
}

if ! command -v git >/dev/null 2>&1; then
    show_error "Git is needed to checkout vcpkg, but it's not installed or not available in PATH."
fi

if [ -e ".git" ]; then
    echo "Updating submodules..."
    git submodule update --init --recursive
fi

if [ ! -e "vcpkg/.git" ]; then
    if [ ! -f "version.json" ]; then
        show_error "Unable to checkout vcpkg without 'version.json'."
    fi

    vcpkg_version=$(sed -n 's/.*"vcpkg": *"\(.*\)".*/\1/p' version.json)
    if [ -z "$vcpkg_version" ]; then
        show_error "Could not extract vcpkg version from version.json."
    fi

    if [ -d "vcpkg" ]; then
        rm -rf "vcpkg"
    fi

    git clone --branch "$vcpkg_version" --depth 1 https://github.com/microsoft/vcpkg.git "vcpkg"
    if [ $? -ne 0 ]; then
        show_error "Checkout of vcpkg failed."
    fi
fi

vcpkg/bootstrap-vcpkg.sh -disableMetrics
if [ $? -ne 0 ]; then
    show_error "vcpkg bootstrap failed."
fi
