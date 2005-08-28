#======================================================================
# ezQuake Makefile for Linux >= 2.0
# based on: Fuhquake Makefile && ZQuake Makefile
#======================================================================


# compilation tool and detection of targets/achitecture
CC			=gcc
CC_BASEVERSION		=$(shell $(CC) -dumpversion | sed -e 's/\..*//g')
MACHINE			=$(shell $(CC) -dumpmachine)
ARCH			=$(shell echo $(MACHINE) | sed -e 's/.*mingw32.*/mingw32/g' -e 's/\-.*//g' -e 's/i.86/x86/g')
STRIP			=strip

# Mac OSX Tiger : powerpc -> ppc
ifeq ($(MACHINE),powerpc-apple-darwin8) # MacOS-10.4/ppc
	ARCH		= ppc
endif

# add special architecture based flags
ifeq ($(ARCH),x86)	# Linux/x86
	DEST_ARCH	=x86
	ARCH_CFLAGS	=-march=$(shell echo $(MACHINE) | sed -e 's/\-.*//g')
endif
ifeq ($(ARCH),mingw32)  # Win32/x86 in MingW environment
	DEST_ARCH       =x86
	ARCH_CFLAGS     =-mwin32 -mno-cygwin
endif
ifeq ($(ARCH),ppc)	# MacOS-X/ppc
	DEST_ARCH	=ppc
	ARCH_CFLAGS	=-arch ppc -faltivec -maltivec -mcpu=7450 -mtune=7450 -mpowerpc -mpowerpc-gfxopt
	ifeq ($(CC_BASEVERSION),4)	# auto vectorize if we're using gcc4.0+
	    ARCH_CFLAGS	+= -ftree-vectorize
	endif
endif


#======================================================================
# Output Directories
#======================================================================
# location of source relative to Makefile
SOURCE_DIR			=.
HEADER_DIR			=.

BUILD_DEBUG_DIR			=debug-$(ARCH)
BUILD_RELEASE_DIR		=release-$(ARCH)

# compiler flags
PRJ_CFLAGS			=-DWITH_ZLIB -DWITH_PNG
XMMS_CFLAGS			=-DWITH_XMMS `glib-config --cflags`
BASE_CFLAGS			=-Wall -Wno-format-y2k $(PRJ_CFLAGS) $(ARCH_CFLAGS)
BASE_RELEASE_CFLAGS		=-ffast-math -fomit-frame-pointer -fexpensive-optimizations
ifneq ($(CC_BASEVERSION),4) # if we're not auto-vectorizing then we can unroll the loops (mdfour ahoy)
	BASE_RELEASE_CFLAGS  += -funroll-loops
endif

ifeq ($(CC_BASEVERSION),2)
	# gcc 2.95.x does not know about -falign-XXX optimizations...
	BASE_RELEASE_CFLAGS	+= -O3
else
	BASE_RELEASE_CFLAGS	+= -O6
ifeq ($(ARCH),ppc)
	BASE_RELEASE_CFLAGS	+= -falign-loops=16 -falign-jumps=16 -falign-functions=16
else
	BASE_RELEASE_CFLAGS	+= -falign-loops=2 -falign-jumps=2 -falign-functions=2
endif
endif
BASE_DEBUG_CFLAGS		=-g -D_DEBUG

ifeq ($(ARCH),x86)		# Linux/x86
	BASE_CFLAGS		+= -Did386 $(XMMS_CFLAGS)
	CL_DLFLAGS		+= -ldl
endif
ifeq ($(ARCH),mingw32)		# Win32/x86 in MingW environment
	# use define for special assembly routines:
	BASE_CFLAGS		+= -Did386 -DMINGW32
endif
ifeq ($(ARCH),ppc)		# MacOS-X/ppc
	BASE_CFLAGS		+= -D__BIG_ENDIAN__ -Ddarwin
endif
ifeq ($(ARCH),powerpc)		# Linux/PPC
	BASE_CFLAGS		+= -D__BIG_ENDIAN__
	CL_DLFLAGS		+= -ldl
endif

BASE_CFLAGS			+= -funsigned-char -pipe -fno-strict-aliasing -I$(HEADER_DIR)
RELEASE_CFLAGS			=$(BASE_CFLAGS) $(BASE_RELEASE_CFLAGS) -DNDEBUG
DEBUG_CFLAGS			=$(BASE_CFLAGS) $(BASE_DEBUG_CFLAGS) -Wimplicit


# software and console (svga) builds
DO_CC				=$(CC) -DWITH_VMODE $(CFLAGS) -o $@ -c $<
ifeq ($(ARCH),mingw32)		# Win32/x86 in MingW environment
	DO_CC			+= -D_WINDOWS -mwindows
endif
DO_O_CC				=$(CC) -O $(CFLAGS) -o $@ -c $<
DO_AS				=$(CC) $(CFLAGS) -x assembler-with-cpp
ifeq ($(ARCH),x86)		# Linux/x86
	DO_AS			+= -DELF
endif
DO_AS				+= -o $@ -c $<

# opengl builds
BASE_GLCFLAGS			=-DWITH_JPEG -DGLQUAKE
ifeq ($(ARCH),x86)		# Linux/x86 -I/usr/include
	ARCH_GLCFLAGS		=-I/usr/include -DWITH_VMODE -DWITH_DGA -DWITH_EVDEV
endif
ifeq ($(ARCH),mingw32)		# Win32/x86 in MingW environment
	ARCH_GLCFLAGS		=-mwindows
endif
ifeq ($(ARCH),ppc)		# MacOS-X/ppc
	ARCH_GLCFLAGS		= -I/opt/local/include/ -I/Developer/Headers/FlatCarbon -I/sw/include -FOpenGL -FAGL
endif
GLCFLAGS			=$(ARCH_GLCFLAGS) $(BASE_GLCFLAGS)

DO_GL_CC			=$(CC) $(CFLAGS) $(GLCFLAGS) -o $@ -c $<
DO_GL_AS			=$(CC) $(CFLAGS) $(GLCFLAGS) -x assembler-with-cpp
ifeq ($(ARCH),x86)		# Linux/x86
	DO_GL_AS		+= -DELF
endif
DO_GL_AS			+= -o $@ -c $<


# linker flags
LDFLAGS				=-lm `glib-config --libs` -lpthread -lexpat -lpcre $(CL_DLFLAGS)
SVGALDFLAGS			=-lvga
X11_LDFLAGS			=-L/usr/X11R6/lib -lX11 -lXext -lXxf86dga -lXxf86vm
ifeq ($(ARCH),mingw32)		# Win32/x86 in MingW environment
	LDFLAGS			+= -lws2_32 -luser32 -lwinmm
endif
ifeq ($(ARCH),ppc)		# MacOS-X/ppc
	LDFLAGS			=-L/sw/lib/ -L/opt/local/lib -framework OpenGL -framework AGL -framework DrawSprocket -framework Carbon -framework ApplicationServices -framework IOKit
endif

# opengl build
BASE_GL_LDFLAGS			=-L/usr/X11R6/lib -lGL -lm -lX11 -lXext
ARCH_GL_LDFLAGS			=
ifeq ($(ARCH),x86)		# Linux/x86
	ARCH_GL_LDFLAGS		=-lXxf86dga -lXxf86vm
endif
ifeq ($(ARCH),mingw32)  # Win32/x86 in MingW environment
	BASE_GL_LDFLAGS          =
	ARCH_GL_LDFLAGS          =-mwindows -lopengl32 -ldxguid -lgdi32
endif
ifeq ($(ARCH),ppc)		# MacOS-X/ppc
	BASE_GL_LDFLAGS		=
endif
GL_LDFLAGS			=$(ARCH_GL_LDFLAGS) $(BASE_GL_LDFLAGS)


#############################################################################
# SETUP AND BUILD
#############################################################################

X11_TARGETS=$(BUILDDIR)/ezquake.x11
GLX_TARGETS=$(BUILDDIR)/ezquake-gl.glx
SVGA_TARGETS=$(BUILDDIR)/ezquake.svga
MAC_TARGETS=$(BUILDDIR)/ezquake-gl.mac

debug: svga_debug x11_debug glx_debug mac_debug

release: svga_release x11_release glx_release mac_release

mac_release:
	@-mkdir -p $(BUILD_RELEASE_DIR)/build-mac-gl
	$(MAKE) mac_targets BUILDDIR="$(BUILD_RELEASE_DIR)" CFLAGS="$(RELEASE_CFLAGS)" BINARY_TYPE=MAC
	$(STRIP) $(BUILD_RELEASE_DIR)/ezquake-gl.mac

mac_debug:
	@-mkdir -p $(BUILD_DEBUG_DIR)/build-mac-gl
	$(MAKE) mac_targets BUILDDIR="$(BUILD_DEBUG_DIR)" CFLAGS="$(DEBUG_CFLAGS)" BINARY_TYPE=MAC

svga_release:
	@-mkdir -p $(BUILD_RELEASE_DIR)/build
	$(MAKE) svga_targets BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS)" BINARY_TYPE=SVGA
	$(STRIP) $(BUILD_RELEASE_DIR)/ezquake.svga

svga_debug:
	@-mkdir -p $(BUILD_DEBUG_DIR)/build
	$(MAKE) svga_targets BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)" BINARY_TYPE=SVGA

x11_release:
	@-mkdir -p $(BUILD_RELEASE_DIR)/build
	$(MAKE) x11_targets BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS)" BINARY_TYPE=X11
	$(STRIP) $(BUILD_RELEASE_DIR)/ezquake.x11

x11_debug:
	@-mkdir -p $(BUILD_DEBUG_DIR)/build
	$(MAKE) x11_targets BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)" BINARY_TYPE=X11

glx_release:
	@-mkdir -p $(BUILD_RELEASE_DIR)/build-gl
	$(MAKE) glx_targets BUILDDIR="$(BUILD_RELEASE_DIR)" CFLAGS="$(RELEASE_CFLAGS)" BINARY_TYPE=GLX
	$(STRIP) $(BUILD_RELEASE_DIR)/ezquake-gl.glx

glx_debug:
	@-mkdir -p $(BUILD_DEBUG_DIR)/build-gl
	$(MAKE) glx_targets BUILDDIR="$(BUILD_DEBUG_DIR)" CFLAGS="$(DEBUG_CFLAGS)" BINARY_TYPE=GLX

targets: $(TARGETS)
x11_targets: $(X11_TARGETS)
glx_targets: $(GLX_TARGETS)
svga_targets: $(SVGA_TARGETS)
mac_targets: $(MAC_TARGETS)

#############################################################################
# CLIENT
#############################################################################

QWCL_OBJS = \
    $(BUILDDIR)/build/host.o \
    $(BUILDDIR)/build/sys_linux.o \
\
    $(BUILDDIR)/build/cd_linux.o \
    $(BUILDDIR)/build/snd_dma.o \
    $(BUILDDIR)/build/snd_mem.o \
    $(BUILDDIR)/build/snd_mix.o \
    $(BUILDDIR)/build/snd_linux.o \
    $(BUILDDIR)/build/snd_oss.o \
    $(BUILDDIR)/build/snd_alsa.o \
\
    $(BUILDDIR)/build/cl_input.o \
    $(BUILDDIR)/build/keys.o \
\
    $(BUILDDIR)/build/net_chan.o \
    $(BUILDDIR)/build/net_udp.o \
\
    $(BUILDDIR)/build/pr_exec.o \
    $(BUILDDIR)/build/pr_edict.o \
    $(BUILDDIR)/build/pr_cmds.o \
\
    $(BUILDDIR)/build/pmove.o \
    $(BUILDDIR)/build/pmovetst.o \
    $(BUILDDIR)/build/sv_ccmds.o \
    $(BUILDDIR)/build/sv_save.o \
    $(BUILDDIR)/build/sv_ents.o \
    $(BUILDDIR)/build/sv_init.o \
    $(BUILDDIR)/build/sv_main.o \
    $(BUILDDIR)/build/sv_move.o \
    $(BUILDDIR)/build/sv_nchan.o \
    $(BUILDDIR)/build/sv_phys.o \
    $(BUILDDIR)/build/sv_send.o \
    $(BUILDDIR)/build/sv_user.o \
    $(BUILDDIR)/build/sv_world.o \
\
    $(BUILDDIR)/build/r_aclip.o \
    $(BUILDDIR)/build/r_alias.o \
    $(BUILDDIR)/build/r_bsp.o \
    $(BUILDDIR)/build/r_draw.o \
    $(BUILDDIR)/build/r_edge.o \
    $(BUILDDIR)/build/r_efrag.o \
    $(BUILDDIR)/build/r_light.o \
    $(BUILDDIR)/build/r_main.o \
    $(BUILDDIR)/build/r_model.o \
    $(BUILDDIR)/build/r_misc.o \
    $(BUILDDIR)/build/r_part.o \
    $(BUILDDIR)/build/r_sky.o \
    $(BUILDDIR)/build/r_sprite.o \
    $(BUILDDIR)/build/r_surf.o \
    $(BUILDDIR)/build/r_rast.o \
    $(BUILDDIR)/build/r_vars.o \
\
    $(BUILDDIR)/build/d_edge.o \
    $(BUILDDIR)/build/d_fill.o \
    $(BUILDDIR)/build/d_init.o \
    $(BUILDDIR)/build/d_modech.o \
    $(BUILDDIR)/build/d_polyse.o \
    $(BUILDDIR)/build/d_scan.o \
    $(BUILDDIR)/build/d_sky.o \
    $(BUILDDIR)/build/d_sprite.o \
    $(BUILDDIR)/build/d_surf.o \
    $(BUILDDIR)/build/d_vars.o \
    $(BUILDDIR)/build/d_zpoint.o \
\
    $(BUILDDIR)/build/cl_cam.o \
    $(BUILDDIR)/build/cl_cmd.o \
    $(BUILDDIR)/build/cl_demo.o \
    $(BUILDDIR)/build/cl_ents.o \
    $(BUILDDIR)/build/cl_main.o \
    $(BUILDDIR)/build/cl_parse.o \
    $(BUILDDIR)/build/cl_pred.o \
    $(BUILDDIR)/build/cl_slist.o \
    $(BUILDDIR)/build/cl_tent.o \
    $(BUILDDIR)/build/cl_view.o \
\
    $(BUILDDIR)/build/cmd.o \
    $(BUILDDIR)/build/common.o \
    $(BUILDDIR)/build/com_msg.o \
    $(BUILDDIR)/build/console.o \
    $(BUILDDIR)/build/crc.o \
    $(BUILDDIR)/build/cvar.o \
    $(BUILDDIR)/build/image.o \
    $(BUILDDIR)/build/mathlib.o \
    $(BUILDDIR)/build/sha1.o \
    $(BUILDDIR)/build/mdfour.o \
    $(BUILDDIR)/build/menu.o \
    $(BUILDDIR)/build/mvd_utils.o \
    $(BUILDDIR)/build/sbar.o \
    $(BUILDDIR)/build/cl_screen.o \
    $(BUILDDIR)/build/skin.o \
    $(BUILDDIR)/build/teamplay.o \
    $(BUILDDIR)/build/version.o \
    $(BUILDDIR)/build/wad.o \
    $(BUILDDIR)/build/zone.o \
    $(BUILDDIR)/build/parser.o \
\
    $(BUILDDIR)/build/modules.o \
    $(BUILDDIR)/build/ignore.o \
    $(BUILDDIR)/build/logging.o \
    $(BUILDDIR)/build/fragstats.o \
    $(BUILDDIR)/build/match_tools.o \
    $(BUILDDIR)/build/movie.o \
    $(BUILDDIR)/build/utils.o \
 \
    $(BUILDDIR)/build/fchecks.o \
    $(BUILDDIR)/build/auth.o \
    $(BUILDDIR)/build/Ctrl.o \
    $(BUILDDIR)/build/Ctrl_EditBox.o \
    $(BUILDDIR)/build/Ctrl_Tab.o \
    $(BUILDDIR)/build/Ctrl_PageViewer.o \
    $(BUILDDIR)/build/EX_FunNames.o \
    $(BUILDDIR)/build/EX_browser.o \
    $(BUILDDIR)/build/EX_browser_net.o \
    $(BUILDDIR)/build/EX_browser_ping.o \
    $(BUILDDIR)/build/EX_browser_sources.o \
    $(BUILDDIR)/build/EX_misc.o \
    $(BUILDDIR)/build/common_draw.o \
    $(BUILDDIR)/build/hud.o \
    $(BUILDDIR)/build/hud_common.o \
    $(BUILDDIR)/build/rulesets.o \
    $(BUILDDIR)/build/config_manager.o \
    $(BUILDDIR)/build/mp3_player.o \
    $(BUILDDIR)/build/fmod.o \
    $(BUILDDIR)/build/localtime_linux.o \
 \
    $(BUILDDIR)/build/xml_test.o \
    $(BUILDDIR)/build/xsd.o \
    $(BUILDDIR)/build/xsd_command.o \
    $(BUILDDIR)/build/xsd_document.o \
    $(BUILDDIR)/build/xsd_variable.o \
    $(BUILDDIR)/build/document_rendering.o \
    $(BUILDDIR)/build/help.o \
    $(BUILDDIR)/build/help_browser.o \
    $(BUILDDIR)/build/help_files.o \
    $(BUILDDIR)/build/EX_FileList.o
    
QWCL_SVGA_AS_OBJS = \
    $(BUILDDIR)/build/d_copy.o

QWCL_AS_OBJS = \
    $(BUILDDIR)/build/cl_math.o \
    $(BUILDDIR)/build/d_draw.o \
    $(BUILDDIR)/build/d_draw16.o \
    $(BUILDDIR)/build/d_parta.o \
    $(BUILDDIR)/build/d_polysa.o \
    $(BUILDDIR)/build/d_scana.o \
    $(BUILDDIR)/build/d_spr8.o \
    $(BUILDDIR)/build/d_varsa.o \
    $(BUILDDIR)/build/math.o \
    $(BUILDDIR)/build/r_aclipa.o \
    $(BUILDDIR)/build/r_aliasa.o \
    $(BUILDDIR)/build/r_drawa.o \
    $(BUILDDIR)/build/r_edgea.o \
    $(BUILDDIR)/build/r_varsa.o \
    $(BUILDDIR)/build/snd_mixa.o \
    $(BUILDDIR)/build/surf8.o \
    $(BUILDDIR)/build/sys_x86.o

QWCL_SVGA_OBJS = $(BUILDDIR)/build/vid_svgalib.o
QWCL_X11_OBJS = $(BUILDDIR)/build/vid_x11.o

$(BUILDDIR)/ezquake.svga : $(QWCL_OBJS) $(QWCL_SVGA_AS_OBJS) $(QWCL_AS_OBJS) $(QWCL_SVGA_OBJS)
	$(CC) $(CFLAGS) -o $@ $(QWCL_OBJS) $(QWCL_SVGA_AS_OBJS) $(QWCL_AS_OBJS) $(QWCL_SVGA_OBJS) \
	$(LDFLAGS) $(SVGALDFLAGS)

$(BUILDDIR)/ezquake.x11 : $(QWCL_OBJS) $(QWCL_AS_OBJS) $(QWCL_X11_OBJS)
	$(CC) $(CFLAGS) -o $@ $(QWCL_OBJS) $(QWCL_AS_OBJS) $(QWCL_X11_OBJS) \
	$(LDFLAGS) $(X11_LDFLAGS)

$(BUILDDIR)/build/host.o :		$(SOURCE_DIR)/host.c
	$(DO_CC)

$(BUILDDIR)/build/sys_linux.o :		$(SOURCE_DIR)/sys_linux.c
	$(DO_CC)

$(BUILDDIR)/build/cd_linux.o :		$(SOURCE_DIR)/cd_linux.c
	$(DO_CC)

$(BUILDDIR)/build/snd_dma.o :		$(SOURCE_DIR)/snd_dma.c
	$(DO_CC)

$(BUILDDIR)/build/snd_mem.o :		$(SOURCE_DIR)/snd_mem.c
	$(DO_CC)

$(BUILDDIR)/build/snd_mix.o :		$(SOURCE_DIR)/snd_mix.c
	$(DO_CC)

$(BUILDDIR)/build/snd_linux.o :		$(SOURCE_DIR)/snd_linux.c
	$(DO_CC)

$(BUILDDIR)/build/snd_oss.o :		$(SOURCE_DIR)/snd_oss.c
	$(DO_CC)

$(BUILDDIR)/build/snd_alsa.o :		$(SOURCE_DIR)/snd_alsa.c
	$(DO_CC)

$(BUILDDIR)/build/cl_demo.o :		$(SOURCE_DIR)/cl_demo.c
	$(DO_CC)

$(BUILDDIR)/build/cl_ents.o :		$(SOURCE_DIR)/cl_ents.c
	$(DO_CC)

$(BUILDDIR)/build/cl_input.o :		$(SOURCE_DIR)/cl_input.c
	$(DO_CC)

$(BUILDDIR)/build/cl_main.o :		$(SOURCE_DIR)/cl_main.c
	$(DO_CC)

$(BUILDDIR)/build/cl_parse.o :		$(SOURCE_DIR)/cl_parse.c
	$(DO_CC)

$(BUILDDIR)/build/cl_pred.o :		$(SOURCE_DIR)/cl_pred.c
	$(DO_CC)

$(BUILDDIR)/build/cl_tent.o :		$(SOURCE_DIR)/cl_tent.c
	$(DO_CC)

$(BUILDDIR)/build/cl_cam.o :		$(SOURCE_DIR)/cl_cam.c
	$(DO_CC)   

$(BUILDDIR)/build/cl_view.o :		$(SOURCE_DIR)/cl_view.c
	$(DO_CC)
                              
$(BUILDDIR)/build/cl_cmd.o :		$(SOURCE_DIR)/cl_cmd.c
	$(DO_CC)

$(BUILDDIR)/build/cl_slist.o :		$(SOURCE_DIR)/cl_slist.c
	$(DO_CC)

$(BUILDDIR)/build/d_edge.o :		$(SOURCE_DIR)/d_edge.c
	$(DO_CC)

$(BUILDDIR)/build/d_fill.o :		$(SOURCE_DIR)/d_fill.c
	$(DO_CC)

$(BUILDDIR)/build/d_init.o :		$(SOURCE_DIR)/d_init.c
	$(DO_CC)

$(BUILDDIR)/build/d_modech.o :		$(SOURCE_DIR)/d_modech.c
	$(DO_CC)

$(BUILDDIR)/build/d_polyse.o :		$(SOURCE_DIR)/d_polyse.c
	$(DO_CC)

$(BUILDDIR)/build/d_scan.o :		$(SOURCE_DIR)/d_scan.c
	$(DO_CC)

$(BUILDDIR)/build/d_sky.o :		$(SOURCE_DIR)/d_sky.c
	$(DO_CC)

$(BUILDDIR)/build/d_sprite.o :		$(SOURCE_DIR)/d_sprite.c
	$(DO_CC)

$(BUILDDIR)/build/d_surf.o :		$(SOURCE_DIR)/d_surf.c
	$(DO_CC)

$(BUILDDIR)/build/d_vars.o :		$(SOURCE_DIR)/d_vars.c
	$(DO_CC)

$(BUILDDIR)/build/d_zpoint.o :		$(SOURCE_DIR)/d_zpoint.c
	$(DO_CC)

$(BUILDDIR)/build/r_aclip.o :		$(SOURCE_DIR)/r_aclip.c
	$(DO_CC)

$(BUILDDIR)/build/r_alias.o :		$(SOURCE_DIR)/r_alias.c
	$(DO_CC)

$(BUILDDIR)/build/r_bsp.o :		$(SOURCE_DIR)/r_bsp.c
	$(DO_CC)         

$(BUILDDIR)/build/r_draw.o :		$(SOURCE_DIR)/r_draw.c
	$(DO_CC)

$(BUILDDIR)/build/r_rast.o :		$(SOURCE_DIR)/r_rast.c
	$(DO_CC)

$(BUILDDIR)/build/r_edge.o :		$(SOURCE_DIR)/r_edge.c
	$(DO_CC)

$(BUILDDIR)/build/r_efrag.o :		$(SOURCE_DIR)/r_efrag.c
	$(DO_CC)

$(BUILDDIR)/build/r_light.o :		$(SOURCE_DIR)/r_light.c
	$(DO_CC)

$(BUILDDIR)/build/r_main.o :		$(SOURCE_DIR)/r_main.c
	$(DO_CC)

$(BUILDDIR)/build/r_misc.o :		$(SOURCE_DIR)/r_misc.c
	$(DO_CC)

$(BUILDDIR)/build/r_model.o :		$(SOURCE_DIR)/r_model.c
	$(DO_CC)

$(BUILDDIR)/build/r_part.o :		$(SOURCE_DIR)/r_part.c
	$(DO_CC)

$(BUILDDIR)/build/r_sky.o :		$(SOURCE_DIR)/r_sky.c
	$(DO_CC)

$(BUILDDIR)/build/r_sprite.o :		$(SOURCE_DIR)/r_sprite.c
	$(DO_CC)

$(BUILDDIR)/build/r_surf.o :		$(SOURCE_DIR)/r_surf.c
	$(DO_CC)

$(BUILDDIR)/build/r_vars.o :		$(SOURCE_DIR)/r_vars.c
	$(DO_CC)

$(BUILDDIR)/build/cmd.o :		$(SOURCE_DIR)/cmd.c
	$(DO_CC)

$(BUILDDIR)/build/common.o :		$(SOURCE_DIR)/common.c
	$(DO_CC)

$(BUILDDIR)/build/com_msg.o :		$(SOURCE_DIR)/com_msg.c
	$(DO_CC)

$(BUILDDIR)/build/console.o :		$(SOURCE_DIR)/console.c
	$(DO_CC)

$(BUILDDIR)/build/crc.o :		$(SOURCE_DIR)/crc.c
	$(DO_CC)

$(BUILDDIR)/build/version.o :		$(SOURCE_DIR)/version.c
	$(DO_CC)

$(BUILDDIR)/build/cvar.o :		$(SOURCE_DIR)/cvar.c
	$(DO_CC)

$(BUILDDIR)/build/keys.o :		$(SOURCE_DIR)/keys.c
	$(DO_CC)  

$(BUILDDIR)/build/mathlib.o :		$(SOURCE_DIR)/mathlib.c
	$(DO_CC)

$(BUILDDIR)/build/menu.o :		$(SOURCE_DIR)/menu.c
	$(DO_CC)

$(BUILDDIR)/build/mvd_utils.o :		$(SOURCE_DIR)/mvd_utils.c
	$(DO_CC)

$(BUILDDIR)/build/movie.o :		$(SOURCE_DIR)/movie.c
	$(DO_CC)

$(BUILDDIR)/build/net_chan.o :		$(SOURCE_DIR)/net_chan.c
	$(DO_CC)

$(BUILDDIR)/build/net_udp.o :		$(SOURCE_DIR)/net_udp.c
	$(DO_CC)

$(BUILDDIR)/build/pmove.o :		$(SOURCE_DIR)/pmove.c
	$(DO_CC)

$(BUILDDIR)/build/pmovetst.o :		$(SOURCE_DIR)/pmovetst.c
	$(DO_CC)

$(BUILDDIR)/build/sbar.o :		$(SOURCE_DIR)/sbar.c
	$(DO_CC)

$(BUILDDIR)/build/cl_screen.o :		$(SOURCE_DIR)/cl_screen.c
	$(DO_CC)

$(BUILDDIR)/build/skin.o :		$(SOURCE_DIR)/skin.c
	$(DO_CC)

$(BUILDDIR)/build/image.o :		$(SOURCE_DIR)/image.c
	$(DO_CC)

$(BUILDDIR)/build/teamplay.o :		$(SOURCE_DIR)/teamplay.c
	$(DO_CC)

$(BUILDDIR)/build/sv_ccmds.o :		$(SOURCE_DIR)/sv_ccmds.c
	$(DO_CC)

$(BUILDDIR)/build/sv_save.o :		$(SOURCE_DIR)/sv_save.c
	$(DO_CC)

$(BUILDDIR)/build/sv_ents.o :		$(SOURCE_DIR)/sv_ents.c
	$(DO_CC)

$(BUILDDIR)/build/sv_init.o :		$(SOURCE_DIR)/sv_init.c
	$(DO_CC)

$(BUILDDIR)/build/sv_main.o :		$(SOURCE_DIR)/sv_main.c
	$(DO_CC)

$(BUILDDIR)/build/sv_move.o :		$(SOURCE_DIR)/sv_move.c
	$(DO_CC)

$(BUILDDIR)/build/sv_nchan.o :		$(SOURCE_DIR)/sv_nchan.c
	$(DO_CC)

$(BUILDDIR)/build/sv_phys.o :		$(SOURCE_DIR)/sv_phys.c
	$(DO_CC)

$(BUILDDIR)/build/sv_send.o :		$(SOURCE_DIR)/sv_send.c
	$(DO_CC)

$(BUILDDIR)/build/sv_user.o :		$(SOURCE_DIR)/sv_user.c
	$(DO_CC)

$(BUILDDIR)/build/sv_world.o :		$(SOURCE_DIR)/sv_world.c
	$(DO_CC)

$(BUILDDIR)/build/pr_edict.o :		$(SOURCE_DIR)/pr_edict.c
	$(DO_CC)

$(BUILDDIR)/build/pr_exec.o :		$(SOURCE_DIR)/pr_exec.c
	$(DO_CC)

$(BUILDDIR)/build/pr_cmds.o :		$(SOURCE_DIR)/pr_cmds.c
	$(DO_CC)

$(BUILDDIR)/build/ignore.o :		$(SOURCE_DIR)/ignore.c
	$(DO_CC)

$(BUILDDIR)/build/logging.o :		$(SOURCE_DIR)/logging.c
	$(DO_CC)

$(BUILDDIR)/build/fragstats.o :		$(SOURCE_DIR)/fragstats.c
	$(DO_CC)

$(BUILDDIR)/build/match_tools.o :	$(SOURCE_DIR)/match_tools.c
	$(DO_CC)

$(BUILDDIR)/build/utils.o :		$(SOURCE_DIR)/utils.c
	$(DO_CC)

$(BUILDDIR)/build/fchecks.o :		$(SOURCE_DIR)/fchecks.c
	$(DO_CC)

$(BUILDDIR)/build/fmod.o :		$(SOURCE_DIR)/fmod.c
	$(DO_CC)

$(BUILDDIR)/build/auth.o :		$(SOURCE_DIR)/auth.c
	$(DO_CC)

$(BUILDDIR)/build/Ctrl.o :		$(SOURCE_DIR)/Ctrl.c
	$(DO_CC)

$(BUILDDIR)/build/Ctrl_EditBox.o :	$(SOURCE_DIR)/Ctrl_EditBox.c
	$(DO_CC)

$(BUILDDIR)/build/Ctrl_Tab.o :		$(SOURCE_DIR)/Ctrl_Tab.c
	$(DO_CC)

$(BUILDDIR)/build/Ctrl_PageViewer.o :	$(SOURCE_DIR)/Ctrl_PageViewer.c
	$(DO_CC)

$(BUILDDIR)/build/EX_FunNames.o :	$(SOURCE_DIR)/EX_FunNames.c
	$(DO_CC)

$(BUILDDIR)/build/EX_browser.o :	$(SOURCE_DIR)/EX_browser.c
	$(DO_CC)

$(BUILDDIR)/build/EX_browser_net.o :	$(SOURCE_DIR)/EX_browser_net.c
	$(DO_CC)

$(BUILDDIR)/build/EX_browser_ping.o :	$(SOURCE_DIR)/EX_browser_ping.c
	$(DO_CC)

$(BUILDDIR)/build/EX_browser_sources.o : $(SOURCE_DIR)/EX_browser_sources.c
	$(DO_CC)

$(BUILDDIR)/build/EX_misc.o :		$(SOURCE_DIR)/EX_misc.c
	$(DO_CC)

$(BUILDDIR)/build/common_draw.o :	$(SOURCE_DIR)/common_draw.c
	$(DO_CC)

$(BUILDDIR)/build/hud.o :		$(SOURCE_DIR)/hud.c
	$(DO_CC)

$(BUILDDIR)/build/hud_common.o :	$(SOURCE_DIR)/hud_common.c
	$(DO_CC)

$(BUILDDIR)/build/rulesets.o :		$(SOURCE_DIR)/rulesets.c
	$(DO_CC)

$(BUILDDIR)/build/config_manager.o :	$(SOURCE_DIR)/config_manager.c
	$(DO_CC)

$(BUILDDIR)/build/localtime_linux.o :	$(SOURCE_DIR)/localtime_linux.c
	$(DO_CC)

$(BUILDDIR)/build/mp3_player.o :	$(SOURCE_DIR)/mp3_player.c
	$(DO_CC)

$(BUILDDIR)/build/modules.o :		$(SOURCE_DIR)/modules.c
	$(DO_CC) -D_Soft_$(BINARY_TYPE)	

$(BUILDDIR)/build/sha1.o :		$(SOURCE_DIR)/sha1.c
	$(DO_CC)

$(BUILDDIR)/build/mdfour.o :		$(SOURCE_DIR)/mdfour.c
	$(DO_CC)

$(BUILDDIR)/build/wad.o :		$(SOURCE_DIR)/wad.c
	$(DO_CC)

$(BUILDDIR)/build/zone.o :		$(SOURCE_DIR)/zone.c
	$(DO_CC)

$(BUILDDIR)/build/parser.o :		$(SOURCE_DIR)/parser.c
	$(DO_CC)

$(BUILDDIR)/build/xml_test.o :		$(SOURCE_DIR)/xml_test.c
	$(DO_CC)

$(BUILDDIR)/build/xsd.o :		$(SOURCE_DIR)/xsd.c
	$(DO_CC)

$(BUILDDIR)/build/xsd_command.o :	$(SOURCE_DIR)/xsd_command.c
	$(DO_CC)

$(BUILDDIR)/build/xsd_document.o :	$(SOURCE_DIR)/xsd_document.c
	$(DO_CC)

$(BUILDDIR)/build/xsd_variable.o :	$(SOURCE_DIR)/xsd_variable.c
	$(DO_CC)

$(BUILDDIR)/build/document_rendering.o : $(SOURCE_DIR)/document_rendering.c
	$(DO_CC)

$(BUILDDIR)/build/help.o :		$(SOURCE_DIR)/help.c
	$(DO_CC)

$(BUILDDIR)/build/help_browser.o :	$(SOURCE_DIR)/help_browser.c
	$(DO_CC)

$(BUILDDIR)/build/help_files.o :	$(SOURCE_DIR)/help_files.c
	$(DO_CC)

$(BUILDDIR)/build/EX_FileList.o :	$(SOURCE_DIR)/EX_FileList.c
	$(DO_CC)

#ASM FILES
$(BUILDDIR)/build/d_draw.o :		$(SOURCE_DIR)/d_draw.s
	$(DO_AS)

$(BUILDDIR)/build/d_draw16.o :		$(SOURCE_DIR)/d_draw16.s
	$(DO_AS)

$(BUILDDIR)/build/d_parta.o :		$(SOURCE_DIR)/d_parta.s
	$(DO_AS)

$(BUILDDIR)/build/d_polysa.o :		$(SOURCE_DIR)/d_polysa.s
	$(DO_AS)

$(BUILDDIR)/build/d_scana.o :		$(SOURCE_DIR)/d_scana.s
	$(DO_AS)

$(BUILDDIR)/build/d_spr8.o :		$(SOURCE_DIR)/d_spr8.s
	$(DO_AS)

$(BUILDDIR)/build/d_varsa.o :		$(SOURCE_DIR)/d_varsa.s
	$(DO_AS)

$(BUILDDIR)/build/math.o :		$(SOURCE_DIR)/math.s
	$(DO_AS)

$(BUILDDIR)/build/r_aclipa.o :		$(SOURCE_DIR)/r_aclipa.s
	$(DO_AS)

$(BUILDDIR)/build/r_aliasa.o :		$(SOURCE_DIR)/r_aliasa.s
	$(DO_AS)

$(BUILDDIR)/build/r_drawa.o :		$(SOURCE_DIR)/r_drawa.s
	$(DO_AS)

$(BUILDDIR)/build/r_edgea.o :		$(SOURCE_DIR)/r_edgea.s
	$(DO_AS)

$(BUILDDIR)/build/r_varsa.o :		$(SOURCE_DIR)/r_varsa.s
	$(DO_AS)

$(BUILDDIR)/build/snd_mixa.o :		$(SOURCE_DIR)/snd_mixa.s
	$(DO_AS)

$(BUILDDIR)/build/surf8.o :		$(SOURCE_DIR)/surf8.s
	$(DO_AS)

$(BUILDDIR)/build/d_copy.o :		$(SOURCE_DIR)/d_copy.s
	$(DO_AS)

$(BUILDDIR)/build/cl_math.o :		$(SOURCE_DIR)/cl_math.s
	$(DO_AS)

$(BUILDDIR)/build/sys_x86.o :		$(SOURCE_DIR)/sys_x86.s
	$(DO_AS)

#VIDEO FILES
$(BUILDDIR)/build/vid_x11.o :		$(SOURCE_DIR)/vid_x11.c
	$(DO_CC)

$(BUILDDIR)/build/vid_svgalib.o :	$(SOURCE_DIR)/vid_svgalib.c
	$(DO_CC)

#############################################################################
# GLCLIENT
#############################################################################

GLQWCL_OBJS = \
    $(BUILDDIR)/build-gl/host.o \
    $(BUILDDIR)/build-gl/sys_linux.o \
\
    $(BUILDDIR)/build-gl/cd_linux.o \
    $(BUILDDIR)/build-gl/snd_dma.o \
    $(BUILDDIR)/build-gl/snd_mem.o \
    $(BUILDDIR)/build-gl/snd_mix.o \
    $(BUILDDIR)/build-gl/snd_linux.o \
    $(BUILDDIR)/build-gl/snd_oss.o \
    $(BUILDDIR)/build-gl/snd_alsa.o \
\
    $(BUILDDIR)/build-gl/cl_input.o \
    $(BUILDDIR)/build-gl/keys.o \
\
    $(BUILDDIR)/build-gl/net_chan.o \
    $(BUILDDIR)/build-gl/net_udp.o \
\
    $(BUILDDIR)/build-gl/pr_exec.o \
    $(BUILDDIR)/build-gl/pr_edict.o \
    $(BUILDDIR)/build-gl/pr_cmds.o \
\
    $(BUILDDIR)/build-gl/pmove.o \
    $(BUILDDIR)/build-gl/pmovetst.o \
    $(BUILDDIR)/build-gl/sv_ccmds.o \
    $(BUILDDIR)/build-gl/sv_save.o \
    $(BUILDDIR)/build-gl/sv_ents.o \
    $(BUILDDIR)/build-gl/sv_init.o \
    $(BUILDDIR)/build-gl/sv_main.o \
    $(BUILDDIR)/build-gl/sv_move.o \
    $(BUILDDIR)/build-gl/sv_nchan.o \
    $(BUILDDIR)/build-gl/sv_phys.o \
    $(BUILDDIR)/build-gl/sv_send.o \
    $(BUILDDIR)/build-gl/sv_user.o \
    $(BUILDDIR)/build-gl/sv_world.o \
\
    $(BUILDDIR)/build-gl/r_part.o \
\
    $(BUILDDIR)/build-gl/gl_draw.o \
    $(BUILDDIR)/build-gl/gl_md3.o \
    $(BUILDDIR)/build-gl/gl_mesh.o \
    $(BUILDDIR)/build-gl/gl_model.o \
    $(BUILDDIR)/build-gl/gl_ngraph.o \
    $(BUILDDIR)/build-gl/gl_refrag.o \
    $(BUILDDIR)/build-gl/gl_rlight.o \
    $(BUILDDIR)/build-gl/gl_rpart.o \
    $(BUILDDIR)/build-gl/gl_rmain.o \
    $(BUILDDIR)/build-gl/gl_rmisc.o \
    $(BUILDDIR)/build-gl/gl_rsurf.o \
    $(BUILDDIR)/build-gl/cl_screen.o \
    $(BUILDDIR)/build-gl/gl_warp.o \
    $(BUILDDIR)/build-gl/gl_texture.o \
\
    $(BUILDDIR)/build-gl/cl_cam.o \
    $(BUILDDIR)/build-gl/cl_cmd.o \
    $(BUILDDIR)/build-gl/cl_demo.o \
    $(BUILDDIR)/build-gl/cl_ents.o \
    $(BUILDDIR)/build-gl/cl_main.o \
    $(BUILDDIR)/build-gl/cl_parse.o \
    $(BUILDDIR)/build-gl/cl_pred.o \
    $(BUILDDIR)/build-gl/cl_slist.o \
    $(BUILDDIR)/build-gl/cl_tent.o \
    $(BUILDDIR)/build-gl/cl_view.o \
\
    $(BUILDDIR)/build-gl/cmd.o \
    $(BUILDDIR)/build-gl/common.o \
    $(BUILDDIR)/build-gl/com_msg.o \
    $(BUILDDIR)/build-gl/console.o \
    $(BUILDDIR)/build-gl/crc.o \
    $(BUILDDIR)/build-gl/cvar.o \
    $(BUILDDIR)/build-gl/image.o \
    $(BUILDDIR)/build-gl/mathlib.o \
    $(BUILDDIR)/build-gl/sha1.o \
    $(BUILDDIR)/build-gl/mdfour.o \
    $(BUILDDIR)/build-gl/menu.o \
    $(BUILDDIR)/build-gl/mvd_utils.o \
    $(BUILDDIR)/build-gl/sbar.o \
    $(BUILDDIR)/build-gl/skin.o \
    $(BUILDDIR)/build-gl/teamplay.o \
    $(BUILDDIR)/build-gl/version.o \
    $(BUILDDIR)/build-gl/wad.o \
    $(BUILDDIR)/build-gl/zone.o \
    $(BUILDDIR)/build-gl/parser.o \
\
    $(BUILDDIR)/build-gl/ignore.o \
    $(BUILDDIR)/build-gl/logging.o \
    $(BUILDDIR)/build-gl/fragstats.o \
    $(BUILDDIR)/build-gl/match_tools.o \
    $(BUILDDIR)/build-gl/utils.o \
    $(BUILDDIR)/build-gl/movie.o \
\
    $(BUILDDIR)/build-gl/fchecks.o \
    $(BUILDDIR)/build-gl/auth.o \
    $(BUILDDIR)/build-gl/Ctrl.o \
    $(BUILDDIR)/build-gl/Ctrl_EditBox.o \
    $(BUILDDIR)/build-gl/Ctrl_Tab.o \
    $(BUILDDIR)/build-gl/Ctrl_PageViewer.o \
    $(BUILDDIR)/build-gl/EX_FunNames.o \
    $(BUILDDIR)/build-gl/EX_browser.o \
    $(BUILDDIR)/build-gl/EX_browser_net.o \
    $(BUILDDIR)/build-gl/EX_browser_ping.o \
    $(BUILDDIR)/build-gl/EX_browser_sources.o \
    $(BUILDDIR)/build-gl/EX_misc.o \
    $(BUILDDIR)/build-gl/common_draw.o \
    $(BUILDDIR)/build-gl/hud.o \
    $(BUILDDIR)/build-gl/hud_common.o \
    $(BUILDDIR)/build-gl/modules.o \
    $(BUILDDIR)/build-gl/rulesets.o \
    $(BUILDDIR)/build-gl/config_manager.o \
    $(BUILDDIR)/build-gl/mp3_player.o \
    $(BUILDDIR)/build-gl/fmod.o \
    $(BUILDDIR)/build-gl/localtime_linux.o \
    $(BUILDDIR)/build-gl/collision.o \
    $(BUILDDIR)/build-gl/cl_collision.o \
    $(BUILDDIR)/build-gl/vx_camera.o \
    $(BUILDDIR)/build-gl/vx_coronas.o \
    $(BUILDDIR)/build-gl/vx_motiontrail.o \
    $(BUILDDIR)/build-gl/vx_stuff.o \
    $(BUILDDIR)/build-gl/vx_tracker.o \
    $(BUILDDIR)/build-gl/vx_vertexlights.o \
 \
    $(BUILDDIR)/build-gl/xml_test.o \
    $(BUILDDIR)/build-gl/xsd.o \
    $(BUILDDIR)/build-gl/xsd_command.o \
    $(BUILDDIR)/build-gl/xsd_document.o \
    $(BUILDDIR)/build-gl/xsd_variable.o \
    $(BUILDDIR)/build-gl/document_rendering.o \
    $(BUILDDIR)/build-gl/help.o \
    $(BUILDDIR)/build-gl/help_browser.o \
    $(BUILDDIR)/build-gl/help_files.o \
    $(BUILDDIR)/build-gl/EX_FileList.o

GLQWCL_AS_OBJS = \
    $(BUILDDIR)/build-gl/cl_math.o \
    $(BUILDDIR)/build-gl/math.o \
    $(BUILDDIR)/build-gl/snd_mixa.o \
    $(BUILDDIR)/build-gl/sys_x86.o

GLQWCL_X11_OBJS = $(BUILDDIR)/build-gl/vid_glx.o $(BUILDDIR)/build-gl/vid_common_gl.o

$(BUILDDIR)/ezquake-gl.glx : $(GLQWCL_OBJS) $(GLQWCL_X11_OBJS) $(GLQWCL_AS_OBJS)
	$(CC) $(CFLAGS) -o $@ $(GLQWCL_OBJS) $(GLQWCL_X11_OBJS) $(GLQWCL_AS_OBJS) \
	$(LDFLAGS) $(GL_LDFLAGS) 

$(BUILDDIR)/build-gl/host.o :			$(SOURCE_DIR)/host.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sys_linux.o :		$(SOURCE_DIR)/sys_linux.c
	$(DO_GL_CC)
       
$(BUILDDIR)/build-gl/cd_linux.o :		$(SOURCE_DIR)/cd_linux.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/snd_dma.o :		$(SOURCE_DIR)/snd_dma.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/snd_mem.o :		$(SOURCE_DIR)/snd_mem.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/snd_mix.o :		$(SOURCE_DIR)/snd_mix.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/snd_linux.o :		$(SOURCE_DIR)/snd_linux.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/snd_oss.o :		$(SOURCE_DIR)/snd_oss.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/snd_alsa.o :		$(SOURCE_DIR)/snd_alsa.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cl_demo.o :		$(SOURCE_DIR)/cl_demo.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cl_ents.o :		$(SOURCE_DIR)/cl_ents.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cl_input.o :		$(SOURCE_DIR)/cl_input.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cl_main.o :		$(SOURCE_DIR)/cl_main.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cl_parse.o :		$(SOURCE_DIR)/cl_parse.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cl_pred.o :		$(SOURCE_DIR)/cl_pred.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cl_tent.o :		$(SOURCE_DIR)/cl_tent.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cl_cam.o :			$(SOURCE_DIR)/cl_cam.c
	$(DO_GL_CC)   

$(BUILDDIR)/build-gl/cl_view.o :		$(SOURCE_DIR)/cl_view.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cl_cmd.o :			$(SOURCE_DIR)/cl_cmd.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cl_slist.o :		$(SOURCE_DIR)/cl_slist.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/r_part.o :			$(SOURCE_DIR)/r_part.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/gl_draw.o :		$(SOURCE_DIR)/gl_draw.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/gl_md3.o :			$(SOURCE_DIR)/gl_md3.c
	$(DO_GL_CC) 

$(BUILDDIR)/build-gl/gl_mesh.o :		$(SOURCE_DIR)/gl_mesh.c
	$(DO_GL_CC) 

$(BUILDDIR)/build-gl/gl_model.o :		$(SOURCE_DIR)/gl_model.c
	$(DO_GL_CC)  

$(BUILDDIR)/build-gl/gl_ngraph.o :		$(SOURCE_DIR)/gl_ngraph.c
	$(DO_GL_CC)   

$(BUILDDIR)/build-gl/gl_refrag.o :		$(SOURCE_DIR)/gl_refrag.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/gl_rlight.o :		$(SOURCE_DIR)/gl_rlight.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/gl_rpart.o :		$(SOURCE_DIR)/gl_rpart.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/gl_rmain.o :		$(SOURCE_DIR)/gl_rmain.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/gl_rmisc.o :		$(SOURCE_DIR)/gl_rmisc.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/gl_rsurf.o :		$(SOURCE_DIR)/gl_rsurf.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cl_screen.o :		$(SOURCE_DIR)/cl_screen.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/gl_warp.o :		$(SOURCE_DIR)/gl_warp.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cmd.o :			$(SOURCE_DIR)/cmd.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/common.o :			$(SOURCE_DIR)/common.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/com_msg.o :		$(SOURCE_DIR)/com_msg.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/console.o :		$(SOURCE_DIR)/console.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/crc.o :			$(SOURCE_DIR)/crc.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/version.o :		$(SOURCE_DIR)/version.c
	$(DO_GL_CC)
               
$(BUILDDIR)/build-gl/cvar.o :			$(SOURCE_DIR)/cvar.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/keys.o :			$(SOURCE_DIR)/keys.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/mathlib.o :		$(SOURCE_DIR)/mathlib.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/menu.o :			$(SOURCE_DIR)/menu.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/mvd_utils.o :		$(SOURCE_DIR)/mvd_utils.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/net_chan.o :		$(SOURCE_DIR)/net_chan.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/net_udp.o :		$(SOURCE_DIR)/net_udp.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/pmove.o :			$(SOURCE_DIR)/pmove.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/pmovetst.o :		$(SOURCE_DIR)/pmovetst.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sbar.o :			$(SOURCE_DIR)/sbar.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/skin.o :			$(SOURCE_DIR)/skin.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/image.o :			$(SOURCE_DIR)/image.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/teamplay.o :		$(SOURCE_DIR)/teamplay.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sv_ccmds.o :		$(SOURCE_DIR)/sv_ccmds.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sv_save.o :		$(SOURCE_DIR)/sv_save.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sv_ents.o :		$(SOURCE_DIR)/sv_ents.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sv_init.o :		$(SOURCE_DIR)/sv_init.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sv_main.o :		$(SOURCE_DIR)/sv_main.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sv_move.o :		$(SOURCE_DIR)/sv_move.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sv_nchan.o :		$(SOURCE_DIR)/sv_nchan.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sv_phys.o :		$(SOURCE_DIR)/sv_phys.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sv_send.o :		$(SOURCE_DIR)/sv_send.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sv_user.o :		$(SOURCE_DIR)/sv_user.c
	$(DO_GL_CC)  

$(BUILDDIR)/build-gl/sv_world.o :		$(SOURCE_DIR)/sv_world.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/pr_edict.o :		$(SOURCE_DIR)/pr_edict.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/pr_exec.o :		$(SOURCE_DIR)/pr_exec.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/pr_cmds.o :		$(SOURCE_DIR)/pr_cmds.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/ignore.o :			$(SOURCE_DIR)/ignore.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/logging.o :		$(SOURCE_DIR)/logging.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/fragstats.o :		$(SOURCE_DIR)/fragstats.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/match_tools.o :		$(SOURCE_DIR)/match_tools.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/utils.o :			$(SOURCE_DIR)/utils.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/movie.o :			$(SOURCE_DIR)/movie.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/fchecks.o :		$(SOURCE_DIR)/fchecks.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/fmod.o :			$(SOURCE_DIR)/fmod.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/auth.o :			$(SOURCE_DIR)/auth.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/Ctrl.o :			$(SOURCE_DIR)/Ctrl.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/Ctrl_EditBox.o :		$(SOURCE_DIR)/Ctrl_EditBox.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/Ctrl_Tab.o :		$(SOURCE_DIR)/Ctrl_Tab.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/Ctrl_PageViewer.o :	$(SOURCE_DIR)/Ctrl_PageViewer.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/EX_browser.o :		$(SOURCE_DIR)/EX_browser.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/EX_browser_net.o :		$(SOURCE_DIR)/EX_browser_net.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/EX_browser_ping.o :	$(SOURCE_DIR)/EX_browser_ping.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/EX_browser_sources.o :	$(SOURCE_DIR)/EX_browser_sources.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/EX_misc.o :		$(SOURCE_DIR)/EX_misc.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/common_draw.o :		$(SOURCE_DIR)/common_draw.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/hud.o :			$(SOURCE_DIR)/hud.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/hud_common.o :		$(SOURCE_DIR)/hud_common.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/EX_FunNames.o :		$(SOURCE_DIR)/EX_FunNames.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/collision.o :		$(SOURCE_DIR)/collision.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/cl_collision.o :		$(SOURCE_DIR)/cl_collision.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/vx_camera.o :		$(SOURCE_DIR)/vx_camera.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/vx_coronas.o :		$(SOURCE_DIR)/vx_coronas.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/vx_motiontrail.o :		$(SOURCE_DIR)/vx_motiontrail.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/vx_stuff.o :		$(SOURCE_DIR)/vx_stuff.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/vx_tracker.o :		$(SOURCE_DIR)/vx_tracker.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/vx_vertexlights.o :	$(SOURCE_DIR)/vx_vertexlights.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/rulesets.o :		$(SOURCE_DIR)/rulesets.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/config_manager.o :		$(SOURCE_DIR)/config_manager.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/mp3_player.o :		$(SOURCE_DIR)/mp3_player.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/modules.o :		$(SOURCE_DIR)/modules.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/gl_texture.o :		$(SOURCE_DIR)/gl_texture.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/sha1.o :			$(SOURCE_DIR)/sha1.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/mdfour.o :			$(SOURCE_DIR)/mdfour.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/wad.o :			$(SOURCE_DIR)/wad.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/zone.o :			$(SOURCE_DIR)/zone.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/parser.o :			$(SOURCE_DIR)/parser.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/localtime_linux.o :	$(SOURCE_DIR)/localtime_linux.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/xml_test.o :		$(SOURCE_DIR)/xml_test.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/xsd.o :			$(SOURCE_DIR)/xsd.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/xsd_command.o :		$(SOURCE_DIR)/xsd_command.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/xsd_document.o :		$(SOURCE_DIR)/xsd_document.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/xsd_variable.o :		$(SOURCE_DIR)/xsd_variable.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/document_rendering.o :	$(SOURCE_DIR)/document_rendering.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/help.o :			$(SOURCE_DIR)/help.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/help_browser.o :		$(SOURCE_DIR)/help_browser.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/help_files.o :		$(SOURCE_DIR)/help_files.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/EX_FileList.o :		$(SOURCE_DIR)/EX_FileList.c
	$(DO_GL_CC)

#ASM FILES
$(BUILDDIR)/build-gl/sys_x86.o :		$(SOURCE_DIR)/sys_x86.s
	$(DO_GL_AS)

$(BUILDDIR)/build-gl/math.o :			$(SOURCE_DIR)/math.s
	$(DO_GL_AS)

$(BUILDDIR)/build-gl/snd_mixa.o :		$(SOURCE_DIR)/snd_mixa.s
	$(DO_GL_AS)

$(BUILDDIR)/build-gl/cl_math.o :		$(SOURCE_DIR)/cl_math.s
	$(DO_GL_AS)

#VIDEO FILES
$(BUILDDIR)/build-gl/vid_glx.o :		$(SOURCE_DIR)/vid_glx.c
	$(DO_GL_CC)

$(BUILDDIR)/build-gl/vid_common_gl.o :		$(SOURCE_DIR)/vid_common_gl.c
	$(DO_GL_CC)















#############################################################################
# GL-MAC-CLIENT
#############################################################################

GLQWCL_MAC_OBJS = \
    $(BUILDDIR)/build-mac-gl/host.o \
\
    $(BUILDDIR)/build-mac-gl/snd_dma.o \
    $(BUILDDIR)/build-mac-gl/snd_mem.o \
    $(BUILDDIR)/build-mac-gl/snd_mix.o \
\
    $(BUILDDIR)/build-mac-gl/cl_input.o \
    $(BUILDDIR)/build-mac-gl/keys.o \
\
    $(BUILDDIR)/build-mac-gl/net_chan.o \
    $(BUILDDIR)/build-mac-gl/net_udp.o \
\
    $(BUILDDIR)/build-mac-gl/pr_exec.o \
    $(BUILDDIR)/build-mac-gl/pr_edict.o \
    $(BUILDDIR)/build-mac-gl/pr_cmds.o \
\
    $(BUILDDIR)/build-mac-gl/pmove.o \
    $(BUILDDIR)/build-mac-gl/pmovetst.o \
    $(BUILDDIR)/build-mac-gl/sv_ccmds.o \
    $(BUILDDIR)/build-mac-gl/sv_save.o \
    $(BUILDDIR)/build-mac-gl/sv_ents.o \
    $(BUILDDIR)/build-mac-gl/sv_init.o \
    $(BUILDDIR)/build-mac-gl/sv_main.o \
    $(BUILDDIR)/build-mac-gl/sv_move.o \
    $(BUILDDIR)/build-mac-gl/sv_nchan.o \
    $(BUILDDIR)/build-mac-gl/sv_phys.o \
    $(BUILDDIR)/build-mac-gl/sv_send.o \
    $(BUILDDIR)/build-mac-gl/sv_user.o \
    $(BUILDDIR)/build-mac-gl/sv_world.o \
\
    $(BUILDDIR)/build-mac-gl/r_part.o \
\
    $(BUILDDIR)/build-mac-gl/gl_draw.o \
    $(BUILDDIR)/build-mac-gl/gl_md3.o \
    $(BUILDDIR)/build-mac-gl/gl_mesh.o \
    $(BUILDDIR)/build-mac-gl/gl_model.o \
    $(BUILDDIR)/build-mac-gl/gl_ngraph.o \
    $(BUILDDIR)/build-mac-gl/gl_refrag.o \
    $(BUILDDIR)/build-mac-gl/gl_rlight.o \
    $(BUILDDIR)/build-mac-gl/gl_rpart.o \
    $(BUILDDIR)/build-mac-gl/gl_rmain.o \
    $(BUILDDIR)/build-mac-gl/gl_rmisc.o \
    $(BUILDDIR)/build-mac-gl/gl_rsurf.o \
    $(BUILDDIR)/build-mac-gl/cl_screen.o \
    $(BUILDDIR)/build-mac-gl/gl_warp.o \
    $(BUILDDIR)/build-mac-gl/gl_texture.o \
\
    $(BUILDDIR)/build-mac-gl/cl_cam.o \
    $(BUILDDIR)/build-mac-gl/cl_cmd.o \
    $(BUILDDIR)/build-mac-gl/cl_demo.o \
    $(BUILDDIR)/build-mac-gl/cl_ents.o \
    $(BUILDDIR)/build-mac-gl/cl_main.o \
    $(BUILDDIR)/build-mac-gl/cl_parse.o \
    $(BUILDDIR)/build-mac-gl/cl_pred.o \
    $(BUILDDIR)/build-mac-gl/cl_slist.o \
    $(BUILDDIR)/build-mac-gl/cl_tent.o \
    $(BUILDDIR)/build-mac-gl/cl_view.o \
\
    $(BUILDDIR)/build-mac-gl/cmd.o \
    $(BUILDDIR)/build-mac-gl/common.o \
    $(BUILDDIR)/build-mac-gl/com_msg.o \
    $(BUILDDIR)/build-mac-gl/console.o \
    $(BUILDDIR)/build-mac-gl/crc.o \
    $(BUILDDIR)/build-mac-gl/cvar.o \
    $(BUILDDIR)/build-mac-gl/image.o \
    $(BUILDDIR)/build-mac-gl/mathlib.o \
    $(BUILDDIR)/build-mac-gl/sha1.o \
    $(BUILDDIR)/build-mac-gl/mdfour.o \
    $(BUILDDIR)/build-mac-gl/menu.o \
    $(BUILDDIR)/build-mac-gl/mvd_utils.o \
    $(BUILDDIR)/build-mac-gl/sbar.o \
    $(BUILDDIR)/build-mac-gl/skin.o \
    $(BUILDDIR)/build-mac-gl/teamplay.o \
    $(BUILDDIR)/build-mac-gl/version.o \
    $(BUILDDIR)/build-mac-gl/wad.o \
    $(BUILDDIR)/build-mac-gl/zone.o \
    $(BUILDDIR)/build-mac-gl/parser.o \
\
    $(BUILDDIR)/build-mac-gl/ignore.o \
    $(BUILDDIR)/build-mac-gl/logging.o \
    $(BUILDDIR)/build-mac-gl/fragstats.o \
    $(BUILDDIR)/build-mac-gl/match_tools.o \
    $(BUILDDIR)/build-mac-gl/utils.o \
    $(BUILDDIR)/build-mac-gl/movie.o \
\
    $(BUILDDIR)/build-mac-gl/fchecks.o \
    $(BUILDDIR)/build-mac-gl/auth.o \
    $(BUILDDIR)/build-mac-gl/Ctrl.o \
    $(BUILDDIR)/build-mac-gl/Ctrl_EditBox.o \
    $(BUILDDIR)/build-mac-gl/Ctrl_Tab.o \
    $(BUILDDIR)/build-mac-gl/Ctrl_PageViewer.o \
    $(BUILDDIR)/build-mac-gl/EX_FunNames.o \
    $(BUILDDIR)/build-mac-gl/EX_browser.o \
    $(BUILDDIR)/build-mac-gl/EX_browser_net.o \
    $(BUILDDIR)/build-mac-gl/EX_browser_ping.o \
    $(BUILDDIR)/build-mac-gl/EX_browser_sources.o \
    $(BUILDDIR)/build-mac-gl/EX_misc.o \
    $(BUILDDIR)/build-mac-gl/common_draw.o \
    $(BUILDDIR)/build-mac-gl/hud.o \
    $(BUILDDIR)/build-mac-gl/hud_common.o \
    $(BUILDDIR)/build-mac-gl/modules.o \
    $(BUILDDIR)/build-mac-gl/rulesets.o \
    $(BUILDDIR)/build-mac-gl/config_manager.o \
    $(BUILDDIR)/build-mac-gl/mp3_player.o \
    $(BUILDDIR)/build-mac-gl/fmod.o \
    $(BUILDDIR)/build-mac-gl/localtime_linux.o \
    $(BUILDDIR)/build-mac-gl/collision.o \
    $(BUILDDIR)/build-mac-gl/cl_collision.o \
    $(BUILDDIR)/build-mac-gl/vx_camera.o \
    $(BUILDDIR)/build-mac-gl/vx_coronas.o \
    $(BUILDDIR)/build-mac-gl/vx_motiontrail.o \
    $(BUILDDIR)/build-mac-gl/vx_stuff.o \
    $(BUILDDIR)/build-mac-gl/vx_tracker.o \
    $(BUILDDIR)/build-mac-gl/vx_vertexlights.o \
 \
    $(BUILDDIR)/build-mac-gl/xml_test.o \
    $(BUILDDIR)/build-mac-gl/xsd.o \
    $(BUILDDIR)/build-mac-gl/xsd_command.o \
    $(BUILDDIR)/build-mac-gl/xsd_document.o \
    $(BUILDDIR)/build-mac-gl/xsd_variable.o \
    $(BUILDDIR)/build-mac-gl/document_rendering.o \
    $(BUILDDIR)/build-mac-gl/help.o \
    $(BUILDDIR)/build-mac-gl/help_browser.o \
    $(BUILDDIR)/build-mac-gl/help_files.o \
    $(BUILDDIR)/build-mac-gl/EX_FileList.o

$(BUILDDIR)/ezquake-gl.mac : $(GLQWCL_MAC_OBJS)
	$(CC) $(CFLAGS) -o $@ $(GLQWCL_MAC_OBJS) \
	$(LDFLAGS) $(GL_LDFLAGS) 

$(BUILDDIR)/build-mac-gl/host.o :			$(SOURCE_DIR)/host.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/snd_dma.o :		$(SOURCE_DIR)/snd_dma.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/snd_mem.o :		$(SOURCE_DIR)/snd_mem.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/snd_mix.o :		$(SOURCE_DIR)/snd_mix.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cl_demo.o :		$(SOURCE_DIR)/cl_demo.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cl_ents.o :		$(SOURCE_DIR)/cl_ents.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cl_input.o :		$(SOURCE_DIR)/cl_input.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cl_main.o :		$(SOURCE_DIR)/cl_main.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cl_parse.o :		$(SOURCE_DIR)/cl_parse.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cl_pred.o :		$(SOURCE_DIR)/cl_pred.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cl_tent.o :		$(SOURCE_DIR)/cl_tent.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cl_cam.o :			$(SOURCE_DIR)/cl_cam.c
	$(DO_GL_CC)   

$(BUILDDIR)/build-mac-gl/cl_view.o :		$(SOURCE_DIR)/cl_view.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cl_cmd.o :			$(SOURCE_DIR)/cl_cmd.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cl_slist.o :		$(SOURCE_DIR)/cl_slist.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/r_part.o :			$(SOURCE_DIR)/r_part.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/gl_draw.o :		$(SOURCE_DIR)/gl_draw.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/gl_md3.o :			$(SOURCE_DIR)/gl_md3.c
	$(DO_GL_CC) 

$(BUILDDIR)/build-mac-gl/gl_mesh.o :		$(SOURCE_DIR)/gl_mesh.c
	$(DO_GL_CC) 

$(BUILDDIR)/build-mac-gl/gl_model.o :		$(SOURCE_DIR)/gl_model.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/gl_ngraph.o :		$(SOURCE_DIR)/gl_ngraph.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/gl_refrag.o :		$(SOURCE_DIR)/gl_refrag.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/gl_rlight.o :		$(SOURCE_DIR)/gl_rlight.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/gl_rpart.o :		$(SOURCE_DIR)/gl_rpart.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/gl_rmain.o :		$(SOURCE_DIR)/gl_rmain.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/gl_rmisc.o :		$(SOURCE_DIR)/gl_rmisc.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/gl_rsurf.o :		$(SOURCE_DIR)/gl_rsurf.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cl_screen.o :		$(SOURCE_DIR)/cl_screen.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/gl_warp.o :		$(SOURCE_DIR)/gl_warp.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cmd.o :			$(SOURCE_DIR)/cmd.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/common.o :			$(SOURCE_DIR)/common.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/com_msg.o :		$(SOURCE_DIR)/com_msg.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/console.o :		$(SOURCE_DIR)/console.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/crc.o :			$(SOURCE_DIR)/crc.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/version.o :		$(SOURCE_DIR)/version.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cvar.o :			$(SOURCE_DIR)/cvar.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/keys.o :			$(SOURCE_DIR)/keys.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/mathlib.o :		$(SOURCE_DIR)/mathlib.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/menu.o :			$(SOURCE_DIR)/menu.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/mvd_utils.o :		$(SOURCE_DIR)/mvd_utils.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/net_chan.o :		$(SOURCE_DIR)/net_chan.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/net_udp.o :		$(SOURCE_DIR)/net_udp.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/pmove.o :			$(SOURCE_DIR)/pmove.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/pmovetst.o :		$(SOURCE_DIR)/pmovetst.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/sbar.o :			$(SOURCE_DIR)/sbar.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/skin.o :			$(SOURCE_DIR)/skin.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/image.o :			$(SOURCE_DIR)/image.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/teamplay.o :		$(SOURCE_DIR)/teamplay.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/sv_ccmds.o :		$(SOURCE_DIR)/sv_ccmds.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/sv_save.o :		$(SOURCE_DIR)/sv_save.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/sv_ents.o :		$(SOURCE_DIR)/sv_ents.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/sv_init.o :		$(SOURCE_DIR)/sv_init.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/sv_main.o :		$(SOURCE_DIR)/sv_main.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/sv_move.o :		$(SOURCE_DIR)/sv_move.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/sv_nchan.o :		$(SOURCE_DIR)/sv_nchan.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/sv_phys.o :		$(SOURCE_DIR)/sv_phys.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/sv_send.o :		$(SOURCE_DIR)/sv_send.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/sv_user.o :		$(SOURCE_DIR)/sv_user.c
	$(DO_GL_CC)  

$(BUILDDIR)/build-mac-gl/sv_world.o :		$(SOURCE_DIR)/sv_world.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/pr_edict.o :		$(SOURCE_DIR)/pr_edict.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/pr_exec.o :		$(SOURCE_DIR)/pr_exec.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/pr_cmds.o :		$(SOURCE_DIR)/pr_cmds.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/ignore.o :			$(SOURCE_DIR)/ignore.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/logging.o :		$(SOURCE_DIR)/logging.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/fragstats.o :		$(SOURCE_DIR)/fragstats.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/match_tools.o :		$(SOURCE_DIR)/match_tools.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/utils.o :			$(SOURCE_DIR)/utils.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/movie.o :			$(SOURCE_DIR)/movie.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/fchecks.o :		$(SOURCE_DIR)/fchecks.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/fmod.o :			$(SOURCE_DIR)/fmod.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/auth.o :			$(SOURCE_DIR)/auth.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/Ctrl.o :			$(SOURCE_DIR)/Ctrl.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/Ctrl_EditBox.o :		$(SOURCE_DIR)/Ctrl_EditBox.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/Ctrl_Tab.o :		$(SOURCE_DIR)/Ctrl_Tab.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/Ctrl_PageViewer.o :	$(SOURCE_DIR)/Ctrl_PageViewer.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/EX_browser.o :		$(SOURCE_DIR)/EX_browser.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/EX_browser_net.o :		$(SOURCE_DIR)/EX_browser_net.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/EX_browser_ping.o :	$(SOURCE_DIR)/EX_browser_ping.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/EX_browser_sources.o :	$(SOURCE_DIR)/EX_browser_sources.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/EX_misc.o :		$(SOURCE_DIR)/EX_misc.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/common_draw.o :		$(SOURCE_DIR)/common_draw.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/hud.o :			$(SOURCE_DIR)/hud.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/hud_common.o :		$(SOURCE_DIR)/hud_common.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/EX_FunNames.o :		$(SOURCE_DIR)/EX_FunNames.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/collision.o :		$(SOURCE_DIR)/collision.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/cl_collision.o :		$(SOURCE_DIR)/cl_collision.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/vx_camera.o :		$(SOURCE_DIR)/vx_camera.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/vx_coronas.o :		$(SOURCE_DIR)/vx_coronas.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/vx_motiontrail.o :		$(SOURCE_DIR)/vx_motiontrail.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/vx_stuff.o :		$(SOURCE_DIR)/vx_stuff.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/vx_tracker.o :		$(SOURCE_DIR)/vx_tracker.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/vx_vertexlights.o :	$(SOURCE_DIR)/vx_vertexlights.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/rulesets.o :		$(SOURCE_DIR)/rulesets.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/config_manager.o :		$(SOURCE_DIR)/config_manager.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/mp3_player.o :		$(SOURCE_DIR)/mp3_player.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/modules.o :		$(SOURCE_DIR)/modules.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/gl_texture.o :		$(SOURCE_DIR)/gl_texture.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/sha1.o :			$(SOURCE_DIR)/sha1.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/mdfour.o :			$(SOURCE_DIR)/mdfour.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/wad.o :			$(SOURCE_DIR)/wad.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/zone.o :			$(SOURCE_DIR)/zone.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/parser.o :			$(SOURCE_DIR)/parser.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/localtime_linux.o :	$(SOURCE_DIR)/localtime_linux.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/xml_test.o :		$(SOURCE_DIR)/xml_test.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/xsd.o :			$(SOURCE_DIR)/xsd.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/xsd_command.o :		$(SOURCE_DIR)/xsd_command.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/xsd_document.o :		$(SOURCE_DIR)/xsd_document.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/xsd_variable.o :		$(SOURCE_DIR)/xsd_variable.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/document_rendering.o :	$(SOURCE_DIR)/document_rendering.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/help.o :			$(SOURCE_DIR)/help.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/help_browser.o :		$(SOURCE_DIR)/help_browser.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/help_files.o :		$(SOURCE_DIR)/help_files.c
	$(DO_GL_CC)

$(BUILDDIR)/build-mac-gl/EX_FileList.o :		$(SOURCE_DIR)/EX_FileList.c
	$(DO_GL_CC)

#############################################################################
# MISC
#############################################################################

clean: clean-debug clean-release

clean-debug:
	$(MAKE) cleanfunc BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)"

clean-release:
	$(MAKE) cleanfunc BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(DEBUG_CFLAGS)"

cleanfunc:
	-rm -f $(QWCL_OBJS) $(QWCL_AS_OBJS) $(QWCL_X11_OBJS) $(GLQWCL_X11_OBJS) \
	$(GLQWCL_OBJS) $(GLQWCL_AS_OBJS) $(QWCL_SVGA_OBJS) $(QWCL_SVGA_AS_OBJS) \
	$(GLQWCL_MAC_OBJS)

