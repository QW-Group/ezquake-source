# ezQuake HUD Modernization Plan

## Overview

A phased plan to modernize how players interact with and customize their HUDs in ezQuake. Divided into 5 sprints over ~12 weeks, ordered so each builds on the previous.

Key discovery: the `ez_controls` GUI widget library (buttons, sliders, windows, scrollbars, listviews) is fully implemented in `src/ez_*.c` but disabled behind `#if 0` in `hud_editor.c`. This is the single biggest unrealized opportunity.

---

## Sprint 1: Quick Wins (Week 1-2)

### 1a. Arrow-key nudging in HUD editor
- **File:** `src/hud_editor.c` (lines 2608-2619, TODO blocks)
- **Size:** S
- **What:** Replace four TODO blocks in `HUD_Editor_Key()` with calls to `HUD_Editor_Move(dx, dy, selected_hud)`. Arrow = 1px, Shift+Arrow = 10px.
- **Dependencies:** None
- **Acceptance:** Arrow keys move selected element. Shift modifier for larger steps.

### 1b. Expose tracker cvars in options menu
- **File:** `src/menu_options.c` (after line 1073, tracker section)
- **Size:** S
- **What:** Add `ADDSET_NUMBER`/`ADDSET_BOOL` entries for:
  - `r_tracker_row_spacing` (NUMBER, 0-20, step 1)
  - `r_tracker_reverse` (BOOL)
  - `r_tracker_color_quadfrag` (STRING)
  - `r_tracker_color_quadfrag_colorfix` (BOOL)
  - `r_tracker_color_quadfrag_noicon` (BOOL)
- **Dependencies:** None (cvars already registered in vx_tracker.c)
- **Acceptance:** Settings appear in Options > Tracker Messages (Advanced). Changes take immediate effect.

### 1c. Add hud_planmode toggle to menu
- **File:** `src/menu_options.c` (HUD section ~line 1040)
- **Size:** S
- **What:** Add `ADDSET_BOOL("HUD Plan Mode", hud_planmode)`. Note: already auto-enabled when entering editor (lines 2494-2495). This just makes it discoverable in the menu for standalone use.
- **Dependencies:** None
- **Acceptance:** Toggle appears in Options > HUD section.

### Sprint 1 Deliverable
Better editor usability + new settings visible in menus.

---

## Sprint 2: Preset System + Reset (Week 3-4)

### 2a. `hud_preset save/load/list` commands
- **File:** `src/config_manager.c` (near `DumpHUD_f` at line 1117)
- **Size:** M
- **What:** New `HUD_Preset_f()` command handler.
  - `hud_preset save <name>` -- reuses `DumpHUD()` logic, writes to `configs/hud_presets/<name>.cfg`
  - `hud_preset load <name>` -- `Cbuf_AddText("exec configs/hud_presets/<name>.cfg")`
  - `hud_preset list` -- enumerate directory, print names
  - Register in `ConfigManager_Init()` at line 1224
- **Dependencies:** None
- **Acceptance:** Save creates file, load restores config, list shows available presets. Survives restarts.

### 2b. Per-element `hud_reset <element>` command
- **File:** `src/hud.c` (near `HUD_Func_f` at line 153)
- **Size:** S
- **What:** New `HUD_Reset_f()`.
  - Parse element name from `Cmd_Argv(1)`, find via `HUD_Find()`
  - Iterate standard cvars + `params[]` array
  - Call `Cvar_Set(cvar, cvar->defaultvalue)` for each
  - No args = reset ALL elements
- **Dependencies:** None
- **Acceptance:** `hud_reset fps` restores FPS element to defaults. `hud_reset` resets everything.

### Sprint 2 Deliverable
Users can save, name, switch, and reset HUD configurations.

---

## Sprint 3: Property Panel via ez_controls (Week 5-7)

### 3a. Enable ez_controls and build property panel
- **Files:** `src/hud_editor.c` (lines 2631-2792), `src/ez_*.c` (9 widget files)
- **Size:** L
- **What:**
  1. Remove `#if 0` / `#endif` around lines 2631-2792 in `HUD_Editor_Init()`
  2. Replace demo widget tree with a docked right-side panel
  3. Create `ez_window_t` with `ez_label_t` + `ez_slider_t` pairs for selected element's params
  4. Wire slider `OnSliderPositionChanged` callbacks to `Cvar_SetValue()`
  5. Dynamic repopulation when `selected_hud` changes
  6. `EZ_tree_EventLoop()` at line 2451 already runs during editor draw
- **Risk:** ez_controls may have bitrotted against current rendering APIs. Needs compile testing.
- **Dependencies:** Sprint 1a (editor already improved)
- **Acceptance:** Property panel appears on right side. Selecting element populates params. Sliders change cvars live.

### Sprint 3 Deliverable
Full visual property editing in the HUD editor -- the big unlock.

---

## Sprint 4: Share Codes + Undo (Week 8-10)

### 4a. Shareable HUD codes
- **File:** `src/config_manager.c`
- **Size:** M
- **What:**
  - `hud_sharecode export` -- collect non-default HUD cvars, compress with zlib (already linked), base64 encode, print to console
  - `hud_sharecode import <code>` -- base64 decode, decompress, parse name=value pairs, apply via `Cvar_Set()`
  - Need small base64 encoder/decoder (~30 lines each)
  - Target: under 500 chars for typical configs
- **Dependencies:** None (but complements Sprint 2 preset system)
- **Acceptance:** Export prints compact string. Import restores full HUD config. Codes work across clients.

### 4b. Undo stack in editor
- **File:** `src/hud_editor.c`
- **Size:** M
- **What:**
  - Ring buffer of 32 snapshots, each storing `{cvar_name, old_value}` pairs
  - Push snapshot before any editor change (move, resize, property panel changes)
  - Ctrl+Z in `HUD_Editor_Key()` pops and restores via `Cvar_Set()`
  - Clear stack on editor exit
- **Dependencies:** Sprint 1a (key handler already modified)
- **Acceptance:** Move element, Ctrl+Z restores position. Works up to 32 levels deep.

### Sprint 4 Deliverable
Discord-friendly config sharing + safe experimentation with undo.

---

## Sprint 5: Context-Aware Auto-HUD (Week 11-12)

### 5a. Generalize MVD auto-HUD to user contexts
- **Files:** `src/hud_common.c` (extends `HUD_AutoLoad_MVD()` at line 425), `src/config_manager.c`
- **Size:** M
- **What:**
  - Add string cvars: `hud_auto_duel`, `hud_auto_4on4`, `hud_auto_spec` (hold preset names)
  - On game state changes (map load, spectator toggle), check context and exec named preset
  - Save/restore using same pattern as MVD autohud (temp config save, exec context config, restore on exit)
  - Reuses Sprint 2 preset infrastructure
- **Dependencies:** Sprint 2a (preset system)
- **Acceptance:** `hud_auto_spec myhud` loads that preset when entering spectator. Original restored on exit.

### Sprint 5 Deliverable
HUD automatically adapts to game context.

---

## Timeline

```
Week  1-2   Sprint 1: Arrow nudging + menu exposure + planmode toggle
Week  3-4   Sprint 2: Preset save/load/list + hud_reset
Week  5-7   Sprint 3: ez_controls property panel
Week  8-10  Sprint 4: Share codes + undo stack
Week 11-12  Sprint 5: Context-aware auto-HUD
```

## Out of Scope (Tier 3 -- Standalone Projects)

These don't touch the C codebase and can be pursued independently:

- **Web-based HUD configurator** -- HTML/JS app that mock-renders HUD elements, exports .cfg files. Can start after Sprint 2 locks down export format.
- **Community HUD gallery** -- Web page on quakeworld.nu or GitHub repo. Screenshots + .cfg downloads.
- **ImGui settings overlay** -- Major rendering integration, multi-month effort.

## Key Source Files Reference

| File | Purpose |
|------|---------|
| `src/hud.c` | Core HUD: registration, positioning, drawing, linked list |
| `src/hud_editor.c` | Drag editor, key handler, ez_controls init |
| `src/hud_common.c` | Common elements, MVD autohud, planmode |
| `src/hud.h` | hud_t struct, flags, macros |
| `src/menu_options.c` | Options menu, ADDSET_* macros, tracker section |
| `src/settings_page.h` | Menu macro definitions |
| `src/config_manager.c` | hud_export, DumpHUD, config save/load |
| `src/cvar.c` / `src/cvar.h` | Cvar system, defaultvalue field |
| `src/ez_*.c` (9 files) | GUI widget library (disabled but complete) |
| `src/vx_tracker.c` | Tracker system, 39+ cvars |
