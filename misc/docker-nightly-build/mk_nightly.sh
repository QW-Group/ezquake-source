#!/bin/bash

if [ ! -d /source/ezquake-source ]; then
	# We need to initialize the build environment
	cd /source && git clone https://github.com/ezQuake/ezquake-source.git
fi

cd /source/ezquake-source

OLD_REV=$(git rev-parse HEAD)
git pull
NEW_REV=$(git rev-parse HEAD)

if [ "$OLD_REV" = "$NEW_REV" ]; then
	echo "No new build needed"
	if [ -z "$FORCEBUILD" ]; then 
		exit 0
	fi
fi

DATE=$(date +"%F")
SHORTREV=$(git rev-parse --short HEAD)

EZQUAKE_FNAME=ezquake-$SHORTREV
ARCHIVE_FNAME="$DATE-$SHORTREV-ezquake.7z"


build() {
	echo "Building ezquake $NEW_REV" | tee -a build.log
	if [ "$TARGET_PLATFORM" = "WINDOWS" ]; then
		EZ_CONFIG_FILE=.config_windows make |& tee -a build.log
	else
		make |& tee -a build.log
	fi

	if [ -e ezquake.exe ]; then
		EZQUAKE_FNAME+=".exe"
		mv ezquake.exe $EZQUAKE_FNAME
	elif [ -e ezquake-linux-x86_64 ]; then
		EZQUAKE_FNAME+="-linux-x86_64"
		mv ezquake-linux-x86_64 $EZQUAKE_FNAME
	else
		echo "No output file"
		make clean
		exit 1
	fi
}

make_readme() {
	cat > README_nightly.txt <<- EOM
		ezQuake nightly build

		Please notice that this build is still in alpha so expect various problems.
		Report any bugs at https://github.com/ezQuake/ezquake-source/issues or in #ezquake on quakenet.

		Changes from last nightly build:
	EOM
	git log $NEW_REV...$OLD_REV >> README_nightly.txt
}

make_archive() {
	if [ -e $EZQUAKE_FNAME ]; then
		FILES+="$EZQUAKE_FNAME "
	fi
	if [ -e build.log ]; then
		FILES+="build.log "
	fi
	if [ -e README_nightly.txt ]; then
		FILES+="README_nightly.txt "
	fi
	7z a /nightly/$ARCHIVE_FNAME $FILES
}

cleanup() {
	make clean
	rm build.log README_nightly.txt $EZQUAKE_FNAME
}

build
make_readme
make_archive
cleanup
