#!/bin/sh
cd ezquake-source

OREV=$(git rev-parse HEAD)
git pull
NREV=$(git rev-parse HEAD)

if [ "$OREV" = "$NREV" ]; then
	echo "No new build needed"
	if [ -z "$FORCEBUILD" ]; then 
		exit 0
	fi
fi

DATE=$(date +"%F")
SHORTREV=$(git rev-parse --short HEAD)

EZQUAKE_FNAME="ezquake-$SHORTREV.exe"
ARCHIVE_FNAME="$DATE-$SHORTREV-ezquake.7z"

build() {
	echo "Building ezquake $NREV" | tee -a build.log
	EZ_CONFIG_FILE=.config_windows make |& tee -a build.log
	if [ -e ezquake.exe ]; then
		mv ezquake.exe $EZQUAKE_FNAME
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
	git log $NREV...$OREV >> README_nightly.txt
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
	rm build.log README_nightly.txt
}

build
make_readme
make_archive
cleanup
