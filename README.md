# ezQuake — Modern QuakeWorld Client
Homepage: [https://ezquake.com][homepage]

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

On Linux, `./build-linux.sh` produces an ezQuake binary in the top directory. 

For a more in-depth description of how to build on all platforms, have a look at 
[BUILD.md](BUILD.md).

## Nightly builds

Nightly builds can be found [here][nightly]

 [nQuake]: http://nquake.com/
 [webchat]: http://webchat.quakenet.org/?channels=#ezquake
 [IRC]: irc://irc.quakenet.org/#ezquake
 [forum]: http://www.quakeworld.nu/forum/8
 [qtv]: http://qtv.quakeworld.nu/
 [nightly]: https://builds.quakeworld.nu/ezquake/snapshots/
 [releases]: https://github.com/ezQuake/ezquake-source/releases
 [issues]: https://github.com/ezQuake/ezquake-source/issues
 [homepage]: https://ezquake.com
 [discord]: http://discord.quake.world/
