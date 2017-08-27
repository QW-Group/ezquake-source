cmake_minimum_required (VERSION 3.2)
project (ezQuake)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include (GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)

set (ezQuake_VERSION_MAJOR 3)
set (ezQuake_VERSION_MINOR 1)
set (ezQuake_VERSION_MICRO 0)

if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE "dev")
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	set (ezQuake_VERSION_REV "")
else ()
	if (GIT_SHA1)
		string (SUBSTRING "${GIT_SHA1}" 0 8 GIT_SHA1_SHORT)
		set (ezQuake_VERSION_REV "-${GIT_SHA1_SHORT}")
	else ()
		set (ezQuake_VERSION_REV "-unknown")
	endif ()
endif ()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.c.in" "${CMAKE_CURRENT_BINARY_DIR}/version.c" @ONLY)

set (C_VISIBILITY_PRESET hidden) 

set (COMMON_SRC
    cmodel.c
    cmd.c
    com_msg.c
    common.c
    crc.c
    cvar.c
    fs.c
    vfs_os.c
    vfs_pak.c
    vfs_zip.c
    vfs_tcp.c
    vfs_gzip.c
    vfs_doomwad.c
    vfs_mmap.c
    vfs_tar.c
    hash.c
    host.c
    mathlib.c
    md4.c
    net.c
    net_chan.c
    q_shared.c
    version.c
    zone.c
    pmove.c
    pmovetst.c
    )
list (APPEND COMMON_SRC "${CMAKE_CURRENT_BINARY_DIR}/version.c" version.h)

set (SERVER_SRC
    pr_cmds.c
    pr_edict.c
    pr_exec.c
    pr2_cmds.c
    pr2_edict.c
    pr2_exec.c
    pr2_vm.c
    sv_ccmds.c
    sv_ents.c
    sv_init.c
    sv_main.c
    sv_master.c
    sv_move.c
    sv_nchan.c
    sv_phys.c
    sv_save.c
    sv_send.c
    sv_user.c
    sv_world.c
    sv_demo.c
    sv_demo_misc.c
    sv_demo_qtv.c
    sv_login.c
    sv_mod_frags.c
    )

set (HELP_SRC
    help_variables.c
    help_commands.c
    )

set (CLIENT_SRC
    ioapi.c
    unzip.c
    Ctrl.c
    Ctrl_EditBox.c
    Ctrl_PageViewer.c
    Ctrl_ScrollBar.c
    Ctrl_Tab.c
    EX_FileList.c
    EX_browser.c
    EX_browser_net.c
    EX_browser_pathfind.c
    EX_browser_ping.c
    EX_browser_qtvlist.c
    EX_browser_sources.c
    EX_qtvlist.c
    ez_controls.c
    ez_scrollbar.c
    ez_scrollpane.c
    ez_label.c
    ez_slider.c
    ez_button.c
    ez_window.c
    cl_cam.c
    cl_cmd.c
    cl_demo.c
    cl_nqdemo.c
    cl_ents.c
    cl_input.c
    cl_main.c
    cl_parse.c
    cl_pred.c
    cl_screen.c
    cl_slist.c
    cl_tcl.c
    cl_tent.c
    cl_view.c
    common_draw.c
    console.c
    config_manager.c
    demo_controls.c
    document_rendering.c
    fchecks.c
    fmod.c
    fragstats.c
    help.c
    help_files.c
    hud.c
    hud_common.c
    hud_weapon_stats.c
    hud_radar.c
    hud_editor.c
    ignore.c
    image.c
    irc_filter.c
    irc.c
    keys.c
    logging.c
    match_tools.c
    menu.c
    menu_demo.c
    menu_ingame.c
    menu_mp3player.c
    menu_multiplayer.c
    menu_options.c
    menu_proxy.c
    modules.c
    movie.c
    mp3_player.c
    mp3_audacious.c
    mp3_xmms.c
    mp3_xmms2.c
    mp3_mpd.c
    mp3_winamp.c
    mvd_autotrack.c
    mvd_utils.c
    mvd_xmlstats.c
    parser.c
    qtv.c
    r_part.c
    rulesets.c
    sbar.c
    settings_page.c
    sha1.c
    skin.c
    snd_main.c
    snd_mem.c
    snd_mix.c
    snd_ov.c
    stats_grid.c
    teamplay.c
    tp_msgs.c
    tp_triggers.c
    textencoding.c
    utils.c
    vx_tracker.c
    wad.c
    xsd.c
    xsd_command.c
    xsd_document.c
    xsd_variable.c
    collision.c
    gl_draw.c
    gl_bloom.c
    gl_md3.c
    gl_mesh.c
    gl_model.c
    gl_ngraph.c
    gl_refrag.c
    gl_rlight.c
    gl_rmain.c
    gl_rmisc.c
    gl_rpart.c
    gl_rsurf.c
    gl_texture.c
    gl_warp.c
    vx_camera.c
    vx_coronas.c
    vx_motiontrail.c
    vx_stuff.c
    vx_vertexlights.c
    vid_common_gl.c
    vid_sdl2.c
    sys_sdl2.c
    in_sdl2.c
    cl_multiview.c
    snd_voip.c
    )

set (LINUX_SRC
    cd_linux.c
    linux_signals.c
    localtime_posix.c
    sys_posix.c
    )

set (MACOS_SRC
    cd_null.c
    linux_signals.c
    localtime_posix.c
    sys_posix.c
    in_osx.c
    sys_osx.m
    )

set (WINDOWS_SRC
    cd_null.c
	localtime_win.c
	movie_avi.c
	sys_win.c
	)
	
## Macros

add_definitions(-DNDEBUG)
add_definitions(-DJSS_CAM)
add_definitions(-DUSE_PR2)
add_definitions(-DWITH_NQPROGS)
add_definitions(-DUSE_SDL2)
add_definitions(-DWITH_ZIP)
add_definitions(-DWITH_JPEG)
add_definitions(-DWITH_PNG)
add_definitions(-D__Q_PNG14__)
add_definitions(-DWITH_SPEEX)
add_definitions(-DWITH_ZLIB)

## LIBRARIES
set (EZQUAKE_LIBS)

if (WIN32)
	# We're linking static libs
	add_definitions(-DXML_STATIC)
	add_definitions(-DPCRE_STATIC)
	add_definitions(-DCURL_STATICLIB)
	add_definitions(-DWITH_JPEG_STATIC)
	add_definitions(-DWITH_PNG_STATIC)
	
	set (PLATFORM_SRC ${WINDOWS_SRC})
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/winquake.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/winquake.rc" @ONLY)

	if(MSVC)
		include_directories(dependencies/include)
		include_directories(dependencies/include/curl)
		include_directories(dependencies/include/speex)
		add_definitions(/arch:SSE)
		add_definitions(/arch:SSE2)
		set (WINDOWS_LIBS
			version
			imm32
			winmm
			dependencies/lib/turbojpeg-static
			dependencies/lib/SDL2
			comctl32
			vcruntimed
			ucrtd
			msimg32
			opengl32
			ws2_32
			wldap32
			kernel32
			user32
			dependencies/lib/libcurld
			dependencies/lib/jansson_d
			dependencies/lib/libexpat
			dependencies/lib/libpng16
			dependencies/lib/libspeex
			dependencies/lib/libspeexdsp
			dependencies/lib/pcred
			dependencies/lib/zlib
			)
	else() # mingw32
		include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/include)
		include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/include/curl)
		include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/include/speex)
		include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/include/libpng16)
		include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/include/SDL2)
		set (WINDOWS_LIBS
			${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/lib/libSDL2.a
			${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/lib/libcurl.a
			${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/lib/libexpat.a
			${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/lib/libpcre.a
			${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/lib/libjansson.a
			${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/lib/libturbojpeg.a
			${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/lib/libz.a
			${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/lib/libspeex.a
			${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/lib/libspeexdsp.a
			${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs/lib/libpng16.a
			ole32
			imm32
			comctl32
			version
			oleaut32
			opengl32
			winmm
			ws2_32
		)

		set (CMAKE_EXE_LINKER_FLAGS "-mwindows -L${CMAKE_CURRENT_SOURCE_DIR}/mingw32-libs -static -Wl,--nxcompat,--dynamicbase")
		set (CMAKE_C_FLAGS "-s -m32 -O2 -msse2 -mfpmath=sse -msse3 -g -D_WIN32_WINNT=0x0501 -D__USE_MINGW_ANSI_STDIO -Wall -Werror=strict-prototypes -Werror=old-style-definition -Wno-strict-aliasing -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -ffast-math -MMD")
	endif()
	set (PLATFORM_LIBS ${WINDOWS_LIBS})
	set (EZQ_BIN_NAME "ezquake") #.exe will be automagically appended

elseif(UNIX AND NOT APPLE) # Linux atm -- FIXME Apple, BSD etc
	set (CMAKE_C_FLAGS "-O2 -g -Wall -Werror=strict-prototypes -Werror=old-style-definition -Wno-strict-aliasing -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -msse3 -ffast-math -MMD")

	# If release, strip binary, on Windows it always strips
	set (CMAKE_C_FLAGS_RELEASE "-s ${CMAKE_C_FLAGS}")

	set (PLATFORM_SRC ${LINUX_SRC})
	
	find_package(SDL2 REQUIRED)
	include_directories(${SDL2_INCLUDE_DIR})
	find_package(jansson REQUIRED)
	find_package(CURL REQUIRED)
	find_package(OpenGL REQUIRED)
	find_package(SPEEX REQUIRED)
	find_package(SPEEXDSP REQUIRED)
	find_package(EXPAT REQUIRED)
	find_package(PNG REQUIRED)
	find_package(JPEG REQUIRED)
	find_package(ZLIB REQUIRED)
	find_package(PCRE REQUIRED)
	
	list(APPEND EZQUAKE_LIBS ${SDL2_LIBRARY} ${CURL_LIBRARY} ${OPENGL_gl_LIBRARY} ${EXPAT_LIBRARIES} ${PNG_LIBRARY} ${JPEG_LIBRARY} ${ZLIB_LIBRARY} ${SPEEX_LIBRARY} ${SPEEXDSP_LIBRARY} ${JANSSON_LIBRARY} ${PCRE_LIBRARY})
	set(LINUX_LIBS
	    ${EZQUAKE_LIBS}
		m
		dl
		pthread
		)
	set (PLATFORM_LIBS ${LINUX_LIBS})
	set (EZQ_BIN_NAME "ezquake-linux-x86_64")

elseif (APPLE)
	include_directories("/usr/local/include") # FIXME: Is this necessary?
	set (CMAKE_EXE_LINKER_FLAGS "-headerpax_max_install_names -mmacosx-version-min=10.8" -framework Foundation -framework OpenGL -framework IOKit -framework CoreServices)
	set (CMAKE_C_FLAGS "-O2 -g -Wall -Werror=strict-prototypes -Werror=old-style-definition -Wno-strict-aliasing -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -msse3 -ffast-math -MMD -mmacosx-version-min=10.8")

	# If release, strip binary, on Windows it always strips -- DOES THIS WORK ON MACOS clang?
	set (CMAKE_C_FLAGS_RELEASE "-s ${CMAKE_C_FLAGS}")

	set (PLATFORM_SRC ${MACOS_SRC})
	
	find_package(SDL2 REQUIRED)
	include_directories(${SDL2_INCLUDE_DIR})
	find_package(jansson REQUIRED)
	find_package(CURL REQUIRED)
	find_package(OpenGL REQUIRED)
	find_package(SPEEX REQUIRED)
	find_package(SPEEXDSP REQUIRED)
	find_package(EXPAT REQUIRED)
	find_package(PNG REQUIRED)
	find_package(JPEG REQUIRED)
	find_package(ZLIB REQUIRED)
	find_package(PCRE REQUIRED)
	
	list(APPEND EZQUAKE_LIBS ${SDL2_LIBRARY} ${CURL_LIBRARY} ${OPENGL_gl_LIBRARY} ${EXPAT_LIBRARIES} ${PNG_LIBRARY} ${JPEG_LIBRARY} ${ZLIB_LIBRARY} ${SPEEX_LIBRARY} ${SPEEXDSP_LIBRARY} ${JANSSON_LIBRARY} ${PCRE_LIBRARY})
	set(MACOS_LIBS ${EZQUAKE_LIBS} m)
	set (PLATFORM_LIBS ${MACOS_LIBS})
	set (EZQ_BIN_NAME "ezquake-darwin-x86_64")
endif()


## Executable

if (WIN32)
	add_executable(ezquake WIN32 ${CLIENT_SRC} ${SERVER_SRC} ${COMMON_SRC} ${HELP_SRC} ${PLATFORM_SRC} winquake.rc)
elseif (APPLE)
	add_executable(ezquake MACOSX_BUNDLE ${CLIENT_SRC} ${SERVER_SRC} ${COMMON_SRC} ${HELP_SRC} ${PLATFORM_SRC})
else ()
	add_executable(ezquake ${CLIENT_SRC} ${SERVER_SRC} ${COMMON_SRC} ${HELP_SRC} ${PLATFORM_SRC})
endif ()

# Set binary name if configured
if (EZQ_BIN_NAME)
	set_target_properties(ezquake PROPERTIES OUTPUT_NAME ${EZQ_BIN_NAME})
endif ()

## Linking
target_link_libraries(ezquake ${PLATFORM_LIBS})

