#!/bin/sh
#
# fixbundle.sh
#
# Copyright (C) 2015 Florian Zwoch <fzwoch@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#

if [ $# -lt 1 ]; then
	echo "Usage: $0 <Bundle.app> [..]"
	echo ""
	echo "  This script will parse the 'Info.plist' of 'Bundle.app' for the excutable."
	echo "  Linked libraries of that executable that are _not_ system libraries will"
	echo "  be copied inside the bundle into 'Bundle.app/Contents/Frameworks'. The"
	echo "  executable's linked library paths are updated to their new locations"
	echo "  inside the bundle."
	echo ""
	echo "  You can list additional plugins that are installed inside the bundle to"
	echo "  be included. Example:"
	echo ""
	echo "    $0 MyApp.app MyApp.app/Contents/Frameworks/Plugins/PluginA.so"
	echo ""
	exit 1
fi

BUNDLE=${1%/}

if [ ! -d "$BUNDLE" ]; then
	echo "ERROR: Bundle '$BUNDLE' does not exist"
	exit 1
fi

EXECUTABLE=$BUNDLE/Contents/MacOS/$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$BUNDLE/Contents/Info.plist")

if [ ! -f "$EXECUTABLE" ]; then
	echo "ERROR: Executable '$EXECUTABLE' does not exist"
	exit 1
fi

for BINARY in "${@:2}"; do
	if [[ "$BINARY" != "$BUNDLE/"* ]]; then
		echo "ERROR: Binary '$BINARY' is not inside '$BUNDLE'"
		exit 1
	fi
	if [ ! -f "$BINARY" ]; then
		echo "ERROR: Binary '$BINARY' does not exist"
		exit 1
	fi
done

FRAMEWORK_DIR=$BUNDLE/Contents/Frameworks

if [ ! -d "$FRAMEWORK_DIR" ]; then
	mkdir -p "$FRAMEWORK_DIR"
fi

function get_deps {
	echo $(otool -L "$1" | grep local | awk '{print $1}')
}

# readlink -f
function realpath {
	cd $(dirname $1)
	FILE=$(basename $1)

	while [ -L "$FILE" ]; do
		FILE=$(readlink $FILE)
		cd $(dirname $FILE)
		FILE=$(basename $FILE)
	done

	echo $(pwd -P)/$FILE
}

function copy_deps {
	for DEP in $(get_deps "$1"); do
		REAL_DEP=$(realpath $DEP)

		if [ ! -f "$FRAMEWORK_DIR/$(basename $REAL_DEP)" ]; then
			cp "$DEP" "$FRAMEWORK_DIR/$(basename $REAL_DEP)"
			chmod 644 "$FRAMEWORK_DIR/$(basename $REAL_DEP)"
			copy_deps "$FRAMEWORK_DIR/$(basename $REAL_DEP)"
		fi
	done
}

for BINARY in "$EXECUTABLE" "${@:2}"; do
	copy_deps "$BINARY"
done

function fix_symbols {
	for DEP in $(get_deps "$1"); do
		install_name_tool -change "$DEP" "@executable_path/../Frameworks/$(basename $(realpath $DEP))" "$1"
	done
}

shopt -s nullglob

for BINARY in "$EXECUTABLE" "${@:2}" "$FRAMEWORK_DIR/"*.dylib "$FRAMEWORK_DIR/"*.so; do
	fix_symbols "$BINARY"
done
