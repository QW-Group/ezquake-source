### ezQuake Makefile based on Q2PRO ###

-include .config

ifdef CONFIG_WINDOWS
    CPU ?= x86
    SYS ?= Win32
else
    ifndef CPU
        CPU := $(shell uname -m | sed -e s/i.86/i386/ -e s/amd64/x86_64/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/)
    endif
    ifndef SYS
        SYS := $(shell uname -s)
    endif
endif

ifndef REV
    REV := $(shell ./version.sh --revision)
endif
ifndef VER
    VER := $(shell ./version.sh --version)
endif

LSYS := $(shell echo $(SYS) | tr A-Z a-z)
CC ?= gcc
WINDRES ?= windres
STRIP ?= strip
RM ?= rm -f
RMDIR ?= rm -rf
MKDIR ?= mkdir -p

CFLAGS ?= -O2 -Wall -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-strict-aliasing -g -MMD $(INCLUDES)
RCFLAGS ?=
LDFLAGS ?=
LIBS ?=

CFLAGS_c :=
RCFLAGS_c :=
LDFLAGS_c :=

ifdef CONFIG_WINDOWS
    LDFLAGS_c += -mwindows

    # Mark images as DEP and ASLR compatible on x86 Windows
    ifeq ($(CPU),x86)
        LDFLAGS_c += -Wl,--nxcompat,--dynamicbase
    endif
else
    # Hide ELF symbols by default
    CFLAGS_c += -fvisibility=hidden

    # Resolve all symbols at link time
    ifeq ($(SYS),Linux)
        LDFLAGS_c += -Wl,--no-undefined
    endif
endif

BUILD_DEFS := -DCPUSTRING='"$(CPU)"'
BUILD_DEFS += -DBUILDSTRING='"$(SYS)"'

VER_DEFS := -DREVISION=$(REV)
VER_DEFS += -DVERSION='"$(VER)"'

SDL2_CFLAGS ?= $(shell sdl2-config --cflags)
SDL2_LIBS ?= $(shell sdl2-config --libs)

CFLAGS_c += $(BUILD_DEFS) $(VER_DEFS) $(PATH_DEFS) $(SDL2_CFLAGS) -DJSS_CAM -DUSE_PR2 -DWITH_NQPROGS -DUSE_SDL2 -DWITH_ZIP
LIBS_c += $(SDL2_LIBS)

# built-in requirements
ZLIB_CFLAGS ?= -DWITH_ZLIB
ZLIB_LIBS ?= -lz
CFLAGS_c += $(ZLIB_CFLAGS)
LIBS_c += $(ZLIB_LIBS)

PCRE_CFLAGS ?= $(shell pkg-config libpcre --cflags)
PCRE_LIBS ?= $(shell pkg-config libpcre --libs)
CFLAGS_c += $(PCRE_CFLAGS)
LIBS_c += $(PCRE_LIBS)

EXPAT_CFLAGS ?= $(shell pkg-config expat --cflags)
EXPAT_LIBS ?= $(shell pkg-config expat --libs)
CFLAGS_c += $(EXPAT_CFLAGS)
LIBS_c += $(EXPAT_LIBS)

PNG_CFLAGS ?= $(shell pkg-config libpng --cflags) -DWITH_PNG -D__Q_PNG14__
PNG_LIBS ?= $(shell pkg-config libpng --libs)
CFLAGS_c += $(PNG_CFLAGS)
LIBS_c += $(PNG_LIBS)

CURL_CFLAGS ?= $(shell pkg-config libcurl --cflags)
CURL_LIBS ?= $(shell pkg-config libcurl --libs)
CFLAGS_c += $(CURL_CFLAGS)
LIBS_c += $(CURL_LIBS)

# windres needs special quoting...
RCFLAGS_c += -DREVISION=$(REV) -DVERSION='\"$(VER)\"'

### Object Files ###

COMMON_OBJS := \
    cmodel.o		\
    cmd.o		\
    com_msg.o		\
    common.o		\
    crc.o		\
    cvar.o		\
    fs.o		\
    vfs_os.o		\
    vfs_pak.o		\
    vfs_zip.o		\
    vfs_tcp.o		\
    vfs_gzip.o		\
    vfs_doomwad.o	\
    vfs_mmap.o		\
    vfs_tar.o		\
    hash.o		\
    host.o		\
    mathlib.o		\
    md4.o		\
    net.o		\
    net_chan.o		\
    q_shared.o		\
    version.o		\
    zone.o		\
    zone2.o

# temporary
SERVER_OBJS := \
    pmove.o \
    pmovetst.o \
    pr_cmds.o \
    pr_edict.o \
    pr_exec.o \
    pr2_cmds.o \
    pr2_edict.o \
    pr2_exec.o \
    pr2_vm.o \
    sv_ccmds.o \
    sv_ents.o \
    sv_init.o \
    sv_main.o \
    sv_master.o \
    sv_move.o \
    sv_nchan.o \
    sv_phys.o \
    sv_save.o \
    sv_send.o \
    sv_user.o \
    sv_world.o \
    sv_demo.o \
    sv_demo_misc.o \
    sv_demo_qtv.o \
    sv_login.o \
    sv_mod_frags.o

OBJS_c := \
    $(COMMON_OBJS) \
    $(SERVER_OBJS) \
    ioapi.o \
    unzip.o \
    Ctrl.o \
    Ctrl_EditBox.o \
    Ctrl_PageViewer.o \
    Ctrl_ScrollBar.o \
    Ctrl_Tab.o \
    EX_FileList.o \
    EX_browser.o \
    EX_browser_net.o \
    EX_browser_pathfind.o \
    EX_browser_ping.o \
    EX_browser_qtvlist.o \
    EX_browser_sources.o \
    ez_controls.o \
    ez_scrollbar.o \
    ez_scrollpane.o \
    ez_label.o \
    ez_slider.o \
    ez_button.o \
    ez_window.o \
    cl_cam.o \
    cl_cmd.o \
    cl_demo.o \
    cl_nqdemo.o \
    cl_ents.o \
    cl_input.o \
    cl_main.o \
    cl_parse.o \
    cl_pred.o \
    cl_screen.o \
    cl_slist.o \
    cl_tcl.o \
    cl_tent.o \
    cl_view.o \
    common_draw.o \
    console.o \
    config_manager.o \
    demo_controls.o \
    document_rendering.o \
    fchecks.o \
    fmod.o \
    fragstats.o \
    help.o \
    help_files.o \
    hud.o \
    hud_common.o \
    hud_editor.o \
    ignore.o \
    image.o \
    irc_filter.o \
    irc.o \
    keys.o \
    logging.o \
    match_tools.o \
    menu.o \
    menu_demo.o \
    menu_ingame.o \
    menu_mp3player.o \
    menu_multiplayer.o \
    menu_options.o \
    menu_proxy.o \
    modules.o \
    movie.o \
    mp3_player.o \
    mp3_audacious.o \
    mp3_xmms.o \
    mp3_xmms2.o \
    mp3_mpd.o \
    mp3_winamp.o \
    mvd_autotrack.o \
    mvd_utils.o \
    mvd_xmlstats.o \
    parser.o \
    qtv.o \
    r_part.o \
    rulesets.o \
    sbar.o \
    settings_page.o \
    sha1.o \
    skin.o \
    snd_dma.o \
    snd_mem.o \
    snd_mix.o \
    snd_ov.o \
    stats_grid.o \
    teamplay.o \
    tp_msgs.o \
    tp_triggers.o \
    textencoding.o \
    utils.o \
    vx_tracker.o \
    wad.o \
    xsd.o \
    xsd_command.o \
    xsd_document.o \
    xsd_variable.o \
    collision.o \
    gl_draw.o \
    gl_bloom.o \
    gl_md3.o \
    gl_mesh.o \
    gl_model.o \
    gl_ngraph.o \
    gl_refrag.o \
    gl_rlight.o \
    gl_rmain.o \
    gl_rmisc.o \
    gl_rpart.o \
    gl_rsurf.o \
    gl_texture.o \
    gl_warp.o \
    vx_camera.o \
    vx_coronas.o \
    vx_motiontrail.o \
    vx_stuff.o \
    vx_vertexlights.o \
    vid_common_gl.o \
    cd_null.o \
    vid_sdl2.o \
    snd_sdl2.o \
    sys_sdl2.o \
    in_sdl2.o

### Configuration Options ###

ifdef CONFIG_WINDOWS
    OBJS_c += \
	movie_avi.o \
	localtime_win.o \
	sys_win.o \
	winquake.o
    LIBS_c += -lopengl32 -lws2_32 -lwinmm
else
    OBJS_c += \
    	localtime_posix.o \
	sys_posix.o \
    	linux_signals.o

    LIBS_c += -lm

    ifeq ($(SYS),Darwin)
        LIBS_c += -framework OpenGL -ldl
    else
        LIBS_c += -lGL
    endif

    ifneq ($(SYS),FreeBSD)
        LIBS_c += -ldl
    endif
endif

ifdef CONFIG_OGG
    OGG_CFLAGS ?= $(shell pkg-config vorbisfile --cflags) -DWITH_OGG_VORBIS
    OGG_LIBS ?= $(shell pkg-config vorbisfile --libs)
    CFLAGS_c += $(OGG_CFLAGS)
    LIBS_c += $(OGG_LIBS)
endif

ifdef CONFIG_JPEG
    JPEG_CFLAGS ?= -DWITH_JPEG
    JPEG_LIBS ?= -ljpeg
    CFLAGS_c += $(JPEG_CFLAGS)
    LIBS_c += $(JPEG_LIBS)
endif

### Targets ###

ifdef CONFIG_WINDOWS
    TARG_c := ezquake.exe
else
    TARG_c := ezquake-$(LSYS)-$(CPU)
endif

all: $(TARG_c)

default: all

.PHONY: all default clean strip

# Define V=1 to show command line.
ifdef V
    Q :=
    E := @true
else
    Q := @
    E := @echo
endif

# Temporary build directories
BUILD_c := .ezquake

# Rewrite paths to build directories
OBJS_c := $(patsubst %,$(BUILD_c)/%,$(OBJS_c))

DEPS_c := $(OBJS_c:.o=.d)

-include $(DEPS_c)

clean:
	$(E) [CLEAN]
	$(Q)$(RM) $(TARG_c)
	$(Q)$(RMDIR) $(BUILD_c)

strip: $(TARG_c)
	$(E) [STRIP]
	$(Q)$(STRIP) $(TARG_c)

# ------

$(BUILD_c)/%.o: %.c
	$(E) [CC] $@
	$(Q)$(MKDIR) $(@D)
	$(Q)$(CC) -c $(CFLAGS) $(CFLAGS_c) -o $@ $<

$(BUILD_c)/%.o: %.rc
	$(E) [RC] $@
	$(Q)$(MKDIR) $(@D)
	$(Q)$(WINDRES) $(RCFLAGS) $(RCFLAGS_c) -o $@ $<

$(TARG_c): $(OBJS_c)
	$(E) [LD] $@
	$(Q)$(MKDIR) $(@D)
	$(Q)$(CC) $(LDFLAGS) $(LDFLAGS_c) -o $@ $(OBJS_c) $(LIBS) $(LIBS_c)
