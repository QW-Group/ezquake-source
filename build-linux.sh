#!/bin/sh -e
# simple build script for linux

# ANSI color codes
RED='\e[31m'
GREEN='\e[32m'
YELLOW='\e[33m'
NC='\e[0m'

BUILD_LOG=/tmp/ezquake-build.log

PKGS_DEB="git build-essential libsdl2-2.0-0 libsdl2-dev libjansson-dev libexpat1-dev libcurl4-openssl-dev libpng-dev libjpeg-dev libspeex-dev libspeexdsp-dev libfreetype6-dev libsndfile1-dev"
PKGS_RPM="pcre-devel mesa-libGL-devel SDL2-devel make gcc jansson-devel expat-devel libcurl-devel libpng-devel libjpeg-turbo-devel speex-devel speexdsp-devel freetype-devel libsndfile-devel libXxf86vm-devel"
PKGS_ARCH=""

CPU=$(uname -m | sed -e s/i.86/i386/ -e s/amd64/x86_64/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/)

error() {
	[ ! -e $BUILD_LOG ] || cat $BUILD_LOG
	printf "${RED}[ERROR]${NC} %s\n" "$*"
	exit 1
}

step() {
	printf "${GREEN}[STEP ]${NC} %s\n" "$*"
}

info() {
	printf "[INFO ] %s\n" "$*"
}

warn() {
	printf "${YELLOW}[WARN ]${NC} %s\n" "$*"
}

install_check_deb() {
	step "Install/check dependecies (packages)..."
	info "You might be prompted to input your password as superuser privileges are required."

	info "Updating apt repo list... (running with sudo)"
	sudo apt-get update -y -qq || error "Failed to update package sources. Exiting."
	info "Checking/installing required packages... (running with sudo)"
	sudo apt-get install -y -q $PKGS_DEB >>$BUILD_LOG 2>&1 || error "Failed to install required packages. Exiting."
}

install_check_rpm() {
	step "Install/check dependecies (packages)..."
	info "You might be prompted to input your password as superuser privileges are required."
	info "Updating yum repo list... (running with sudo)"
	sudo yum clean all -yqqq && sudo yum check-update -yqqq >>$BUILD_LOG 2>&1 || error "Failed to update repo list. Exiting."
	info "Checking/installing required packages... (running with sudo)"
	sudo yum install -yq $PKGS_RPM >>$BUILD_LOG 2>&1 || error "Failed to install required packages. Exiting."
}

install_check_arch() {
	error "NOT IMPLEMENTED YET. Feel free to put a pull request on https://github.com/ezQuake/ezquake-source with arch packages"
	step "Install/check dependecies (packages)..."
	info "You might be prompted to input your password as superuser privileges are required."
	sudo pacman -Sy >>$BUILD_LOG 2>&1 || error "Failed to update repository cache. Exiting."
	sudo pacman -S $PKGS_ARCH >>$BUILD_LOG 2>&1 || error "Failed to install required packages. Exiting."
}


if [ -f $BUILD_LOG ];then
	rm -f $BUILD_LOG ||:
fi

[ -e Makefile ] || error "Cannot find 'Makefile', please run this script from the source code directory."
command -v sudo >/dev/null 2>&1 || error "Could not find sudo, please install it. Exiting."


if [ -f /etc/os-release ]; then
	. /etc/os-release || error "Failed to source os-release file"
else
	error "Your environment isn't supported by this script. Exiting."
fi

[ -n "${ID}" ] || error "Your dist does not specify ID in /etc/os-release. Exiting."
[ -n "${VERSION_ID}" ] || VERSION_ID=0
case $ID in
	arch)
		# FIXME: Not implemented yet (package list missing)
		install_check_arch
		;;
	linuxmint)
		[ $VERSION_ID -ge 18 ] || error "Your Linux Mint version '$VERSION_ID' is too old. Exiting."
		install_check_deb
		;;
	ubuntu)
		VERSION_ID=${VERSION_ID%.*}
		[ $VERSION_ID -ge 16 ] || error "Your Ubuntu version '$VERSION_ID' is too old. Exiting."
		install_check_deb
		;;
	debian)
		if [ $VERSION_ID -gt 0 ] && [ $VERSION_ID -lt 8 ]; then
			error "Your Debian version '$VERSION_ID' is too old. Exiting."
		fi
		# Includes Debian testing/unstable as they don't provide a VERSION_ID
		install_check_deb
		;;
	centos|rhel|fedora)
		# FIXME: Versions checks?
		install_check_rpm
		;;
	*)
		error "Your dist '$ID' isn't supported by this script. Exiting."
		;;
esac

step "Cleaning up any previous build files..."
make clean >>$BUILD_LOG 2>&1 || error "Failed to cleanup old build files"

step "Compiling sources (this might take a while, please wait)..."
njobs=2
! command -v nproc >/dev/null 2>&1 || njobs=$(($(nproc) + 1))
make -j$njobs >>$BUILD_LOG 2>&1 || error "Compilation failed. Exiting."

printf "\n${GREEN}Build completed successfully.${NC}\n"
printf "Copy ${YELLOW}ezquake-linux-${CPU}${NC} into your quake directory.\n\n"
