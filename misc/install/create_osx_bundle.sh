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

BUNDLE_NAME=ezQuake.app
BINARY=ezquake-darwin-x86_64
ICON_FILE=ezquake.icns

if [ -d $BUNDLE_NAME ]; then
	echo "$BUNDLE_NAME already exists"
	exit 1
fi

if [ ! -f $BINARY ]; then
	echo "$BINARY not found"
	exit 1
fi

mkdir -p $BUNDLE_NAME/Contents/MacOS
mkdir -p $BUNDLE_NAME/Contents/Resources/id1
cp $BINARY $BUNDLE_NAME/Contents/MacOS/.
cp $(dirname $0)/$ICON_FILE $BUNDLE_NAME/Contents/Resources/.
touch $BUNDLE_NAME/Contents/Resources/id1/Copy\ your\ pak0.pak\ and\ pak1.pak\ files\ here.txt

echo '#!/bin/sh' > $BUNDLE_NAME/Contents/MacOS/ezquake
echo '' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo 'DIRNAME="$(cd "$(dirname "$0")"; pwd -P)"' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo 'if [ ! -f "$DIRNAME"/../Resources/id1/pak0.pak ]; then' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '    osascript -e "tell app \"Finder\" to open (\"${DIRNAME%/*}/Resources/id1/\" as POSIX file)"' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '    osascript -e "tell app \"Finder\" to activate"' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '    exit' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo 'fi' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo '' >> $BUNDLE_NAME/Contents/MacOS/ezquake
echo "exec \"\$DIRNAME\"/$BINARY -basedir \"\$DIRNAME\"/../Resources" >> $BUNDLE_NAME/Contents/MacOS/ezquake

chmod u+x $BUNDLE_NAME/Contents/MacOS/ezquake

/usr/libexec/PlistBuddy -c "Add :CFBundleName string \"ezQuake\"" $BUNDLE_NAME/Contents/Info.plist > /dev/null
/usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string \"$ICON_FILE\"" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleExecutable string \"ezquake\"" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleIdentifier string \"net.sf.ezquake\"" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleVersion string \"3.0.0\"" $BUNDLE_NAME/Contents/Info.plist

# qw:// protocol support
/usr/libexec/PlistBuddy -c "Add :CFBundleURLTypes array" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleURLTypes:0 dict" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleURLTypes:0:CFBundleURLName string QW" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleURLTypes:0:CFBundleURLSchemes array" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleURLTypes:0:CFBundleURLSchemes:0 string qw" $BUNDLE_NAME/Contents/Info.plist

# .mvd file type support
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes array" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0 dict" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeName string MVD demo" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeRole string Viewer" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeIconFile string $ICON_FILE" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeExtensions array" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleDocumentTypes:0:CFBundleTypeExtensions:0 string mvd" $BUNDLE_NAME/Contents/Info.plist

sh $(dirname $0)/fixbundle.sh $BUNDLE_NAME $BUNDLE_NAME/Contents/MacOS/$BINARY
ditto -c -k --keepParent --arch x86_64 $BUNDLE_NAME ezquake.zip
