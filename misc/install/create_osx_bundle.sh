#!/bin/sh
#
# call from main ezquake-source directory. e.g. you probably do something like this:
#
#  $ make
#  $ sh misc/install/create_osx_bundle.sh
#
# current directory should have an ezQuake.app folder which is the app.
# there will also be an ezquake.zip which basically just zips up the .app.
#

ARCH=$(uname -m | sed -e s/i.86/i386/ -e s/amd64/x86_64/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/)

BUNDLE_NAME=ezQuake.app
BINARY=ezquake-darwin-${ARCH}
ICON_FILE=ezquake.icns
VERSION=$(cat src/version.h | grep VERSION_NUMBER | cut -d " " -f 3 | sed -e 's/^"//' -e 's/"$//')

if [ -d $BUNDLE_NAME ]; then
	echo "$BUNDLE_NAME already exists"
	exit 1
fi

if [ ! -f $BINARY ]; then
	echo "$BINARY not found"
	exit 1
fi

mkdir -p $BUNDLE_NAME/Contents/MacOS
mkdir -p $BUNDLE_NAME/Contents/Resources
cp $BINARY $BUNDLE_NAME/Contents/MacOS/.
cp $(dirname $0)/$ICON_FILE $BUNDLE_NAME/Contents/Resources/.

echo '#!/bin/sh' > $BUNDLE_NAME/Contents/MacOS/ezquake
echo '' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo 'DIRNAME="$HOME"/Library/Application\ Support/ezQuake' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo 'if [ ! -f "$DIRNAME"/id1/pak0.pak ]; then' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '    mkdir -p "$DIRNAME"/id1' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '    touch "$DIRNAME"/id1/Copy\ your\ pak0.pak\ and\ pak1.pak\ files\ here.txt' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '    osascript -e "tell app \"Finder\" to open (\"${DIRNAME}/id1/\" as POSIX file)"' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '    osascript -e "tell app \"Finder\" to activate"' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '    exit' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo 'fi' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo 'PNAME="$(cd "$(dirname "$0")"; pwd -P)"' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo "exec \"\$PNAME\"/$BINARY -basedir \"\$DIRNAME\"" >> $BUNDLE_NAME/Contents/MacOS/ezquake

chmod u+x $BUNDLE_NAME/Contents/MacOS/ezquake

/usr/libexec/PlistBuddy -c "Add :CFBundleName string \"ezQuake\"" $BUNDLE_NAME/Contents/Info.plist > /dev/null
/usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string \"$ICON_FILE\"" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleExecutable string \"ezquake\"" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleIdentifier string \"io.github.ezquake\"" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleVersion string \"$VERSION\"" $BUNDLE_NAME/Contents/Info.plist

# qw:// protocol support
/usr/libexec/PlistBuddy -c "Add :CFBundleURLTypes array" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleURLTypes:0 dict" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleURLTypes:0:CFBundleURLName string QW" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleURLTypes:0:CFBundleURLSchemes array" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleURLTypes:0:CFBundleURLSchemes:0 string qw" $BUNDLE_NAME/Contents/Info.plist

# .mvd/.qwd/.dem file type support
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes array" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0 dict" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeName string Quake demo" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeRole string Viewer" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeIconFile string $ICON_FILE" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeExtensions array" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeExtensions:0 string mvd" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeExtensions:1 string qwd" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeExtensions:2 string dem" $BUNDLE_NAME/Contents/Info.plist

sh $(dirname $0)/fixbundle.sh $BUNDLE_NAME $BUNDLE_NAME/Contents/MacOS/$BINARY
ditto -c -k --keepParent --arch x86_64 --arch arm64 $BUNDLE_NAME ezquake.zip
