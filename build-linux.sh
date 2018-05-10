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

#ansi color codes
RED='\e[31m'
GREEN='\e[32m'
NC='\e[0m'

CPU=$(uname -m | sed -e s/i.86/i386/ -e s/amd64/x86_64/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/)

trap "if [ \$? -ne 0 ];then echo -e \"${RED}Failure, bailing out.${NC}\";if [ -e $buildlog ];then echo \"Please see $buildlog for details.\";fi;exit 0;fi" INT TERM EXIT

if [ ! -e Makefile ];then
	echo "Cannot find Makefile, please run this script from the source code directory."
	exit 1
fi

if ! type "sudo" > /dev/null; then
	echo "Could not find sudo, please install it.  Exiting."
	exit 1
fi


function check_status() {
	if [ $? -ne 0 ];then
		cat "$buildlog"
		echo -e "${RED}$1 - Step failed.${NC}"
		exit 2 
	fi
}

if [ -f /etc/os-release ];then
	. /etc/os-release
else
	echo "Your environment isn't supported by this script.  Bailing out."
	exit 1
fi

debian=0

if [ "$ID" == "debian" ] || [ "$ID_LIKE" == "debian" ];then
	debian=1
elif [ "$ID" != "fedora" ];then
	echo "Your environment isn't supported by this script.  Bailing out."
	exit 1
fi

if [ $debian -eq 1 ];then
	echo "Updating apt repo list... (running with sudo)"
	sudo apt-get update -y -qq
	check_status "Repo update"
	if [ $(grep -e 'stretch' -e '8.[0-9]' -c '/etc/debian_version') -eq 1 ];then
		packages="$deb_old"
	else
		packages="$deb"
	fi
	echo "Installing packages... (running with sudo)"
	sudo apt-get install -y -q $packages > "$buildlog" 2>&1
	check_status "Dependency installation"
else
	echo "Updating yum repo list... (running with sudo)"
	sudo yum clean all -yqqq
	sudo yum check-update -yqqq > "$buildlog" 2>&1
	check_status "Repo update"
	echo "Installing packages..."
	packages="$rpm"
	sudo yum install -yq $packages >> "$buildlog" 2>&1
	check_status "Dependency installation"
fi

echo "Cleaning up any previous build files..."
make clean >> "$buildlog" 2>&1
echo "Building source..."
make -j$(nproc) >> "$buildlog" 2>&1
check_status "Compilation"
echo -e "${GREEN}Build completed successfully.  Copy ezquake-linux-${CPU} into your quake directory.${NC}"

exit 0
