### ezQuake Makefile based on Q2PRO ###

ifdef EZ_CONFIG_FILE
    -include $(EZ_CONFIG_FILE)
else
    -include .config
endif

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
JSON2C ?= ./json2c.sh

CFLAGS ?= -O2 -Wall -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-strict-aliasing -Werror=strict-prototypes -Werror=old-style-definition -g -MMD $(INCLUDES)
RCFLAGS ?=
LDFLAGS ?=
LIBS ?=

#Temporarily disable tree vectorization optimization enabled at O3 due to gcc bug
CFLAGS += -fno-tree-vectorize

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
    	CFLAGS += -DX11_GAMMA_WORKAROUND
        LDFLAGS_c += -Wl,--no-undefined
    endif

    ifeq ($(SYS),Darwin)
        # 10.11 El Capitan does not search for header files here by default
        CFLAGS_c += -I/usr/local/include -mmacosx-version-min=10.8

        # For re-link/deploy dynamic libraries
        LDFLAGS_c += -headerpad_max_install_names -mmacosx-version-min=10.8

        # From  10.10 at least, expat is a system library
        EXPAT_CFLAGS =
        EXPAT_LIBS = -lexpat

        CURL_CFLAGS =
        CURL_LIBS = -lcurl
    endif
endif

BUILD_DEFS := -DCPUSTRING='"$(CPU)"'
BUILD_DEFS += -DBUILDSTRING='"$(SYS)"'

VER_DEFS := -DREVISION=$(REV)
VER_DEFS += -DVERSION='"$(VER)"'

SDL2_CFLAGS ?= $(shell sdl2-config --cflags)
SDL2_LIBS ?= $(shell sdl2-config --libs)

ifdef TRAVIS_BUILD
    BUILD_DEFS += -DTRAVIS_BUILD
endif

CFLAGS_c += $(BUILD_DEFS) $(VER_DEFS) $(PATH_DEFS) $(SDL2_CFLAGS) -DNDEBUG -DJSS_CAM -DUSE_PR2 -DWITH_NQPROGS -DUSE_SDL2 -DWITH_ZIP
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

JPEG_CFLAGS ?= -DWITH_JPEG
JPEG_LIBS ?= -ljpeg
CFLAGS_c += $(JPEG_CFLAGS)
LIBS_c += $(JPEG_LIBS)

CURL_CFLAGS ?= $(shell pkg-config libcurl --cflags)
CURL_LIBS ?= $(shell pkg-config libcurl --libs)
CFLAGS_c += $(CURL_CFLAGS)
LIBS_c += $(CURL_LIBS)

JANSSON_CFLAGS ?= $(shell pkg-config jansson --cflags)
JANSSON_LIBS ?= $(shell pkg-config jansson --libs)
CFLAGS += $(JANSSON_CFLAGS)
LIBS_c += $(JANSSON_LIBS)

SPEEX_LIBS ?= $(shell pkg-config speex --libs) $(shell pkg-config speexdsp --libs)
ifdef SPEEX_LIBS
    CFLAGS_c += -DWITH_SPEEX
endif
LIBS_c += $(SPEEX_LIBS)

ifndef CONFIG_WINDOWS
    ifeq ($(shell pkg-config --exists freetype2 && echo 1),1)
        FREETYPE_CFLAGS ?= $(shell pkg-config freetype2 --cflags)
        FREETYPE_LIBS ?= $(shell pkg-config freetype2 --libs)
    endif
endif

ifdef FREETYPE_LIBS
    CFLAGS_c += -DEZ_FREETYPE_SUPPORT
    LIBS_c += $(FREETYPE_LIBS)
    CFLAGS += $(FREETYPE_CFLAGS)
endif

# windres needs special quoting...
RCFLAGS_c += -DREVISION=$(REV) -DVERSION='\"$(VER)\"'

### Object Files ###

COMMON_OBJS := \
    cmodel.o           \
    cmd.o              \
    com_msg.o          \
    common.o           \
    crc.o              \
    cvar.o             \
    fs.o               \
    vfs_os.o           \
    vfs_pak.o          \
    vfs_zip.o          \
    vfs_tcp.o          \
    vfs_gzip.o         \
    vfs_doomwad.o      \
    vfs_mmap.o         \
    vfs_tar.o          \
    hash.o             \
    host.o             \
    mathlib.o          \
    md4.o              \
    net.o              \
    net_chan.o         \
    q_shared.o         \
    version.o          \
    zone.o             \
    pmove.o            \
    pmovetst.o

SERVER_OBJS := \
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

HELP_OBJS := \
    $(patsubst help_%.json,help_%.o,$(wildcard help_*.json))

GLSL_OBJS := \
    $(patsubst glsl/%.glsl,glsl_%.glsl.o,$(wildcard glsl/*.glsl))

MODERN_OPENGL_OBJS := \
    glm_aliasmodel.o \
    glm_brushmodel.o \
    glm_draw.o \
    glm_framebuffer.o \
    glm_lightmaps.o \
    glm_md3.o \
    glm_misc.o \
    glm_particles.o \
    glm_performance.o \
    glm_program.o \
    glm_main.o \
    glm_rmain.o \
    glm_rsurf.o \
    glm_sdl.o \
    glm_sky.o \
    glm_sprite.o \
    glm_sprite3d.o \
    glm_texture_arrays.o \
    glm_vao.o

CLASSIC_OPENGL_OBJS := \
    glc_aliasmodel.o \
    glc_bloom.o \
    glc_brushmodel.o \
    glc_draw.o \
    glc_fog.o \
    glc_lightmaps.o \
    glc_matrix.o \
    glc_md3.o \
    glc_misc.o \
    glc_particles.o \
    glc_performance.o \
    glc_sdl.o \
    glc_sky.o \
    glc_sprite.o \
    glc_sprite3d.o \
    glc_state.o \
    glc_surf.o \
    glc_turb_surface.o \
    glc_vao.o \
    glc_warp.o

COMMON_OPENGL_OBJS := \
    gl_aliasmodel.o \
    gl_aliasmodel_md3.o \
    gl_brushmodel.o \
    gl_buffers.o \
    gl_debug.o \
    gl_drawcall_wrappers.o \
    gl_framebuffer.o \
    gl_misc.o \
    gl_sprite3d.o \
    gl_state.o \
    gl_texture.o \
    gl_texture_functions.o \
    vid_common_gl.o

COMMON_RENDERER_OBJS := \
    r_aliasmodel.o \
    r_aliasmodel_md3.o \
    r_aliasmodel_mesh.o \
    r_aliasmodel_skins.o \
    r_atlas.o \
    r_bloom.o \
    r_brushmodel.o \
    r_brushmodel_bspx.o \
    r_brushmodel_sky.o \
    r_brushmodel_surfaces.o \
    r_brushmodel_textures.o \
    r_brushmodel_warpsurfaces.o \
    r_buffers.o \
    r_chaticons.o \
    r_draw.o \
    r_draw_charset.o \
    r_draw_circle.o \
    r_draw_image.o \
    r_draw_line.o \
    r_draw_polygon.o \
    r_hud.o \
    r_lightmaps.o \
    r_main.o \
    r_matrix.o \
    r_misc.o \
    r_model.o \
    r_netgraph.o \
    r_palette.o \
    r_part.o \
    r_particles_qmb.o \
    r_particles_qmb_spawn.o \
    r_performance.o \
    r_refrag.o \
    r_rlight.o \
    r_rmain.o \
    r_rmisc.o \
    r_screenshot.o \
    r_sprite3d.o \
    r_sprites.o \
    r_states.o \
    r_texture.o \
    r_texture_cvars.o \
    r_texture_load.o \
    r_texture_util.o \
    vx_camera.o \
    vx_coronas.o \
    vx_motiontrail.o \
    vx_stuff.o \
    vx_vertexlights.o

HUD_OBJS := \
    hud.o \
    hud_262.o \
    hud_common.o \
    hud_editor.o \
    hud_radar.o \
    hud_speed.o \
    hud_teaminfo.o \
    hud_weapon_stats.o \
    hud_autoid.o \
    hud_clock.o \
    hud_ammo.o \
    hud_items.o \
    hud_mp3.o \
    hud_net.o \
    hud_guns.o \
    hud_groups.o \
    hud_armor.o \
    hud_health.o \
    hud_gamesummary.o \
    hud_face.o \
    hud_frags.o \
    hud_tracking.o \
    hud_scores.o \
    hud_performance.o

OBJS_c := \
    $(COMMON_OBJS) \
    $(HELP_OBJS) \
    $(GLSL_OBJS) \
    $(HUD_OBJS) \
    $(COMMON_RENDERER_OBJS) \
    $(COMMON_OPENGL_OBJS) \
    $(MODERN_OPENGL_OBJS) \
    $(CLASSIC_OPENGL_OBJS) \
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
    EX_qtvlist.o \
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
    ignore.o \
    image.o \
    irc_filter.o \
    irc.o \
    keys.o \
    logging.o \
    match_tools.o \
    match_tools_challenge.o \
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
    rulesets.o \
    sbar.o \
    settings_page.o \
    sha1.o \
    skin.o \
    snd_main.o \
    snd_mem.o \
    snd_mix.o \
    snd_ov.o \
    stats_grid.o \
    teamplay.o \
    teamplay_locfiles.o \
    tp_msgs.o \
    tp_triggers.o \
    textencoding.o \
    utils.o \
    vx_tracker.o \
    wad.o \
    xsd.o \
    xsd_document.o \
    collision.o \
    vid_vsync.o \
    cd_null.o \
    vid_sdl2.o \
    sys_sdl2.o \
    in_sdl2.o \
    cl_multiview.o \
    snd_voip.o \
    cl_screenshot.o \
    fonts.o

### Configuration Options ###

ifndef CLIENT_ONLY
    OBJS_c += $(SERVER_OBJS)
else
    CFLAGS += -DCLIENTONLY
endif

ifdef CONFIG_WINDOWS
    OBJS_c += \
        movie_avi.o \
        localtime_win.o \
        sys_win.o \
        winquake.o
    LIBS_c += -lopengl32 -lws2_32 -lwinmm -lpthread
else
    OBJS_c += \
    	localtime_posix.o \
	sys_posix.o \
    	linux_signals.o

    LIBS_c += -lm

    ifeq ($(SYS),Darwin)
        LIBS_c += -framework Foundation -framework OpenGL -framework IOKit -framework CoreServices
        OBJS_c += in_osx.o sys_osx.o
    else
        LIBS_c += -lGL -lpthread
    endif

    ifeq ($(SYS),Linux)
        LIBS_c += -lXxf86vm
    endif

    ifneq ($(SYS),FreeBSD)
        ifneq ($(SYS),OpenBSD)
            LIBS_c += -ldl
	endif
    endif
endif

ifdef CONFIG_OGG
    OGG_CFLAGS ?= $(shell pkg-config vorbisfile --cflags) -DWITH_OGG_VORBIS
    OGG_LIBS ?= $(shell pkg-config vorbisfile --libs)
    CFLAGS_c += $(OGG_CFLAGS)
    LIBS_c += $(OGG_LIBS)
endif

ifeq ($(USE_SYSTEM_MINIZIP),1)
	MINIZIP_CFLAGS ?= $(shell pkg-config --cflags minizip)
	MINIZIP_LIBS ?= $(shell pkg-config --libs minizip)
	CFLAGS_c += $(MINIZIP_CFLAGS)
	LIBS_c += $(MINIZIP_LIBS)
else
	OBJS_c += minizip/ioapi.o minizip/unzip.o
	CFLAGS_c += -Iminizip
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

$(BUILD_c)/glsl_%.glsl.o: glsl/%.glsl
	$(E) [GLSL] $@
	$(Q)$(JSON2C) $< > $(BUILD_c)/$*.c
	$(Q)$(CC) -c $(CFLAGS) $(CFLAGS_c) -o $@ $(BUILD_c)/$*.c

$(BUILD_c)/%.o: %.json
	$(E) [JS] $@
	$(Q)$(JSON2C) $< > $(BUILD_c)/$*.c
	$(Q)$(CC) -c $(CFLAGS) $(CFLAGS_c) -o $@ $(BUILD_c)/$*.c

$(BUILD_c)/%.o: %.c
	$(E) [CC] $@
	$(Q)$(MKDIR) $(@D)
	$(Q)$(CC) -c $(CFLAGS) $(CFLAGS_c) -o $@ $<

$(BUILD_c)/%.o: %.m
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
