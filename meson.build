# See http://mesonbuild.com/ for more details.
#
# $ meson --buildtype=release build
# $ ninja -C build

# For Windows:
#
# Compile environment should be MSYS2 (http://www.msys2.org/)
# with a MinGW compiler.

# For macOS (homebrew):
#
# Some homebrew packages are not symlinked ("keg-only") to prevent
# interfering with system libraries. So we have to expclicitly list
# them for pkg-config.
#
# export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/opt/expat/lib/pkgconfig
# export PKT_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/opt/curl/lib/pkgconfig
# export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/opt/jpeg-turbo/lib/pkgconfig

project('ezquake', 'c')

deps = [
	dependency('expat'),
	dependency('jansson'),
	dependency('libcurl'),
	dependency('libpcre'),
	dependency('sdl2'),
	dependency('threads'),
	dependency('sndfile'),
	meson.get_compiler('c').find_library('dl', required : false),
	meson.get_compiler('c').find_library('m', required : false),
]

ver = find_program(meson.source_root() + '/version.sh')

version = run_command(ver, '--version')
revision = run_command(ver, '--revision')

c_args = [
	'-DREVISION=' + revision.stdout().strip(),
	'-DVERSION="' + version.stdout().strip() + '"',
	'-DNDEBUG',
	'-DJSS_CAM',
	'-DUSE_PR2',
	'-DWITH_NQPROGS',
	'-DUSE_SDL2',
	'-DWITH_ZIP',
	'-Wno-strict-aliasing',
	'-Wno-int-to-pointer-cast',
	'-Wno-pointer-to-int-cast',
]

if meson.get_compiler('c').has_argument('-msse2')
	c_args += '-msse2'
endif

png = dependency('libpng', required : false)
if png.found()
	deps += png
	c_args += '-DWITH_PNG'
	if png.version() >= '1.4.0'
		c_args += '-D__Q_PNG14__'
	endif
endif

jpeg = dependency('libjpeg', required : false)
if jpeg.found()
	deps += jpeg
	c_args += '-DWITH_JPEG'
endif

speex = dependency('speex', required : false)
speexdsp = dependency('speexdsp', required : false)
if speex.found() and speexdsp.found()
	deps += [
		speex,
		speexdsp
	]
	c_args += '-DWITH_SPEEX'
endif

#vorbis = dependency('vorbisfile', required : false)
#if vorbis.found()
#	deps += vorbis
# crashed on a quick test
#	c_args += '-DWITH_OGG_VORBIS'
#endif

zlib = dependency('zlib', required : false)
if zlib.found()
	deps += zlib
	c_args += '-DWITH_ZLIB'
endif

sources = [
	'central.c',
	'cl_cam.c',
	'cl_cmd.c',
	'cl_demo.c',
	'cl_ents.c',
	'cl_input.c',
	'cl_main.c',
	'cl_multiview.c',
	'cl_nqdemo.c',
	'cl_parse.c',
	'cl_pred.c',
	'cl_screen.c',
	'cl_skygroups.c',
	'cl_slist.c',
	'cl_tent.c',
	'cl_view.c',
	'cmd.c',
	'cmodel.c',
	'collision.c',
	'common.c',
	'common_draw.c',
	'com_msg.c',
	'config_manager.c',
	'console.c',
	'crc.c',
	'Ctrl.c',
	'Ctrl_EditBox.c',
	'Ctrl_PageViewer.c',
	'Ctrl_ScrollBar.c',
	'Ctrl_Tab.c',
	'cvar.c',
	'demo_controls.c',
	'document_rendering.c',
	'EX_browser.c',
	'EX_browser_net.c',
	'EX_browser_pathfind.c',
	'EX_browser_ping.c',
	'EX_browser_qtvlist.c',
	'EX_browser_sources.c',
	'EX_FileList.c',
	'EX_qtvlist.c',
	'ez_button.c',
	'ez_controls.c',
	'ez_label.c',
	'ez_listview.c',
	'ez_listviewitem.c',
	'ez_scrollbar.c',
	'ez_scrollpane.c',
	'ez_slider.c',
	'ez_window.c',
	'fchecks.c',
	'fmod.c',
	'fragstats.c',
	'fs.c',
	'hash.c',
	'help.c',
	'help_files.c',
	'host.c',
	'hud.c',
	'hud_common.c',
	'hud_editor.c',
	'hud_radar.c',
	'hud_weapon_stats.c',
	'ignore.c',
	'image.c',
	'in_sdl2.c',
	'irc.c',
	'irc_filter.c',
	'keys.c',
	'logging.c',
	'match_tools.c',
	'match_tools_challenge.c',
	'mathlib.c',
	'md4.c',
	'menu.c',
	'menu_demo.c',
	'menu_ingame.c',
	'menu_multiplayer.c',
	'menu_options.c',
	'menu_proxy.c',
	'movie.c',
	'mvd_autotrack.c',
	'mvd_utils.c',
	'mvd_xmlstats.c',
	'net.c',
	'net_chan.c',
	'parser.c',
	'pmove.c',
	'pmovetst.c',
	'pr2_cmds.c',
	'pr2_edict.c',
	'pr2_exec.c',
	'pr2_vm.c',
	'pr_cmds.c',
	'pr_edict.c',
	'pr_exec.c',
	'q_shared.c',
	'qtv.c',
	'rulesets.c',
	'sbar.c',
	'settings_page.c',
	'sha1.c',
	'skin.c',
	'snd_main.c',
	'snd_mem.c',
	'snd_mix.c',
	'snd_voip.c',
	'stats_grid.c',
	'sv_ccmds.c',
	'sv_demo.c',
	'sv_demo_misc.c',
	'sv_demo_qtv.c',
	'sv_ents.c',
	'sv_init.c',
	'sv_login.c',
	'sv_main.c',
	'sv_master.c',
	'sv_mod_frags.c',
	'sv_move.c',
	'sv_nchan.c',
#	'sv_null.c',
	'sv_phys.c',
	'sv_save.c',
	'sv_send.c',
#	'sv_sys_unix.c',
#	'sv_sys_win.c'
	'sv_user.c',
	'sv_world.c',
	'sys_sdl2.c',
	'teamplay.c',
	'teamplay_locfiles.c',
	'textencoding.c',
	'tp_msgs.c',
	'tp_triggers.c',
	'utils.c',
	'version.c',
	'vfs_doomwad.c',
	'vfs_gzip.c',
	'vfs_mmap.c',
	'vfs_os.c',
	'vfs_pak.c',
	'vfs_tar.c',
	'vfs_tcp.c',
	'vfs_zip.c',
	'vid_common_gl.c',
	'vid_sdl2.c',
	'vx_camera.c',
	'vx_coronas.c',
	'vx_stuff.c',
	'vx_tracker.c',
	'vx_vertexlights.c',
	'wad.c',
	'xsd.c',
	'xsd_document.c',
	'zone.c',
]

includes = []
minizip = dependency('minizip', required : false)
if minizip.found()
	deps += minizip
else
	sources += [
		'minizip/ioapi.c',
		'minizip/unzip.c',
	]
	includes += include_directories('minizip')
endif

# OpenGL (common)
sources += [
	'gl_aliasmodel_md3.c',
	'gl_buffers.c',
	'gl_debug.c',
	'gl_drawcall_wrappers.c',
	'gl_framebuffer.c',
	'gl_misc.c',
	'gl_program.c',
	'gl_sdl.c',
	'gl_sprite3d.c',
	'gl_state.c',
	'gl_texture.c',
	'gl_texture_functions.c',
	'vid_common_gl.c'
]

# OpenGL (modern)
sources += [
	'glm_aliasmodel.c',
	'glm_brushmodel.c',
	'glm_draw.c',
	'glm_framebuffer.c',
	'glm_lightmaps.c',
	'glm_main.c',
	'glm_md3.c',
	'glm_misc.c',
	'glm_particles.c',
	'glm_performance.c',
	'glm_rmain.c',
	'glm_rsurf.c',
	'glm_sdl.c',
	'glm_sprite.c',
	'glm_sprite3d.c',
	'glm_state.c',
	'glm_texture_arrays.c',
	'glm_vao.c'
]

# OpenGL (classic)
sources += [
	'glc_aliasmodel.c',
	'glc_aliasmodel_mesh.c',
	'glc_bloom.c',
	'glc_brushmodel.c',
	'glc_draw.c',
	'glc_fog.c',
	'glc_framebuffer.c',
	'glc_lightmaps.c',
	'glc_main.c',
	'glc_matrix.c',
	'glc_md3.c',
	'glc_misc.c',
	'glc_particles.c',
	'glc_performance.c',
	'glc_sdl.c',
	'glc_sky.c',
	'glc_sprite3d.c',
	'glc_state.c',
	'glc_surf.c',
	'glc_turb_surface.c',
	'glc_vao.c',
	'glc_warp.c'
]

# renderer/common/hud
sources += [
	'r_draw_charset.c',
	'r_draw_circle.c',
	'r_draw_image.c',
	'r_draw_line.c',
	'r_draw_polygon.c',
	'r_hud.c'
]

# renderer/common
sources += [
	'r_aliasmodel.c',
	'r_aliasmodel_md3.c',
	'r_aliasmodel_mesh.c',
	'r_aliasmodel_skins.c',
	'r_atlas.c',
	'r_bloom.c',
	'r_brushmodel.c',
	'r_brushmodel_bspx.c',
	'r_brushmodel_load.c',
	'r_brushmodel_sky.c',
	'r_brushmodel_surfaces.c',
	'r_brushmodel_textures.c',
	'r_brushmodel_warpsurfaces.c',
	'r_buffers.c',
	'r_chaticons.c',
	'r_draw.c',
	'r_lightmaps.c',
	'r_main.c',
	'r_matrix.c',
	'r_misc.c',
	'r_model.c',
	'r_netgraph.c',
	'r_palette.c',
	'r_part.c',
	'r_part_trails.c',
	'r_particles_qmb.c',
	'r_particles_qmb_spawn.c',
	'r_particles_qmb_trails.c',
	'r_performance.c',
	'r_refrag.c',
	'r_rlight.c',
	'r_rmain.c',
	'r_rmisc.c',
	'r_sprite3d.c',
	'r_sprites.c',
	'r_states.c',
	'r_texture.c',
	'r_texture_cvars.c',
	'r_texture_load.c',
	'r_texture_util.c',
	'sbar.c',
	'vx_camera.c',
	'vx_coronas.c',
	'vx_stuff.c',
	'vx_vertexlights.c'
]

# meag: extra source files for renderer update
sources += [
	'hud_262.c',
	'hud_speed.c',
	'hud_teaminfo.c',
	'vid_vsync.c',
	'hud_autoid.c',
	'cl_screenshot.c',
	'hud_clock.c',
	'hud_ammo.c',
	'hud_items.c',
	'hud_net.c',
	'hud_guns.c',
	'hud_groups.c',
	'hud_armor.c',
	'hud_health.c',
	'hud_gamesummary.c',
	'hud_face.c',
	'hud_frags.c',
	'hud_tracking.c',
	'hud_scores.c',
	'hud_performance.c',
	'hud_centerprint.c',
	'hud_qtv.c',
	'fonts.c'
]

awk = find_program('awk')
od = find_program('od')
gen = generator(find_program(meson.source_root() + '/json2c.sh'),
	output : '@BASENAME@.c',
	arguments : ['@INPUT@', '@OUTPUT@']
)
sources += gen.process(
	'help_commands.json',
	'help_variables.json',
	'help_macros.json',
	'help_cmdline_params.json'
)

sources += gen.process(
	'glsl/common.glsl',
	'glsl/constants.glsl',
	'glsl/draw_aliasmodel.fragment.glsl',
	'glsl/draw_aliasmodel.vertex.glsl',
	'glsl/draw_sprites.fragment.glsl',
	'glsl/draw_sprites.vertex.glsl',
	'glsl/draw_world.fragment.glsl',
	'glsl/draw_world.vertex.glsl',
	'glsl/fx_world_geometry.fragment.glsl',
	'glsl/fx_world_geometry.vertex.glsl',
	'glsl/hud_draw_circle.fragment.glsl',
	'glsl/hud_draw_circle.vertex.glsl',
	'glsl/hud_draw_image.fragment.glsl',
	'glsl/hud_draw_image.vertex.glsl',
	'glsl/hud_draw_line.fragment.glsl',
	'glsl/hud_draw_line.vertex.glsl',
	'glsl/hud_draw_polygon.fragment.glsl',
	'glsl/hud_draw_polygon.vertex.glsl',
	'glsl/lighting.compute.glsl',
	'glsl/post_process_screen.fragment.glsl',
	'glsl/post_process_screen.vertex.glsl',
	'glsl/simple.fragment.glsl',
	'glsl/simple.vertex.glsl',
	'glsl/simple3d.fragment.glsl',
	'glsl/simple3d.vertex.glsl',
)

sources += gen.process(
	'glsl/glc/glc_aliasmodel_shadow.fragment.glsl',
	'glsl/glc/glc_aliasmodel_shadow.vertex.glsl',
	'glsl/glc/glc_aliasmodel_shell.fragment.glsl',
	'glsl/glc/glc_aliasmodel_shell.vertex.glsl',
	'glsl/glc/glc_aliasmodel_std.fragment.glsl',
	'glsl/glc/glc_aliasmodel_std.vertex.glsl',
	'glsl/glc/glc_caustics.fragment.glsl',
	'glsl/glc/glc_caustics.vertex.glsl',
	'glsl/glc/glc_draw_sprites.fragment.glsl',
	'glsl/glc/glc_draw_sprites.vertex.glsl',
	'glsl/glc/glc_hud_images.fragment.glsl',
	'glsl/glc/glc_hud_images.vertex.glsl',
	'glsl/glc/glc_post_process_screen.fragment.glsl',
	'glsl/glc/glc_post_process_screen.vertex.glsl',
	'glsl/glc/glc_sky.fragment.glsl',
	'glsl/glc/glc_sky.vertex.glsl',
	'glsl/glc/glc_turbsurface.fragment.glsl',
	'glsl/glc/glc_turbsurface.vertex.glsl',
	'glsl/glc/glc_world_drawflat.fragment.glsl',
	'glsl/glc/glc_world_drawflat.vertex.glsl',
	'glsl/glc/glc_world_secondpass.fragment.glsl',
	'glsl/glc/glc_world_secondpass.vertex.glsl',
	'glsl/glc/glc_world_textured.fragment.glsl',
	'glsl/glc/glc_world_textured.vertex.glsl',
)

link_args = []

if host_machine.system() == 'windows'
	sources += [
		'cd_null.c',
		'localtime_win.c',
		'movie_avi.c',
		'sys_win.c',
	]
	deps += meson.get_compiler('c').find_library('opengl32')
	deps += meson.get_compiler('c').find_library('ws2_32')
	deps += meson.get_compiler('c').find_library('comctl32')
	deps += meson.get_compiler('c').find_library('winmm')
elif host_machine.system() == 'darwin'
	add_languages('objc')
	sources += [
		'cd_null.c',
		'in_osx.c',
		'localtime_posix.c',
		'sys_osx.m',
		'sys_posix.c',
	]
	link_args = [
		'-Wl,-framework,AppKit',
		'-Wl,-framework,IOKit',
		'-Wl,-framework,OpenGL',
	]
else # Otherwise assume a roughly POSIX compliant OS with X11.
	deps += dependency('gl')
	deps += dependency('xxf86vm')
	sources += [
		'cd_linux.c',
		'linux_signals.c',
		'localtime_posix.c',
		'sys_posix.c',
	]
	c_args += '-DX11_GAMMA_WORKAROUND'
endif

c_args += ['-DRENDERER_OPTION_CLASSIC_OPENGL', '-DRENDERER_OPTION_MODERN_OPENGL']

executable('ezquake-' + host_machine.system() + '-' + host_machine.cpu(), sources,
	dependencies : deps,
	include_directories : includes,
	c_args : c_args,
	link_args : link_args,
)
