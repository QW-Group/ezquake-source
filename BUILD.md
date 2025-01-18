# Compiling ezQuake

## Introduction

To provide a consistent build on Windows, Linux, macOS and BSD across various compilers,
dynamic and static linking, all described in one single file, [CMake][cmake] is used, with
the help of [vcpkg][vcpkg] to support static linking.

In its most basic form on Linux/BSD, with dynamic linking, a build can be invoked as follows:

```shell
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release # Configuration phase
make -j $(nproc)                    # Build phase
```

This will locate all system-wide dependencies and show a clear message if a mandatory
dependency is not installed. Check the output to see if it matches your expectations
in regard to optional dependencies.

When configure phase passes, the build phase will produce a dynamically linked binary.
The resulting binary can be found in the build directory at `build/ezquake-linux-x86_64`.

The default mode of compilation is to not show the full command line, only errors and
warnings. To enable verbose mode, set `CMAKE_VERBOSE_MAKEFILE` to `ON`.

While CMake can be configured via `CMAKE_C_CFLAGS`, it also supports picking up `CFLAGS`
from the aptly named environment variable `CFLAGS`.

The ezQuake build also declares a few options on its own to customize the resulting executable.
Unfortunately CMake CLI has no built-in command to list them, so check `CMakeLists.txt`:

```shell
grep -E ^option CMakeLists.txt
```

Putting the above customizations to work may look like this:
```shell
export CFLAGS=-march=native
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_VERBOSE_MAKEFILE=ON -DRENDERER_CLASSIC_OPENGL=OFF
make -j $(nproc)
```

CMake caches as much as possible when the build is initialized to allow for fast iteration
when developing. Should an option not activate as expected, removing the build directory
and starting over is always a failsafe. Static dependencies are [cached elsewhere](#Caches)
and will not have to be rebuilt if only the build directory is removed.

## CMake Presets

[CMake presets][cmake-presets] can be seen as aliases that associates a number of build
settings to a name. While this may not add much value for the trivial dynamically linked
Linux/BSD build, the presets do add some desirable conveniences for other targets as is 
seen in later sections.

There are two types of presets relevant to the ezQuake build:

* Configuration Presets
* Build Presets

The available presets can be listed via:

```shell
$ cmake --list-presets
Available configure presets:

  "mingw64-x64-cross"
  "mingw64-i686-cross"
  ...

$ cmake --build --list-presets
Available build presets:

  "msbuild-x64-debug"                 - Build msbuild-x64 debug
  "msbuild-x64-release"               - Build msbuild-x64 release
  "msbuild-x64-relwithdebinfo"        - Build msbuild-x64 release with debug info
  ...
```

These can be invoked, regardless of underlying build system, via:

```shell
# Creates build-dynamic directory, and initializes the build
cmake --preset dynamic 

# Builds executable in build-dynamic/Release/ezquake
cmake --build --preset dynamic-release

# ...or if skipping the build preset, pass path to build directory instead
camke --build build-dynamic --config Release
```

The build directory is by convention always called `build-${presetName}` when using presets.

All declared presets are of type _Multi-Config_ so they're able to produce _Debug_,
_Release_, and _RelWithDebInfo_ variants from the same configuration.

While build presets are somewhat optional when building from the terminal, they add
convenience when working from within an editor with CMake support.

## Static linking

Static builds are supported on Windows, Linux and macOS and offloads building of
dependencies to [vcpkg][vcpkg]. The CMake configuration is agnostic to dynamic or
static linking and enabling of static linking happens via parametrizing the CMake
invocation with the vcpkg toolchain. This is done automatically by using any of the
Windows, macOS, or the static presets.

Before starting a static build, vcpkg needs to be initialized. This is done by invoking
`bootstrap.sh` on *nix-like platforms. Depending on which [type of build](#Visual-Studio)
is used on Windows, the `bootstrap.ps1` PowerShell script does the equivalent.

The invocation of a Linux static build is near identical to a dynamic build:
```shell
./bootstrap.sh
cmake --preset static
cmake --build build-static --config Release
```

If you haven't compiled a static version before, or if the project has updated the vcpkg
repository version since last build, this will take a few minutes.

Note that for Linux/BSD builds, you have to install the appropriate X11/Wayland development
headers for the static build to work with each of those targets.

## Cross-Compilation to Windows

Any *nix environment can produce a Windows build via:
```shell
./bootstrap.sh
cmake --preset mingw64-x64-cross
cmake --build build-mingw64-x64-cross --config Release
```

Be sure to install mingw-w64 before running the above commands. A native compiler is also needed as parts of the vcpkg
build need to execute on the host.

## Visual Studio

### CMake Mode

Microsoft is highly invested in both CMake and vcpkg, so native support in Visual Studio has
existed for a number of years by now. Importing the project in Visual Studio 17 detects the
CMake and vcpkg combination and builds the dependencies.

Once done, the build presets will be listed in the _Configurations_ drop down. The `msbuild-*`
or `ninja-msvc-*` related build presets are typically a good fit.

To run ezQuake against a specific game directory, go via menu to _Debug_ > _Debug and Launch
Settings for ezquake_ which will open up the `launch_schema.json` file where you introduce
`currentDir` similar to the following:

```json
{
  "version": "...",
  "defaults": {},
  "configurations": [
    {
      "type": "...",
      "currentDir": "C:\\Quake"
    }
  ]
}
```

### Visual Studio Solution Mode

As the Visual Studio Solution is generated first after CMake configuration phase has finished,
the `./bootstrap.ps1` PowerShell script must be invoked the first time to initialize vcpkg.

```shell
powershell -File bootstrap.ps1
cmake --preset msbuild-x64
```

Once done you will have a Visual Studio Solution in `build-msbuild-x64/ezquake.sln`.
Any compilation changes that should be upstreamed must be updated in `CMakeLists.txt`.

During the configuration phase, if a `ezquake.vcxproj.user` file exists in the top directory,
this will be copied to the build directory next to the Visual Studio Solution to allow for
persisting custom settings as the solution is generated.

## Xcode / macOS

To simplify bundling static linking is used to build ezQuake on macOS, so start off by
invoking `bootstrap.sh` first. When initializing for example the `macos-arm64` preset
an Xcode project will be produced at `build-macos-arm64/ezquake.xcodeproj` with similar
structure to that of Visual Studio.

The same preset is also used when building via the terminal:
```
./bootstrap.sh
cmake --preset macos-arm64
cmake --build build-macos-arm64 --config Release
```
This will produce `ezQuake.app` under `build-macos-arm64/Release/ezQuake.app` by invoking
`xcodebuild` behind the scenes to do the actual building and bundling.

## Caches

If you don't intend to build ezQuake again and want to reclaim some space, you can find the shared vcpkg cache at:

* All platforms
  * `./vcpkg/buildtrees`
* *nix
  * `~/.cache/vcpkg/`
* Windows
  * `c:\Users\$UserName\AppData\Local\vcpkg`

## Developer Tidbits

### Adding new files to the project

In [CMakeLists.txt](CMakeLists.txt) source files are categorized into their approximate use
cases (client, server, common, sys, etc), and this also applies to header files. The reason
for this is to serve editors with the correct context, and if generating either a Xcode or
Visual Studio project this categorization is also visualized in the project tree view.

Looking forward, this will allow adding a `ezquake-sv` target by just reusing the relevant
subset of source files.

### Managing static dependencies

Vcpkg is used in [manifest mode][vcpkg-manifest], with the dependencies declared in `vcpkg.json`.
While it's possible to lock dependencies at a specific version, this is not used today. Instead
the vcpkg submodule dictates the set of dependency versions the project relies on.

If adding a new mandatory dependency that static versions of ezQuake should use, first find it
at [vcpkg.io][vcpkg-list], and if missing, read up on [overlay ports][vcpkg-overlay].

### Target platforms

If a specific platform requires customizations to how the static dependencies are built a [_triplet_][vcpkg-triplets]
for this platform can be introduced. An example of such an override is the [x64 MinGW](cmake/triplets/x64-mingw-static.cmake)
triplet that adds `-march=nehalem` when building dependencies.

 [cmake]: https://cmake.org/
 [cmake-presets]: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
 [cmake-cflags]: https://cmake.org/cmake/help/latest/envvar/CFLAGS.html
 [vcpkg]: https://learn.microsoft.com/en-us/vcpkg/
 [vcpkg-manifest]: https://learn.microsoft.com/en-us/vcpkg/concepts/manifest-mode
 [vcpkg-list]: https://vcpkg.io/en/
 [vcpkg-overlay]: https://learn.microsoft.com/en-us/vcpkg/concepts/overlay-ports
 [vcpkg-triplets]: https://learn.microsoft.com/en-us/vcpkg/users/triplets
 [vscmake]: https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio
