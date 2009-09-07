#======================================================================
# ezQuake Makefile
# based on: Fuhquake Makefile && ZQuake Makefile && JoeQuake Makefile
#======================================================================
#	$Id: Makefile,v 1.83 2007-10-26 07:55:44 dkure Exp $

# compilation tool and detection of targets/achitecture
_E = @
CC = gcc
CC_BASEVERSION = $(shell $(CC) -dumpversion | sed -e 's/\..*//g')

# TYPE = release debug
TYPE=release
STRIP = $(_E)strip
STRIPFLAGS = --strip-unneeded --remove-section=.comment

# ARCH = x86 ppc
# OS = linux darwin freebsd
ARCH = $(shell uname -m | sed -e 's/i.86/x86/g' -e 's/Power Macintosh/ppc/g' -e 's/amd64/x86_64/g')
OS = $(shell uname -s | tr A-Z a-z)

# add special architecture based flags
ifeq ($(ARCH),x86_64)
	ARCH_CFLAGS = -march=nocona -m64
endif
ifeq ($(ARCH),x86)
	ARCH_CFLAGS = -march=i686 -mtune=generic -mmmx -Did386
endif
ifeq ($(ARCH),ppc)
	ARCH_CFLAGS = -arch ppc -faltivec -maltivec -mcpu=7450 -mtune=7450 -mpowerpc -mpowerpc-gfxopt
endif

ifeq ($(OS),linux)
	DEFAULT_TARGET = glx
	OS_GL_CFLAGS = -DWITH_DGA -DWITH_EVDEV -DWITH_VMODE -DWITH_JOYSTICK
endif
ifeq ($(OS),darwin)
	ARCH_CFLAGS = -arch i686 -arch ppc -msse2
	STRIPFLAGS =
	DEFAULT_TARGET = mac
	OS_GL_CFLAGS = -I/opt/local/include/ -I/Developer/Headers/FlatCarbon -FOpenGL -FAGL
endif
ifeq ($(OS),freebsd)
	DEFAULT_TARGET = glx
	OS_GL_CFLAGS = -DWITH_DGA -DWITH_VMODE
endif

LIB_PREFIX=$(OS)-$(ARCH)

default_target: $(DEFAULT_TARGET)

all: glx x11 svga

################################
# Directories for object files #
################################

GLX_DIR	= $(TYPE)-$(ARCH)/glx
X11_DIR	= $(TYPE)-$(ARCH)/x11
SVGA_DIR = $(TYPE)-$(ARCH)/svga
MAC_DIR	= $(TYPE)-$(ARCH)/mac

################
# Binary files #
################

GLX_TARGET = $(TYPE)-$(ARCH)/ezquake-gl.glx
X11_TARGET = $(TYPE)-$(ARCH)/ezquake.x11
SVGA_TARGET = $(TYPE)-$(ARCH)/ezquake.svga
MAC_TARGET = $(TYPE)-$(ARCH)/ezquake-gl.mac
QUAKE_DIR="/opt/quake/"

################

C_BUILD = $(_E)$(CC) -c -o $@ $(_CFLAGS) $<
S_BUILD = $(_E)$(CC) -c -o $@ $(_CFLAGS) -DELF -x assembler-with-cpp $<
BUILD = $(_E)$(CC) -o $@ $(_OBJS) $(_LDFLAGS)
MKDIR = $(_E)mkdir -p $@

################

$(GLX_DIR) $(X11_DIR) $(SVGA_DIR) $(MAC_DIR):
	$(MKDIR)

# compiler flags
# -DWITH_XMMS      for xmms      MP3 player support
# -DWITH_AUDACIOUS for audacious MP3 player support
# -DWITH_XMMS2     for xmms2     MP3 player support
# -DWITH_MPD       for mpd       MP3 player support
# -DWITH_WINAMP    for winamp    MP3 player support
PRJ_CFLAGS = -DWITH_ZLIB -DWITH_PNG -DJSS_CAM -DWITH_ZIP -DWITH_FTE_VFS -DUSE_PR2 -DWITH_IRC -DWITH_TCL -DWITH_NQPROGS
BASE_CFLAGS = -pipe -Wall -funsigned-char $(ARCH_CFLAGS) $(PRJ_CFLAGS) -I./libs -I./libs/libircclient


########################
# pkg-config includes  #
########################
# Add support for MP3 Libraries
ifneq ($(shell echo $(PRJ_CFLAGS) | grep WITH_XMMS),)
BASE_CFLAGS += `pkg-config --libs-only-L --libs --cflags glib-2.0`
endif # WITH_XMMS
ifneq ($(shell echo $(PRJ_CFLAGS) |grep WITH_AUDACIOUS),)
BASE_CFLAGS += `pkg-config --libs-only-L --libs --cflags glib-2.0 dbus-glib-1`
endif # WITH_AUDACIOUS
ifneq ($(shell echo $(PRJ_CFLAGS) | grep WITH_XMMS2),)
BASE_CFLAGS += `pkg-config --libs-only-L --libs --cflags xmms2-client`
endif # WITH_XMMS2

ifneq ($(shell echo $(PRJ_CFLAGS) | grep WITH_OGG_VORBIS),)
BASE_CFLAGS += `pkg-config --libs-only-L --libs --cflags vorbisfile`
endif # WITH_OGG_VORBIS

########################

RELEASE_CFLAGS = -O2 -fno-strict-aliasing -ffast-math -funroll-loops
DEBUG_CFLAGS = -ggdb

# opengl builds
GLCFLAGS=-DGLQUAKE -DWITH_JPEG $(OS_GL_CFLAGS)

ifeq ($(TYPE),release)
CFLAGS = $(BASE_CFLAGS) $(RELEASE_CFLAGS) -DNDEBUG
else
CFLAGS = $(BASE_CFLAGS) $(DEBUG_CFLAGS) -D_DEBUG
endif

ifeq ($(TYPE),release)
LDFLAGS = -lm -lpthread -lrt
else
LDFLAGS = -ggdb -lm -lpthread -lrt
endif

COMMON_LIBS = libs/$(LIB_PREFIX)/minizip.a libs/$(LIB_PREFIX)/libpng.a libs/$(LIB_PREFIX)/libz.a libs/$(LIB_PREFIX)/libpcre.a libs/$(LIB_PREFIX)/libexpat.a libs/$(LIB_PREFIX)/libtcl.a libs/$(LIB_PREFIX)/libircclient.a
GL_LIBS = libs/$(LIB_PREFIX)/libjpeg.a

ifeq ($(OS),freebsd)
LOCALBASE ?= /usr/local
CFLAGS += -I$(LOCALBASE)/include
LDFLAGS += -L$(LOCALBASE)/lib
endif

include Makefile.list

#######
# GLX #
#######

GLX_C_OBJS = $(addprefix $(GLX_DIR)/, $(addsuffix .o, $(GLX_C_FILES)))
GLX_S_OBJS = $(addprefix $(GLX_DIR)/, $(addsuffix .o, $(GLX_S_FILES)))
GLX_CFLAGS = $(CFLAGS) $(GLCFLAGS)
GLX_LDFLAGS = $(LDFLAGS) -lGL -lXxf86dga -lXxf86vm

glx: _DIR = $(GLX_DIR)
glx: _OBJS = $(GLX_C_OBJS) $(GLX_S_OBJS) $(COMMON_LIBS) $(GL_LIBS)
glx: _LDFLAGS = $(GLX_LDFLAGS)
glx: _CFLAGS = $(GLX_CFLAGS)
glx: $(GLX_TARGET)

$(GLX_TARGET): $(GLX_DIR) $(GLX_C_OBJS) $(GLX_S_OBJS)
	@echo [LINK] $@
	$(BUILD)
ifeq ($(TYPE),release)
	@echo [STRIP] $@
	$(STRIP) $(STRIPFLAGS) $(GLX_TARGET)
endif

$(GLX_C_OBJS): $(GLX_DIR)/%.o: %.c
	@echo [CC] $<
	$(C_BUILD)

$(GLX_S_OBJS): $(GLX_DIR)/%.o: %.s
	@echo [CC] $<
	$(S_BUILD)

#######
# X11 #
#######

X11_C_OBJS = $(addprefix $(X11_DIR)/, $(addsuffix .o, $(X11_C_FILES)))
X11_S_OBJS = $(addprefix $(X11_DIR)/, $(addsuffix .o, $(X11_S_FILES)))
X11_CFLAGS = $(CFLAGS) -D_Soft_X11
X11_LDFLAGS = $(LDFLAGS) -lX11 -lXext

x11: _DIR = $(X11_DIR)
x11: _OBJS = $(X11_C_OBJS) $(X11_S_OBJS) $(COMMON_LIBS)
x11: _LDFLAGS = $(X11_LDFLAGS)
x11: _CFLAGS = $(X11_CFLAGS)
x11: $(X11_TARGET)

$(X11_TARGET): $(X11_DIR) $(X11_C_OBJS) $(X11_S_OBJS)
	@echo [LINK] $@
	$(BUILD)
ifeq ($(TYPE),release)
	@echo [STRIP] $@
	$(STRIP) $(STRIPFLAGS) $(X11_TARGET)
endif

$(X11_C_OBJS): $(X11_DIR)/%.o: %.c
	@echo [CC] $<
	$(C_BUILD)

$(X11_S_OBJS): $(X11_DIR)/%.o: %.s
	@echo [CC] $<
	$(S_BUILD)

########
# SVGA #
########

SVGA_C_OBJS = $(addprefix $(SVGA_DIR)/, $(addsuffix .o, $(SVGA_C_FILES)))
SVGA_S_OBJS = $(addprefix $(SVGA_DIR)/, $(addsuffix .o, $(SVGA_S_FILES)))
SVGA_CFLAGS = $(CFLAGS) -D_Soft_SVGA
SVGA_LDFLAGS = $(LDFLAGS) -lvga
ifeq ($(OS),linux)
# FreeBSD don't need -ldl, but it need /usr/ports/graphics/svgalib to be installed:
# > cd /usr/ports/graphics/svgalib
# > make install clean
# for compiling and for running.
SVGA_LDFLAGS += -ldl
endif

svga: _DIR = $(SVGA_DIR)
svga: _OBJS = $(SVGA_C_OBJS) $(SVGA_S_OBJS) $(COMMON_LIBS)
svga: _LDFLAGS = $(SVGA_LDFLAGS)
svga: _CFLAGS = $(SVGA_CFLAGS)
svga: $(SVGA_TARGET)

$(SVGA_TARGET): $(SVGA_DIR) $(SVGA_C_OBJS) $(SVGA_S_OBJS)
	@echo [LINK] $@
	$(BUILD)
ifeq ($(TYPE),release)
	@echo [STRIP] $@
	$(STRIP) $(STRIPFLAGS) $(SVGA_TARGET)
endif

$(SVGA_C_OBJS): $(SVGA_DIR)/%.o: %.c
	@echo [CC] $<
	$(C_BUILD)

$(SVGA_S_OBJS): $(SVGA_DIR)/%.o: %.s
	@echo [CC] $<
	$(S_BUILD)

#######
# MAC #
#######

MAC_C_OBJS = $(addprefix $(MAC_DIR)/, $(addsuffix .o, $(MAC_C_FILES)))
MAC_CFLAGS = $(CFLAGS) $(GLCFLAGS)
MAC_LDFLAGS = $(LDFLAGS) -arch i686 -arch ppc -isysroot /Developer/SDKs/MacOSX10.5.sdk -framework OpenGL -framework AGL -framework DrawSprocket -framework Carbon -framework ApplicationServices -framework IOKit

mac: _DIR = $(MAC_DIR)
mac: _OBJS = $(MAC_C_OBJS) $(COMMON_LIBS) $(GL_LIBS)
mac: _LDFLAGS = $(MAC_LDFLAGS)
mac: _CFLAGS = $(MAC_CFLAGS)
mac: $(MAC_TARGET)

$(MAC_TARGET): $(MAC_DIR) $(MAC_C_OBJS) $(MAC_S_OBJS)
	@echo [LINK] $@
	$(BUILD)
ifeq ($(TYPE),release)
	@echo [STRIP] $@
	$(STRIP) $(STRIPFLAGS) $(MAC_TARGET)
endif

$(MAC_C_OBJS): $(MAC_DIR)/%.o: %.c
	@echo [CC] $<
	$(C_BUILD)

#################
clean:
	@echo [CLEAN]
	@-rm -rf $(GLX_DIR) $(X11_DIR) $(SVGA_DIR) $(MAC_DIR)

help:
	@echo "all     - make all the targets possible"
	@echo "install - Installs all made clients to /opt/quake"
	@echo "clean   - removes all output"
	@echo "glx     - GLX GL client"
	@echo "x11     - X11 software client"
	@echo "svga    - SVGA software client"
	@echo "mac     - Mac client"


install:
	@echo [CP] $(GLX_TARGET) 	$(QUAKE_DIR)
	@cp $(GLX_TARGET) 			$(QUAKE_DIR)
#	@echo [CP] $(X11_TARGET) 	$(QUAKE_DIR)
#	@cp $(X11_TARGET)			$(QUAKE_DIR)
#	@echo [CP] $(SVGA_TARGET) 	$(QUAKE_DIR)
#	@cp $(SVGA_TARGET)			$(QUAKE_DIR)
#	@echo [CP] $(MAC_TARGET) 	$(QUAKE_DIR)
#	@cp $(MAC_TARGET)			$(QUAKE_DIR)
