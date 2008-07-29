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

# / Localghost

# Local paths
BUILDDIR=~quake/ezquake_nightly/build

# Server settings
LOGIN=nightly
SERVERHOST=uttergrottan.localghost.net
REMOTEPATH=nightlybuilds/linux

if [ ! -d ${BUILDDIR} ]
then
	mkdir ${BUILDDIR}
	FIRSTRUN=1
fi

cd ${BUILDDIR}

if (( $FIRSTRUN ))
then
	OLDREVISION='-1'
	svn co https://ezquake.svn.sourceforge.net/svnroot/ezquake/trunk/ezquake .
	cd libs/linux-x86 && ./download.sh
	cd ${BUILDDIR}
else
	OLDREVISION=`/usr/bin/svn info | grep Revision | awk '{print $2}'`
	svn update
fi

make clean
rm -rf release-x86/*

DATE=`/bin/date +"%F"`
REVISION=`/usr/bin/svn info | grep Revision | awk '{print $2}'`
EXECNAME=ezquake-gl-r${REVISION}.glx
FILENAME=${DATE}-${REVISION}-ezquake-gl.glx.7z

echo "Old revision: $OLDREVISION"
echo "New revision: $REVISION"
if test $OLDREVISION = $REVISION
then
	echo "No need for new build, revision hasn't changed"
	exit
fi

# This is needed because my version of gcc does not support "-mtune=generic"
cp Makefile Makefile.tmp
cat Makefile.tmp | sed -e 's/-mtune=generic//' > Makefile

make
cd release-x86
if [ -e ezquake-gl.glx ]
then
	mv ezquake-gl.glx ${EXECNAME}
	p7zip ${EXECNAME}
	mv ${EXECNAME}.7z ${FILENAME}
	scp ${FILENAME} ${LOGIN}@${SERVERHOST}:${REMOTEPATH}
else
	echo "ezquake-gl.glx not found! Build failed"
	exit
fi

