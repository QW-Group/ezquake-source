## Compiling for Windows

### Using Visual Studio

Open Visual Studio 2022, select _Clone a repository_ and point to this repository.

This will automatically open the project as a CMake project and build the dependencies.

The `msbuild-*` presets listed in the _Configurations_ drop-down are a good pick for this setup.

To configure the game dir you want to run with you select `ezquake.exe` in the _Build Targets_ drop down
and continue to the menu _Debug_ -> _Debug and Launch Settings for ezquake_. This opens up `launch_schema.json`
where you add `currentDir` for the target you intend to launch:

```
{
  "version": ...,
  "defaults": {},
  "configurations": [
    {
      "type": ...,
      "currentDir": "c:\\Quake"
    }
  ]
}
```

### Command line and/or Visual Studio Solution

If this is a fresh clone, then start by running the `bootstrap.ps1` PowerShell script which
initializes support for building the 3rd party dependencies. You can do this via the right click
context menu on the file where you should see _'Run With PowerShell'_.

First initialize the build system, which will build all 3rd party dependencies, takes some time.
```
cmake --preset msbuild-x64
```
This will also generate a Visual Studio Solution under `build-msbuild-x64/ezquake.sln` based on
the compiler settings declared in `CMakeLists.txt` should you prefer that to Visual Studio's CMake
support.

Then to build a specific configuration.
```
cmake --build build-msbuild-x64 --config Release
```


### Preparing Ubuntu Bash / WSL

You can use the new Ubuntu Bash feature in Windows 10 to compile ezQuake for Windows.

To enable Bash for Windows, press the `Start` button and type `Turn Windows f` and select `Turn Windows features on or off`. Scroll down to `Windows Subsystem for Linux (Beta)` and enable it.

Now press WINDOWS+I, go to `Update & security` and then to the `For developers` tab. Enable `Developer mode`.

Now press the `Start` button again and enter `bash`. Click it and install Bash.

### Cross compiling from Linux or WSL

Enter the following command to install all required prerequisites to build ezQuake:

```
sudo apt-get install -y \
  autoconf automake libtool pkg-config curl zip unzip tar \
  cmake ninja-build mingw-w64
```

Now clone the ezQuake source code:

```
git clone https://github.com/ezQuake/ezquake-source.git ezquake
```

Initialize the build and compile dependencies:
```
./bootstrap.sh
cmake --preset mingw64-x64-cross
```
This will take some time the first invocation, but is a one-off cost.

The dependency build cache is found in `${HOME}/.cache/vcpkg` if you
don't intend to build again and want to reclaim some space.

Now build the ezQuake executable:
```
cmake --build build-mingw64-x64-cross --config Release
```

This will produce `ezquake.exe` in the current directory. 