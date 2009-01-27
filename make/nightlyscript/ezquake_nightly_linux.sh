#!/bin/bash

# Script to build the latest version of ezquake and upload to a server
# Requirements: 
# * Subversion 	http://subversion.tigris.org/
# * p7zip 	http://p7zip.sourceforge.net/ 
#
# To run every night at 06:00 add this line to crontab -e
#   0 6 * * *             /path/to/mknightly.sh
#
# To be able to copy the file to the remote system without entering a password:
# 1) Create a public ssh-key:
#	ssh-keygen	( use default values and empty passphrase )
# 2) This will create the file ~/.ssh/id_rsa.pub. Copy the contents of this file into the clipboard
# 3) SSH to the remote server and in the homedir:
#	mkdir .ssh
#	edit ".ssh/authorized_keys" and paste the contents of the clipboard into this file
#
# / Localghost

# Local paths
WORKDIR=~quake/ezquake_nightly
BUILDDIR=$WORKDIR/build

# Server settings
LOGIN=nightly
SERVERHOST=uttergrottan.localghost.net
REMOTEPATH=nightlybuilds/linux

if [ ! -d ${BUILDDIR} ]
then
	mkdir -p ${BUILDDIR}
	FIRSTRUN=1
fi

cd ${BUILDDIR}

if (( $FIRSTRUN ))
then
	# First time the script is executed, checkout source and download required libraries
	OLDREVISION='-1'
	svn co https://ezquake.svn.sourceforge.net/svnroot/ezquake/trunk/ezquake . >> ${BUILDDIR}/nightly_info.tmp 2>&1
	cd libs/linux-x86 && ./download.sh
	cd ${BUILDDIR}
else
	OLDREVISION=`/usr/bin/svn info | grep Revision | awk '{print $2}'`
	svn update >> ${BUILDDIR}/nightly_info.tmp 2>&1
fi

make clean >> ${BUILDDIR}/nightly_info.tmp 2>&1

rm -rf release-x86/*

DATE=`/bin/date +"%F"`
REVISION=`/usr/bin/svn info | grep Revision | awk '{print $2}'`
EXECNAME=ezquake-gl-r${REVISION}.glx
FILENAME=${DATE}-r${REVISION}-ezquake-gl.glx.7z

echo "+-------------------------+"
echo "| Nigtly build $DATE |"
echo "+-------------------------+"
echo "Old revision: $OLDREVISION"
echo "New revision: $REVISION"

if test $OLDREVISION = $REVISION
then
	echo "No need for new build, revision hasn't changed"
	exit
fi

# Print svn log-message(s) for the new revision(s)
svn log -r $(($OLDREVISION+1)):$REVISION > ${BUILDDIR}/nightly_loginfo.tmp 2>&1
cat ${BUILDDIR}/nightly_loginfo.tmp >> ${BUILDDIR}/nightly_info.tmp

# This is needed because my version of gcc does not support "-mtune=generic"
cp Makefile Makefile.tmp
cat Makefile.tmp | sed -e 's/-mtune=generic//' > Makefile

make >> ${BUILDDIR}/nightly_info.tmp 2>&1

cd release-x86
if [ -e ezquake-gl.glx ]
then

	echo "ezQuake nightly build for linux ($DATE)" > README.TXT
	echo "Changes in this build:" >> README.TXT
	cat ${BUILDDIR}/nightly_loginfo.tmp >> README.TXT
	echo "Please notice that this build is still in alpha so expect various problems." >> README.TXT
	echo "Report any bugs in #ezquake on quakenet." >> README.TXT
	echo "/ LocalGhost" >> README.TXT

	mv ezquake-gl.glx ${EXECNAME}
	7zr a ${FILENAME} ${EXECNAME} README.TXT >> ${BUILDDIR}/nightly_info.tmp 2>&1
	scp ${FILENAME} ${LOGIN}@${SERVERHOST}:${REMOTEPATH} >> ${BUILDDIR}/nightly_info.tmp 2>&1
	echo "Build successful"
else
	echo "ezquake-gl.glx not found! Build failed"
fi

echo "+--------------------+"
echo "| Build information: |"
echo "+--------------------+"
cat ${BUILDDIR}/nightly_info.tmp
rm ${BUILDDIR}/nightly_info.tmp

