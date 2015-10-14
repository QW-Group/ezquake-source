FROM base/archlinux

MAINTAINER Anders Lindmark <anders.lindmark@gmail.com>

# Install prerequisites
RUN pacman-key --init && pacman-key --refresh-keys
RUN pacman --noconfirm -Syu && pacman-db-upgrade
RUN pacman --noconfirm -S gcc git make pkg-config p7zip sdl2 libpng jansson libjpeg-turbo mingw-w64-gcc

# Add libraries and buildscript
ADD mingw32-libs.tar.gz /usr/i686-w64-mingw32
ADD mk_nightly.sh /

# Target volume for the nightly archive
VOLUME /source
VOLUME /nightly

# Default target platform
ENV TARGET_PLATFORM WINDOWS

CMD /mk_nightly.sh
