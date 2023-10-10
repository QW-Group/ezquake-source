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

if [ $# -lt 2 ]; then
	echo "Usage: $0 <Bundle.app> <executable> [..]"
	echo ""
	echo "  Linked libraries of that executable that are _not_ system libraries will"
	echo "  be copied inside the bundle into 'Bundle.app/Contents/Frameworks'. The"
	echo "  executable's linked library paths are updated to their new locations"
	echo "  inside the bundle."
	echo ""
	exit 1
fi

BUNDLE=${1%/}

if [ ! -d "$BUNDLE" ]; then
	echo "ERROR: Bundle '$BUNDLE' does not exist"
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
	# Lines ending with : refers to the library itself
	echo $(otool -L "$1" | grep 'local\|homebrew\|@loader_path' | awk '{print $1}' | grep -Ev ':$')
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
		if [[ "$(realpath $DEP)" == "$(realpath $1)" ]]; then
			continue
		fi;

		# Handle libraries with relative load paths
		if [[ "$DEP" == "@loader_path/"* ]]; then
			LIBDIR=$(dirname "$1")
			REAL_DEP=$(realpath "${DEP/@loader_path/$LIBDIR}")
		else
			REAL_DEP=$(realpath $DEP)
		fi

		FRAMEWORK_TARGET="$FRAMEWORK_DIR/$(basename $1)"
		FRAMEWORK_DEP="$FRAMEWORK_DIR/$(basename $REAL_DEP)"

		if [ ! -f "$FRAMEWORK_DIR/$(basename $REAL_DEP)" ]; then
			cp "$REAL_DEP" "$FRAMEWORK_DEP"
			chmod 644 "$FRAMEWORK_DEP"
		fi

		# Fix transitive deps where we still know the origin of parent dependency
		install_name_tool -change "$DEP" "@executable_path/../Frameworks/$(basename $REAL_DEP)" "$FRAMEWORK_TARGET"

		copy_deps $REAL_DEP
	done
}

for BINARY in "${@:2}"; do
	copy_deps "$BINARY"
done

function fix_symbols {
	for DEP in $(get_deps "$1"); do
		install_name_tool -change "$DEP" "@executable_path/../Frameworks/$(basename $(realpath $DEP))" "$1"
	done
}

shopt -s nullglob

for BINARY in "${@:2}"; do
	fix_symbols "$BINARY"
done

codesign --force --deep --sign - $BUNDLE
