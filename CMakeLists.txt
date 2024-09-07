cmake_minimum_required(VERSION 3.22)

project(ezquake C)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    enable_language(OBJC)
endif()

option(USE_SYSTEM_LIBS          "Use system libraries instead of VCPKG"       ON)
option(RENDERER_MODERN_OPENGL   "Enable modern OpenGL renderer"               ON)
option(RENDERER_CLASSIC_OPENGL  "Enable classic OpenGL renderer"              ON)
option(DEBUG_MEMORY_ALLOCATIONS "Enable debug prints for memory allocations" OFF)
option(RENDERING_TRACE          "Enable tracing of the rendering pipeline"   OFF)
option(ENABLE_SANDBOX           "Enables application sandboxing (macOS)"      ON)
option(ENABLE_LTO               "Enable Link Time Optimization"               ON)

if(NOT RENDERER_CLASSIC_OPENGL AND NOT RENDERER_MODERN_OPENGL)
    message(FATAL_ERROR "At least one of RENDERER_CLASSIC_OPENGL or RENDERER_MODERN_OPENGL must be enabled.")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

include(GitUtils)
include(AddResources)
include(CheckIPOSupported)
include(CheckCCompilerFlag)
include(CheckDependency)

git_refresh_submodules()
git_extract_version(git_version)

# Xcode has its own LTO features.
if(ENABLE_LTO AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    check_ipo_supported(RESULT USE_LTO)
    if(USE_LTO)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    else()
        message(FATAL_ERROR "Link Time Optimization requested, but not available.")
    endif()
endif()

if(ENABLE_LTO)
    message("-- Link Time Optimization: Enabled")
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(
            /nologo
            /W3
            /WX-
            /diagnostics:column
    )

    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT $<$<CONFIG:Debug>:ProgramDatabase>)

    # Enable automatic parallelization of MSBuild
    set(CMAKE_VS_GLOBALS
            "UseMultiToolTask=true"
            "EnforceProcessCountAcrossBuilds=true"
    )

    if(EXISTS "${PROJECT_SOURCE_DIR}/ezquake.vcxproj.user")
        configure_file("${PROJECT_SOURCE_DIR}/ezquake.vcxproj.user" "${CMAKE_BINARY_DIR}/ezquake.vcxproj.user" COPYONLY)
    endif()
else()
    add_compile_options(
            -std=gnu89

            -Wall
            -Wno-pointer-to-int-cast
            -Wno-int-to-pointer-cast
            -Wno-strict-aliasing
            -Wno-deprecated-declarations

            -fvisibility=hidden

            -Werror=format
            -Werror=strict-prototypes
            -Werror=old-style-definition

            $<$<CONFIG:Release>:-Werror=unused-function>
            $<$<CONFIG:Release>:-Werror=unused-variable>
    )

    check_c_compiler_flag("-Wstrlcpy-strlcat-size" HAS_STRLCPY_STRLCAT_SIZE)
    if(HAS_STRLCPY_STRLCAT_SIZE)
        add_compile_options("-Werror=strlcpy-strlcat-size")
    endif()
    check_c_compiler_flag("-Wformat-truncation" HAS_FORMAT_TRUNCATION)
    if(HAS_FORMAT_TRUNCATION)
        add_compile_options("-Wno-error=format-truncation" "-Wno-format-truncation")
    endif()
    check_c_compiler_flag("-Wparentheses" HAS_PARENTHESIS)
    if(HAS_PARENTHESIS)
        add_compile_options("-Wno-parentheses")
    endif()
    check_c_compiler_flag("-Wmisleading-indentation" HAS_MISLEADING_INDENTATION)
    if(HAS_MISLEADING_INDENTATION)
        add_compile_options("-Wno-misleading-indentation")
    endif()
    # While not gnu89, it's in practice allowed by all supported compilers
    check_c_compiler_flag("-Wtypedef-redefinition" HAS_TYPE_REDEFINITION)
    if(HAS_TYPE_REDEFINITION)
        add_compile_options("-Wno-typedef-redefinition")
    endif()
    # Should mostly be fixed in mvdsv, and next sync can remove this
    check_c_compiler_flag("-Wshorten-64-to-32" HAS_SHORTEN_64_TO_32)
    if(HAS_TYPE_REDEFINITION)
        add_compile_options("-Wno-shorten-64-to-32")
    endif()
endif()

find_library(MATH m)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# Args: target name, pkg-config name, vcpkg package name, vcpkg target name, extra args
check_dependency(Expat    "expat"      "EXPAT"              "expat::expat"                 REQUIRED CONFIG)
check_dependency(FreeType "freetype2"  "Freetype"           "Freetype::Freetype"           OPTIONAL)
check_dependency(JPEG     "libjpeg"    "JPEG"               "JPEG::JPEG"                   REQUIRED)
check_dependency(Jansson  "jansson"    "jansson"            "jansson::jansson"             REQUIRED)
check_dependency(MiniZip  "minizip"    "unofficial-minizip" "unofficial::minizip::minizip" REQUIRED)
check_dependency(PCRE2    "libpcre2-8" "PCRE2"              "pcre2::pcre2-8-static"        REQUIRED)
check_dependency(PNG      "libpng"     "PNG"                "PNG::PNG"                     REQUIRED)
check_dependency(SDL2     "sdl2"       "SDL2"               "SDL2::SDL2-static"            REQUIRED)
check_dependency(SndFile  "sndfile"    "SndFile"            "SndFile::sndfile"             REQUIRED)
check_dependency(Speex    "speex"      "Speex"              "SPEEX::SPEEX"                 OPTIONAL)
check_dependency(SpeexDSP "speexdsp"   "SpeexDSP"           "SPEEX::SPEEXDSP"              OPTIONAL)
check_dependency(cURL     "libcurl"    "CURL"               "CURL::libcurl"                REQUIRED CONFIG)
check_dependency(zlib     "zlib"       "ZLIB"               "ZLIB::ZLIB"                   REQUIRED)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_library(FRAMEWORK_APPKIT AppKit REQUIRED)
    find_library(FRAMEWORK_FOUNDATION Foundation REQUIRED)
    find_library(FRAMEWORK_CORESERVICES CoreServices REQUIRED)
    find_library(FRAMEWORK_GAMECONTROLLER GameController REQUIRED)
endif()

# Place special CMake targets in separate VS/Xcode folder
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT "ezquake")

set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)

set(common_headers
        ${SOURCE_DIR}/bspfile.h
        ${SOURCE_DIR}/cmd.h
        ${SOURCE_DIR}/cmdline_params.h
        ${SOURCE_DIR}/cmdline_params_ids.h
        ${SOURCE_DIR}/cmodel.h
        ${SOURCE_DIR}/common.h
        ${SOURCE_DIR}/crc.h
        ${SOURCE_DIR}/cvar.h
        ${SOURCE_DIR}/cvar_groups.h
        ${SOURCE_DIR}/fs.h
        ${SOURCE_DIR}/hash.h
        ${SOURCE_DIR}/macro_definitions.h
        ${SOURCE_DIR}/macro_ids.h
        ${SOURCE_DIR}/mathlib.h
        ${SOURCE_DIR}/net.h
        ${SOURCE_DIR}/parser.h
        ${SOURCE_DIR}/pmove.h
        ${SOURCE_DIR}/q_platform.h
        ${SOURCE_DIR}/q_shared.h
        ${SOURCE_DIR}/sha1.h
        ${SOURCE_DIR}/sha3.h
        ${SOURCE_DIR}/version.h
        ${SOURCE_DIR}/vfs.h
        ${SOURCE_DIR}/vfs_tar.h
        ${SOURCE_DIR}/zone.h
)
set(common
        ${SOURCE_DIR}/cmd.c
        ${SOURCE_DIR}/cmodel.c
        ${SOURCE_DIR}/com_msg.c
        ${SOURCE_DIR}/common.c
        ${SOURCE_DIR}/crc.c
        ${SOURCE_DIR}/cvar.c
        ${SOURCE_DIR}/fs.c
        ${SOURCE_DIR}/hash.c
        ${SOURCE_DIR}/mathlib.c
        ${SOURCE_DIR}/md4.c
        ${SOURCE_DIR}/net.c
        ${SOURCE_DIR}/net_chan.c
        ${SOURCE_DIR}/parser.c
        ${SOURCE_DIR}/pmove.c
        ${SOURCE_DIR}/pmovetst.c
        ${SOURCE_DIR}/q_shared.c
        ${SOURCE_DIR}/sha1.c
        ${SOURCE_DIR}/sha3.c
        ${SOURCE_DIR}/version.c
        ${SOURCE_DIR}/vfs_doomwad.c
        ${SOURCE_DIR}/vfs_gzip.c
        ${SOURCE_DIR}/vfs_mmap.c
        ${SOURCE_DIR}/vfs_os.c
        ${SOURCE_DIR}/vfs_pak.c
        ${SOURCE_DIR}/vfs_tar.c
        ${SOURCE_DIR}/vfs_tcp.c
        ${SOURCE_DIR}/vfs_zip.c
        ${SOURCE_DIR}/zone.c
        ${common_headers}
)
source_group(TREE ${SOURCE_DIR} PREFIX "Source Files/common" FILES ${common})
source_group(TREE ${SOURCE_DIR} PREFIX "Header Files/common" FILES ${common_headers})

set(QWPROT_SOURCE_DIR ${SOURCE_DIR}/qwprot/src)
set(qwprot_headers
        ${QWPROT_SOURCE_DIR}/protocol.h
)
source_group(TREE ${QWPROT_SOURCE_DIR} PREFIX "Header Files/qwprot" FILES ${qwprot_headers})

set(server_headers
        ${SOURCE_DIR}/g_public.h
        ${SOURCE_DIR}/pr2.h
        ${SOURCE_DIR}/pr_comp.h
        ${SOURCE_DIR}/progdefs.h
        ${SOURCE_DIR}/progs.h
        ${SOURCE_DIR}/qwsvdef.h
        ${SOURCE_DIR}/server.h
        ${SOURCE_DIR}/sv_log.h
        ${SOURCE_DIR}/sv_mod_frags.h
        ${SOURCE_DIR}/sv_world.h
        ${SOURCE_DIR}/vm.h
        ${SOURCE_DIR}/vm_local.h
)
set(server
        ${SOURCE_DIR}/pr2_cmds.c
        ${SOURCE_DIR}/pr2_edict.c
        ${SOURCE_DIR}/pr2_exec.c
        ${SOURCE_DIR}/pr_cmds.c
        ${SOURCE_DIR}/pr_edict.c
        ${SOURCE_DIR}/pr_exec.c
        ${SOURCE_DIR}/sv_ccmds.c
        ${SOURCE_DIR}/sv_demo.c
        ${SOURCE_DIR}/sv_demo_misc.c
        ${SOURCE_DIR}/sv_demo_qtv.c
        ${SOURCE_DIR}/sv_ents.c
        ${SOURCE_DIR}/sv_init.c
        ${SOURCE_DIR}/sv_login.c
        ${SOURCE_DIR}/sv_main.c
        ${SOURCE_DIR}/sv_master.c
        ${SOURCE_DIR}/sv_mod_frags.c
        ${SOURCE_DIR}/sv_move.c
        ${SOURCE_DIR}/sv_nchan.c
        ${SOURCE_DIR}/sv_phys.c
        ${SOURCE_DIR}/sv_save.c
        ${SOURCE_DIR}/sv_send.c
        ${SOURCE_DIR}/sv_user.c
        ${SOURCE_DIR}/sv_world.c
        ${SOURCE_DIR}/vm.c
        ${SOURCE_DIR}/vm_interpreted.c
        ${SOURCE_DIR}/vm_x86.c
        ${server_headers}
)
source_group(TREE ${SOURCE_DIR} PREFIX "Source Files/server" FILES ${server})
source_group(TREE ${SOURCE_DIR} PREFIX "Header Files/server" FILES ${server_headers})

if(RENDERER_MODERN_OPENGL)
    set(MODERN_GLSL_DIR ${SOURCE_DIR}/glsl)
    add_resources(shaders_modern
            ${MODERN_GLSL_DIR}/common.glsl
            ${MODERN_GLSL_DIR}/constants.glsl
            ${MODERN_GLSL_DIR}/draw_aliasmodel.fragment.glsl
            ${MODERN_GLSL_DIR}/draw_aliasmodel.vertex.glsl
            ${MODERN_GLSL_DIR}/draw_sprites.fragment.glsl
            ${MODERN_GLSL_DIR}/draw_sprites.vertex.glsl
            ${MODERN_GLSL_DIR}/draw_world.fragment.glsl
            ${MODERN_GLSL_DIR}/draw_world.vertex.glsl
            ${MODERN_GLSL_DIR}/fx_world_geometry.fragment.glsl
            ${MODERN_GLSL_DIR}/fx_world_geometry.vertex.glsl
            ${MODERN_GLSL_DIR}/hud_draw_circle.fragment.glsl
            ${MODERN_GLSL_DIR}/hud_draw_circle.vertex.glsl
            ${MODERN_GLSL_DIR}/hud_draw_image.fragment.glsl
            ${MODERN_GLSL_DIR}/hud_draw_image.geometry.glsl
            ${MODERN_GLSL_DIR}/hud_draw_image.vertex.glsl
            ${MODERN_GLSL_DIR}/hud_draw_line.fragment.glsl
            ${MODERN_GLSL_DIR}/hud_draw_line.vertex.glsl
            ${MODERN_GLSL_DIR}/hud_draw_polygon.fragment.glsl
            ${MODERN_GLSL_DIR}/hud_draw_polygon.vertex.glsl
            ${MODERN_GLSL_DIR}/lighting.compute.glsl
            ${MODERN_GLSL_DIR}/lighting_copy.compute.glsl
            ${MODERN_GLSL_DIR}/post_process_screen.fragment.glsl
            ${MODERN_GLSL_DIR}/post_process_screen.vertex.glsl
            ${MODERN_GLSL_DIR}/simple.fragment.glsl
            ${MODERN_GLSL_DIR}/simple.vertex.glsl
            ${MODERN_GLSL_DIR}/simple3d.fragment.glsl
            ${MODERN_GLSL_DIR}/simple3d.vertex.glsl
    )

    set(modern_opengl_headers
            ${SOURCE_DIR}/glm_brushmodel.h
            ${SOURCE_DIR}/glm_draw.h
            ${SOURCE_DIR}/glm_local.h
            ${SOURCE_DIR}/glm_particles.h
            ${SOURCE_DIR}/glm_texture_arrays.h
            ${SOURCE_DIR}/glm_vao.h
    )
    set(modern_opengl
            ${SOURCE_DIR}/glm_aliasmodel.c
            ${SOURCE_DIR}/glm_brushmodel.c
            ${SOURCE_DIR}/glm_draw.c
            ${SOURCE_DIR}/glm_framebuffer.c
            ${SOURCE_DIR}/glm_lightmaps.c
            ${SOURCE_DIR}/glm_main.c
            ${SOURCE_DIR}/glm_md3.c
            ${SOURCE_DIR}/glm_misc.c
            ${SOURCE_DIR}/glm_particles.c
            ${SOURCE_DIR}/glm_performance.c
            ${SOURCE_DIR}/glm_rmain.c
            ${SOURCE_DIR}/glm_rsurf.c
            ${SOURCE_DIR}/glm_sdl.c
            ${SOURCE_DIR}/glm_sprite.c
            ${SOURCE_DIR}/glm_sprite3d.c
            ${SOURCE_DIR}/glm_state.c
            ${SOURCE_DIR}/glm_texture_arrays.c
            ${SOURCE_DIR}/glm_vao.c
            ${modern_opengl_headers}
    )
    source_group(TREE ${SOURCE_DIR} PREFIX "Source Files/modern_opengl" FILES ${modern_opengl})
    source_group(TREE ${SOURCE_DIR} PREFIX "Header Files/modern_opengl" FILES ${modern_opengl_headers})
endif()

if(RENDERER_CLASSIC_OPENGL)
    set(CLASSIC_GLSL_DIR ${SOURCE_DIR}/glsl/glc)
    add_resources(shaders_classic
            ${CLASSIC_GLSL_DIR}/glc_aliasmodel_shadow.fragment.glsl
            ${CLASSIC_GLSL_DIR}/glc_aliasmodel_shadow.vertex.glsl
            ${CLASSIC_GLSL_DIR}/glc_aliasmodel_shell.fragment.glsl
            ${CLASSIC_GLSL_DIR}/glc_aliasmodel_shell.vertex.glsl
            ${CLASSIC_GLSL_DIR}/glc_aliasmodel_std.fragment.glsl
            ${CLASSIC_GLSL_DIR}/glc_aliasmodel_std.vertex.glsl
            ${CLASSIC_GLSL_DIR}/glc_caustics.fragment.glsl
            ${CLASSIC_GLSL_DIR}/glc_caustics.vertex.glsl
            ${CLASSIC_GLSL_DIR}/glc_draw_sprites.fragment.glsl
            ${CLASSIC_GLSL_DIR}/glc_draw_sprites.vertex.glsl
            ${CLASSIC_GLSL_DIR}/glc_hud_images.fragment.glsl
            ${CLASSIC_GLSL_DIR}/glc_hud_images.vertex.glsl
            ${CLASSIC_GLSL_DIR}/glc_post_process_screen.fragment.glsl
            ${CLASSIC_GLSL_DIR}/glc_post_process_screen.vertex.glsl
            ${CLASSIC_GLSL_DIR}/glc_sky.fragment.glsl
            ${CLASSIC_GLSL_DIR}/glc_sky.vertex.glsl
            ${CLASSIC_GLSL_DIR}/glc_turbsurface.fragment.glsl
            ${CLASSIC_GLSL_DIR}/glc_turbsurface.vertex.glsl
            ${CLASSIC_GLSL_DIR}/glc_world_drawflat.fragment.glsl
            ${CLASSIC_GLSL_DIR}/glc_world_drawflat.vertex.glsl
            ${CLASSIC_GLSL_DIR}/glc_world_secondpass.fragment.glsl
            ${CLASSIC_GLSL_DIR}/glc_world_secondpass.vertex.glsl
            ${CLASSIC_GLSL_DIR}/glc_world_textured.fragment.glsl
            ${CLASSIC_GLSL_DIR}/glc_world_textured.vertex.glsl
    )

    set(classic_opengl_headers
            ${SOURCE_DIR}/glc_local.h
            ${SOURCE_DIR}/glc_matrix.h
            ${SOURCE_DIR}/glc_state.h
            ${SOURCE_DIR}/glc_vao.h
    )
    set(classic_opengl
            ${SOURCE_DIR}/glc_aliasmodel.c
            ${SOURCE_DIR}/glc_aliasmodel_mesh.c
            ${SOURCE_DIR}/glc_bloom.c
            ${SOURCE_DIR}/glc_brushmodel.c
            ${SOURCE_DIR}/glc_draw.c
            ${SOURCE_DIR}/glc_framebuffer.c
            ${SOURCE_DIR}/glc_lightmaps.c
            ${SOURCE_DIR}/glc_main.c
            ${SOURCE_DIR}/glc_matrix.c
            ${SOURCE_DIR}/glc_md3.c
            ${SOURCE_DIR}/glc_misc.c
            ${SOURCE_DIR}/glc_particles.c
            ${SOURCE_DIR}/glc_performance.c
            ${SOURCE_DIR}/glc_sdl.c
            ${SOURCE_DIR}/glc_sky.c
            ${SOURCE_DIR}/glc_sprite3d.c
            ${SOURCE_DIR}/glc_state.c
            ${SOURCE_DIR}/glc_surf.c
            ${SOURCE_DIR}/glc_turb_surface.c
            ${SOURCE_DIR}/glc_vao.c
            ${SOURCE_DIR}/glc_warp.c
            ${classic_opengl_headers}
    )
    source_group(TREE ${SOURCE_DIR} PREFIX "Source Files/classic_opengl" FILES ${classic_opengl})
    source_group(TREE ${SOURCE_DIR} PREFIX "Header Files/classic_opengl" FILES ${classic_opengl_headers})
endif()

set(common_opengl_headers
        ${SOURCE_DIR}/gl_framebuffer.h
        ${SOURCE_DIR}/gl_local.h
        ${SOURCE_DIR}/gl_sprite3d.h
        ${SOURCE_DIR}/gl_texture.h
        ${SOURCE_DIR}/gl_texture_internal.h
)
set(common_opengl
        ${SOURCE_DIR}/gl_aliasmodel.c
        ${SOURCE_DIR}/gl_aliasmodel_md3.c
        ${SOURCE_DIR}/gl_buffers.c
        ${SOURCE_DIR}/gl_debug.c
        ${SOURCE_DIR}/gl_drawcall_wrappers.c
        ${SOURCE_DIR}/gl_framebuffer.c
        ${SOURCE_DIR}/gl_misc.c
        ${SOURCE_DIR}/gl_program.c
        ${SOURCE_DIR}/gl_sdl.c
        ${SOURCE_DIR}/gl_sprite3d.c
        ${SOURCE_DIR}/gl_state.c
        ${SOURCE_DIR}/gl_texture.c
        ${SOURCE_DIR}/gl_texture_functions.c
        ${SOURCE_DIR}/vid_common_gl.c
        ${SOURCE_DIR}/r_vao.h
        ${common_opengl_headers}
)
source_group(TREE ${SOURCE_DIR} PREFIX "Source Files/common_opengl" FILES ${common_opengl})
source_group(TREE ${SOURCE_DIR} PREFIX "Header Files/common_opengl" FILES ${common_opengl_headers})

set(common_renderer_headers
        ${SOURCE_DIR}/anorm_dots.h
        ${SOURCE_DIR}/anorms.h
        ${SOURCE_DIR}/draw.h
        ${SOURCE_DIR}/gl_model.h
        ${SOURCE_DIR}/modelgen.h
        ${SOURCE_DIR}/particles_classic.h
        ${SOURCE_DIR}/qmb_particles.h
        ${SOURCE_DIR}/quakedef.h
        ${SOURCE_DIR}/r_aliasmodel.h
        ${SOURCE_DIR}/r_aliasmodel_md3.h
        ${SOURCE_DIR}/r_brushmodel.h
        ${SOURCE_DIR}/r_brushmodel_sky.h
        ${SOURCE_DIR}/r_brushmodel_warpsurfaces_sin.h
        ${SOURCE_DIR}/r_buffers.h
        ${SOURCE_DIR}/r_chaticons.h
        ${SOURCE_DIR}/r_draw.h
        ${SOURCE_DIR}/r_framestats.h
        ${SOURCE_DIR}/r_lighting.h
        ${SOURCE_DIR}/r_lightmaps.h
        ${SOURCE_DIR}/r_lightmaps_internal.h
        ${SOURCE_DIR}/r_local.h
        ${SOURCE_DIR}/r_matrix.h
        ${SOURCE_DIR}/r_particles_qmb.h
        ${SOURCE_DIR}/r_performance.h
        ${SOURCE_DIR}/r_program.h
        ${SOURCE_DIR}/r_renderer.h
        ${SOURCE_DIR}/r_renderer_structure.h
        ${SOURCE_DIR}/r_shared.h
        ${SOURCE_DIR}/r_sprite3d.h
        ${SOURCE_DIR}/r_sprite3d_internal.h
        ${SOURCE_DIR}/r_state.h
        ${SOURCE_DIR}/r_texture.h
        ${SOURCE_DIR}/r_texture_internal.h
        ${SOURCE_DIR}/r_trace.h
        ${SOURCE_DIR}/render.h
        ${SOURCE_DIR}/spritegn.h
        ${SOURCE_DIR}/vx_camera.h
        ${SOURCE_DIR}/vx_stuff.h
        ${SOURCE_DIR}/vx_tracker.h
        ${SOURCE_DIR}/vx_vertexlights.h
)
set(common_renderer
        ${SOURCE_DIR}/r_aliasmodel.c
        ${SOURCE_DIR}/r_aliasmodel_md3.c
        ${SOURCE_DIR}/r_aliasmodel_mesh.c
        ${SOURCE_DIR}/r_aliasmodel_skins.c
        ${SOURCE_DIR}/r_atlas.c
        ${SOURCE_DIR}/r_bloom.c
        ${SOURCE_DIR}/r_brushmodel.c
        ${SOURCE_DIR}/r_brushmodel_bspx.c
        ${SOURCE_DIR}/r_brushmodel_load.c
        ${SOURCE_DIR}/r_brushmodel_sky.c
        ${SOURCE_DIR}/r_brushmodel_surfaces.c
        ${SOURCE_DIR}/r_brushmodel_textures.c
        ${SOURCE_DIR}/r_brushmodel_warpsurfaces.c
        ${SOURCE_DIR}/r_buffers.c
        ${SOURCE_DIR}/r_chaticons.c
        ${SOURCE_DIR}/r_draw.c
        ${SOURCE_DIR}/r_draw_charset.c
        ${SOURCE_DIR}/r_draw_circle.c
        ${SOURCE_DIR}/r_draw_image.c
        ${SOURCE_DIR}/r_draw_line.c
        ${SOURCE_DIR}/r_draw_polygon.c
        ${SOURCE_DIR}/r_hud.c
        ${SOURCE_DIR}/r_lightmaps.c
        ${SOURCE_DIR}/r_main.c
        ${SOURCE_DIR}/r_matrix.c
        ${SOURCE_DIR}/r_misc.c
        ${SOURCE_DIR}/r_model.c
        ${SOURCE_DIR}/r_netgraph.c
        ${SOURCE_DIR}/r_palette.c
        ${SOURCE_DIR}/r_part.c
        ${SOURCE_DIR}/r_part_trails.c
        ${SOURCE_DIR}/r_particles_qmb.c
        ${SOURCE_DIR}/r_particles_qmb_spawn.c
        ${SOURCE_DIR}/r_particles_qmb_trails.c
        ${SOURCE_DIR}/r_performance.c
        ${SOURCE_DIR}/r_refrag.c
        ${SOURCE_DIR}/r_rlight.c
        ${SOURCE_DIR}/r_rmain.c
        ${SOURCE_DIR}/r_rmisc.c
        ${SOURCE_DIR}/r_sprite3d.c
        ${SOURCE_DIR}/r_sprites.c
        ${SOURCE_DIR}/r_states.c
        ${SOURCE_DIR}/r_texture.c
        ${SOURCE_DIR}/r_texture_cvars.c
        ${SOURCE_DIR}/r_texture_load.c
        ${SOURCE_DIR}/r_texture_util.c
        ${SOURCE_DIR}/vx_camera.c
        ${SOURCE_DIR}/vx_coronas.c
        ${SOURCE_DIR}/vx_stuff.c
        ${SOURCE_DIR}/vx_vertexlights.c
        ${common_renderer_headers}
)
source_group(TREE ${SOURCE_DIR} PREFIX "Source Files/common_renderer" FILES ${common_renderer})
source_group(TREE ${SOURCE_DIR} PREFIX "Header Files/common_renderer" FILES ${common_renderer_headers})

set(common_hud_headers
        ${SOURCE_DIR}/common_draw.h
        ${SOURCE_DIR}/hud.h
        ${SOURCE_DIR}/hud_common.h
        ${SOURCE_DIR}/hud_editor.h
)
set(common_hud
        ${SOURCE_DIR}/hud.c
        ${SOURCE_DIR}/hud_262.c
        ${SOURCE_DIR}/hud_ammo.c
        ${SOURCE_DIR}/hud_armor.c
        ${SOURCE_DIR}/hud_autoid.c
        ${SOURCE_DIR}/hud_centerprint.c
        ${SOURCE_DIR}/hud_clock.c
        ${SOURCE_DIR}/hud_common.c
        ${SOURCE_DIR}/hud_editor.c
        ${SOURCE_DIR}/hud_face.c
        ${SOURCE_DIR}/hud_frags.c
        ${SOURCE_DIR}/hud_gamesummary.c
        ${SOURCE_DIR}/hud_groups.c
        ${SOURCE_DIR}/hud_guns.c
        ${SOURCE_DIR}/hud_health.c
        ${SOURCE_DIR}/hud_items.c
        ${SOURCE_DIR}/hud_net.c
        ${SOURCE_DIR}/hud_performance.c
        ${SOURCE_DIR}/hud_qtv.c
        ${SOURCE_DIR}/hud_radar.c
        ${SOURCE_DIR}/hud_scores.c
        ${SOURCE_DIR}/hud_speed.c
        ${SOURCE_DIR}/hud_teaminfo.c
        ${SOURCE_DIR}/hud_tracking.c
        ${SOURCE_DIR}/hud_weapon_stats.c
        ${common_hud_headers}
)
source_group(TREE ${SOURCE_DIR} PREFIX "Source Files/common_hud" FILES ${common_hud})
source_group(TREE ${SOURCE_DIR} PREFIX "Header Files/common_hud" FILES ${common_hud_headers})

set(DOCUMENTATION_DIR ${PROJECT_SOURCE_DIR})
add_resources(documentation
        ${DOCUMENTATION_DIR}/help_cmdline_params.json
        ${DOCUMENTATION_DIR}/help_commands.json
        ${DOCUMENTATION_DIR}/help_macros.json
        ${DOCUMENTATION_DIR}/help_variables.json
)

set(client_headers
        ${SOURCE_DIR}/Ctrl.h
        ${SOURCE_DIR}/Ctrl_EditBox.h
        ${SOURCE_DIR}/Ctrl_PageViewer.h
        ${SOURCE_DIR}/Ctrl_Tab.h
        ${SOURCE_DIR}/EX_FileList.h
        ${SOURCE_DIR}/EX_browser.h
        ${SOURCE_DIR}/EX_qtvlist.h
        ${SOURCE_DIR}/cdaudio.h
        ${SOURCE_DIR}/cl_slist.h
        ${SOURCE_DIR}/cl_view.h
        ${SOURCE_DIR}/client.h
        ${SOURCE_DIR}/config_manager.h
        ${SOURCE_DIR}/console.h
        ${SOURCE_DIR}/demo_controls.h
        ${SOURCE_DIR}/document_rendering.h
        ${SOURCE_DIR}/ez_button.h
        ${SOURCE_DIR}/ez_controls.h
        ${SOURCE_DIR}/ez_label.h
        ${SOURCE_DIR}/ez_listview.h
        ${SOURCE_DIR}/ez_listviewitem.h
        ${SOURCE_DIR}/ez_scrollbar.h
        ${SOURCE_DIR}/ez_scrollpane.h
        ${SOURCE_DIR}/ez_slider.h
        ${SOURCE_DIR}/ez_window.h
        ${SOURCE_DIR}/fchecks.h
        ${SOURCE_DIR}/fmod.h
        ${SOURCE_DIR}/fonts.h
        ${SOURCE_DIR}/help.h
        ${SOURCE_DIR}/ignore.h
        ${SOURCE_DIR}/image.h
        ${SOURCE_DIR}/input.h
        ${SOURCE_DIR}/keys.h
        ${SOURCE_DIR}/logging.h
        ${SOURCE_DIR}/menu.h
        ${SOURCE_DIR}/menu_demo.h
        ${SOURCE_DIR}/menu_ingame.h
        ${SOURCE_DIR}/menu_multiplayer.h
        ${SOURCE_DIR}/menu_options.h
        ${SOURCE_DIR}/menu_proxy.h
        ${SOURCE_DIR}/movie.h
        ${SOURCE_DIR}/movie_avi.h
        ${SOURCE_DIR}/mvd_utils.h
        ${SOURCE_DIR}/mvd_utils_common.h
        ${SOURCE_DIR}/qsound.h
        ${SOURCE_DIR}/qtv.h
        ${SOURCE_DIR}/rulesets.h
        ${SOURCE_DIR}/sbar.h
        ${SOURCE_DIR}/screen.h
        ${SOURCE_DIR}/settings.h
        ${SOURCE_DIR}/settings_page.h
        ${SOURCE_DIR}/stats_grid.h
        ${SOURCE_DIR}/sys.h
        ${SOURCE_DIR}/teamplay.h
        ${SOURCE_DIR}/textencoding.h
        ${SOURCE_DIR}/tp_msgs.h
        ${SOURCE_DIR}/tp_triggers.h
        ${SOURCE_DIR}/tr_types.h
        ${SOURCE_DIR}/utils.h
        ${SOURCE_DIR}/vid.h
        ${SOURCE_DIR}/wad.h
        ${SOURCE_DIR}/xsd.h
        ${SOURCE_DIR}/xsd_document.h
)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND client_headers ${SOURCE_DIR}/movie_avi.h)
endif()
set(client
        ${SOURCE_DIR}/Ctrl.c
        ${SOURCE_DIR}/Ctrl_EditBox.c
        ${SOURCE_DIR}/Ctrl_PageViewer.c
        ${SOURCE_DIR}/Ctrl_ScrollBar.c
        ${SOURCE_DIR}/Ctrl_Tab.c
        ${SOURCE_DIR}/EX_FileList.c
        ${SOURCE_DIR}/EX_browser.c
        ${SOURCE_DIR}/EX_browser_net.c
        ${SOURCE_DIR}/EX_browser_pathfind.c
        ${SOURCE_DIR}/EX_browser_ping.c
        ${SOURCE_DIR}/EX_browser_qtvlist.c
        ${SOURCE_DIR}/EX_browser_sources.c
        ${SOURCE_DIR}/EX_qtvlist.c
        ${SOURCE_DIR}/cd_null.c
        ${SOURCE_DIR}/cl_cam.c
        ${SOURCE_DIR}/cl_cmd.c
        ${SOURCE_DIR}/cl_demo.c
        ${SOURCE_DIR}/cl_ents.c
        ${SOURCE_DIR}/cl_input.c
        ${SOURCE_DIR}/cl_main.c
        ${SOURCE_DIR}/cl_multiview.c
        ${SOURCE_DIR}/cl_nqdemo.c
        ${SOURCE_DIR}/cl_parse.c
        ${SOURCE_DIR}/cl_pred.c
        ${SOURCE_DIR}/cl_screen.c
        ${SOURCE_DIR}/cl_screenshot.c
        ${SOURCE_DIR}/cl_skygroups.c
        ${SOURCE_DIR}/cl_slist.c
        ${SOURCE_DIR}/cl_tent.c
        ${SOURCE_DIR}/cl_view.c
        ${SOURCE_DIR}/collision.c
        ${SOURCE_DIR}/common_draw.c
        ${SOURCE_DIR}/config_manager.c
        ${SOURCE_DIR}/console.c
        ${SOURCE_DIR}/demo_controls.c
        ${SOURCE_DIR}/document_rendering.c
        ${SOURCE_DIR}/ez_button.c
        ${SOURCE_DIR}/ez_controls.c
        ${SOURCE_DIR}/ez_label.c
        ${SOURCE_DIR}/ez_scrollbar.c
        ${SOURCE_DIR}/ez_scrollpane.c
        ${SOURCE_DIR}/ez_slider.c
        ${SOURCE_DIR}/ez_window.c
        ${SOURCE_DIR}/fchecks.c
        ${SOURCE_DIR}/fmod.c
        ${SOURCE_DIR}/fonts.c
        ${SOURCE_DIR}/fragstats.c
        ${SOURCE_DIR}/help.c
        ${SOURCE_DIR}/help_files.c
        ${SOURCE_DIR}/host.c
        ${SOURCE_DIR}/ignore.c
        ${SOURCE_DIR}/image.c
        ${SOURCE_DIR}/in_sdl2.c
        ${SOURCE_DIR}/irc.c
        ${SOURCE_DIR}/irc_filter.c
        ${SOURCE_DIR}/keys.c
        ${SOURCE_DIR}/logging.c
        ${SOURCE_DIR}/match_tools.c
        ${SOURCE_DIR}/match_tools_challenge.c
        ${SOURCE_DIR}/menu.c
        ${SOURCE_DIR}/menu_demo.c
        ${SOURCE_DIR}/menu_ingame.c
        ${SOURCE_DIR}/menu_multiplayer.c
        ${SOURCE_DIR}/menu_options.c
        ${SOURCE_DIR}/menu_proxy.c
        ${SOURCE_DIR}/movie.c
        ${SOURCE_DIR}/mvd_autotrack.c
        ${SOURCE_DIR}/mvd_utils.c
        ${SOURCE_DIR}/mvd_xmlstats.c
        ${SOURCE_DIR}/qtv.c
        ${SOURCE_DIR}/rulesets.c
        ${SOURCE_DIR}/sbar.c
        ${SOURCE_DIR}/settings_page.c
        ${SOURCE_DIR}/skin.c
        ${SOURCE_DIR}/snd_main.c
        ${SOURCE_DIR}/snd_mem.c
        ${SOURCE_DIR}/snd_mix.c
        ${SOURCE_DIR}/snd_voip.c
        ${SOURCE_DIR}/stats_grid.c
        ${SOURCE_DIR}/sys_sdl2.c
        ${SOURCE_DIR}/teamplay.c
        ${SOURCE_DIR}/teamplay_locfiles.c
        ${SOURCE_DIR}/textencoding.c
        ${SOURCE_DIR}/tp_msgs.c
        ${SOURCE_DIR}/tp_triggers.c
        ${SOURCE_DIR}/utils.c
        ${SOURCE_DIR}/vid_sdl2.c
        ${SOURCE_DIR}/vid_vsync.c
        ${SOURCE_DIR}/vx_tracker.c
        ${SOURCE_DIR}/wad.c
        ${SOURCE_DIR}/xsd.c
        ${SOURCE_DIR}/xsd_document.c
        ${client_headers}
)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND client ${SOURCE_DIR}/movie_avi.c)
endif()
source_group(TREE ${SOURCE_DIR} PREFIX "Source Files/client" FILES ${client})
source_group(TREE ${SOURCE_DIR} PREFIX "Header Files/client" FILES ${client_headers})

set(sys_headers ${SOURCE_DIR}/localtime.h)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(sys ${SOURCE_DIR}/localtime_win.c)
else()
    set(sys
            ${SOURCE_DIR}/localtime_posix.c
            ${SOURCE_DIR}/linux_signals.c
    )
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND sys_headers
            ${SOURCE_DIR}/in_osx.h
    )
    list(APPEND sys
            ${SOURCE_DIR}/in_osx.m
            ${SOURCE_DIR}/sys_osx.m
    )
endif()
list(APPEND sys ${sys_headers})
source_group(TREE ${SOURCE_DIR} PREFIX "Source Files/sys" FILES ${sys})
source_group(TREE ${SOURCE_DIR} PREFIX "Header Files/sys" FILES ${sys_headers})

set(central_headers
        ${SOURCE_DIR}/central.h
)
set(central
        ${SOURCE_DIR}/central.c
        ${central_headers}
)
source_group(TREE ${SOURCE_DIR} PREFIX "Source Files/central" FILES ${central})
source_group(TREE ${SOURCE_DIR} PREFIX "Header Files/central" FILES ${central_headers})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(main ${SOURCE_DIR}/sys_win.c)
else()
    set(main ${SOURCE_DIR}/sys_posix.c)
endif()
source_group(TREE ${SOURCE_DIR} PREFIX "Source Files/main" FILES ${main})

get_target_property(version_major git_version VERSION_MAJOR)
get_target_property(version_minor git_version VERSION_MINOR)
get_target_property(version_patch git_version VERSION_PATCH)
get_target_property(version_build git_version REVISION)

# macOS icon
set(macos_icon "${PROJECT_SOURCE_DIR}/misc/install/ezquake.icns")
set_source_files_properties(${macos_icon} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

# Windows icon, and exe metadata
set(windows_icon "${CMAKE_CURRENT_BINARY_DIR}/ezQuake.rc")
set(EZQUAKE_RESOURCE_AUTHOR "QW-Group")
set(EZQUAKE_RESOURCE_NAME "ezQuake")
set(EZQUAKE_RESOURCE_DESCRIPTION "ezQuake - a QuakeWorld client")
set(EZQUAKE_RESOURCE_ICON "${PROJECT_SOURCE_DIR}/ezquake.ico")
set(EZQUAKE_RESOURCE_VERSION "${version_major},${version_minor},${version_patch},${version_build}")
configure_file("${PROJECT_SOURCE_DIR}/ezQuake.rc.in" ${windows_icon} @ONLY)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Mark the executable as a non-console application
    set(TARGET_TYPE WIN32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # Mark the executable for bundling as .app
    set(TARGET_TYPE MACOSX_BUNDLE)
endif()

add_executable(ezquake ${TARGET_TYPE}
        ${main}
        ${sys}
        ${central}
        ${common}
        ${common_hud}
        ${server}
        ${client}

        ${common_opengl}
        ${common_renderer}
        $<$<BOOL:${RENDERER_MODERN_OPENGL}>:${modern_opengl}>
        $<$<BOOL:${RENDERER_CLASSIC_OPENGL}>:${classic_opengl}>

        ${qwprot_headers}

        $<IF:$<PLATFORM_ID:Darwin>,${macos_icon},>
        $<IF:$<PLATFORM_ID:Windows>,${windows_icon},>
)

target_include_directories(ezquake PRIVATE
        ${SOURCE_DIR}/qwprot/src
)

target_compile_definitions(ezquake PRIVATE
        BUILDSTRING="${CMAKE_SYSTEM_NAME}"
        CPUSTRING="${CMAKE_SYSTEM_PROCESSOR}"

        JSS_CAM
        USE_PR2
        WITH_NQPROGS

        $<$<BOOL:${DEBUG_MEMORY_ALLOCATIONS}>:DEBUG_MEMORY_ALLOCATIONS>
        $<$<BOOL:${RENDERING_TRACE}>:WITH_RENDERING_TRACE>

        $<$<PLATFORM_ID:Darwin>:GL_SILENCE_DEPRECATION>

        $<$<BOOL:${RENDERER_MODERN_OPENGL}>:RENDERER_OPTION_MODERN_OPENGL>
        $<$<BOOL:${RENDERER_CLASSIC_OPENGL}>:RENDERER_OPTION_CLASSIC_OPENGL>

        WITH_PNG
        WITH_JPEG
        WITH_ZIP
        WITH_ZLIB

        $<$<BOOL:${HAVE_FREETYPE}>:EZ_FREETYPE_SUPPORT>
        $<$<BOOL:${HAVE_FREETYPE}>:EZ_FREETYPE_SUPPORT_STATIC>

        $<$<AND:$<BOOL:${HAVE_SPEEX}>,$<BOOL:${HAVE_SPEEXDSP}>>:WITH_SPEEX>

        PCRE2_CODE_UNIT_WIDTH=8
)

target_link_libraries(ezquake PRIVATE
        $<$<BOOL:${RENDERER_MODERN_OPENGL}>:shaders_modern>
        $<$<BOOL:${RENDERER_CLASSIC_OPENGL}>:shaders_classic>
        documentation
        git_version

        ${CMAKE_DL_LIBS}

        Dep::zlib
        Dep::cURL
        Dep::Expat
        Dep::JPEG
        Dep::PCRE2
        Dep::SDL2
        Dep::Jansson
        Dep::SndFile
        Dep::PNG
        Dep::MiniZip

        $<$<BOOL:${HAVE_FREETYPE}>:Dep::FreeType>

        $<$<AND:$<BOOL:${HAVE_SPEEX}>,$<BOOL:${HAVE_SPEEXDSP}>>:Dep::Speex>
        $<$<AND:$<BOOL:${HAVE_SPEEX}>,$<BOOL:${HAVE_SPEEXDSP}>>:Dep::SpeexDSP>

        OpenGL::GL
        Threads::Threads

        ${FRAMEWORK_APPKIT}
        ${FRAMEWORK_FOUNDATION}
        ${FRAMEWORK_CORESERVICES}
        ${FRAMEWORK_GAMECONTROLLER}
)

if(MATH)
    target_link_libraries(ezquake PRIVATE ${MATH})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    string(TOLOWER "ezQuake" EXECUTABLE_NAME)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    string(TOLOWER "ezquake" EXECUTABLE_NAME)
else()
    string(TOLOWER "ezquake-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}" EXECUTABLE_NAME)
endif()

set_target_properties(ezquake PROPERTIES OUTPUT_NAME ${EXECUTABLE_NAME})

# Make executable easy to find in project root, strip if release (macOS does this on its own).
# If CMake was invoked with an installation prefix, don't override with project root.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(INSTALL_DESTINATION "${PROJECT_SOURCE_DIR}" CACHE PATH "Installation Directory" FORCE)

    install(TARGETS ezquake DESTINATION ${INSTALL_DESTINATION})

    add_custom_command(
        TARGET ezquake POST_BUILD
        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} $<$<CONFIG:Release>:--strip> --config $<CONFIG>
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    get_target_property(version git_version GIT_DESCRIBE)

    set_target_properties(ezquake PROPERTIES
            XCODE_ATTRIBUTE_PRODUCT_NAME                            "ezQuake"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER               "com.ezquake.ezQuake"
            XCODE_ATTRIBUTE_MARKETING_VERSION                       "${version}"
            XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION                 "${version}"
            XCODE_ATTRIBUTE_LLVM_LTO                                $<IF:$<OR:$<CONFIG:Debug>,$<NOT:$<BOOL:${ENABLE_LTO}>>>,NO,Monolithic>
            XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL                  $<IF:$<CONFIG:Debug>,0,3>
            XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING                     YES
            XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE                 YES
            XCODE_ATTRIBUTE_INFOPLIST_KEY_CFBundleDisplayName       "ezQuake"
            XCODE_ATTRIBUTE_INFOPLIST_KEY_LSApplicationCategoryType "public.app-category.action-games"
            XCODE_ATTRIBUTE_INFOPLIST_KEY_NSHumanReadableCopyright  "GNU General Public License, version 2"
            MACOSX_BUNDLE_ICON_FILE                                 "ezquake"
    )

    if(ENABLE_SANDBOX)
        set_target_properties(ezquake PROPERTIES
                XCODE_ATTRIBUTE_ENABLE_APP_SANDBOX                 YES
                XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME            YES
                XCODE_ATTRIBUTE_CODE_SIGN_INJECT_BASE_ENTITLEMENTS YES
                XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS             "misc/install/ezquake.entitlements.plist"
        )
    endif()

    add_custom_command(
            TARGET ezquake POST_BUILD
            COMMAND /usr/libexec/PlistBuddy
            -c "Add :NSHighResolutionCapable bool YES"
            "$<TARGET_FILE_DIR:ezquake>/../Info.plist"
    )

    # qw:// protocol support
    add_custom_command(
            TARGET ezquake POST_BUILD
            COMMAND /usr/libexec/PlistBuddy
            -c "Add :CFBundleURLTypes array"
            -c "Add :CFBundleURLTypes:0 dict"
            -c "Add :CFBundleURLTypes:0:CFBundleURLName string QW"
            -c "Add :CFBundleURLTypes:0:CFBundleURLSchemes array"
            -c "Add :CFBundleURLTypes:0:CFBundleURLSchemes:0 string qw"
            "$<TARGET_FILE_DIR:ezquake>/../Info.plist"
    )

    # .mvd/.qwd/.dem file type support
    add_custom_command(
            TARGET ezquake POST_BUILD
            COMMAND /usr/libexec/PlistBuddy
            -c "Add :CFBundleDocumentTypes array"
            -c "Add :CFBundleDocumentTypes:0 dict"
            -c "Add :CFBundleDocumentTypes:0:CFBundleTypeName string \"Quake demo\""
            -c "Add :CFBundleDocumentTypes:0:CFBundleTypeRole string Viewer"
            -c "Add :CFBundleDocumentTypes:0:CFBundleTypeIconFile string ezquake.icns"
            -c "Add :CFBundleDocumentTypes:0:CFBundleTypeExtensions array"
            -c "Add :CFBundleDocumentTypes:0:CFBundleTypeExtensions: string mvd"
            -c "Add :CFBundleDocumentTypes:0:CFBundleTypeExtensions: string qwd"
            -c "Add :CFBundleDocumentTypes:0:CFBundleTypeExtensions: string dem"
            "$<TARGET_FILE_DIR:ezquake>/../Info.plist"
    )
endif()
