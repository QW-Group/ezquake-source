Leaving these here as bugs between dev versions of 3.6 aren't in the release notes.

### Changes from alpha8=>alpha9 (July 13th, ongoing)

- On startup (after `autoexec.cfg` executed), a `vid_restart`/`s_restart` will be issued if any latched variables were changed (#458)
- Fixed bug causing off-by-one error when drawing rectangle outlines (3.5 bug, reported by Matrix, #536)
- Detection of AMD drivers .13399 wasn't being working in classic renderer (#416)
- Added `/gl_smoothmodels` back in, (requested by Repast via [quakeworld.nu](https://www.quakeworld.nu/forum/topic/7508/why-is-the-command-glsmoothmodels-r))
- `/in_raw 0` behaviour changed to hopefully fix on MacOS and not break Windows
- Fixed `/r_drawflat 1`, `/r_drawflat_mode 0` affecting ammo boxes etc in classic renderer

### Changes from alpha7=>alpha8 (Feb 9th => July 13th)

- Fix increased memory buffer causing slow speed of demo_jump (#453)
- Fix turbalpha causing rendering artifacts on non-vissed maps (#473)
- Fix bug in SZ_Print writing to invalid memory (oldest bug in ezQuake?)
- Fix KTX autotrack jumping too often to players with numbers in name
- Fix cl_delay_packet causing problems changing maps on internal server (#488)
- Fix brushmodel entities (health etc) disappearing with r_drawflat 1 in classic renderer (#558)
- Fix hardware gamma being updated too often during 
- Fix rendering fps affecting speed of turning left/right using keyboard
- Fix bug causing .loc files to not load during demo playback
- Fix weaker r_drawflat_mode on classic renderer
- Fix incorrectly offset mouse cursor image
- Fix rendering of right-aligned tracker when using proportional fonts (#543)
- Fix no background on power bar when using hud_frags_fliptext
- Fix using invalid lightmap when moving to a map with a higher number of lightmaps (#540)
- Fix invalid rendering when using r_drawflat and moving to a map with higher number of lightmaps (#540)
- Fix invalid texture2D() call when using modern glsl
- Fix playback of MVDs recorded in FTE where first packet read into frame 1 rather than frame 0 (#551)
- Fix drawflat not affecting alpha-surfaces when using glsl
- Fix using incorrect lightmap when drawflat rendering in immediate-mode OpenGL
- Fix alpha surfaces rendered opaque (or fully transparent) based on top-left of image
- Fix issue rendering on AMD core profile due to VBO alignment issue
- Fix issue causing r_dynamic 2 to render fullbright lightmap if r_fullbright valid at map load
- Added workaround for rendering issues with AMD 4.5.13399 drivers (#416)
- Added workaround for OpenGL errors elsewhere causing ezQuake to think texture arrays not created (#475)
- Help with missing player models on MVDs using extended FTE model limits (#551)
- Added /gl_turb_effects, controls if nails/shotgun particles spawn bubbles underwater (#553)
- Added /gl_turb_fire, controls if explosions spawn bubbles underwater (#553)
- Added +fire_ar, an anti-rollover +fire
- Support for chaticons not 256x256 (#477)
- hud_ammo more consistent when using non-ammo weapons (#206)
- hud_fps_drop accepts negative values, interpretted as related to cl_maxfps (#556)
- hud_teaminfo headers are now optional
- nquake's default.cfg is ignored (is executed by different script)
- Improved server-side weapon switching (still incomplete)
- When paused, mouse input is disabled, not hidden until unpaused
- Internal server is synced with mvdsv (circa February)
- Server browser shows info at lower resolutions
- Menu has options for gl_modulate, turn left/right, cvar descriptions match /describe
- Build support: improve ARM build support, mingw clang, FreeBSD/powerpc 32-bit, ubuntu 20.04 dockerfile, FDK AppStream

Thanks to all contributors and those who assisted with bug reporting.
