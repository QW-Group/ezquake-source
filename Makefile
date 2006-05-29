#======================================================================
# ezQuake Makefile
# based on: Fuhquake Makefile && ZQuake Makefile && JoeQuake Makefile
#======================================================================


# compilation tool and detection of targets/achitecture
_E			=@
CC			=gcc
CC_BASEVERSION		=$(shell $(CC) -dumpversion | sed -e 's/\..*//g')
MACHINE			=$(shell $(CC) -dumpmachine)
ARCH			=$(shell echo $(MACHINE) | sed -e 's/.*mingw32.*/mingw32/g' -e 's/\-.*//g' -e 's/i.86/x86/g')
#Tonik: OS X make doesn't support the long options
#make them per-architecture if you really want them
#STRIP			=strip --strip-unneeded --remove-section=.comment
STRIP			=strip

# Mac OSX Tiger : powerpc -> ppc
ifeq ($(MACHINE),powerpc-apple-darwin8) # MacOS-10.4/ppc
	ARCH		=ppc
endif

# Mac OSX Tiger : i686 -> macx86
ifeq ($(MACHINE),i686-apple-darwin8) # MacOS-10.4/x86
	ARCH		=macx86
endif

# add special architecture based flags
ifeq ($(ARCH),x86)	# Linux/x86
	DEST_ARCH	=x86
	ARCH_CFLAGS	=-march=$(shell echo $(MACHINE) | sed -e 's/\-.*//g')
endif
ifeq ($(ARCH),mingw32)	# Win32/x86 in MingW environment
	DEST_ARCH	=x86
	ARCH_CFLAGS	=-mwin32 -mno-cygwin
endif
ifeq ($(ARCH),ppc)	# MacOS-X/ppc
	DEST_ARCH	=ppc
	ARCH_CFLAGS	=-arch ppc -faltivec -maltivec -mcpu=7450 -mtune=7450 -mpowerpc -mpowerpc-gfxopt
endif


################

ifeq ($(ARCH),x86)              # Linux/x86
	DEFAULT_TARGET = glx
endif
ifeq ($(ARCH),powerpc)          # Linux/PPC
	DEFAULT_TARGET = glx
endif
ifeq ($(ARCH),mingw32)          # Win32/x86 in MingW environment
	DEFAULT_TARGET = glx	#FIXME
endif
ifeq ($(ARCH),ppc)              # MacOS-X/ppc
	DEFAULT_TARGET = mac
endif
ifeq ($(ARCH),macx86)           # MacOS-X/x86
	DEFAULT_TARGET = mac
endif

default_target: $(DEFAULT_TARGET)

all: glx x11 svga

################################
# Directories for object files #
################################

GLX_DIR		:= release-$(ARCH)/glx
X11_DIR		:= release-$(ARCH)/x11
SVGA_DIR	:= release-$(ARCH)/svga
MAC_DIR		:= release-$(ARCH)/mac

################
# Binary files #
################

GLX_TARGET	:= release-$(ARCH)/ezquake-gl.glx
X11_TARGET	:= release-$(ARCH)/ezquake.x11
SVGA_TARGET	:= release-$(ARCH)/ezquake.svga
MAC_TARGET	:= release-$(ARCH)/ezquake-gl.mac

################

C_BUILD = $(_E)$(CC) -c -o $@ $(_CFLAGS) $<
S_BUILD = $(_E)$(CC) -c -o $@ $(_CFLAGS) -DELF -x assembler-with-cpp $<
BUILD = $(_E)$(CC) -o $@ $(_OBJS) $(_LDFLAGS)
MKDIR = $(_E)mkdir -p $@
RM_FILES = $(_E)rm -f $(_FILES)
RM_DIRS = $(_E)rm -fr $(_DIRS)

################

$(GLX_DIR) $(X11_DIR) $(SVGA_DIR) $(MAC_DIR):
	$(MKDIR)

# compiler flags
PRJ_CFLAGS =-DWITH_ZLIB -DWITH_PNG -DEMBED_TCL -DJSS_CAM
XMMS_CFLAGS =-DWITH_XMMS `glib-config --cflags`
BASE_CFLAGS =-Wall -Wsign-compare $(PRJ_CFLAGS) $(ARCH_CFLAGS) -funsigned-char
BASE_RELEASE_CFLAGS = -pipe -O2 -fno-strict-aliasing -ffast-math -fomit-frame-pointer -fexpensive-optimizations
ifeq ($(CC_BASEVERSION),4) # auto vectorize if we're using gcc4.0+
	BASE_RELEASE_CFLAGS +=-ftree-vectorize
else # if we're not auto-vectorizing then we can unroll the loops (mdfour ahoy)
	BASE_RELEASE_CFLAGS +=-funroll-loops
endif

BASE_DEBUG_CFLAGS               =-ggdb

ifeq ($(ARCH),x86)              # Linux/x86
	BASE_CFLAGS             +=-Did386 $(XMMS_CFLAGS)
endif
ifeq ($(ARCH),mingw32)          # Win32/x86 in MingW environment
	# use define for special assembly routines:
	BASE_CFLAGS             +=-Did386 -DMINGW32
endif
ifeq ($(ARCH),ppc)              # MacOS-X/ppc
	BASE_CFLAGS             +=-D__BIG_ENDIAN__ -Ddarwin
endif
ifeq ($(ARCH),macx86)           # MacOS-X/x86
	BASE_CFLAGS             +=-Ddarwin
endif
ifeq ($(ARCH),powerpc)          # Linux/PPC
	BASE_CFLAGS             +=-D__BIG_ENDIAN__
endif

RELEASE_CFLAGS                  =$(BASE_CFLAGS) $(BASE_RELEASE_CFLAGS) -DNDEBUG
DEBUG_CFLAGS                    =$(BASE_CFLAGS) $(BASE_DEBUG_CFLAGS) -D_DEBUG

# opengl builds
ifeq ($(ARCH),x86)              # Linux/x86
	ARCH_GLCFLAGS           =-I/usr/include -DWITH_VMODE -DWITH_DGA -DWITH_EVDEV
endif
ifeq ($(ARCH),mingw32)          # Win32/x86 in MingW environment
	ARCH_GLCFLAGS           =-mwindows -I/opt/xmingw/include/
endif
ifeq ($(ARCH),ppc)              # MacOS-X/ppc
	ARCH_GLCFLAGS           =-I/opt/local/include/ -I/Developer/Headers/FlatCarbon -I/sw/include -FOpenGL -FAGL
endif
ifeq ($(ARCH),macx86)           # MacOS-X/x86
	ARCH_GLCFLAGS           =-I/opt/local/include/ -I/Developer/Headers/FlatCarbon -I/sw/include -FOpenGL -FAGL
endif

GLCFLAGS=$(ARCH_GLCFLAGS) $(BASE_GLCFLAGS)

CFLAGS += $(RELEASE_CFLAGS)
LDFLAGS := -lm -ldl -lpthread

COMMON_OBJS := libs/libpng.a libs/zlib.a libs/libpcre.a libs/libexpat.a libs/libtcl8.4.a libs/libglib.a libs/libjpeg.a

include Makefile.list

#######
# GLX #
#######

GLX_C_OBJS := $(addprefix $(GLX_DIR)/, $(addsuffix .o, $(GLX_C_FILES)))
GLX_S_OBJS := $(addprefix $(GLX_DIR)/, $(addsuffix .o, $(GLX_S_FILES)))

GLX_CFLAGS := $(CFLAGS) $(GLCFLAGS) -DGLQUAKE -DWITH_JPEG -I/usr/X11R6/include

GLX_LDFLAGS := $(LDFLAGS) -lGL -L/usr/X11R6/lib -lX11 -lXext # -lXxf86dga -lXxf86vm

glx: _DIR := $(GLX_DIR)
glx: _OBJS := $(GLX_C_OBJS) $(GLX_S_OBJS) $(COMMON_OBJS) /usr/lib/libXxf86vm.a /usr/lib/libXxf86dga.a
glx: _LDFLAGS := $(GLX_LDFLAGS)
glx: _CFLAGS := $(GLX_CFLAGS)
glx: $(GLX_TARGET)

$(GLX_TARGET): $(GLX_DIR) $(GLX_C_OBJS) $(GLX_S_OBJS)
	$(BUILD)
	$(STRIP) $(GLX_TARGET)

$(GLX_C_OBJS): $(GLX_DIR)/%.o: %.c
	$(C_BUILD)

$(GLX_S_OBJS): $(GLX_DIR)/%.o: %.s
	$(S_BUILD)

glxclean: _FILES += $(GLX_C_OBJS) $(GLX_S_OBJS)
glxclean: _DIRS := $(GLX_DIR)
glxclean:
	$(RM_FILES)
	$(RM_DIRS)

glxclobber: _FILES := $(GLX_TARGET)
glxclobber: glxclean

#######
# X11 #
#######

X11_C_OBJS := $(addprefix $(X11_DIR)/, $(addsuffix .o, $(X11_C_FILES)))
X11_S_OBJS := $(addprefix $(X11_DIR)/, $(addsuffix .o, $(X11_S_FILES)))

X11_CFLAGS := $(CFLAGS) -D_Soft_X11

X11_LDFLAGS := $(LDFLAGS) -L/usr/X11R6/lib -lX11 -lXext 

x11: _DIR := $(X11_DIR)
x11: _OBJS := $(X11_C_OBJS) $(X11_S_OBJS) $(COMMON_OBJS)
x11: _LDFLAGS := $(X11_LDFLAGS)
x11: _CFLAGS := $(X11_CFLAGS)
x11: $(X11_TARGET)

$(X11_TARGET): $(X11_DIR) $(X11_C_OBJS) $(X11_S_OBJS)
	$(BUILD)
	$(STRIP) $(X11_TARGET)

$(X11_C_OBJS): $(X11_DIR)/%.o: %.c quakedef.h
	$(C_BUILD)

$(X11_S_OBJS): $(X11_DIR)/%.o: %.s
	$(S_BUILD)

x11clean: _FILES += $(X11_C_OBJS) $(X11_S_OBJS)
x11clean: _DIRS := $(X11_DIR)
x11clean:
	$(RM_FILES)
	$(RM_DIRS)

x11clobber: _FILES := $(X11_TARGET)
x11clobber: x11clean

########
# SVGA #
########

SVGA_C_OBJS := $(addprefix $(SVGA_DIR)/, $(addsuffix .o, $(SVGA_C_FILES)))
SVGA_S_OBJS := $(addprefix $(SVGA_DIR)/, $(addsuffix .o, $(SVGA_S_FILES)))

SVGA_CFLAGS := $(CFLAGS) -D_Soft_SVGA

SVGA_LDFLAGS := $(LDFLAGS) -lvga

svga: _DIR := $(SVGA_DIR)
svga: _OBJS := $(SVGA_C_OBJS) $(SVGA_S_OBJS) $(COMMON_OBJS)
svga: _LDFLAGS := $(SVGA_LDFLAGS)
svga: _CFLAGS := $(SVGA_CFLAGS)
svga: $(SVGA_TARGET)

$(SVGA_TARGET): $(SVGA_DIR) $(SVGA_C_OBJS) $(SVGA_S_OBJS)
	$(BUILD)
	$(STRIP) $(SVGA_TARGET)

$(SVGA_C_OBJS): $(SVGA_DIR)/%.o: %.c quakedef.h
	$(C_BUILD)

$(SVGA_S_OBJS): $(SVGA_DIR)/%.o: %.s
	$(S_BUILD)

svgaclean: _FILES += $(SVGA_C_OBJS) $(SVGA_S_OBJS)
svgaclean: _DIRS := $(SVGA_DIR)
svgaclean:
	$(RM_FILES)
	$(RM_DIRS)

svgaclobber: _FILES := $(SVGA_TARGET)
svgaclobber: svgaclean

#######
# MAC #
#######

MAC_C_OBJS := $(addprefix $(MAC_DIR)/, $(addsuffix .o, $(MAC_C_FILES)))

MAC_CFLAGS := $(CFLAGS) $(GLCFLAGS) -DGLQUAKE -DWITH_JPEG

MAC_LDFLAGS := $(LDFLAGS) -L/sw/lib -lexpat -lpcre -ltcl -ljpeg -lpng -framework OpenGL -framework AGL -framework DrawSprocket -framework Carbon -framework ApplicationServices -framework IOKit

mac: _DIR := $(MAC_DIR)
mac: _OBJS := $(MAC_C_OBJS)
mac: _LDFLAGS := $(MAC_LDFLAGS)
mac: _CFLAGS := $(MAC_CFLAGS)
mac: $(MAC_TARGET)

$(MAC_TARGET): $(MAC_DIR) $(MAC_C_OBJS) $(MAC_S_OBJS)
	$(BUILD)
	$(STRIP) $(MAC_TARGET)

$(MAC_C_OBJS): $(MAC_DIR)/%.o: %.c quakedef.h
	$(C_BUILD)

macclean: _FILES += $(MAC_C_OBJS)
macclean: _DIRS := $(MAC_DIR)
macclean:
	$(RM_FILES)
	$(RM_DIRS)

macclobber: _FILES := $(MAC_TARGET)
macclobber: macclean

#################
clean: glxclean x11clean svgaclean macclean

clobber: _DIRS := $(GLX_DIR) $(X11_DIR) $(SVGA_DIR) $(MAC_DIR)
clobber:
	$(RM_DIRS)
