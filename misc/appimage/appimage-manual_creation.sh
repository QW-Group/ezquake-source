#!/bin/bash

set -e

SKIP_DEPS="${SKIP_DEPS:-0}"

ARCH=$(uname -m)
ARCHDASH=$(echo "$ARCH"|tr '_' '-')
APPIMAGETOOL="https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-${ARCH}.AppImage"

curl -Ls "$APPIMAGETOOL" > appimagetool
if [ $? -ne 0 ];then
	echo "failed to retrieve appimagetool.  bailing out."
	exit 1
fi
chmod -f +x appimagetool

#unused but must exist
DESKTOP_ENTRY='[Desktop Entry]
Comment=A modern QuakeWorld client focused on competitive online play
Name=ezQuake
Exec=ezquake-linux-'$ARCH'
Icon=io.github.ezQuake
Type=Application
StartupNotify=true
Terminal=false
Categories=Game;
Keywords=quake;first;person;shooter;multiplayer;'

QUAKE_SCRIPT='#!/usr/bin/env bash
export LD_LIBRARY_PATH="${APPIMAGE_LIBRARY_PATH}:${APPDIR}/usr/lib:${LD_LIBRARY_PATH}"
cd "$OWD"
exec "${APPDIR}/usr/bin/ezquake-linux-'$ARCH'" $*'
#exec "${APPDIR}/usr/lib/ld-linux-'$ARCHDASH'.so.2" "${APPDIR}/usr/bin/ezquake-linux-'$ARCH'" $*'

unset CC
if [ "$ARCH" == "x86_64" ];then
	march="-march=nehalem"
fi
export CFLAGS="$march -O3 -pipe -flto=$(nproc)"
export LDFLAGS="$CFLAGS"

DIR="$(pwd)"

if [ -d AppDir ];then
	rm -rf AppDir
fi
mkdir -p "$DIR/build" || exit 1
mkdir -p "$DIR/AppDir/usr/bin" || exit 1
mkdir -p "$DIR/AppDir/usr/lib" || exit 1

VERSION=$(sed -n 's/.*VERSION_NUMBER.*"\(.*\)".*/\1/p' src/version.h)
REVISION=$(git log -n 1|head -1|awk '{print $2}'|cut -c1-6)

if [ $SKIP_DEPS -eq 0 ];then
  chmod +x ./build-linux.sh && \
  nice ./build-linux.sh || exit 3
else
  make -j$(nproc)
fi

cp -f ezquake-linux-$ARCH "$DIR/AppDir/usr/bin/." || exit 4
rm -f "$DIR/AppDir/AppRun"
echo "$QUAKE_SCRIPT" > "$DIR/AppDir/AppRun" || exit 4
chmod +x "$DIR/AppDir/AppRun" || exit 4
echo "$DESKTOP_ENTRY" > "$DIR/AppDir/io.github.ezQuake.desktop" || exit 4
cp "$DIR/dist/linux/io.github.ezQuake.128.png" "$DIR/AppDir/io.github.ezQuake.png"||true #copy over quake png if it exists
mkdir -p "$DIR/AppDir/usr/share/metainfo"
sed 's,EZQUAKE_VERSION,'$VERSION-$REVISION',g;s,EZQUAKE_DATE,'$(date +%F)',g' "$DIR/misc/appimage/ezquake.appdata.xml.template" > "$DIR/AppDir/usr/share/metainfo/ezquake.appdata.xml"
ldd "$DIR/AppDir/usr/bin/ezquake-linux-$ARCH" | \
	grep --color=never -v libGL| \
	grep --color=never -v libc.so| \
	awk '{print $3}'| \
	xargs -I% cp -Lf "%" "$DIR/AppDir/usr/lib/." || exit 5
strip -s "$DIR/AppDir/usr/lib/"* || exit 5
strip -s "$DIR/AppDir/usr/bin/"* || exit 5

cd "$DIR" || exit 5
./appimagetool AppDir ezQuake-$ARCH.AppImage || exit 7
