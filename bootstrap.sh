#!/bin/sh -e

show_error() {
    printf "\e[31mError\e[0m: $1\n"
    exit 1
}

required_commands="cmake ninja git automake autoconf pkg-config curl zip unzip tar"

missing_deps=""
for cmd in $required_commands; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        missing_deps="$missing_deps $cmd"
    fi
done

# naming differs on macOS/Linux
if ! command -v "libtoolize" >/dev/null 2>&1 && ! command -v "glibtoolize" >/dev/null 2>&1; then
    missing_deps="$missing_deps libtool";
fi

if [ -n "$missing_deps" ]; then
    show_error "Install packages that provide support for:$missing_deps"
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
