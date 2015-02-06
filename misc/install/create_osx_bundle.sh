#!/bin/sh
#
# call from main ezquake-source directory. e.g. you probably do something like this:
#
#  $ make
#  $ sh misc/install/create_osx_bundle.sh
#
# current directory should have an EZQuake.app folder which is the app.
# there will also be an ezquake.zip which basically just zips up the .app.
#

BUNDLE_NAME=EZQuake.app
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

echo "#!/bin/sh\nif [ ! -f \"\$(dirname \$0)/../Resources/id1/pak1.pak\" ]; then\n\tosascript -e 'display dialog \"ERROR: pak1.pak not found\"'\n\texit\nfi\n\nexec \$(dirname \$0)/$BINARY -basedir \$(dirname \$0)/../Resources" > $BUNDLE_NAME/Contents/MacOS/ezquake
chmod u+x $BUNDLE_NAME/Contents/MacOS/ezquake

/usr/libexec/PlistBuddy -c "Add :CFBundleName string \"EZQuake\"" $BUNDLE_NAME/Contents/Info.plist > /dev/null
/usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string \"$ICON_FILE\"" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleExecutable string \"ezquake\"" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleIdentifier string \"net.sf.ezquake\"" $BUNDLE_NAME/Contents/Info.plist
/usr/libexec/PlistBuddy -c "Add :CFBundleVersion string \"3.0.0\"" $BUNDLE_NAME/Contents/Info.plist

sh $(dirname $0)/fixbundle.sh $BUNDLE_NAME $BUNDLE_NAME/Contents/MacOS/$BINARY
ditto -c -k --keepParent --arch x86_64 $BUNDLE_NAME ezquake.zip
