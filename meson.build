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
	'cl_slist.c',
	'cl_tcl.c',
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
	'gl_bloom.c',
	'gl_draw.c',
	'gl_framebuffer.c',
	'gl_md3.c',
	'gl_mesh.c',
	'gl_model.c',
	'gl_ngraph.c',
	'gl_refrag.c',
	'gl_rlight.c',
	'gl_rmain.c',
	'gl_rmisc.c',
	'gl_rpart.c',
	'gl_rsurf.c',
	'gl_texture.c',
	'gl_warp.c',
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
	'r_part.c',
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
	'vx_motiontrail.c',
	'vx_stuff.c',
	'vx_tracker.c',
	'vx_vertexlights.c',
	'wad.c',
	'xsd.c',
	'xsd_command.c',
	'xsd_document.c',
	'xsd_variable.c',
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

awk = find_program('awk')
od = find_program('od')
gen = generator(find_program(meson.source_root() + '/json2c.sh'),
	output : '@BASENAME@.c',
	arguments : ['@INPUT@', '@OUTPUT@']
)
sources += gen.process(
	'help_commands.json',
	'help_variables.json',
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

executable('ezquake-' + host_machine.system() + '-' + host_machine.cpu(), sources,
	dependencies : deps,
	include_directories : includes,
	c_args : c_args,
	link_args : link_args,
)
