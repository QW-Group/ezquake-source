# Anti-Lag / Lag Compensation Report (idTech2, ezquake-source)

Date: 2026-03-04
Branch context: `antilag`

## 1) Executive Summary

Your branch already contains meaningful anti-lag infrastructure: per-client history buffers, command-time based target selection, rewound collision checks, projectile hooks, and demo/debug introspection.

The current system is a solid foundation, but it is not yet "best possible" for competitive correctness. The main missing pieces are:

- Robust target-time calculation (command timestamp + one-way latency + interpolation, with anti-spoof sanity checks).
- History quality (time-bounded records with discontinuity markers and richer state: bbox/stance/alive).
- Edge-case correctness (very old target times, teleports, stale history, projectile lifetime semantics).
- Strict safety rails (max unlag, max command-time error, ping cutoffs, stuck/restore policy).
- Deterministic validation (automated regression scenarios for jitter/loss/corner shots/teleports).

The recommended endpoint is a **hybrid backward-reconciliation design**:

- Full lag compensation for instant-hit and melee traces.
- Optional projectile policy tiers (off / limited 50 ms / full experimental).
- Explicit anti-abuse caps.
- Deep observability for tuning and disputes.

## 2) Current Branch Assessment (What Exists Today)

### Core data model and hooks

- Per-client history and lagged entities exist in server state:
  - `antilag_positions[MAX_ANTILAG_POSITIONS]`, `laggedents[MAX_CLIENTS]` in [`src/server.h`](src/server.h).
  - `MAX_ANTILAG_POSITIONS` is `128`.
- `MOVE_LAGGED` and `FL_LAGGEDMOVE` exist for lagged tracing paths in [`src/sv_world.h`](src/sv_world.h) and [`src/server.h`](src/server.h).

### Rewind-time target selection

- Per-command `target_time` computed in [`src/sv_user.c`](src/sv_user.c) around lines 4394-4457.
- Uses `frame->sv_time` (snapshot send-time saved in [`src/sv_ents.c`](src/sv_ents.c):565), optional prediction offset, interpolation/extrapolation between stored positions.

### Trace integration

- Lagged traces are integrated in [`src/sv_world.c`](src/sv_world.c):
  - setup in `SV_AntilagClipSetUp` (owner/attacker based)
  - extra checks in `SV_AntilagClipCheck`
  - consumed from `SV_Trace`
- PR1/PR2 instant-hit and melee traces (`traceline`/`tracebox`/`TraceCapsule`) now share unified anti-lag trace policy routing.

### Projectile integration

- Projectiles can be traced with lagged mode via `SV_PushEntity`/`SV_Physics_Toss` when enabled in [`src/sv_phys.c`](src/sv_phys.c):403-407 and 750.

### Debugging and demo introspection

- Hidden MVD debug stream for server rewind positions exists in [`src/sv_user.c`](src/sv_user.c):4183+.
- Client parse and visual debug hooks exist in [`src/cl_parse.c`](src/cl_parse.c):4286+ and related client debug cvars.

## 3) Key Gaps and Risks

### A) Time selection and trust model

- Current target-time logic is heuristic and not explicitly validated against command timestamp abuse.
- No explicit equivalent of Source-style "delta too large, fall back to latency-based tick" guard.

Impact:
- Vulnerable to edge desync and potentially manipulated command timing in bad conditions.

### B) History structure is too minimal

- Stored rewind record currently holds only `origin` + `localtime`.
- Missing explicit rewind state fields (bbox mins/maxs, alive/dead, duck/stance, teleport/discontinuity marker).

Impact:
- Incorrect rewinds around stance/size transitions and discontinuous moves.

### C) Out-of-range history edge case

- In `SV_ExecuteClientMessage` interpolation search, index `j` can run below valid historical floor before dereference (`base = ... [j % MAX_ANTILAG_POSITIONS]`) around lines 4443-4449 in [`src/sv_user.c`](src/sv_user.c).

Impact:
- Potential invalid indexing/undefined behavior in extreme lag + short history scenarios.

### D) Teleport/discontinuity reset coverage

- Reset helper exists (`SV_AntilagReset`), but only clearly called from PR2 `setorigin` path.

Impact:
- Teleports or sharp discontinuities outside that path can pollute rewind history.

### E) Projectile semantics are underspecified

- Half-Life paper and Unlagged both treat projectile lag compensation as fundamentally trickier than instant-hit.
- Current owner-based lagged collision for projectiles needs strict policy and guardrails.

Impact:
- Fairness paradoxes, dodge inconsistency, harder-to-debug outcomes.

### F) Scaling/perf controls

- Rewind candidate set is broad (`MAX_CLIENTS`), and there is limited selective filtering by attack ray/context.

Impact:
- Worst-case cost spikes under high player count or aggressive projectile modes.

## 4) External Findings (Primary Sources)

### Valve/Bernier model

- Lag compensation = rewind server-side world state to what attacker saw, execute command, restore.
- Must include **both connection latency and interpolation latency**.
- Projectile lag compensation is non-trivial and often excluded by default.

### Source SDK implementation patterns

- Keep bounded history (`sv_maxunlag`), prune by time.
- Compute correction from outgoing latency + interpolation ticks.
- Clamp correction and reject implausible command-time deltas (>200 ms in reference implementation).
- Guard teleports/discontinuities and stuck restore cases.
- Support rewind modes (bounds only vs full hitboxes vs along-ray optimization).

### Unlagged (Quake 3) patterns

- Backward reconciliation for instant-hit traces with command timestamp.
- Store enough history for high ping but cap realistically.
- Reset history on major discontinuities (teleports).
- Built-in frame-delay correction and optional projectile nudge model.
- Separate "true state" from "rendered/smoothed state" for skip correction.

## 4.1) Opportunity Catalog (Methods + idTech2 Fit)

This is the complete practical menu for FPS anti-lag in an idTech2 family engine:

1. **No rewind (classic lead-your-shots)**  
   Fit: trivial, lowest CPU, lowest fairness for high ping.

2. **Hitscan backward reconciliation (server rewind at fire time)**  
   Fit: best ROI, competitive standard, recommended core.

3. **Backward reconciliation + interpolation compensation**  
   Fit: required for correctness when targets are render-interpolated client-side.

4. **Bounds-only rewind (AABB/capsule), not full hitbox animation**  
   Fit: high performance, strong baseline for idTech2 where many mods use bbox collisions.

5. **Full hitbox rewind (pose-aware)**
   Fit: highest precision but higher complexity; only needed if gameplay depends on detailed hitboxes.

6. **Along-ray rewind optimization (only rewind entities intersecting attack ray corridor)**  
   Fit: strong optimization at high player counts and for expensive hitbox modes.

7. **Projectile compensation: disabled**  
   Fit: simplest/fair conservative baseline; many classic engines keep this default.

8. **Projectile compensation: limited fixed window (e.g. built-in 50 ms style)**  
   Fit: practical compromise; lowers paradox frequency vs full projectile rewind.

9. **Projectile compensation: owner-latency full rewind**  
   Fit: possible but high paradox risk and balancing cost; recommend opt-in only.

10. **Client-authoritative hit confirmation with server audit**  
    Fit: usually not appropriate for competitive idTech2 servers due trust/cheat risk.

11. **Full rollback netcode (rewind entire sim and replay)**  
    Fit: generally overkill for idTech2 FPS; high complexity, major deterministic constraints.

12. **Per-weapon adaptive anti-lag policy table**  
    Fit: highly recommended. lets hitscan, melee, and projectile classes use different compensation rigor.

Recommended idTech2 target combination:

- Method 2 + 3 + 4 as default core.
- Method 6 for scale/perf.
- Method 8 optional, Method 9 experimental/admin-only.
- Method 12 mandatory for long-term maintainability.

## 5) Recommended "Best Possible" Design for idTech2

## 5.1 Design goals

- Competitive fairness over visual illusion.
- Server authority preserved at all times.
- Deterministic replay/debug capability.
- Hard bounded CPU/memory cost.
- Tunable policy by ruleset.

## 5.2 Data model (server)

Create a richer rewind record (per player, ring buffer):

- `server_time` (double or int ms)
- `origin`
- `mins`, `maxs`
- `velocity` (optional but useful for controlled extrapolation)
- `alive` / `solid` / `ducked_or_size_class`
- `teleport_or_discontinuity` bit
- optional `anim/frame` if hitbox model needs it

History policy:

- Time-bounded first (`sv_antilag_maxunlag`), capacity-bounded second.
- Prune old entries each server frame.
- Never interpolate across discontinuity markers.

## 5.3 Target-time derivation

Per incoming usercmd:

1. Start with client command time reference.
2. Compute one-way correction from measured network latency.
3. Add client interpolation delay estimate.
4. Clamp to `[0, sv_antilag_maxunlag]`.
5. Validate against latency-derived expectation:
   - If command-derived delta exceeds threshold (`sv_antilag_max_cmd_delta`), fall back to latency-derived target.

This mirrors proven Source behavior while fitting idTech2 command flow.

## 5.4 Rewind execution model

For each lag-compensated trace/hull test:

- Build candidate list (alive, relevant team, PVS/radius/ray overlap filters).
- For each candidate, fetch two history records bracketing target time.
- Interpolate state (never extrapolate beyond allowed micro-window).
- Perform trace against rewound candidate state.

Implementation choice in this codebase:

- Keep your current **trace-space offset approach** (not global mutation) for safety and minimal side effects.
- Add optional "full entity rewind + restore" path only if future hitbox/animation needs require it.

## 5.5 Weapon policy

Use explicit per-weapon anti-lag mode table:

- Instant-hit: full lag compensation (default on).
- Melee/hull traces: full lag compensation.
- Projectiles:
  - Mode 0: off (most conservative)
  - Mode 1: 50 ms built-in correction equivalent (recommended default)
  - Mode 2: full owner-latency compensation (experimental/admin only)

Rationale: preserves fairness and avoids projectile paradox explosion.

## 5.6 Safety rails

- `sv_antilag_maxunlag` (hard cap, default 0.2-0.3s for competitive)
- `sv_antilag_max_cmd_delta` (anti-spoof threshold)
- `sv_antilag_ping_limit` (disable/limit compensation above threshold)
- Teleport distance/discontinuity cut (`sv_antilag_teleport_dist`)
- Optional stuck-prevention restore policy for full rewind mode

## 5.7 Observability

Keep and expand existing MVD debug stream:

- target_time chosen
- attacker latency/interp inputs
- per-target rewind result: exact/interpolated/fallback/missed
- rejected due to cap/invalid time
- counters exported for server stats/console

## 5.8 Rollout strategy

- Dark-launch instrumentation first.
- Hitscan-only enablement next.
- Projectile limited mode after validation.
- Full projectile mode only if competitive data supports it.

## 6) Recommended Cvar/API Surface

Keep compatibility with existing `sv_antilag*` but add structured controls:

- `sv_antilag` (`0` off, `1` hitscan/melee, `2` include projectile policy)
- `sv_antilag_maxunlag` (seconds, hard cap)
- `sv_antilag_max_cmd_delta` (seconds, anti-spoof)
- `sv_antilag_projectile_mode` (`0/1/2` as above)
- `sv_antilag_rollout_stage` (`0` instrumentation only, `1` hitscan/melee, `2` projectile-limited gated, `3` full projectile eligible)
- `sv_antilag_projectile_playtest_signoff` (`0/1` rollout gate for projectile enablement at stage 2+)
- `sv_antilag_projectile_full_admin` (`0/1` admin gate for allowing projectile mode `2`)
- `sv_antilag_ping_limit`
- `sv_antilag_teleport_dist`
- `sv_antilag_debug` (verbosity level)

Suggested internal API:

- `SV_Antilag_BeginForCommand(client_t* attacker, const usercmd_t* cmd, antilag_ctx_t* ctx)`
- `SV_Antilag_ResolveTargetState(antilag_ctx_t* ctx, int target_entnum, rewind_state_t* out)`
- `SV_Antilag_End(antilag_ctx_t* ctx)`

## 7) Checkboxed Implementation Plan

## Phase 0: Baseline + guardrails

- [x] Add `docs/` design note for anti-lag invariants and threat model.
- [x] Add `sv_antilag_maxunlag`, `sv_antilag_max_cmd_delta`, `sv_antilag_ping_limit` cvars.
- [x] Wire startup defaults by ruleset (competitive/casual).

## Phase 1: Data model hardening

- [x] Replace minimal `antilag_position_t` with richer rewind record (time, origin, mins/maxs, flags).
- [x] Convert history management to time-pruned ring semantics (not just count).
- [x] Add discontinuity marker flag and helper.

## Phase 2: Correct time math

- [x] Implement target-time computation from cmd timestamp + measured latency + interpolation.
- [x] Add delta sanity check with fallback to latency-derived target time.
- [x] Clamp to max-unlag window and reject outliers with debug counters.

## Phase 3: Fix known correctness bugs

- [x] Fix out-of-range history indexing in `SV_ExecuteClientMessage` interpolation search.
- [x] Handle no-bracket cases explicitly (use oldest/newest valid record with policy).
- [x] Guard zero/negative interpolation denominators.

## Phase 4: Teleport/discontinuity correctness

- [x] Call `SV_AntilagReset` (or new discontinuity insert) from all teleport/setorigin paths (PR1 + PR2 + engine events).
- [x] Add jump-distance discontinuity detection in history writer.
- [x] Ensure spawn/death transitions create clean rewind boundaries.

## Phase 5: Rewind candidate filtering and perf

- [x] Add candidate prefilter: connected, alive, relevant team, possibly visible, broadphase overlap.
- [x] Add optional ray-based narrowing ("along ray" style) for expensive modes.
- [x] Add perf counters and server frame budget alarms.

## Phase 6: Hitscan/melee integration finalization

- [x] Route all instant-hit and melee hull traces through unified anti-lag resolver.
- [x] Validate parity across `pr_cmds.c` and `pr2_cmds.c` paths.
- [x] Add deterministic replay tests for classic edge cases (peek shot, crossing strafe, jitter target).
- [x] Extend deterministic replay command with trace-policy parity checks (hitscan/melee mode gates, bot bypass, null attacker safety).

## Phase 7: Projectile policy implementation

- [x] Implement `sv_antilag_projectile_mode` with three tiers (off/50ms/full).
- [x] Add owner-staleness guard: projectile compensation must not use stale owner context beyond threshold.
- [x] Add opt-in per-mod/per-weapon toggles.
- [x] Add deterministic projectile-policy replay coverage (mode tier gates, owner freshness/state guards, opt-in/allow/deny precedence, blend fraction checks).

## Phase 8: Debug and tooling

- [x] Expand hidden MVD block with reason codes and timing fields.
- [x] Add server commands to dump recent per-client anti-lag decisions.
- [x] Add client HUD debug overlay for rewind deltas and fallback reasons.
- [x] Extend tooling ergonomics: dump commands now include cumulative counters, HUD extended mode includes command-window and server/target timing.
- [x] Decouple HUD summary from spectator-only gating (stats can be shown whenever debug payload is available; line overlays remain spectator/demo-focused).

## Phase 9: Validation harness

- [x] Build scripted bot scenarios with controlled latency/jitter/loss/reorder.
- [x] Record golden outcomes for hit registration.
- [x] Add CI test target that runs anti-lag regression suite headless.
- [x] Enforce strict golden integrity (duplicate-name rejection, fail on unmatched golden rows) and broaden scenario matrix (`no_pred`, command-msec clamping, delta-guard disabled, heavy-loss window).

## Phase 10: Rollout

- [x] Ship instrumentation-only build to collect baseline metrics.
- [x] Enable hitscan/melee anti-lag by default after metric pass.
- [x] Enable projectile limited mode only after dedicated playtest signoff.
- [x] Keep full projectile mode admin-only until statistically validated.

## Phase 11: Documentation and operations

- [x] Publish admin tuning guide (recommended cvar presets by server type).
- [x] Publish player-facing explanation of paradoxes and fairness tradeoffs.
- [x] Add troubleshooting matrix ("shot around corner", high jitter, false misses).

Phase 11 documentation deliverables:

- `docs/ANTI_LAG_ADMIN_TUNING.md`
- `docs/ANTI_LAG_PLAYER_FAIRNESS.md`
- `docs/ANTI_LAG_TROUBLESHOOTING.md`

## 8) Pitfall Matrix (Quick Reference)

- Incorrect command time trust -> clamp + delta sanity fallback.
- History too short/too sparse -> time-bounded history + min sampling frequency.
- Teleport poisoning -> discontinuity markers + reset hooks.
- Stance/bbox mismatch -> store mins/maxs and state flags per record.
- Projectile fairness paradox -> limited mode default; full mode opt-in.
- Performance spikes -> candidate filtering + mode-based rewind depth.
- Dispute/debug blindness -> always-on counters + MVD rewind traces.

## 9) Recommended Defaults (Competitive)

- `sv_antilag 1`
- `sv_antilag_projectile_mode 1` (limited 50 ms model) or `0` for strictest tournaments
- `sv_antilag_maxunlag 0.25`
- `sv_antilag_max_cmd_delta 0.20`
- `sv_antilag_ping_limit 400`
- `sv_antilag_teleport_dist 64`

## 10) Sources

Primary sources used:

- Valve Developer Community, Lag Compensation:
  - https://developer.valvesoftware.com/wiki/Lag_compensation
- Yahn Bernier, *Latency Compensating Methods in Client/Server In-game Protocol Design and Optimization*:
  - https://www.gamedevs.org/uploads/latency-compensation-in-client-server-protocols.pdf
- Unlagged documentation (Quake 3):
  - Networking primer: https://www.ra.is/unlagged/network.html
  - Code integration: https://www.ra.is/unlagged/code.html
  - Walkthroughs: https://www.ra.is/unlagged/walkthroughs.html
  - FAQ: https://www.ra.is/unlagged/faq.html
  - Solution/tradeoffs: https://www.ra.is/unlagged/solution.html
- Valve lag compensation code reference (Source SDK lineage):
  - https://swarm.workshop.perforce.com/files/guest/knut_wikstrom/ValveSDKCodedlls/player_lagcompensation.cpp

Repository code analyzed:

- [`src/server.h`](src/server.h)
- [`src/sv_user.c`](src/sv_user.c)
- [`src/sv_world.c`](src/sv_world.c)
- [`src/sv_phys.c`](src/sv_phys.c)
- [`src/sv_ents.c`](src/sv_ents.c)
- [`src/pr_cmds.c`](src/pr_cmds.c)
- [`src/pr2_cmds.c`](src/pr2_cmds.c)
- [`src/cl_parse.c`](src/cl_parse.c)
