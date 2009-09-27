#
# This is Windows+Cygwin Makefile to create official ezQuake distro package
#
# made by johnnycz
#
# usage: make SVN_EXPORT_URL=https://ezquake.svn.sourceforge.net/svnroot/ezquake/branches/...
#
# fixme: only "make clean; make" succeeds, attempts to assembly only some parts
# (e.g. archive with source codes) typically fail, in overall this should have been made just
# as a shell script, makefile is not suitable (or I just lack appropriate skills)
# typically svn export creates files with old dates, which probably makes
# the make algorithm useless, also the way dirs are created/checked is wrong
#

# --- SETTINGS ---

# Typically no reason to change this
SVN_BASE_URL="https://ezquake.svn.sourceforge.net/svnroot/ezquake"

#
# You can (and typically want to) add this via make parameter
# e.g. make clean; make SVN_EXPORT_URL=https://ezquake.svn.sourceforge.net/svnroot/ezquake/branches/ezquake_20
# otherwise this default will be used
#
SVN_EXPORT_URL="$(SVN_BASE_URL)/trunk/ezquake"

#
# Typically you do not want to change this, it is used to download game graphics
# and other files, which do not change across versions.
#
SVN_MEDIA_URL="$(SVN_BASE_URL)/trunk/media"
SVN_DOCUMENTATION_URL="$(SVN_BASE_URL)/trunk/documentation"
SVN_LIBS_URL="$(SVN_BASE_URL)/trunk/libs"
#
# version - only useful for final file names
# 
VERSION=xxxx

# --- main ---

OPERATING_SYSTEM=$(shell uname -o | tr A-Z a-z)

DEPENDENCY_FILE1=masm32/gas2masm.exe
SOURCE_DIR=tmp/src
SOURCE_FILE=$(SOURCE_DIR)/cl_main.c
NSISMAKE="C:/Program Files/NSIS/makensis.exe"


# srcarchive has to be made before distfiles, otherwise it would contain also temporary
# files created during compilation
all: check intro srcarchive distfiles debugarchive mainarchive installer

check:
ifneq (cygwin,$(OPERATING_SYSTEM))
	@echo "Only Cygwin is supported at the moment, sorry"
	@echo "Your system is -$(OPERATING_SYSTEM)-"
	@false
endif

intro:
	@echo "ezQuake distro package creator"
	@echo "usage:"
	@echo " - make SVN_EXPORT_URL=https://ezquake.svn.sourceforge.net/svnroot/ezquake/branches/..."
	@echo "prerequisities:"
	@echo " - go to docs/admin web interface and perform "release commands", "release variables" (only docs admin can do that)"
	@echo
	@echo "Press any key when ready or Ctrl+C to abort."
	@read

tmp: ; mkdir -p tmp

tmp/help: ; mkdir -p tmp/help

tmp/help/commands: ; mkdir -p tmp/help/commands

tmp/help/variables: ; mkdir -p tmp/help/variables

tmp/commands.zip: tmp
	wget http://ezquake.sourceforge.net/docs/admin/index.php?download=commands -O tmp/commands.zip

tmp/variables.zip: tmp
	wget http://ezquake.sourceforge.net/docs/admin/index.php?download=variables -O tmp/variables.zip

dist: ; mkdir -p dist

dist/ezquake: dist ; mkdir -p dist/ezquake

dist/ezquake/help.pk3: dist/ezquake tmp/help/commands tmp/help/variables tmp/variables.zip tmp/commands.zip
	unzip -d tmp/help/commands/ tmp/commands.zip
	unzip -d tmp/help/variables/ tmp/variables.zip
	cd tmp ; find help | zip -9 -@ -q ../dist/ezquake/help.pk3

$(SOURCE_FILE): tmp
	svn export $(SVN_EXPORT_URL) $(SOURCE_DIR)

$(DEPENDENCY_FILE1): $(SOURCE_FILE)
	svn export $(SVN_LIBS_URL)/windows-x86/ tmp/libs
	mv tmp/libs/* $(SOURCE_DIR)/libs/windows-x86
	mv $(SOURCE_DIR)/libs/windows-x86/gas2masm.exe $(SOURCE_DIR)/masm32
	mv $(SOURCE_DIR)/libs/windows-x86/ML.EXE $(SOURCE_DIR)/masm32
	chmod 770 $(SOURCE_DIR)/masm32/*

dist/ezquake-gl.exe: dist
	"$(VS90COMNTOOLS)/../IDE/devenv.exe" $(SOURCE_DIR)/make/msvs2008/ezquake_msvs_90.sln /Out tmp/build.log /Build GLRelease
	mv $(SOURCE_DIR)/make/ezquake-gl.exe ./dist

dist/ezquake.exe: dist
	"$(VS90COMNTOOLS)/../IDE/devenv.exe" $(SOURCE_DIR)/make/msvs2008/ezquake_msvs_90.sln /Out tmp/build.log /Build Release
	mv $(SOURCE_DIR)/make/ezquake.exe ./dist

dist/ezquake.nsi: dist
	cd dist ; svn export $(SVN_EXPORT_URL)/misc/install/ezquake.nsi

pub: ; mkdir -p pub

pub/ezquake_source_$(VERSION).zip: pub $(SOURCE_FILE) $(DEPENDENCY_FILE1)
	cd tmp ; find src | zip -9 -@ ../pub/ezquake_source_$(VERSION).zip

srcarchive: pub/ezquake_source_$(VERSION).zip

dist/gnu.txt:
	cd dist ; svn export $(SVN_EXPORT_URL)/gnu.txt

dist/qw: ; mkdir -p dist/qw

dist/qw/fragfile.dat: dist/qw
	cd dist/qw/ ; svn export $(SVN_EXPORT_URL)/misc/fragfile/fragfile.dat

binaries: dist/ezquake.exe dist/ezquake-gl.exe

distfiles: dist/ezquake \
	binaries \
	dist/gnu.txt \
	dist/qw/fragfile.dat \
	dist/ezquake \
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

tmp/media: tmp ; mkdir -p tmp/media
tmp/media/game: tmp/media ; mkdir -p tmp/media/game

gamemedia: tmp/media
	cd tmp/media ; rm -rf game ; svn export $(SVN_MEDIA_URL)/game

dist/qw/skins: gamemedia dist/qw
	rm -rf dist/ezquake/skins
	mv tmp/media/game/skins dist/qw/

tmp/textures: tmp ; mkdir -p tmp/textures

dist/ezquake/hud.pk3: tmp/textures
	mv tmp/media/game/textures/wad tmp/textures/wad
	cd tmp ; find textures/wad/ | zip -0 -@ ../dist/ezquake/hud.pk3

dist/ezquake/levelshots.pk3: tmp/textures
	mv tmp/media/game/textures/levelshots tmp/textures/
	cd tmp ; find textures/levelshots/ | zip -0 -@ ../dist/ezquake/levelshots.pk3

dist/ezquake/locs.pk3:
	mv tmp/media/game/locs tmp/locs
	cd tmp/ ; find locs | zip -9 -@ ../dist/ezquake/locs.pk3

dist/ezquake/base.pk3: dist/qw/skins dist/ezquake/hud.pk3 dist/ezquake/locs.pk3 dist/ezquake/levelshots.pk3
	cd tmp/media/game ; find . | zip -0 -@ ../../../dist/ezquake/base.pk3

dist/ezquake/pak.lst: dist/ezquake
	cd dist/ezquake ; svn export $(SVN_EXPORT_URL)/misc/pak.lst

tmp/qwprogs.dat:
	wget http://zquake.frag.ru/files/qwprogs.dat -O tmp/qwprogs.dat

tmp/spprogs.dat:
	wget http://zquake.frag.ru/files/spprogs.dat -O tmp/spprogs.dat	

dist/ezquake/progs.pk3: tmp/qwprogs.dat tmp/spprogs.dat
	cd tmp/ ; find *progs.dat | zip -0 -@ ../dist/ezquake/progs.pk3

dist/ezquake/cfg: dist/ezquake
	rm -rf dist/ezquake/cfg/ ; svn export $(SVN_EXPORT_URL)/misc/cfg dist/ezquake/cfg

dist/ezquake/help: dist/ezquake
	rm -rf dist/ezquake/help
	mkdir dist/ezquake/help
	cd dist/ezquake/help ; svn export $(SVN_DOCUMENTATION_URL)/index.xml
	cd dist/ezquake/help ; svn export $(SVN_DOCUMENTATION_URL)/manual
	cd dist/ezquake/help ; svn export $(SVN_DOCUMENTATION_URL)/xsd
	cd dist/ezquake/help ; svn export $(SVN_DOCUMENTATION_URL)/xsl

dist/ezquake/keymaps: dist/ezquake
	rm -rf dist/ezquake/keymaps
	svn export $(SVN_EXPORT_URL)/misc/keymaps dist/ezquake/keymaps

dist/ezquake/manual: dist/ezquake
	rm -rf dist/ezquake/manual
	mkdir dist/ezquake/manual
	cd dist/ezquake/manual ; echo "Online manual: http://ezquake.sourceforge.net/docs/" > visit.txt 

dist/ezquake/sb: dist/ezquake
	rm -rf dist/ezquake/sb
	cd dist/ezquake ; svn export $(SVN_EXPORT_URL)/misc/sb sb

dist/inst_gfx: dist
	rm -rf dist/inst_gfx
	cd dist ; svn export $(SVN_MEDIA_URL)/installer inst_gfx

mainarchive:
	cd dist ; find . | zip -9 -@ ../pub/ezquake_win32_$(VERSION).zip 

installer: dist/inst_gfx dist/ezquake.nsi
	cd dist ; $(NSISMAKE) ezquake.nsi
	mv dist/ezquake_win32_xxxx.exe pub/ezquake_win32_$(VERSION).exe

clean:
	rm -rf tmp
	rm -rf dist
	rm -rf pub
