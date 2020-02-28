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
- `hud_sortrules_includeself 0` could lead to player not being included in visible list

### Other changes

- Removed functionality to control external MP3 player through ezQuake

### Build/meta

- OSX: Dialog to find .pak files on initial installation could point to 'my computer' location on Mojave
- Added azure-pipelines.yml
- Server: can now be built without USE_PR2 defined
- .exe exports fields that AMD/nvidia drivers can use to always use accelerated graphics
