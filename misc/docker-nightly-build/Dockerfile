FROM	base/archlinux

MAINTAINER	Anders Lindmark <anders.lindmark@gmail.com>

# Install prerequisites
RUN		pacman-key --init && pacman-key --refresh-keys
RUN		pacman --noconfirm -Syu && pacman-db-upgrade
RUN		pacman --noconfirm -S gcc git make pkg-config p7zip sdl2 libpng jansson mingw-w64-gcc

RUN		git clone https://github.com/ezQuake/ezquake-source.git

ADD		mingw32-libs.tar.gz /usr/i686-w64-mingw32
ADD		mk_nightly.sh /

# Target volume for the nightly archive
VOLUME	/nightly

CMD		/mk_nightly.sh
