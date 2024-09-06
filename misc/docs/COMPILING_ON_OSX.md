# How to compile on macOS

Get Homebrew: http://brew.sh

Run exactly as it says on the front page:
```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

Make sure you run the 'brew doctor' as instructed before doing anything else.

Then run:
```
brew install autoconf automake libtool cmake ninja
```

When it's done, initialize cmake to build the dependencies:
```
./bootstrap.sh
cmake --preset macos-arm64 (or -x64)
```

And finally building ezQuake:
```
cmake --build build-macos-arm64 --config Release
```

This will produce `build-macos-arm64/Release/ezQuake.app` which you can place anywhere. The first
launch will prompt you to select your Quake directory which will be automatically reused for
subsequent invocations. The application is sandboxed to this directory to keep your files safe.