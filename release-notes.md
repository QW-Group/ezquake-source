# Changes in 3.6

3.6 merges 3.2 and the un-released 3.5 - changes listed here are relative to those versions.

(if viewing on github, download links are at bottom of page)

### Bugs

- Fix crash to desktop when finding best routes to server and number of proxies too high (old bug, also fixed in 3.5-alpha24 and 3.2.1)
- When using non-desktop resolution and running fullscreen, image was cropped (3.5 bug, reported by bgnr)
- HUD renders circles again (affects speed2, radar, itemsclock with certain settings) (3.5 bug, reported by numerous people)
- Fixed visual artifact around classic particles in classic OpenGL (3.5 bug, reported by hemostick)
- Space prefix in console didn't send remaining text as chat (3.2 bug, reported by ciscon, BLooD_DoG & VVD)
- OpenGL viewport state invalidated if window is resized (3.5 bug, reported by blindcant)
- NULL pointer dereference if OpenGL implementation didn't support buffers (3.5 bug)
- Corrupt lightmap rendering when lots of options enabled for world-drawing GLSL (3.5 bug)
- Gamma adjustment on screenshots led to them being colored incorrectly (3.1/3.5 bug re-introduced during merge)
- Stop levelshot (console background) from potentially being deleted too early (3.5 bug)
- Don't draw hud/console, then full console background then server browser (old)
- Skip rendering glyphs for spaces when the line has been turned red (old, affected server browser)
- `/gl_texturemode` doesn't affect the texture used for framebuffers (3.5 bug, reported by hemostx)
- When using scaled framebuffer, mouse cursor co-ordinates are incorrect (3.5 bug, reported by hemostx)
- When using scaled framebuffer, screenshots use correct dimensions (3.5 bug, reported by hemostx)
- When using 2d&3d framebuffers, world-view was being over-written during 2d draw (3.5 bug, reported by hemostx)
- Fix rendering of fullbright textures that aren't luma/external-32bit textures (3.5 bug, reported by ciscon & lurq)
- `-gamma` command line option now sets `/gl_gamma` default, rather than also setting the gamma adjustment on some in-game textures (old, very old)
- `/gl_detpacklights` now controls if coronas created on detpacks in TF (very old bug, reported by Trickle)
- `/gl_textureless 1` on glsl path in classic renderer caused bmodel entities to be rendered textureless (3.5 bug, reported by hemostx)
- `/gl_outline 2` fixed (3.5 bug, reported by fourier)
- `/gl_shaftlight 0` fixed on glsl path in classic renderer (3.5 bug, reported by maniac)
- `/r_dynamic 2` was calculating too many lightmaps (3.5 bug, reported by ciscon)
- `/packet` command is now only blocked when active on server (old bug)
- Fixed bug causing read-only file handle to config being kept open, preventing backup from being taken (old bug)
- Fixed bug causing MVD-stats code to cause `/tp_loadloc` to effectively always be forced to 1 (old bug)
- Fixed bug causing multiple item timers to spawn when using `/demo_jump` (3.2 bug, reported by Milton)
- Fixed bug causing buffer-overrun if loading a corrupt/truncated .wav file (old bug)
- Fixed bug causing buffer-overrun if loading a .mdl file with invalid vertex indexes (old bug, reported by Pulseczar1)
- Fixed bug causing `rcon` timestamps to be truncated when using `/cl_crypt_rcon` and server has `/sv_timestamplen` set (old bug)
- Fixed bug causing invalid texture references to be used if `/vid_restart` issued while disconnected (3.5 bug)
- Fixed bug causing team & player sorting to be different (3.2 bug)
- Fixed bug causing stats hud display to be incorrect if cl_mvdisplayhud not set to 1 and multiview inset view enabled (old bug)
- Fixed bug causing oblique particles if sprite array dimensions were unmatched (width != height) (3.5 bug - reported by ciscon)
- Fixed bug causing flashblend to render as transparent if sprite array dimensions were unmatched (width != height) (3.5 bug - reported by ciscon)
- Fixed bug causing crash to desktop if invalid .png found (now be told of path before terminating, still not great) (old bug, #317)
- Fixed bug causing keypad binds to be executed in console when `/cl_keypad 1` set, also enter key is treated as return in console (#319, 3.2 bug)
- Fixed bug causing mouse wheel start & stop events to be fired in the same frame (3.x bug, #200)
- Fixed bug causing `/gl_brush_polygonoffset` not to work in modern-glsl renderer (3.5 bug, #404)
- Fixed bug causing powerup shell to not obey `/r_lerpframes` when aliasmodel VAO already bound pre-draw (3.5 bug, #421)
- Fixed bug causing radar texture to wrap edges (old bug, #424, reported by hemostx) 
- Fixed bug causing gunshot positions on radar to always be displayed around the origin (old bug, #425, reported by hemostx)
- Fixed bug causing sky surfaces on submodels to be displayed without z-buffer enabled (3.5 bug, #426, reported by Pulseczar1)
- Fixed bug causing submodels to not be displayed if only surfaces visible were sky textures (3.5 bug)
- Fixed bug causing sky surfaces on submodels to not be drawn as skybox, if loaded (old bug)
- Fixed bug causing corrupt rendering if HUD contained lines but no images (3.5 bug)
- Fixed bug causing corrupt HUD rendering until GLSL HUD enabled or enabled but disabled and then r_smooth option disabled (3.5 bug)
- Fixed bug causing program to terminate when recording demo and next packetsize exceeded limits (old bug)

### Ruleset-related changes

- `/gl_outline` now allowed in ruleset `thunderdome` (requested by VVD)
- `/vid_hwgammacontrol` is now forced on when using ruleset `mtfl` (3.0 bug that this was removed)
- `/enemyforceskins` descriptions in `f_ruleset` and `f_skins` responses has been clarified to specify individuals will be identifiable (reported by Ake_Vader)
- `/enemyforceskins` cannot be changed during match (old)

### Other changes

- `/cfg_backup` will now not save the config if backup cannot be taken (previous behaviour was to over-write)
- `/cfg_save` will now accept subdirectories (e.g. `/cfg_save backups/test1.cfg`)  Absolute paths are still blocked.
- `/cl_keypad 1` - keypad works as cursor keys in menu
- `/cl_keypad 2` - keypad will behave as `/cl_keypad 0` in-game, but `/cl_keypad 1` in console etc
- `/cl_net_clientport` - allows the network client port to be specified in-game (rather than just `-clientport` command line switch)
- `/cl_pext_serversideweapon` - protocol extension to move weapon selection to server (requires updated mvdsv)
- `/cl_sv_packetsync` - when using internal server & delay packet, controls if server processes packets as they are received (fixes #292)
- `/cl_weaponforgetondeath` - resets weapon to shotgun when respawning
- `/cl_weaponforgetorder 2` - sets the best weapon then falls back to sg or axe (as per `/cl_weaponhide_axe`)
- `/cl_window_caption 2` - window title will be 'ezQuake' and will not change with connection status
- `/cl_username` & `/authenticate` to support optional logins via badplace.eu (see [guide](https://github.com/ezQuake/ezquake-source/wiki/Authentication))
- `/demo_format` supported in non-Windows builds
- `/enemyforceskins 1` will search for player names in lower case (#345)
- `/gl_custom_grenade_tf` allows `/gl_custom_grenade_*` variables to be ignored when playing Team Fortress
` `/gl_mipmap_viewmodels` removed, replaced with `/gl_texturemode_viewmodels`
- `/hud_clock_content 1` changes output to show the uptime of the client
- `/hud_clock_content 2` changes output to show time connected to the server (should match `/cl_clock 1` in oldhud)
- `/hud_ammo_show_always 1` stops the hud element from being hidden when axe is selected
- `/hud_iammo_show_always 1` stops the hud element from being hidden when axe is selected
- `/in_ignore_touch_events` added - allows mouse clicks from touch input devices
- `/in_ignore_unfocused_keyb` added - should ignore keyboard events immediately after receiving input focus (linux only)
- `/menu_botmatch_gamedir` added - allows packages to customise the directory when starting a bot match
- `/menu_botmatch_mod_old` added - controls if newer features should be disabled when starting a bot match (to support fbca, lgc etc)
- `/net_tcp_timeout` added - allows timeout period to be set when connecting to QTV etc
- `/qtv_adjustbuffer 2` added - targets a specific delay period, rather than % of buffer being full
- `/r_drawflat_mode` allows textures to be shaded rather than solid color (GLSL only)
- `/register_qwurl_protocol` reports success if run from command line (or rather, run without 'quiet' as 1st argument)
- `/r_rockettrail` & `/r_grenadetrail` options requiring QMB particles degrade to '1' if QMB not initialised
- `/r_smoothalphahack 1` - during hud rendering, shader will apply lerped alpha to lerped color (behaves as per ezquake < 3.5)
- `/scr_sbar_drawarmor666` - `/hud_armor_pent_666` for oldhud (controls if '666' or armor value is shown when player has pent)
- `/scr_scoreboard_login_names` will replace player's name with login when it is sent by server
- `/scr_scoreboard_login_flagfile` maps player flags to graphics to be shown next to player's name when they are logged in
- `/scr_scoreboard_login_indicator` will be shown next to a player's name when they are logged in (if flag not available)
- `/scr_scoreboard_login_color` controls the color of a player's name when they are logged in
- `/set_ex2` command added, same functionality as `/set_ex` but doesn't resolve funchars - useful if script needs to compare value later (#428)
- `/timedemo` commands show extra info at end to try and highlight stutter (measuring worst frametimes)
- `/timedemo2` command renders demo in stop-motion at a particular fps
- `/tp_poweruptextstyle` controls if `$colored_powerups` or `$colored_short_powerups` is used in internal reporting commands
- `/tp_point` will show targets in priority order, if `/tp_pointpriorities` is enabled
- `/vid_framebuffer_smooth` controls linear or nearest filtering (thanks to Calinou)
- `/vid_framebuffer_sshotmode` controls if screenshot is of framebuffer or screen size
- `-oldgamma` command line option to re-instate old `-gamma` behaviour
- `-r-trace` command line option in debug build - writes out API calls for each frame to qw/trace/ directory (will kill fps, just for debugging)
- `-noatlas` command line option to stop the system building a 2D atlas at startup
- `+qtv_delay` command, to be used with `/qtv_adjustbuffer 2`... pauses QTV stream.  When released, QTV buffer length set to length of buffer
- GLSL gamma now supported in classic renderer
- MVD player lerping is disabled at the point of a player being gibbed (reported by hangtime)
- Player LG beams hidden during intermission (no more beams in screenshots)
- ezQuake will re-calculate normals on shared vertices as model is loaded (bug in models with normals set per-surface)
- When gameplay-related protocols are enabled but not supported by server, you will be warned during connection
- IP addresses with 5-character TLDs are detected - fixes .world addresses (thanks to bogojoker)
- Added hud element 'frametime', similar to fps but measuring (max) frametime
- Changed file-handling when viewing demos from within .zip|.gz to reduce temporary files being left on hard drive
- PNG warning messages now printed to console rather than stdout
- Added macro $timestamp, which is in format YYYYMMDD-hhmmss
- Qizmo-compressed files can be played back in Qizmo

### Build/meta

- Uses libsndfile to load .wav files, if available (if using version < 1.025, set OLD_WAV_LOADING=1 during build to use old custom .wav-loading)
- Duplicate declarations fixed (supports -fno-common, reported by ciscon)
- Updated Azure Pipelines builds to latest ubuntu/macos
- Visual Studio project, Azure Pipelines builds windows binaries (64-bit binaries are VERY beta, not recommended)
- meson build updated (out of date on 3.5)

# Changes in 3.5 (not released, based on 3.1)

### Renderer changes

- General overview
  - Major source code re-organisation to support multiple rendering paths
  - Executable can be compiled specific to a single renderer or to have multiple (`/vid_renderer` to choose)
  - GLSL-only renderer added (requires OpenGL 4.3+)
  - Classic renderer uses more advanced OpenGL features if available, including GLSL
  - Threaded driver experience should be improved (removed glGet* calls for matrices etc)
  - Reduced state changes by caching all OpenGL state in memory
  - Reduced state changes by using trying to consistently use pre-multiplied alpha blending
  - Reduced state/texture changes by using atlas for as many 2D elements as practicable
- QMB particle system
  - Reduced state changes by using same pre-multiplied alpha when loading particle textures
  - Reduced hull traces by caching distance from nearest clipping plane
  - Reduced hull traces/frame by changing physics option for some particle types
  - Reduced particle count caused by particle trails spawning multiple trails/entity and trampling on trail end-point data (SF#292?)
  - Sparks appear at furthest point of beam (not clipping platform edges - SF#143, SF#5?)
  - Coronas around torches kick in when the centre of the flame is visible, and don't fade in (SF#51)
  - `/gl_nailtrail 1` replaced, should be significantly faster
  - `/gl_particle_fire 1` re-written, should be signficantly faster (SF#51)
  - `/gl_part_fasttrails` removed
- World rendering
  - `/r_nearclip` has minimum value 2, increased from 0.1 (lower values produce too many errors on 16-bit z-buffers)
  - `/r_farclip` has maximum value 16384, no previous limit set (again, 16-bit z-buffers)
  - `/gl_scaleskytextures` added to keep scaling for sky & world textures distinct (default: off)
  - `/gl_caustics` checks the center of worldmodels (not the origin - fixes #321)
  - `/gl_turb_ripple` removed
  - `/gl_motionblur` removed
- MD3 models
  - Powerup shells support added
  - Simpleitem replacements work as expected
  - Model hints set correctly when using MD3 models
  - Skin handling improved for MD3 models
  - Lighting improved
  - Outlining supported
- Entity rendering
  - Powerup shells are consistent size regardless of model's internal scaling factor
  - ng/sng are special cases to not lerp back to the frame 0 when firing stops
  - block lerping of vertices if they come from behind the player (instead of by distance)
  - `/r_lerpframes` only lerps frames in an animation sequence (mostly smooth, stops axe-script distortion)
  - `/gl_affinemodels` removed
  - `/gl_smoothmodels` removed
  - `/gl_motiontrails` removed
  - `/gl_mipmap_viewmodels` added: controls mipmapping of textures for viewmodel (stops muzzleflash bleeding on to edge of weapon)
  - `/gl_scaleModelSimpleTextures` added: controls scaling of simple item textures (previously used `gl_scalemodeltextures`)
- HUD rendering
  - 2D elements are added to a single atlas (will cause delay at startup or when loading new images)
  - Tracker, centerprint & QTV stats are all added as HUD elements
  - Tracker splits strings up as messages are created, not when rendering
  - HUD notify no longer loses last character on line if that is also last character on screen - SF#513
  - 2D elements no longer rounded to integer coordinates before rendering - SF#397
  - `score_bar` attributes can have formatting applied (`%<10r>T` etc)
  - `/loadfont` added: proportional font support if freetype available (Windows requires freetype.dll)
  - `/fontlist` enumerates fonts available on system
  - `/gl_smoothfont` removed: now use `/r_smoothcrosshair`, `/r_smoothtext`, `/r_smoothimages`
  - `/gl_charsets_min` added: restricts to basic & cyrillic fonts during startup (vastly reduces number of attempted file opens)
  - `/r_drawdisc` added: option to disable the IO-indicator
  - `/r_tracker_inconsole` messages now have the image in the console rather than the name of the image
  - `/scr_new_hud_clear` removed, and is essentially always enabled
  - `/show_velocity_3d` removed
  - `/hud_speed_text_align 0` hides the text value (as per documentation)
- Lighting
  - Bugfix: `/gl_modulate` change now updates all lightmaps immediately
  - Software lighting (`/r_dynamic 1`) waits until all changes to the lightmap are logged, then uploads lightmap once per frame
  - Hardware lighting (`/r_dynamic 2`) available if compute shaders available - move lighting to GPU
  - Maximum number of dynamic lights has been reduced to 32, as the upper 32 weren't processed anyway due to bitflags overflow
  - If driver supports OpenGL 4.3 (or appropriate extensions), use driver-supplied preferred texture format
  - `/gl_lighting_colour` depends on `/gl_lighting_vertex`, should be more consistent
  - `/r_lightmap` removed - equivalent functionality via `/r_drawflat 1;/r_wallcolor 255 255 255;/r_floorcolor 255 255 255`
  - `/r_lightmap_lateupload` added - might give better performance for those using `-nomtex`
  - `/r_lightmap_packbytexture` added - very naive idea but reduces texture switches when rendering world (when texture arrays not available)
- Render to framebuffers
  - `/vid_framebuffer 1` - render to framebuffer
  - `/vid_framebuffer_width`/`height`/`scale` - resizes main buffer
  - `/vid_framebuffer 2` - renders the 3D scene to framebuffer but 2D elements (hud etc) at standard resolution
  - `/vid_framebuffer_palette 1` to use shader gamma/contrast etc (hardware is fastest & still default)
  - `/vid_framebuffer_hdr 1` to use float render buffer (required for `/vid_framebuffer_hdr_*` options)
  - `/r_fx_geometry 1` outlines the world based on surface normals, not surfaces.
- Modern OpenGL (GLSL)
  - `/vid_renderer 1` to enable in multi-renderer builds
  - `/vid_gl_core_profile 1` if you need/want a core profile.  Performance difference will vary per system/driver.
  - `/r_dynamic 2` to move surface lighting to GPU (cheapest 'hardware-lighting' possible, but quick for dm maps)
  - Seamless turb surface rendering
  - Expect delay when starting map, as textures/models are processed
  - Luma/skybox textures will be resized on load as required
- 'Classic' OpenGL (`/vid_renderer 0` in multi-renderer build)
  - All OpenGL state cached to reduce redundant state changes
  - Uses VBOs & batch draw calls (opengl 2.0-ish)
  - Lightmap rendering in OpenGL 1.1 fixed
  - Can use framebuffers for software gamma/palette changes (depends on extensions)
  - Skyboxes will render using cubemaps if using OpenGL 3.2+ (minor performance boost, depends on number of sky surfaces)
  - `/r_program_XXX` controls if the program will try to use GLSL for that particular aspect of rendering
  - `/r_fastturb 0` rendering performance improved
  - `/r_drawflat 2` and `/r_drawflat 3` rendering performance improved (drawflat surfaces have their own chain)
  - `/r_bloom` & `/r_drawviewmodel` used in combination greatly reduced bloom effect (advise setting `/r_bloom_alpha` lower if you now find effect overpowering). (SF#296,SF#297,SF#393)
  - `/gl_ext_arb_texture_non_power_of_two` removed (use `-nonpot` command-line option instead)
  - `/gl_maxtmu2` removed (use `-maxtmu2` instead)
- Other changes
  - `/cl_bob` affects weapon again, not just the player's head
  - `/cl_bobhead` allows `/cl_bob` settings to **only** affect the weapon model and not the player's head
  - `/gl_simpleitems_orientation 2`: simple items stay upright when camera is affected by rollangle

### Documentation
- Macros are explicitly enumerated, and have their own documentation.json file
- Command-line parameters are explicitly enumerated, and have their own documentation.json file
- Client can generate its own documentation.json files, including default values for variables

### General changes
- Bugfix: players no longer highlighted as sending VOIP when `-nosound` command-line option used
- Bugfix: invalid demo_dir caused demo browser to be empty & locked (SF#283 - related bug, anyway)
- Bugfix: files are identified as archive if extension is not lower-case (SF#313)
- Bugfix: healthdamage/armordamage reset when point of view changes (SF#146)
- Bugfix: maps with sky texture named "SKY*" instead of "sky*" could lead to memory corruption
- Bugfix: on OSX, processor information string was left pointing to stack-allocated memory
- Bugfix: `/cl_fakeshaft` is disabled during intermission, so auto-screenshots should no longer have an LG beam
- Teamfortress: block r_skincolormode working on TF or other server with skin/color forcing enabled
- Teamfortress: color in scoreboard is set based on team name if on TF server (team renaming not supported)
- The hunk is no longer used to store skins & models, only maps (no cached data being flushed and reloaded)
- In competitive rulesets, logging console to disk is disabled during match
- Server: PR1 strings overflowing integer offset stored in lookup table with negative integer instead (Windows 64-bit)
- Some commands have been renamed dev_* and are only available when running with `-dev` on command line (req by dopeskillz)
- Filesystem startup no longer lists .pak files being opened, lists `/path` after completion instead (SF#354)
- `/file_browser_sort_archives` added - if set, archives are still at top of list, but sorted in same way files are (SF#273)
- `/cl_startupdemo` shouldn't conflict with `/cl_onload` options (SF#467,SF#468)
- `/cl_clock_format` can be changed when connected to server and has same values/logic as `/hud_clock_format`
- `/scr_shownick_order` can be changed when connected to server
- `/timedemo` automatically raises console as demo starts
- Context creation changed, if context can't be created msaa/24bit-depth/hw-accel/srgb then various fallbacks attempted (previously only msaa), rather than falling back directly to software rendering
- cvars limited by ruleset max/min values weren't checked during application-startup
- toggling console exits hud_editor mode
- default.cfg executes on config load/reset (default.cfg in id1/pak0.pak is ignored)

### Changeover issues/notes

- If you are using particle explosions/effects and notice small artifacts around the smoke etc, check your particlefont.png and make sure it doesn't have any stray non-alpha pixels around 64-pixel boundaries.  ezQuake 3.5 takes the particle font and splits it into component textures, so any errors around the edges will be more visible (reported with config set to `/r_explosionType 1`, `/gl_part_explosions 1`)

---

# Changes in 3.2

### Improvements

- Windows static libary: SDL upgraded from 2.0.5 => SDL 2.0.8 (WASAPI sound by default)
- Windows static libary: zlib upgraded from 1.2.8 => 1.2.11
- Windows static libary: libcurl upgraded from 7.37.1 => 7.61.0
- Windows static libary: expat upgraded from 2.1.0 => 2.2.6
- Windows static libary: jansson upgraded from 2.7.0 => 2.11
- `mvd_moreinfo` improvements (older servers/demos, still does a lot of guesswork)
  - only print location name on pickup announcements when more than one item on the map
  - list location of items if > 1 of that item on the map
  - if raw item name is in location name, only print location name when announcing item pickups
  - announces when the pickup is from a pack, rather than the item
  - location of item is stored when simple clock entries are added
  - store location of megas as players pick them up (maximum 4 megas)
- `itemsclock` improvements (requires server to be running KTX 1.38+)
  - if KTX pickup/drop/spawn notifications are available they will be used (accurate, no .mvd guessing)
  - items-clock can have fixed entries for each major item on the map
  - items-clock can be filled with entries as match starts (`/mvd_autoadd`)
  - user can assign name major items on the map or remove (`/mvd_name_item`, `/mvd_remove_item`)
  - trigger `f_demomatchstart` fired at match start
  - `/mvd_list_items` will generate example list of items for config file generation
  - `hud_itemsclock_style 5`: lists backpacks, a space then items, in format "`itemname` : `info`"
  - backpacks can be added to the items list (`/hud_itemsclock_backpacks`), will be listed with the person who dropped the pack and who picked it up before disappearing
- In-game backpacks can be colored according to their contents (`/gl_custom_rlpack_color`, `/gl_custom_lgpack_color`) (QTV/MVD only, KTX 1.38+ only)
- Failed [ServeMe] connections will be removed from the playerlist (caused by `nospecs`)
- Teaminfo hud element can include frags (%f/%F)
- In KTX race mode, other players can be silenced with `/s_silent_racing` cvar
- Added macros $team1 & $team2 to access the first two teams on the server (#256)
- Demo capture can produce animated .png files (`/sshot_format apng`)
- `/ignore` can now contain a regular expression
- `f_qtvfinalscores` trigger fired when `//finalscores` notification detected in qtv stream
- `/cl_mvinset` position & size can be set by user (`/cl_mvinset_size_x` sets relative size compared to screen)
- `/r_lerpframes` is no longer disabled when using multiview
- Team fortress grenade models should be flagged as grenades now (`/gl_custom_grenade_color` etc should work)

### Ruleset changes

- `thunderdome` allows simple-item backpacks & texture replacements for backpacks
- `thunderdome`, `smackdown` & `qcon` all block hud group-picture changes during the match
- `thunderdome`, `smackdown` & `qcon` all block `/gl_outline` as it can lead to seeing items through walls
- `r_skincolormode`, `r_enemyskincolor`, `r_teamskincolor` blocked in TF games or if server blocks skin/color forcing
- Team Fortress: scoreboard always color players by their teamname

### Bugfixes

- Loading a single player save file on 64-bit systems may cause crash shortly after resuming play (#297)
- Dynamic lights correctly set during .dem playback (#298)
- When viewing pre-selected weapon, the weapon only switches once the animation frame goes back to non-firing (#182)
- `/r_viewmodellastfired` no longer shows last-fired weapon when starting match, moving to a new map or dying while not firing
- QTV URLs can now include the QTV password (must have '/' separating command and password) (#295)
- Gamma on screenshots was incorrect when system had more than 256 gamma ramps (linux) (#296)
- BSP2 maps: fix crash when turb surfaces outside -9999/+9999 bounds (#323)
- `/ignore $name` (allow the player to ignore their own messages) produced output during certain teamplay messages (#257)
- `/in_raw 0` - mouse input fixed (#308)
- `/scr_cursor_sensitivity` is now functional again
- Toggling multiview with inset window should alway keeps the current player in the primary view
- Multiview was trying to cause increase in frames rendered (`/cl_maxfps` multiplied for each extra view)
- Multiview inset window border now correctly set regardless of console:screen ratio
- `/mvd_moreinfo` - multiple bugfixes
  - fix incorrect location name when reporting item pickups
  - pack drops are announced when the player dies, not when they respawn
  - weapon dropped now based on active weapon when dying, not last weapon fired
  - {} white-text wasn't stripped from item name cvars
- Halflife maps had wrong bounding box sizes
- `/hud_sortrules_includeself 0` could lead to player not being included in visible list

### Other changes

- Removed functionality to control external MP3 player through ezQuake

### Build/meta

- OSX: Dialog to find .pak files on initial installation could point to 'my computer' location on Mojave
- Added azure-pipelines.yml
- Server: can now be built without USE_PR2 defined
- .exe exports fields that AMD/nvidia drivers can use to always use accelerated graphics
