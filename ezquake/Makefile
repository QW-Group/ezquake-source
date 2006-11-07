#======================================================================
# ezQuake Makefile
# based on: Fuhquake Makefile && ZQuake Makefile && JoeQuake Makefile
#======================================================================
#	$Id: Makefile,v 1.52 2006-11-07 16:25:58 disconn3ct Exp $

# compilation tool and detection of targets/achitecture
_E = @
CC = gcc
CC_BASEVERSION = $(shell $(CC) -dumpversion | sed -e 's/\..*//g')

STRIP = $(_E)strip
STRIPFLAGS = --strip-unneeded --remove-section=.comment

# MACHINE = i686-linux-gnu, etc
# ARCH = x86 ppc
# OS = linux darwin
# TODO: fix ARCH and OS for darwin
MACHINE	= $(shell $(CC) -dumpmachine)
ARCH = $(shell echo $(MACHINE) | sed -e 's/i.86/x86/g' -e 's/powerpc/ppc/' -e 's/-gnu//' -e 's/-linux//')
OS = $(shell echo $(MACHINE) | sed -e 's/i.86-//g' -e 's/-gnu//')

# add special architecture based flags
ifeq ($(ARCH),x86)
	DEST_ARCH = x86
	ARCH_CFLAGS = -march=$(shell echo $(MACHINE) | sed -e 's/\-.*//g')
endif
ifeq ($(ARCH),ppc)
	DEST_ARCH = ppc
	ARCH_CFLAGS = -arch ppc -faltivec -maltivec -mcpu=7450 -mtune=7450 -mpowerpc -mpowerpc-gfxopt
endif

# TODO: LIB_PREFIX must be $(OS)-$(ARCH)
ifeq ($(OS),linux)
	DEFAULT_TARGET = glx
	LIB_PREFIX = linux
endif
ifeq ($(OS),darwin)
	DEFAULT_TARGET = mac
	LIB_PREFIX = mac
endif

default_target: $(DEFAULT_TARGET)

all: glx x11 svga

################################
# Directories for object files #
################################

GLX_DIR	:= release-$(ARCH)/glx
X11_DIR	:= release-$(ARCH)/x11
SVGA_DIR := release-$(ARCH)/svga
MAC_DIR	:= release-$(ARCH)/mac

################
# Binary files #
################

GLX_TARGET := release-$(ARCH)/ezquake-gl.glx
X11_TARGET := release-$(ARCH)/ezquake.x11
SVGA_TARGET := release-$(ARCH)/ezquake.svga
MAC_TARGET := release-$(ARCH)/ezquake-gl.mac

################

C_BUILD = $(_E)$(CC) -c -o $@ $(_CFLAGS) $<
S_BUILD = $(_E)$(CC) -c -o $@ $(_CFLAGS) -DELF -x assembler-with-cpp $<
BUILD = $(_E)$(CC) -o $@ $(_OBJS) $(_LDFLAGS)
MKDIR = $(_E)mkdir -p $@

################

$(GLX_DIR) $(X11_DIR) $(SVGA_DIR) $(MAC_DIR):
	$(MKDIR)

# compiler flags
PRJ_CFLAGS = -DWITH_ZLIB -DWITH_PNG -DEMBED_TCL -DJSS_CAM
BASE_CFLAGS = -Wall -funsigned-char $(ARCH_CFLAGS) $(PRJ_CFLAGS) -I ./libs
BASE_RELEASE_CFLAGS = -pipe -O2 -fno-strict-aliasing -ffast-math -fomit-frame-pointer -fexpensive-optimizations
ifeq ($(CC_BASEVERSION),4) # auto vectorize if we're using gcc4.0+
	BASE_RELEASE_CFLAGS +=-ftree-vectorize
else # if we're not auto-vectorizing then we can unroll the loops (mdfour ahoy)
	BASE_RELEASE_CFLAGS +=-funroll-loops
endif

BASE_DEBUG_CFLAGS = -ggdb

ifeq ($(OS),darwin)
	BASE_CFLAGS += -Ddarwin
endif
ifeq ($(ARCH),x86)
	BASE_CFLAGS += -D__LITTLE_ENDIAN__Q__ -Did386
endif
ifeq ($(ARCH),ppc)
	BASE_CFLAGS += -D__BIG_ENDIAN__Q__
endif

RELEASE_CFLAGS = $(BASE_CFLAGS) $(BASE_RELEASE_CFLAGS) -DNDEBUG
DEBUG_CFLAGS = $(BASE_CFLAGS) $(BASE_DEBUG_CFLAGS) -D_DEBUG

# opengl builds
GLCFLAGS=-DGLQUAKE -DWITH_JPEG
ifeq ($(OS),linux)
	GLCFLAGS += -DWITH_VMODE -DWITH_DGA -DWITH_EVDEV
endif
ifeq ($(OS),darwin)
	GLCFLAGS += -I/opt/local/include/ -I/Developer/Headers/FlatCarbon -I/sw/include -FOpenGL -FAGL
endif

CFLAGS += $(RELEASE_CFLAGS)
LDFLAGS := -lm -lpthread

COMMON_LIBS := libs/$(LIB_PREFIX)/libpng.a libs/$(LIB_PREFIX)/libz.a libs/$(LIB_PREFIX)/libpcre.a libs/$(LIB_PREFIX)/libexpat.a libs/$(LIB_PREFIX)/libtcl8.4.a libs/$(LIB_PREFIX)/libjpeg.a

include Makefile.list

#######
# GLX #
#######

GLX_C_OBJS := $(addprefix $(GLX_DIR)/, $(addsuffix .o, $(GLX_C_FILES)))
GLX_S_OBJS := $(addprefix $(GLX_DIR)/, $(addsuffix .o, $(GLX_S_FILES)))
GLX_CFLAGS := $(CFLAGS) $(GLCFLAGS)
GLX_LDFLAGS := $(LDFLAGS) -lGL -lXxf86dga -lXxf86vm

glx: _DIR := $(GLX_DIR)
glx: _OBJS := $(GLX_C_OBJS) $(GLX_S_OBJS) $(COMMON_LIBS)
glx: _LDFLAGS := $(GLX_LDFLAGS)
glx: _CFLAGS := $(GLX_CFLAGS)
glx: $(GLX_TARGET)

$(GLX_TARGET): $(GLX_DIR) $(GLX_C_OBJS) $(GLX_S_OBJS)
	@echo [LINK] $@
	$(BUILD)
	@echo [STRIP] $@
	$(STRIP) $(STRIPFLAGS) $(GLX_TARGET)

$(GLX_C_OBJS): $(GLX_DIR)/%.o: %.c
	@echo [CC] $<
	$(C_BUILD)

$(GLX_S_OBJS): $(GLX_DIR)/%.o: %.s
	@echo [CC] $<
	$(S_BUILD)

#######
# X11 #
#######

X11_C_OBJS := $(addprefix $(X11_DIR)/, $(addsuffix .o, $(X11_C_FILES)))
X11_S_OBJS := $(addprefix $(X11_DIR)/, $(addsuffix .o, $(X11_S_FILES)))
X11_CFLAGS := $(CFLAGS) -D_Soft_X11
X11_LDFLAGS := $(LDFLAGS) -lX11 -lXext

x11: _DIR := $(X11_DIR)
x11: _OBJS := $(X11_C_OBJS) $(X11_S_OBJS) $(COMMON_LIBS)
x11: _LDFLAGS := $(X11_LDFLAGS)
x11: _CFLAGS := $(X11_CFLAGS)
x11: $(X11_TARGET)

$(X11_TARGET): $(X11_DIR) $(X11_C_OBJS) $(X11_S_OBJS)
	@echo [LINK] $@
	$(BUILD)
	@echo [STRIP] $@
	$(STRIP) $(STRIPFLAGS) $(X11_TARGET)

$(X11_C_OBJS): $(X11_DIR)/%.o: %.c
	@echo [CC] $<
	$(C_BUILD)

$(X11_S_OBJS): $(X11_DIR)/%.o: %.s
	@echo [CC] $<
	$(S_BUILD)

########
# SVGA #
########

SVGA_C_OBJS := $(addprefix $(SVGA_DIR)/, $(addsuffix .o, $(SVGA_C_FILES)))
SVGA_S_OBJS := $(addprefix $(SVGA_DIR)/, $(addsuffix .o, $(SVGA_S_FILES)))
SVGA_CFLAGS := $(CFLAGS) -D_Soft_SVGA
SVGA_LDFLAGS := $(LDFLAGS) -lvga -ldl

svga: _DIR := $(SVGA_DIR)
svga: _OBJS := $(SVGA_C_OBJS) $(SVGA_S_OBJS) $(COMMON_LIBS)
svga: _LDFLAGS := $(SVGA_LDFLAGS)
svga: _CFLAGS := $(SVGA_CFLAGS)
svga: $(SVGA_TARGET)

$(SVGA_TARGET): $(SVGA_DIR) $(SVGA_C_OBJS) $(SVGA_S_OBJS)
	@echo [LINK] $@
	$(BUILD)
	@echo [STRIP] $@
	$(STRIP) $(STRIPFLAGS) $(SVGA_TARGET)

$(SVGA_C_OBJS): $(SVGA_DIR)/%.o: %.c
	@echo [CC] $<
	$(C_BUILD)

$(SVGA_S_OBJS): $(SVGA_DIR)/%.o: %.s
	@echo [CC] $<
	$(S_BUILD)

#######
# MAC #
#######

MAC_C_OBJS := $(addprefix $(MAC_DIR)/, $(addsuffix .o, $(MAC_C_FILES)))
MAC_CFLAGS := $(CFLAGS) $(GLCFLAGS)
MAC_LDFLAGS := $(LDFLAGS) -L/sw/lib -framework OpenGL -framework AGL -framework DrawSprocket -framework Carbon -framework ApplicationServices -framework IOKit

mac: _DIR := $(MAC_DIR)
mac: _OBJS := $(MAC_C_OBJS) $(COMMON_LIBS)
mac: _LDFLAGS := $(MAC_LDFLAGS)
mac: _CFLAGS := $(MAC_CFLAGS)
mac: $(MAC_TARGET)

$(MAC_TARGET): $(MAC_DIR) $(MAC_C_OBJS) $(MAC_S_OBJS)
	@echo [LINK] $@
	$(BUILD)
	@echo [STRIP] $@
	$(STRIP) $(STRIPFLAGS) $(MAC_TARGET)

$(MAC_C_OBJS): $(MAC_DIR)/%.o: %.c
	@echo [CC] $<
	$(C_BUILD)

#################
clean:
	@echo [CLEAN]
	@-rm -rf $(GLX_DIR) $(X11_DIR) $(SVGA_DIR) $(MAC_DIR)
