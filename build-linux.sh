#!/bin/bash 
# simple build script for linux

#package list for debian 9.x+
deb="git build-essential libsdl2-2.0-0 libsdl2-dev libjansson-dev libexpat1-dev libcurl4-openssl-dev libpng-dev libjpeg-dev libspeex-dev libspeexdsp-dev"
#package list for debian 8.x
deb_old="git build-essential libsdl2-2.0-0 libsdl2-dev libjansson-dev libexpat1-dev libcurl4-openssl-dev libpng12-dev libjpeg-dev libspeex-dev libspeexdsp-dev"
#package list for fedora/el
rpm="vim-common pcre-devel mesa-libGL-devel SDL2-devel make gcc jansson-devel expat-devel libcurl-devel libpng-devel libjpeg-turbo-devel speex-devel speexdsp-devel"

buildlog="/tmp/ezquake-build.log"

if [ -e "$buildlog" ];then
	rm -f "$buildlog"
fi

debian=0

#ansi color codes
RED='\e[31m'
GREEN='\e[32m'
NC='\e[0m'

trap "if [ \$? -ne 0 ];then echo -e \"${RED}Failure, bailing out.${NC}\";if [ -e $buildlog ];then echo \"Please see $buildlog for details.\";fi;exit 0;fi" INT TERM EXIT

if [ $(id -u) -ne 0 ];then
	echo "You must be root to run this script as it interfaces with your package manager, run with sudo or root privileges."
	exit 1
fi

if [ ! -e Makefile ];then
	echo "Cannot find Makefile, please run this script from the source code directory."
	exit 1
fi

function check_status() {
	if [ $? -ne 0 ];then
		echo "Step failed."
		exit 2 
	fi
}

if [ -e /etc/debian_version ];then
	debian=1
	old=$(grep -e 'stretch' -e '8.[0-9]' -c '/etc/debian_version')
elif [ ! -e /etc/redhat-release ];then
	echo "Your environment isn't supported by this script.  Bailing out."
	exit 1
fi

if [ $debian -eq 1 ];then

	echo "Updating apt repo list..."
	apt-get update -y -qq
	check_status

	if [ $old -eq 1 ];then
		packages="$deb_old"
	else
		packages="$deb"
	fi

	echo "Installing packages..."
	apt-get install -y -q $packages > "$buildlog" 2>&1
	check_status

else

	echo "Updating yum repo list..."
	yum clean all -yqqq
	yum check-update -yqqq > /dev/null 2>&1
	check_status
	echo "Installing packages..."
	packages="$rpm"
	yum install -yq $packages > "$buildlog" 2>&1
	check_status

fi

echo "Cleaning up any previous build files..."
make clean >/dev/null 2>&1
echo "Building source..."
make -j$(nproc) >> "$buildlog" 2>&1
check_status
echo -e "${GREEN}Build completed successfully.${NC}"

exit 0
