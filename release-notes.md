# Changes in 3.6

3.6 merges 3.2 and the un-released 3.5 - changes listed here are relative to those versions.

(if viewing on github, download links are at bottom of page)

### Bugs affecting older clients

- Fix crash to desktop when finding best routes to server and number of proxies too high (old bug, also fixed in 3.5-alpha24 and 3.2.1)
- Don't draw hud/console, then full console background then server browser (old)
- Skip rendering glyphs for spaces when the line has been turned red (old, affected server browser)
- `-gamma` command line option now sets `/gl_gamma` default, rather than also setting the gamma adjustment on some in-game textures (old, very old)
- `/gl_detpacklights` now controls if coronas created on detpacks in TF (very old bug, reported by Trickle)
- `/packet` command is now only blocked when active on server (old bug)
- `/scr_newhud` elements are hidden when free-floating in spectator mode (old, thanks to hemostx)
- `/scr_autoid` only shows extended info when in mvd playback (old)
- Fixed bug causing read-only file handle to config being kept open, preventing backup from being taken (old bug)
- Fixed bug causing MVD-stats code to cause `/tp_loadloc` to effectively always be forced to 1 (old bug)
- Fixed bug causing buffer-overrun if loading a corrupt/truncated .wav file (old bug)
- Fixed bug causing buffer-overrun if loading a .mdl file with invalid vertex indexes (old bug, reported by Pulseczar1)
- Fixed bug causing `rcon` timestamps to be truncated when using `/cl_crypt_rcon` and server has `/sv_timestamplen` set (old bug)
- Fixed bug causing stats hud display to be incorrect if cl_mvdisplayhud not set to 1 and multiview inset view enabled (old bug)
- Fixed bug causing crash to desktop if invalid .png found (now be told of path before terminating, still not great) (old bug, #317)
- Fixed bug causing radar texture to wrap edges (old bug, #424, reported by hemostx) 
- Fixed bug causing gunshot positions on radar to always be displayed around the origin (old bug, #425, reported by hemostx)
- Fixed bug causing sky surfaces on submodels to not be drawn as skybox, if loaded (old bug)
- Fixed bug causing program to terminate when recording demo and next packetsize exceeded limits (old bug)
- Fixed bug causing spectator issue when tracking player on server, reconnecting and then tracking again (old bug)
- Fixed bug causing `show net` to not show valid information when playing back .qwd files (old bug)
- Fixed bug causing 'crouch' step-smoothing to kick in when respawning at a (very) nearby spawn point (old bug)
- Fixed bug causing player to suddenly rotate when game was unpaused as mouse movement during pause suddenly took effect (old bug)
- Fixed bug causing other players to move around on screen when game is paused (old bug)
- Fixed bug causing `/cl_earlypackets` to cause frames to be rendered at teleport entrance position but player pointing at teleport exit angle (old bug)
- Fixed bug causing parts of the screen covered by turb surfaces to not be redrawn when translucent with no map support (#473, old bug, reported by HangTime)
- Fixed bug causing writing to invalid memory when buffer overflows when printing string (very old bug)
- Fixed bug causing client to keep reading from stream after `/map <mapname>` while connected to QTV (linked to #488, reported by HangTime)
- Fixed bug causing client to prioritise player with userid in name rather than the userid in `/track`, `/ignore`, `/unignore` (affected autotrack - reported by andeh, exploited by an1k vs userID 1 henu)
- Fixed bug causing client to receive playerinfo packet before knowing which protocol extensions are enabled when using `/cl_delay_packet` on local server (#488, reported by pattah)

### Bugs which affected 3.x

- Space prefix in console didn't send remaining text as chat (3.2 bug, reported by ciscon, BLooD_DoG & VVD)
- Fixed bug causing multiple item timers to spawn when using `/demo_jump` (3.2 bug, reported by Milton)
- Fixed bug causing team & player sorting to be different (3.2 bug)
- Fixed bug causing keypad binds to be executed in console when `/cl_keypad 1` set, also enter key is treated as return in console (#319, 3.2 bug)
- Fixed bug causing mouse wheel start & stop events to be fired in the same frame (3.x bug, #200)
- Fixed bug causing friendly/enemy teams to switch mid-demo when using `teamlock 1` (3.2 bug)
- Fixed bug causing `score_enemy`/`score_difference` hud elements to use next player depending on sort rules, rather than best opponent (#469, 3.2 bug, reported by doomie)
- Fixed bug causing "Unset entity number" message & program termination as entnum overwritten from local baselines after avoiding buffer overflow in previous frame (reported during sdCup2 on Discord)
- Fixed bug causing `/cl_mvinsetcrosshair 1` crosshair to not move with the inset view (#462, 3.2 bug, reported by ptdev)
- Fixed bug causing `/scr_cursor_iconoffset_x` & `/scr_cursor_iconoffset_y` to have no effect (3.x bug, fix by ciscon)

#### Bugs which affected 3.5 (typically related to renderer rewrite)

- When using non-desktop resolution and running fullscreen, image was cropped (3.5 bug, reported by bgnr)
- HUD renders circles again (affects speed2, radar, itemsclock with certain settings) (3.5 bug, reported by numerous people)
- Fixed visual artifact around classic particles in classic OpenGL (3.5 bug, reported by hemostick)
- OpenGL viewport state invalidated if window is resized (3.5 bug, reported by blindcant)
- NULL pointer dereference if OpenGL implementation didn't support buffers (3.5 bug)
- Corrupt lightmap rendering when lots of options enabled for world-drawing GLSL (3.5 bug)
- Gamma adjustment on screenshots led to them being colored incorrectly (3.1/3.5 bug re-introduced during merge)
- Stop levelshot (console background) from potentially being deleted too early (3.5 bug)
- `/gl_texturemode` doesn't affect the texture used for framebuffers (3.5 bug, reported by hemostx)
- When using scaled framebuffer, mouse cursor co-ordinates are incorrect (3.5 bug, reported by hemostx)
- When using scaled framebuffer, screenshots use correct dimensions (3.5 bug, reported by hemostx)
- When using 2d&3d framebuffers, world-view was being over-written during 2d draw (3.5 bug, reported by hemostx)
- Fix rendering of fullbright textures that aren't luma/external-32bit textures (3.5 bug, reported by ciscon & lurq)
- `/gl_textureless 1` on glsl path in classic renderer caused bmodel entities to be rendered textureless (3.5 bug, reported by hemostx)
- `/gl_outline 2` fixed (3.5 bug, reported by fourier)
- `/gl_shaftlight 0` fixed on glsl path in classic renderer (3.5 bug, reported by maniac)
- `/r_dynamic 2` was calculating too many lightmaps (3.5 bug, reported by ciscon)
- Fixed bug causing invalid texture references to be used if `/vid_restart` issued while disconnected (3.5 bug)
- Fixed bug causing oblique particles if sprite array dimensions were unmatched (width != height) (3.5 bug - reported by ciscon)
- Fixed bug causing flashblend to render as transparent if sprite array dimensions were unmatched (width != height) (3.5 bug - reported by ciscon)
- Fixed bug causing `/gl_brush_polygonoffset` not to work in modern-glsl renderer (3.5 bug, #404)
- Fixed bug causing powerup shell to not obey `/r_lerpframes` when aliasmodel VAO already bound pre-draw (3.5 bug, #421)
- Fixed bug causing sky surfaces on submodels to be displayed without z-buffer enabled (3.5 bug, #426, reported by Pulseczar1)
- Fixed bug causing submodels to not be displayed if only surfaces visible were sky textures (3.5 bug)
- Fixed bug causing corrupt rendering if HUD contained lines but no images (3.5 bug)
- Fixed bug causing corrupt HUD rendering until GLSL HUD enabled or enabled but disabled and then r_smooth option disabled (3.5 bug)
- Fixed bug causing crash with old-hud rendering and `r_damagestats` enabled (3.5 bug, #432, reported by eb)
- Fixed bug causing atlas textures to not be rendered when using ATI drivers reporting version x.y.13399 (seems to be driver issue? #416)
- Fixed bug causing rendering issue when using `/gl_contrast` to brighten screen in classic renderer (3.5 bug, #442, reported by hammer)
- Fixed bug causing no rendering of aliasmodels when VAs not supported but `/gl_program_aliasmodels` set (3.5 bug)
- Fixed bug causing geometry-edge overlay to not be aligned when using framebuffer scaling (3.5 bug, reported by hemostx)
- Fixed bug causing crash when tracker fills up when minimised (3.5 bug)
- Fixed bug causing incorrect texture to be bound when rendering once new texture created (#452, 3.5 bug, reported by pattah)
- Fixed bug causing differences in rendering md3 viewmodels in glsl vs std renderer (explosion surface is additive - same awful hack until we support shaders) (3.5 bug but 3.2 was even worse)
- Fixed bug causing aliasmodels to be rendered with the normal map overlaid instead of caustics texture (#457, 3.5 bug, reported by hammer)
- Fixed bug causing geometry outlines to be rendered incorrectly in sub-views when multiview enabled (3.5 bug)

### Ruleset-related changes

- `/gl_outline` now allowed in ruleset `thunderdome` (requested by VVD)
- `/enemyforceskins` descriptions in `f_ruleset` and `f_skins` responses has been clarified to specify individuals will be identifiable (reported by Ake_Vader)
- `/enemyforceskins` cannot be changed during match (old)
- `/cl_rollangle` is now limited to a maximum of 5 when using rulesets `smackdown`, `qcon` & `thunderdome` (requested by VVD)
- `/gl_outline` changed to render by projecting backfaces away by surface normal (rather than lines) - to be tested
- `/vid_hwgammacontrol` is now forced on when using ruleset `mtfl` (3.0 bug that this was removed)
- sign of value movement speed cvars is ignored (old - used to create `/cl_idrive`-like movement scripts)

### Debugging protocol changes (weapon scripts)

- `/cl_debug_weapon_send`: sends information about client-side weapon selection, which is stored in .mvd on supported servers
- `/cl_debug_weapon_view`: views the client-side weapon debugging messages, if stored in .mvd

- `/cl_debug_antilag_send`: sends location of opponents, which is stored in .mvd on supported servers
- `/cl_debug_antilag_view`: chooses which location is used when rendering players on supported .mvd/qtv streams (0 = normal, 1 = antilag-rewind, 2 = client position)
- `/cl_debug_antilag_ghost`: allows rendering a translucent copy of player position on support .mvd/qtv streams (0 = normal, 1 = antilag-rewind, 2 = client position)
- `/cl_debug_antilag_lines`: chooses if lines are drawn between the different player positions on supported .mvd/qtv streams

### Other changes

- `/cfg_backup` will now not save the config if backup cannot be taken (previous behaviour was to over-write)
- `/cfg_save` will now accept subdirectories (e.g. `/cfg_save backups/test1.cfg`)  Absolute paths are still blocked.
- `/cl_c2sdupe` will send duplicate packets to the server (a little like Qizmo) (credit to mushi & Spike)
- `/cl_demo_qwd_delta` will create smaller .qwd files but older clients will be unable to play them back (enabled by default)
- `/cl_keypad 1` - keypad works as cursor keys in menu
- `/cl_keypad 2` - keypad will behave as `/cl_keypad 0` in-game, but `/cl_keypad 1` in console etc
- `/cl_delay_packet_target` - like cl_delay_packet, but half delay is applied to outgoing and the incoming delay is flexible to match the value
- `/cl_net_clientport` - allows the network client port to be specified in-game (rather than just `-clientport` command line switch)
- `/cl_pext_serversideweapon` - protocol extension to move weapon selection to server (requires updated mvdsv)
- `/cl_sv_packetsync` - when using internal server & delay packet, controls if server processes packets as they are received (fixes #292)
- `/cl_weaponforgetondeath` - resets weapon to shotgun when respawning
- `/cl_weaponforgetorder` - now sets the best weapon then falls back to sg or axe (as per `/cl_weaponhide_axe`)
- `/cl_window_caption 2` - window title will be 'ezQuake' and will not change with connection status
- `/cl_username` & `/authenticate` to support optional logins via badplace.eu (see [guide](https://github.com/ezQuake/ezquake-source/wiki/Authentication))
- `/demo_format` supported in non-Windows builds
- `/demo_jump` during demo playback should now be faster (#453)
- `/enemyforceskins 1` will search for player names in lower case (#345)
- `/gl_custom_grenade_tf` allows `/gl_custom_grenade_*` variables to be ignored when playing Team Fortress
` `/gl_mipmap_viewmodels` removed, replaced with `/gl_texturemode_viewmodels`
- `/hud_clock_content 1` changes output to show the uptime of the client
- `/hud_clock_content 2` changes output to show time connected to the server (should match `/cl_clock 1` in oldhud)
- `/hud_ammo_show_always 1` (and equivalent `iammo`) shows current ammo when non-ammo weapon is selected (#206, suggested by VianTORISU)
-` /hud_keys` supports user commands hidden in .mvd files & qtv streams
- `/in_ignore_touch_events` added - allows mouse clicks from touch input devices
- `/in_ignore_unfocused_keyb` added - should ignore keyboard events immediately after receiving input focus (linux only)
- `/menu_botmatch_gamedir` added - allows packages to customise the directory when starting a bot match
- `/menu_botmatch_mod_old` added - controls if newer features should be disabled when starting a bot match (to support fbca, lgc etc)
- `/net_tcp_timeout` added - allows timeout period to be set when connecting to QTV etc
- `/qtv_adjustbuffer 2` added - targets a specific delay period, rather than % of buffer being full
- `/r_drawflat_mode` allows textures to be shaded rather than solid color (GLSL only)
- `/r_tracker_name_remove_prefixes` is now `/hud_name_remove_prefixes` and affects `teaminfo` as well (also more efficient, #471, req by HangTime)
- `/r_rockettrail` & `/r_grenadetrail` options requiring QMB particles degrade to '1' if QMB not initialised
- `/r_smoothalphahack 1` - during hud rendering, shader will apply lerped alpha to lerped color (behaves as per ezquake < 3.5)
- `/register_qwurl_protocol` reports success if run from command line (or rather, run without 'quiet' as 1st argument)
- `/scr_sbar_drawarmor666` - `/hud_armor_pent_666` for oldhud (controls if '666' or armor value is shown when player has pent)
- `/scr_damage_hitbeep` - will play `dmg-notification.wav` when current player does damage (on supported .mvd files & qtv streams)
- `/scr_damage_floating` - will display floating damage numbers when current player does damage (on supported .mvd files & qtv streams)
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
- `-r-verify` command line option in debug build - regularly downloads GL state from driver, for use with -r-trace
- `-noatlas` command line option to stop the system building a 2D atlas at startup
- `-r-nomultibind` command line option to disable calls to glBindTextures
- `+qtv_delay` command, to be used with `/qtv_adjustbuffer 2`... pauses QTV stream.  When released, QTV buffer length set to length of buffer
- On startup, `default.cfg` is executed before config is loaded (nQuake's default.cfg will be ignored)
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
- Qizmo-compressed files can be played back using Qizmo on linux
- When watching mvd/qtv, `/record` & `/stop` become `/mvdrecord` and `/mvdstop` respectively (suggested by hangtime)
- Internal server has been updated to match latest mvdsv codebase
- Removed chaticons limitation where source image had to be 256x256 pixels (#477, reported by timbergeron)
- Demo signoff messages are no longer random

### Build/meta

- Uses libsndfile to load .wav files, if available (if using version < 1.025, set OLD_WAV_LOADING=1 during build to use old custom .wav-loading)
- Duplicate declarations fixed (supports -fno-common, reported by ciscon)
- Updated Azure Pipelines builds to latest ubuntu/macos
- Visual Studio project, Azure Pipelines builds windows binaries (64-bit binaries are VERY beta, not recommended)
- meson build updated (out of date on 3.5)
- Fixed build on FreeBSD/powerpc64 (thanks to pkubaj)
- Remove unsupported 666-deflect message from fragfile.dat (reported by eb, #461)
