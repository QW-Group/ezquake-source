# Changes in 3.2-dev

### Improvements

- Windows static libary: SDL upgraded from 2.0.5 => SDL 2.0.8 (WASAPI sound by default)
- Windows static libary: zlib upgraded from 1.2.8 => 1.2.11
- Windows static libary: libcurl upgraded from 7.37.1 => 7.61.0
- Windows static libary: expat upgraded from 2.1.0 => 2.2.6
- Windows static libary: jansson upgraded from 2.7.0 => 2.11
- `mvd_moreinfo`/`itemsclock` improvements
  - only print location name on pickup announcements when more than one item on the map
  - list location of items if > 1 of that item on the map
  - if raw item name is in location name, only print location name when announcing item pickups
  - announces when the pickup is from a pack, rather than the item
  - location of item is stored when simple clock entries are added
  - store location of megas as players pick them up (maximum 4 megas)
- Itemsclock entries can be static, tied to a particular entity and named by the user (requires KTX 1.38)
- Failed [ServeMe] connections will be removed from the playerlist (caused by `nospecs`)
- Teaminfo hud element can include frags (%f/%F)
- In KTX race mode, other players can be silenced with `s_silent_racing` cvar
- Added macros $team1 & $team2 to access the first two teams on the server
- `/ignore` can now contain a regular expression
- `f_qtvfinalscores` trigger fired when `//finalscores` notification detected in qtv stream
- In multiview, the position of the inset view (`cl_mvinset`) can be controlled

### Bugfixes

- Loading a single player save file on 64-bit systems may cause crash shortly after resuming play (#297)
- Dynamic lights correctly set during .dem playback (#298)
- When viewing pre-selected weapon, the weapon only switches once the animation frame goes back to non-firing (#182)
- QTV URLs can now include the QTV password (#295)
- Gamma on screenshots was incorrect when system had more than 256 gamma ramps (linux) 
- `/ignore $name` (allow the player to ignore their own messages) produced output during certain teamplay messages (#257)
- `mvd_moreinfo` - multiple bugfixes
  - fix incorrect location name when reporting item pickups
  - pack drops are announced when the player dies, not when they respawn
  - weapon dropped now based on active weapon when dying, not last weapon fired
  - {} white-text wasn't stripped from item name cvars

### Build/meta

- OSX: Dialog to find .pak files on initial installation could point to 'my computer' location on Mojave
- Added azure-pipelines.yml
- Server: can now be built without USE_PR2 defined


