#
# This is Cygwin Makefile to create official win32 ezQuake distro package
#
# made by johnnycz
#
# usage: make TARBALL_BRANCH=ezquake_XX
#
# fixme: only "make clean; make" succeeds, attempts to assembly only some parts
# (e.g. archive with source codes) may fail
#

# --- SETTINGS ---

# Path to NSIS make executable
NSISMAKE="C:/Program Files/NSIS/makensis.exe"

# Typically no reason to change this
GIT_ROOT="https://github.com/ezQuake"
TARBALL_BASE_URL="$(GIT_ROOT)/ezquake-source/tarball/"

#
# You can (and typically want to) add this via make parameter
# e.g. make clean; make TARBALL_BRANCH=ezquake_21
# otherwise this default will be used
#
TARBALL_BRANCH=master

#
# Typically you do not want to change this, it is used to download game graphics
# and other files, which do not change across versions.
#
TARBALL_SOURCES="$(TARBALL_BASE_URL)$(TARBALL_BRANCH)"
TARBALL_MEDIA="$(GIT_ROOT)/ezquake-media/tarball/master"
TARBALL_DOCUMENTATION="$(GIT_ROOT)/ezquake-documentation/tarball/master"
TARBALL_LIBS="$(GIT_ROOT)/ezquake-libs/tarball/master"
#
# version - only useful for final file names
# 
VERSION=xxxx

# --- main ---

OPERATING_SYSTEM=$(shell uname -o | tr A-Z a-z)

SOURCE_DIR=tmp/src
SOURCE_FILE=$(SOURCE_DIR)/cl_main.c


# srcarchive has to be made before distfiles, otherwise it would contain also temporary
# files created during compilation
all: check intro init srcarchive distfiles debugarchive mainarchive installer

check:
ifneq (cygwin,$(OPERATING_SYSTEM))
	@echo "Only Cygwin is supported at the moment, sorry"
	@echo "Your system is -$(OPERATING_SYSTEM)-"
	@false
endif

intro:
	@echo "ezQuake distro package creator"
	@echo "usage:"
	@echo " - make TARBALL_BRANCH=branchname"
	@echo "prerequisities:"
	@echo " - go to docs/admin web interface and perform "release commands", "release variables" (only docs admin can do that)"
	@echo
	@echo "Press any key when ready or Ctrl+C to abort."
	@read

init: initdirs initdownload initextract

initdirs:
	mkdir -p tmp
	mkdir -p tmp/dl
	mkdir -p tmp/help
	mkdir -p tmp/help/commands
	mkdir -p tmp/help/variables
	mkdir -p tmp/textures
	mkdir -p dist
	mkdir -p dist/ezquake
	mkdir -p dist/qw
	mkdir -p pub

initdownload: tmp/dl/sources.tar.gz tmp/dl/libs.tar.gz tmp/dl/media.tar.gz tmp/dl/docs.tar.gz

initextract: $(SOURCE_DIR) tmp/media tmp/libs tmp/docs

$(SOURCE_DIR):
	cd tmp/dl ; mkdir -p sources_outer ; tar zxfv sources.tar.gz -C sources_outer ; cd sources_outer ; mv `ls` ../../../$(SOURCE_DIR)
	
tmp/media:
	cd tmp/dl ; mkdir -p media_outer ; tar zxfv media.tar.gz -C media_outer ; cd media_outer ; mv `ls` ../../media
	
tmp/libs:
	cd tmp/dl ; mkdir -p libs_outer ; tar zxfv libs.tar.gz -C libs_outer ; cd libs_outer ; mv `ls` ../../libs

tmp/docs:
	cd tmp/dl ; mkdir -p docs_outer ; tar zxfv docs.tar.gz -C docs_outer ; cd docs_outer ; mv `ls` ../../docs

tmp/dl/sources.tar.gz:
	curl -L -k -o tmp/dl/sources.tar.gz $(TARBALL_SOURCES)

tmp/dl/media.tar.gz:
	curl -L -k -o tmp/dl/media.tar.gz $(TARBALL_MEDIA)

tmp/dl/libs.tar.gz:
	curl -L -k -o tmp/dl/libs.tar.gz $(TARBALL_LIBS)

tmp/dl/docs.tar.gz:
	curl -L -k -o tmp/dl/docs.tar.gz $(TARBALL_DOCUMENTATION)

tmp/commands.tar.gz:
	wget http://ezquake.sourceforge.net/docs/admin/index.php?download=commands -O tmp/commands.tar.gz

tmp/variables.tar.gz:
	wget http://ezquake.sourceforge.net/docs/admin/index.php?download=variables -O tmp/variables.tar.gz

dist/ezquake/help.pk3: tmp/commands.tar.gz tmp/variables.tar.gz
	tar zxfv tmp/commands.tar.gz -C tmp/help/
	tar zxfv tmp/variables.tar.gz -C tmp/help/
	cd tmp ; find help | zip -9 -@ -q ../dist/ezquake/help.pk3

build_dependencies: $(SOURCE_DIR)/libs/windows-x86 $(SOURCE_DIR)/masm32/gas2masm.exe $(SOURCE_DIR)/masm32/ML.EXE

$(SOURCE_DIR)/libs/windows-x86/libjpeg.lib:
	mv tmp/libs/windows-x86/* $(SOURCE_DIR)/libs/windows-x86
	
$(SOURCE_DIR)/masm32/gas2masm.exe: $(SOURCE_DIR)/libs/windows-x86/libjpeg.lib
	mv $(SOURCE_DIR)/libs/windows-x86/gas2masm.exe $(SOURCE_DIR)/masm32

$(SOURCE_DIR)/masm32/ML.EXE: $(SOURCE_DIR)/libs/windows-x86/libjpeg.lib
	mv $(SOURCE_DIR)/libs/windows-x86/ML.EXE $(SOURCE_DIR)/masm32
	chmod 770 $(SOURCE_DIR)/masm32/*

dist/ezquake-gl.exe:
	"$(VS90COMNTOOLS)/../IDE/devenv.exe" $(SOURCE_DIR)/make/msvs2008/ezquake_msvs_90.sln /Out tmp/build.log /Build GLRelease
	mv $(SOURCE_DIR)/make/ezquake-gl.exe ./dist

dist/ezquake.exe:
	"$(VS90COMNTOOLS)/../IDE/devenv.exe" $(SOURCE_DIR)/make/msvs2008/ezquake_msvs_90.sln /Out tmp/build.log /Build Release
	mv $(SOURCE_DIR)/make/ezquake.exe ./dist

dist/ezquake.nsi:
	cp $(SOURCE_DIR)/misc/install/ezquake.nsi dist/

pub/ezquake_source_$(VERSION).zip: build_dependencies
	cd tmp ; find src | zip -9 -@ ../pub/ezquake_source_$(VERSION).zip

srcarchive: pub/ezquake_source_$(VERSION).zip

dist/gnu.txt:
	cp $(SOURCE_DIR)/gnu.txt dist/

dist/qw/fragfile.dat:
	cp $(SOURCE_DIR)/misc/fragfile/fragfile.dat dist/qw/

binaries: dist/ezquake.exe dist/ezquake-gl.exe

distfiles: binaries \
	dist/gnu.txt \
	dist/qw/fragfile.dat \
	dist/qw/skins \
	dist/ezquake/pak.lst \
	dist/ezquake/help.pk3 \
	dist/ezquake/base.pk3 \
	dist/ezquake/progs.pk3 \
	dist/ezquake/keymaps \
	dist/ezquake/sb \
	dist/ezquake/cfg \
	dist/ezquake/help \
	dist/ezquake/manual

debugarchive:
	cd tmp/src/make/ ; find *.pdb | zip -9 -@ ../../../pub/ezquake_debug_win32_$(VERSION).zip

dist/qw/skins:
	rm -rf dist/ezquake/skins
	mv tmp/media/game/skins dist/qw/

dist/ezquake/hud.pk3:
	mv tmp/media/game/textures/wad tmp/textures/wad
	cd tmp ; find textures/wad/ | zip -0 -@ ../dist/ezquake/hud.pk3

dist/ezquake/levelshots.pk3:
	mv tmp/media/game/textures/levelshots tmp/textures/
	cd tmp ; find textures/levelshots/ | zip -0 -@ ../dist/ezquake/levelshots.pk3

dist/ezquake/locs.pk3:
	mv tmp/media/game/locs tmp/locs
	cd tmp/ ; find locs | zip -9 -@ ../dist/ezquake/locs.pk3

dist/ezquake/base.pk3: dist/ezquake/hud.pk3 dist/ezquake/locs.pk3 dist/ezquake/levelshots.pk3
	cd tmp/media/game ; find . | zip -0 -@ ../../../dist/ezquake/base.pk3

dist/ezquake/pak.lst:
	cp $(SOURCE_DIR)/misc/pak.lst dist/ezquake

tmp/qwprogs.dat:
	wget http://zquake.frag.ru/files/qwprogs.dat -O tmp/qwprogs.dat

tmp/spprogs.dat:
	wget http://zquake.frag.ru/files/spprogs.dat -O tmp/spprogs.dat	

dist/ezquake/progs.pk3: tmp/qwprogs.dat tmp/spprogs.dat
	cd tmp/ ; find *progs.dat | zip -0 -@ ../dist/ezquake/progs.pk3

dist/ezquake/cfg:
	rm -rf dist/ezquake/cfg/
	mv $(SOURCE_DIR)/misc/cfg dist/ezquake/cfg/

dist/ezquake/help:
	rm -rf dist/ezquake/help
	mkdir dist/ezquake/help
	mv tmp/docs/index.xml dist/ezquake/help
	mv tmp/docs/manual dist/ezquake/help
	mv tmp/docs/xsd dist/ezquake/help
	mv tmp/docs/xsl dist/ezquake/help

dist/ezquake/keymaps:
	rm -rf dist/ezquake/keymaps
	mv $(SOURCE_DIR)/misc/keymaps dist/ezquake/keymaps

dist/ezquake/manual:
	rm -rf dist/ezquake/manual
	mkdir dist/ezquake/manual
	cd dist/ezquake/manual ; echo "Online manual: http://ezquake.sourceforge.net/docs/" > visit.txt 

dist/ezquake/sb:
	rm -rf dist/ezquake/sb
	mv $(SOURCE_DIR)/misc/sb dist/ezquake/sb

dist/inst_gfx:
	rm -rf dist/inst_gfx
	mv tmp/media/installer dist/inst_gfx

mainarchive:
	cd dist ; find . | zip -9 -@ ../pub/ezquake_win32_$(VERSION).zip 

installer: dist/ezquake.nsi dist/inst_gfx
	cd dist ; $(NSISMAKE) ezquake.nsi
	mv dist/ezquake_win32_xxxx.exe pub/ezquake_win32_$(VERSION).exe

clean:
	rm -rf tmp
	rm -rf dist
	rm -rf pub
