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

CFLAGS ?= -std=gnu89 -O2 -Wall -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-strict-aliasing -Werror=strict-prototypes -Werror=old-style-definition -g -MMD $(INCLUDES)
RCFLAGS ?=
LDFLAGS ?=
LIBS ?=

CFLAGS_c :=
RCFLAGS_c :=
LDFLAGS_c :=

SRC_DIR = src

ifneq (,$(wildcard $(SRC_DIR)/qwprot/src/*.h))
    INCLUDES = -I$(SRC_DIR)/qwprot/src
else
    $(error qwprot submodule missing, try initializing submodules with "git submodule update --init --recursive --remote")
endif

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
        #CFLAGS += -DX11_GAMMA_WORKAROUND
        LDFLAGS_c += -Wl,--no-undefined
    endif

    ifeq ($(SYS),Darwin)
        # 10.11 El Capitan does not search for header files here by default
        CFLAGS_c += -I/usr/local/include

        # For re-link/deploy dynamic libraries
        LDFLAGS_c += -headerpad_max_install_names

        # Cross-compiling support
        ifneq ($(DARWIN_TARGET),)
            CFLAGS_c += -target $(DARWIN_TARGET)
            LDFLAGS_c += -target $(DARWIN_TARGET)
        else
            CFLAGS_c += -mmacosx-version-min=11.0
            LDFLAGS_c += -mmacosx-version-min=11.0
        endif

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

PCRE2_CFLAGS ?= $(shell pkg-config libpcre2-8 --cflags)
PCRE2_LIBS ?= $(shell pkg-config libpcre2-8 --libs)
CFLAGS_c += $(PCRE2_CFLAGS) -DPCRE2_CODE_UNIT_WIDTH=8
LIBS_c += $(PCRE2_LIBS)

EXPAT_CFLAGS ?= $(shell pkg-config expat --cflags)
EXPAT_LIBS ?= $(shell pkg-config expat --libs)
CFLAGS_c += $(EXPAT_CFLAGS)
LIBS_c += $(EXPAT_LIBS)

PNG_CFLAGS ?= $(shell pkg-config libpng --cflags) -DWITH_PNG
PNG_LIBS ?= $(shell pkg-config libpng --libs)
CFLAGS_c += $(PNG_CFLAGS)
LIBS_c += $(PNG_LIBS)

JPEG_CFLAGS ?= $(shell pkg-config libjpeg --cflags) -DWITH_JPEG
JPEG_LIBS ?= $(shell pkg-config libjpeg --libs)
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

MINIZIP_CFLAGS ?= $(shell pkg-config --cflags minizip)
MINIZIP_LIBS ?= $(shell pkg-config --libs minizip)
CFLAGS_c += $(MINIZIP_CFLAGS)
LIBS_c += $(MINIZIP_LIBS)

SPEEX_LIBS ?= $(shell pkg-config speex --libs) $(shell pkg-config speexdsp --libs)
ifdef SPEEX_LIBS
    CFLAGS_c += $(shell pkg-config speex --cflags) $(shell pkg-config speexdsp --cflags) -DWITH_SPEEX
endif
LIBS_c += $(SPEEX_LIBS)

ifndef CONFIG_WINDOWS
    ifeq ($(shell pkg-config --exists freetype2 && echo 1),1)
        FREETYPE_CFLAGS ?= $(shell pkg-config freetype2 --cflags)
        FREETYPE_LIBS ?= $(shell pkg-config freetype2 --libs)
    endif

    ifdef FREETYPE_LIBS
        CFLAGS_c += -DEZ_FREETYPE_SUPPORT_STATIC
    endif
endif

ifdef FREETYPE_LIBS
    CFLAGS_c += -DEZ_FREETYPE_SUPPORT
    LIBS_c += $(FREETYPE_LIBS)
    CFLAGS += $(FREETYPE_CFLAGS)
endif

ifdef OLD_WAV_LOADING
    CFLAGS_c += -DOLD_WAV_LOADING
else
    SNDFILE_CFLAGS ?= $(shell pkg-config sndfile --cflags)
    SNDFILE_LIBS ?= $(shell pkg-config sndfile --libs)
    CFLAGS += $(SNDFILE_CFLAGS)
    LIBS_c += $(SNDFILE_LIBS)
endif

# windres needs special quoting...
RCFLAGS_c += -DREVISION=$(REV) -DVERSION='\"$(VER)\"'

### Object Files ###

COMMON_OBJS := \
    $(SRC_DIR)/cmodel.o           \
    $(SRC_DIR)/cmd.o              \
    $(SRC_DIR)/com_msg.o          \
    $(SRC_DIR)/common.o           \
    $(SRC_DIR)/crc.o              \
    $(SRC_DIR)/cvar.o             \
    $(SRC_DIR)/fs.o               \
    $(SRC_DIR)/vfs_os.o           \
    $(SRC_DIR)/vfs_pak.o          \
    $(SRC_DIR)/vfs_zip.o          \
    $(SRC_DIR)/vfs_tcp.o          \
    $(SRC_DIR)/vfs_gzip.o         \
    $(SRC_DIR)/vfs_doomwad.o      \
    $(SRC_DIR)/vfs_mmap.o         \
    $(SRC_DIR)/vfs_tar.o          \
    $(SRC_DIR)/hash.o             \
    $(SRC_DIR)/host.o             \
    $(SRC_DIR)/mathlib.o          \
    $(SRC_DIR)/md4.o              \
    $(SRC_DIR)/sha3.o             \
    $(SRC_DIR)/net.o              \
    $(SRC_DIR)/net_chan.o         \
    $(SRC_DIR)/q_shared.o         \
    $(SRC_DIR)/version.o          \
    $(SRC_DIR)/zone.o             \
    $(SRC_DIR)/pmove.o            \
    $(SRC_DIR)/pmovetst.o

SERVER_OBJS := \
    $(SRC_DIR)/pr_cmds.o \
    $(SRC_DIR)/pr_edict.o \
    $(SRC_DIR)/pr_exec.o \
    $(SRC_DIR)/pr2_cmds.o \
    $(SRC_DIR)/pr2_edict.o \
    $(SRC_DIR)/pr2_exec.o \
    $(SRC_DIR)/sv_ccmds.o \
    $(SRC_DIR)/sv_ents.o \
    $(SRC_DIR)/sv_init.o \
    $(SRC_DIR)/sv_main.o \
    $(SRC_DIR)/sv_master.o \
    $(SRC_DIR)/sv_move.o \
    $(SRC_DIR)/sv_nchan.o \
    $(SRC_DIR)/sv_phys.o \
    $(SRC_DIR)/sv_save.o \
    $(SRC_DIR)/sv_send.o \
    $(SRC_DIR)/sv_user.o \
    $(SRC_DIR)/sv_world.o \
    $(SRC_DIR)/sv_demo.o \
    $(SRC_DIR)/sv_demo_misc.o \
    $(SRC_DIR)/sv_demo_qtv.o \
    $(SRC_DIR)/sv_login.o \
    $(SRC_DIR)/sv_mod_frags.o \
    $(SRC_DIR)/vm.o \
    $(SRC_DIR)/vm_interpreted.o \
    $(SRC_DIR)/vm_x86.o

HELP_OBJS := \
    $(patsubst help_%.json,help_%.o,$(wildcard help_*.json))

MODERN_GLSL_OBJS := \
    $(patsubst $(SRC_DIR)/glsl/%.glsl,$(SRC_DIR)/glsl_%.glsl.o,$(wildcard $(SRC_DIR)/glsl/*.glsl))

CLASSIC_GLSL_OBJS := \
    $(patsubst $(SRC_DIR)/glsl/glc/%.glsl,$(SRC_DIR)/glsl_%.glsl.o,$(wildcard $(SRC_DIR)/glsl/glc/*.glsl))

MODERN_OPENGL_OBJS := \
    $(MODERN_GLSL_OBJS) \
    $(SRC_DIR)/glm_aliasmodel.o \
    $(SRC_DIR)/glm_brushmodel.o \
    $(SRC_DIR)/glm_draw.o \
    $(SRC_DIR)/glm_framebuffer.o \
    $(SRC_DIR)/glm_lightmaps.o \
    $(SRC_DIR)/glm_md3.o \
    $(SRC_DIR)/glm_misc.o \
    $(SRC_DIR)/glm_particles.o \
    $(SRC_DIR)/glm_performance.o \
    $(SRC_DIR)/glm_main.o \
    $(SRC_DIR)/glm_rmain.o \
    $(SRC_DIR)/glm_rsurf.o \
    $(SRC_DIR)/glm_sdl.o \
    $(SRC_DIR)/glm_sprite.o \
    $(SRC_DIR)/glm_sprite3d.o \
    $(SRC_DIR)/glm_state.o \
    $(SRC_DIR)/glm_texture_arrays.o \
    $(SRC_DIR)/glm_vao.o

CLASSIC_OPENGL_OBJS := \
    $(CLASSIC_GLSL_OBJS) \
    $(SRC_DIR)/glc_aliasmodel.o \
    $(SRC_DIR)/glc_aliasmodel_mesh.o \
    $(SRC_DIR)/glc_bloom.o \
    $(SRC_DIR)/glc_brushmodel.o \
    $(SRC_DIR)/glc_draw.o \
    $(SRC_DIR)/glc_framebuffer.o \
    $(SRC_DIR)/glc_lightmaps.o \
    $(SRC_DIR)/glc_main.o \
    $(SRC_DIR)/glc_matrix.o \
    $(SRC_DIR)/glc_md3.o \
    $(SRC_DIR)/glc_misc.o \
    $(SRC_DIR)/glc_particles.o \
    $(SRC_DIR)/glc_performance.o \
    $(SRC_DIR)/glc_sdl.o \
    $(SRC_DIR)/glc_sky.o \
    $(SRC_DIR)/glc_sprite3d.o \
    $(SRC_DIR)/glc_state.o \
    $(SRC_DIR)/glc_surf.o \
    $(SRC_DIR)/glc_turb_surface.o \
    $(SRC_DIR)/glc_vao.o \
    $(SRC_DIR)/glc_warp.o

COMMON_OPENGL_OBJS := \
    $(SRC_DIR)/gl_aliasmodel.o \
    $(SRC_DIR)/gl_aliasmodel_md3.o \
    $(SRC_DIR)/gl_buffers.o \
    $(SRC_DIR)/gl_debug.o \
    $(SRC_DIR)/gl_drawcall_wrappers.o \
    $(SRC_DIR)/gl_framebuffer.o \
    $(SRC_DIR)/gl_misc.o \
    $(SRC_DIR)/gl_program.o \
    $(SRC_DIR)/gl_sdl.o \
    $(SRC_DIR)/gl_sprite3d.o \
    $(SRC_DIR)/gl_state.o \
    $(SRC_DIR)/gl_texture.o \
    $(SRC_DIR)/gl_texture_functions.o \
    $(SRC_DIR)/vid_common_gl.o

COMMON_RENDERER_OBJS := \
    $(SRC_DIR)/r_aliasmodel.o \
    $(SRC_DIR)/r_aliasmodel_md3.o \
    $(SRC_DIR)/r_aliasmodel_mesh.o \
    $(SRC_DIR)/r_aliasmodel_skins.o \
    $(SRC_DIR)/r_atlas.o \
    $(SRC_DIR)/r_bloom.o \
    $(SRC_DIR)/r_brushmodel.o \
    $(SRC_DIR)/r_brushmodel_bspx.o \
    $(SRC_DIR)/r_brushmodel_load.o \
    $(SRC_DIR)/r_brushmodel_sky.o \
    $(SRC_DIR)/r_brushmodel_surfaces.o \
    $(SRC_DIR)/r_brushmodel_textures.o \
    $(SRC_DIR)/r_brushmodel_warpsurfaces.o \
    $(SRC_DIR)/r_buffers.o \
    $(SRC_DIR)/r_chaticons.o \
    $(SRC_DIR)/r_draw.o \
    $(SRC_DIR)/r_draw_charset.o \
    $(SRC_DIR)/r_draw_circle.o \
    $(SRC_DIR)/r_draw_image.o \
    $(SRC_DIR)/r_draw_line.o \
    $(SRC_DIR)/r_draw_polygon.o \
    $(SRC_DIR)/r_hud.o \
    $(SRC_DIR)/r_lightmaps.o \
    $(SRC_DIR)/r_main.o \
    $(SRC_DIR)/r_matrix.o \
    $(SRC_DIR)/r_misc.o \
    $(SRC_DIR)/r_model.o \
    $(SRC_DIR)/r_netgraph.o \
    $(SRC_DIR)/r_palette.o \
    $(SRC_DIR)/r_part.o \
    $(SRC_DIR)/r_part_trails.o \
    $(SRC_DIR)/r_particles_qmb.o \
    $(SRC_DIR)/r_particles_qmb_trails.o \
    $(SRC_DIR)/r_particles_qmb_spawn.o \
    $(SRC_DIR)/r_performance.o \
    $(SRC_DIR)/r_refrag.o \
    $(SRC_DIR)/r_rlight.o \
    $(SRC_DIR)/r_rmain.o \
    $(SRC_DIR)/r_rmisc.o \
    $(SRC_DIR)/r_sprite3d.o \
    $(SRC_DIR)/r_sprites.o \
    $(SRC_DIR)/r_states.o \
    $(SRC_DIR)/r_texture.o \
    $(SRC_DIR)/r_texture_cvars.o \
    $(SRC_DIR)/r_texture_load.o \
    $(SRC_DIR)/r_texture_util.o \
    $(SRC_DIR)/vx_camera.o \
    $(SRC_DIR)/vx_coronas.o \
    $(SRC_DIR)/vx_stuff.o \
    $(SRC_DIR)/vx_vertexlights.o

HUD_OBJS := \
    $(SRC_DIR)/hud.o \
    $(SRC_DIR)/hud_262.o \
    $(SRC_DIR)/hud_common.o \
    $(SRC_DIR)/hud_editor.o \
    $(SRC_DIR)/hud_radar.o \
    $(SRC_DIR)/hud_speed.o \
    $(SRC_DIR)/hud_teaminfo.o \
    $(SRC_DIR)/hud_weapon_stats.o \
    $(SRC_DIR)/hud_autoid.o \
    $(SRC_DIR)/hud_clock.o \
    $(SRC_DIR)/hud_ammo.o \
    $(SRC_DIR)/hud_items.o \
    $(SRC_DIR)/hud_net.o \
    $(SRC_DIR)/hud_guns.o \
    $(SRC_DIR)/hud_groups.o \
    $(SRC_DIR)/hud_armor.o \
    $(SRC_DIR)/hud_health.o \
    $(SRC_DIR)/hud_gamesummary.o \
    $(SRC_DIR)/hud_face.o \
    $(SRC_DIR)/hud_frags.o \
    $(SRC_DIR)/hud_tracking.o \
    $(SRC_DIR)/hud_scores.o \
    $(SRC_DIR)/hud_performance.o \
    $(SRC_DIR)/hud_centerprint.o \
    $(SRC_DIR)/hud_qtv.o

OBJS_c := \
    $(COMMON_OBJS) \
    $(HELP_OBJS) \
    $(HUD_OBJS) \
    $(COMMON_RENDERER_OBJS) \
    $(SRC_DIR)/Ctrl.o \
    $(SRC_DIR)/Ctrl_EditBox.o \
    $(SRC_DIR)/Ctrl_PageViewer.o \
    $(SRC_DIR)/Ctrl_ScrollBar.o \
    $(SRC_DIR)/Ctrl_Tab.o \
    $(SRC_DIR)/EX_FileList.o \
    $(SRC_DIR)/EX_browser.o \
    $(SRC_DIR)/EX_browser_net.o \
    $(SRC_DIR)/EX_browser_pathfind.o \
    $(SRC_DIR)/EX_browser_ping.o \
    $(SRC_DIR)/EX_browser_qtvlist.o \
    $(SRC_DIR)/EX_browser_sources.o \
    $(SRC_DIR)/EX_qtvlist.o \
    $(SRC_DIR)/ez_controls.o \
    $(SRC_DIR)/ez_scrollbar.o \
    $(SRC_DIR)/ez_scrollpane.o \
    $(SRC_DIR)/ez_label.o \
    $(SRC_DIR)/ez_slider.o \
    $(SRC_DIR)/ez_button.o \
    $(SRC_DIR)/ez_window.o \
    $(SRC_DIR)/cl_cam.o \
    $(SRC_DIR)/cl_cmd.o \
    $(SRC_DIR)/cl_demo.o \
    $(SRC_DIR)/cl_nqdemo.o \
    $(SRC_DIR)/cl_ents.o \
    $(SRC_DIR)/cl_input.o \
    $(SRC_DIR)/cl_main.o \
    $(SRC_DIR)/cl_parse.o \
    $(SRC_DIR)/cl_pred.o \
    $(SRC_DIR)/cl_screen.o \
    $(SRC_DIR)/cl_slist.o \
    $(SRC_DIR)/cl_tent.o \
    $(SRC_DIR)/cl_view.o \
    $(SRC_DIR)/common_draw.o \
    $(SRC_DIR)/console.o \
    $(SRC_DIR)/config_manager.o \
    $(SRC_DIR)/demo_controls.o \
    $(SRC_DIR)/document_rendering.o \
    $(SRC_DIR)/fchecks.o \
    $(SRC_DIR)/fmod.o \
    $(SRC_DIR)/fragstats.o \
    $(SRC_DIR)/help.o \
    $(SRC_DIR)/help_files.o \
    $(SRC_DIR)/ignore.o \
    $(SRC_DIR)/image.o \
    $(SRC_DIR)/irc_filter.o \
    $(SRC_DIR)/irc.o \
    $(SRC_DIR)/keys.o \
    $(SRC_DIR)/logging.o \
    $(SRC_DIR)/match_tools.o \
    $(SRC_DIR)/match_tools_challenge.o \
    $(SRC_DIR)/menu.o \
    $(SRC_DIR)/menu_demo.o \
    $(SRC_DIR)/menu_ingame.o \
    $(SRC_DIR)/menu_multiplayer.o \
    $(SRC_DIR)/menu_options.o \
    $(SRC_DIR)/menu_proxy.o \
    $(SRC_DIR)/movie.o \
    $(SRC_DIR)/mvd_autotrack.o \
    $(SRC_DIR)/mvd_utils.o \
    $(SRC_DIR)/mvd_xmlstats.o \
    $(SRC_DIR)/parser.o \
    $(SRC_DIR)/qtv.o \
    $(SRC_DIR)/rulesets.o \
    $(SRC_DIR)/sbar.o \
    $(SRC_DIR)/settings_page.o \
    $(SRC_DIR)/sha1.o \
    $(SRC_DIR)/skin.o \
    $(SRC_DIR)/snd_main.o \
    $(SRC_DIR)/snd_mem.o \
    $(SRC_DIR)/snd_mix.o \
    $(SRC_DIR)/stats_grid.o \
    $(SRC_DIR)/teamplay.o \
    $(SRC_DIR)/teamplay_locfiles.o \
    $(SRC_DIR)/tp_msgs.o \
    $(SRC_DIR)/tp_triggers.o \
    $(SRC_DIR)/textencoding.o \
    $(SRC_DIR)/utils.o \
    $(SRC_DIR)/vx_tracker.o \
    $(SRC_DIR)/wad.o \
    $(SRC_DIR)/xsd.o \
    $(SRC_DIR)/xsd_document.o \
    $(SRC_DIR)/collision.o \
    $(SRC_DIR)/vid_vsync.o \
    $(SRC_DIR)/cd_null.o \
    $(SRC_DIR)/vid_sdl2.o \
    $(SRC_DIR)/sys_sdl2.o \
    $(SRC_DIR)/in_sdl2.o \
    $(SRC_DIR)/cl_multiview.o \
    $(SRC_DIR)/snd_voip.o \
    $(SRC_DIR)/cl_screenshot.o \
    $(SRC_DIR)/fonts.o \
    $(SRC_DIR)/cl_skygroups.o

### Configuration Options ###

ifdef MODERN_OPENGL_ONLY
    OBJS_c += $(COMMON_OPENGL_OBJS)
    OBJS_c += $(MODERN_OPENGL_OBJS)
    CFLAGS += -DRENDERER_OPTION_MODERN_OPENGL
    EZ_POSTFIX := "-glsl"
else
    ifdef CLASSIC_OPENGL_ONLY
        OBJS_c += $(COMMON_OPENGL_OBJS)
        OBJS_c += $(CLASSIC_OPENGL_OBJS)
        CFLAGS += -DRENDERER_OPTION_CLASSIC_OPENGL
        EZ_POSTFIX := "-std"
    else
        OBJS_c += $(COMMON_OPENGL_OBJS)
        OBJS_c += $(MODERN_OPENGL_OBJS)
        OBJS_c += $(CLASSIC_OPENGL_OBJS)
        CFLAGS += -DRENDERER_OPTION_CLASSIC_OPENGL
        CFLAGS += -DRENDERER_OPTION_MODERN_OPENGL
        EZ_POSTFIX := ""
    endif
endif

ifndef CLIENT_ONLY
    OBJS_c += $(SERVER_OBJS)
else
    CFLAGS += -DCLIENTONLY
endif

ifdef CURL_LIBS
    OBJS_c += \
        $(SRC_DIR)/central.o
endif

ifdef CONFIG_WINDOWS
    OBJS_c += \
        $(SRC_DIR)/movie_avi.o \
        $(SRC_DIR)/localtime_win.o \
        $(SRC_DIR)/sys_win.o \
        $(SRC_DIR)/winquake.o
    LIBS_c += -lopengl32 -lws2_32 -lwinmm -lpthread
else
    OBJS_c += \
        $(SRC_DIR)/localtime_posix.o \
        $(SRC_DIR)/sys_posix.o \
        $(SRC_DIR)/linux_signals.o

LIBS_c += -lm

    ifeq ($(SYS),Darwin)
        LIBS_c += -framework Foundation -framework OpenGL -framework AppKit -framework CoreServices -framework GameController
        OBJS_c += $(SRC_DIR)/in_osx.o $(SRC_DIR)/sys_osx.o
    else
        LIBS_c += -lGL -lpthread
    endif

    ifeq ($(SYS),Linux)
        LIBS_c += -lXxf86vm
    endif

    ifneq ($(SYS),FreeBSD)
        ifneq ($(SYS),OpenBSD)
            ifneq ($(SYS),NetBSD)
                LIBS_c += -ldl
            endif
        endif
    endif
endif

#ifdef CONFIG_OGG
#    OGG_CFLAGS ?= $(shell pkg-config vorbisfile --cflags) -DWITH_OGG_VORBIS
#    OGG_LIBS ?= $(shell pkg-config vorbisfile --libs)
#    CFLAGS_c += $(OGG_CFLAGS)
#    LIBS_c += $(OGG_LIBS)
#endif

### Targets ###

ifdef CONFIG_WINDOWS
    TARG_c := ezquake$(EZ_POSTFIX).exe
else
    TARG_c := ezquake-$(LSYS)-$(CPU)$(EZ_POSTFIX)
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


$(BUILD_c)/$(SRC_DIR)/glsl_%.glsl.o: $(SRC_DIR)/glsl/%.glsl
	$(E) [GLSL] $@
	$(Q)$(JSON2C) $< > $(BUILD_c)/$*.c
	$(Q)$(CC) -c $(CFLAGS) $(CFLAGS_c) $(INCLUDES) -o $@ $(BUILD_c)/$*.c

$(BUILD_c)/$(SRC_DIR)/glsl_%.glsl.o: $(SRC_DIR)/glsl/glc/%.glsl
	$(E) [GLSL] $@
	$(Q)$(JSON2C) $< > $(BUILD_c)/$*.c
	$(Q)$(CC) -c $(CFLAGS) $(CFLAGS_c) $(INCLUDES) -o $@ $(BUILD_c)/$*.c

$(BUILD_c)/%.o: %.json
	$(E) [JS] $@
	$(Q)$(JSON2C) $< > $(BUILD_c)/$*.c
	$(Q)$(CC) -c $(CFLAGS) $(CFLAGS_c) $(INCLUDES) -o $@ $(BUILD_c)/$*.c

$(BUILD_c)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(E) [CC] $@
	$(Q)$(MKDIR) $(@D)
	$(Q)$(CC) -c $(CFLAGS) $(CFLAGS_c) $(INCLUDES) -o $@ $<

$(BUILD_c)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.m
	$(E) [CC] $@
	$(Q)$(MKDIR) $(@D)
	$(Q)$(CC) -c $(CFLAGS) $(CFLAGS_c) $(INCLUDES) -o $@ $<

$(BUILD_c)/%.o: %.rc
	$(E) [RC] $@
	$(Q)$(MKDIR) $(@D)
	$(Q)$(WINDRES) $(RCFLAGS) $(RCFLAGS_c) -o $@ $<

$(TARG_c): $(OBJS_c)
	$(E) [LD] $@
	$(Q)$(MKDIR) $(@D)
	$(Q)$(CC) $(LDFLAGS) $(LDFLAGS_c) -o $@ $(OBJS_c) $(LIBS) $(LIBS_c)
