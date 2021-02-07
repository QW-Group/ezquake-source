# ezQuake — Modern QuakeWorld Client
[![Build Status](https://travis-ci.org/ezQuake/ezquake-source.svg?branch=master)](https://travis-ci.org/ezQuake/ezquake-source)

Homepage: [https://ezquake.github.io/][homepage]

Community discord: [http://discord.quake.world][discord]

This is the right place to start playing QuakeWorld&reg; — the fastest first
person shooter action game ever.

Combining the features of all modern QuakeWorld® clients, ezQuake makes
QuakeWorld&reg; easier to start and play. The immortal first person shooter
Quake&reg; in the brand new skin with superb graphics and extremely fast
gameplay.

## Features

 * Modern graphics
 * [QuakeTV][qtv] support
 * Rich menus
 * Multiview support
 * Tons of features to serve latest pro-gaming needs
 * Built in server browser & MP3 player control
 * Recorded games browser
 * Customization of all possible graphics elements of the game including Heads Up Display
 * All sorts of scripting possibilities
 * Windows, Linux, MacOSX and FreeBSD platforms supported (SDL2).

Our client comes only with bare minimum of game media. If you want to
experience ezQuake with modern graphics and other additional media including
custom configurations, maps, textures and more, try using the [nQuake][nQuake]-installer.

## Support

Need help with using ezQuake? Try #dev-corner on [discord][discord]

Or (less populated these days) visit us on IRC at QuakeNet, channel #ezQuake: [webchat][webchat] or [IRC][IRC].

Sometimes help from other users of ezQuake might be more useful to you so you
can also try visiting the [quakeworld.nu Client Talk-forums][forum].

If you have found a bug, please report it [here][issues]

## Installation guide

To play Quakeworld you need the files *pak0.pak* and *pak1.pak* from the original Quake-game.

### Install ezQuake to an existing Quake-installation
If you have an existing Quake-installation simply extract the ezQuake executable into your Quake-directory.

A typical error message when installing ezQuake into a pre-existing directory is about *glide2x.dll* missing.
To get rid of this error, remove the file *opengl32.dll* from your Quake directory.

### Upgrade an nQuake-installation
If you have a version of [nQuake][nQuake] already installed you can upgrade ezQuake by extracting the new executable into the nQuake-directory.

### Minimal clean installation
If you want to make a clean installation of ezQuake you can do this by following these steps:

1. Create a new directory
2. Extract the ezQuake-executable into this directory
3. Create a subdirectory called *id1*
4. Copy *pak0.pak* and *pak1.pak* into this subdirectory

## Compiling

### Compiling a Windows binary

#### Using Ubuntu Bash (WSL)

You can use the new Ubuntu Bash feature in Windows 10 to compile ezQuake for Windows.

To enable Bash for Windows, press the `Start` button and type `Turn Windows f` and select `Turn Windows features on or off`. Scroll down to `Windows Subsystem for Linux (Beta)` and enable it.

Now press WINDOWS+I, go to `Update & security` and then to the `For developers` tab. Enable `Developer mode`.

Now press the `Start` button again and enter `bash`. Click it and install Bash.

Enter the following command to install all required prerequisites to build ezQuake:

```
sudo apt-get install -y git mingw-w64 build-essential libspeexdsp-dev dos2unix pkg-config
```

Now clone the ezQuake source code:

```
git clone https://github.com/ezQuake/ezquake-source.git ezquake
```

Make sure line endings are not CRLF:

```
dos2unix *.sh
```

Now build the ezQuake executable:

```
EZ_CONFIG_FILE=.config_windows make
```

Copy the compiled binary to your Quake folder, the binary is called `ezquake.exe`.

#### Using a Linux system

Make sure you have mingw32 toolchain installed. On Arch Linux it's `mingw-w64` (select complete group).

Build an executable using the following command:

```
EZ_CONFIG_FILE=.config_windows make
```

You can add `-jN` as a parameter to `make` to build in parallell. Use number of cpu cores plus 1 (e.g. `-j5` if you have a quad core processor).

### Compiling a Linux binary

_These instructions were tested on Ubuntu_

Make sure you have the dependencies installed:

- For *Ubuntu 15.10-16.04*
```
sudo apt-get install git build-essential libsdl2-2.0-0 libsdl2-dev libjansson-dev libexpat1-dev libcurl4-openssl-dev libpng12-dev libjpeg-dev libspeex-dev libspeexdsp-dev
```
- For *Ubuntu 16.10+*
```
sudo apt install git build-essential libsdl2-2.0-0 libsdl2-dev libjansson-dev libexpat1-dev libcurl4-openssl-dev libpng-dev libjpeg-dev libspeex-dev libspeexdsp-dev
```
- For *openSUSE Tumbleweed*
```
sudo zypper install -t pattern devel_C_C++
sudo zypper install git pcre-devel Mesa-libGL-devel libSDL2-devel libjansson-devel libexpat-devel libcurl-devel libpng16-devel libjpeg8-devel libjpeg-turbo libsndfile-devel speex-devel speexdsp-devel libXxf86vm-devel
```

Clone the git repository:
```
git clone https://github.com/ezQuake/ezquake-source.git
```

Switch to `ezquake-source` path:
```
 cd ~/ezquake-source/
```
Run the compilation (replace 5 with the number of cpu cores you have +1):
```
make -j5
```
You can add `-jN` as a parameter to `make` to build in parallell. Use number of cpu cores plus 1 (e.g. `-j5` if you have a quad core processor).

Copy the compiled binary to your Quake folder, on 64bit linux the binary will be called `ezquake-linux-x86_64`.

### Compiling an OS X binary

_These instructions were tested on Mac OS X 10.10._

Get [Homebrew](http://brew.sh)

Run exactly as it says on the front page:

```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

Make sure you run the `brew doctor` as instructed before doing anything else.

Then run:

```
brew install sdl2 sdl2_net sdl2_image sdl2_gfx sdl2_mixer pcre jansson pkg-config speex speexdsp libsndfile
```

When it's done, just run `make` and it should compile without errors.


#### Creating an app bundle

Call from main ezquake-source directory, e.g. you probably do something like this:

```
make
sh misc/install/create_osx_bundle.sh
```

Current directory should have an `ezQuake.app` folder which is the app.

There will also be an `ezquake.zip` which basically just zips up the .app.

## Nightly builds

Nightly builds for Windows can be found [here][nightly]

 [nQuake]: http://nquake.com/
 [webchat]: http://webchat.quakenet.org/?channels=#ezquake
 [IRC]: irc://irc.quakenet.org/#ezquake
 [forum]: http://www.quakeworld.nu/forum/8
 [qtv]: http://qtv.quakeworld.nu/
 [nightly]: http://uttergrottan.localghost.net/ezquake/dev/nightlybuilds/win32/
 [releases]: https://github.com/ezQuake/ezquake-source/releases
 [issues]: https://github.com/ezQuake/ezquake-source/issues
 [homepage]: https://ezquake.github.io/
 [discord]: http://discord.quake.world/
