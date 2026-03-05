# Anti-Lag Admin Tuning Guide

This guide is for server operators running anti-lag in production.

## Goals

- Keep hit registration fair across mixed pings.
- Keep paradox frequency understandable and bounded.
- Keep CPU cost predictable under load.
- Keep rollout and escalation operationally safe.

## Core Controls

- `sv_antilag`
  - `0`: disable anti-lag gameplay rewinds
  - `1`: hitscan/melee rewinds
  - `2`: include projectile policy path
- `sv_antilag_rollout_stage`
  - `0`: instrumentation-only (records decisions, no lagged trace application)
  - `1`: hitscan/melee enabled
  - `2`: projectile limited eligible with signoff
  - `3`: full projectile eligibility with admin gate
- `sv_antilag_projectile_mode`
  - `0`: off
  - `1`: limited window
  - `2`: full owner-latency projectile rewind
- `sv_antilag_projectile_playtest_signoff`
  - `0/1` rollout gate for projectile enablement at stage `2+`
- `sv_antilag_projectile_full_admin`
  - `0/1` admin gate for allowing effective projectile mode `2`
- `sv_antilag_maxunlag`
  - hard rewind depth cap in seconds
- `sv_antilag_max_cmd_delta`
  - anti-spoof drift threshold in seconds
- `sv_antilag_ping_limit`
  - optional hard ping gate in milliseconds (`0` means disabled)
- `sv_antilag_teleport_dist`
  - discontinuity distance threshold in world units
- `sv_antilag_prefilter_pvs`, `sv_antilag_prefilter_team`
  - candidate filtering toggles
- `sv_antilag_ray_narrow`, `sv_antilag_ray_narrow_pad`
  - along-ray narrowing controls
- `sv_antilag_frame_budget_ms`
  - soft per-frame budget alarm threshold

## Recommended Presets by Server Type

### Tournament / Strict Competitive

- `sv_antilag 1`
- `sv_antilag_rollout_stage 1`
- `sv_antilag_projectile_mode 0`
- `sv_antilag_projectile_playtest_signoff 0`
- `sv_antilag_projectile_full_admin 0`
- `sv_antilag_maxunlag 0.25`
- `sv_antilag_max_cmd_delta 0.20`
- `sv_antilag_ping_limit 400`
- `sv_antilag_teleport_dist 64`
- `sv_antilag_prefilter_pvs 1`
- `sv_antilag_prefilter_team 1`
- `sv_antilag_ray_narrow 1`
- `sv_antilag_ray_narrow_pad 16`
- `sv_antilag_frame_budget_ms 0.75`

Why: strongest fairness predictability, lowest projectile paradox risk.

### Public Competitive (Mixed Ping)

- `sv_antilag 2`
- `sv_antilag_rollout_stage 2`
- `sv_antilag_projectile_mode 1`
- `sv_antilag_projectile_playtest_signoff 1`
- `sv_antilag_projectile_full_admin 0`
- `sv_antilag_maxunlag 0.25`
- `sv_antilag_max_cmd_delta 0.20`
- `sv_antilag_ping_limit 500`
- `sv_antilag_teleport_dist 64`
- `sv_antilag_prefilter_pvs 1`
- `sv_antilag_prefilter_team 1`
- `sv_antilag_ray_narrow 1`
- `sv_antilag_ray_narrow_pad 16`
- `sv_antilag_frame_budget_ms 1.00`

Why: keeps projectile support limited and controlled while serving broader pings.

### Casual / Community

- `sv_antilag 2`
- `sv_antilag_rollout_stage 2`
- `sv_antilag_projectile_mode 1`
- `sv_antilag_projectile_playtest_signoff 1`
- `sv_antilag_projectile_full_admin 0`
- `sv_antilag_maxunlag 0.50`
- `sv_antilag_max_cmd_delta 0.35`
- `sv_antilag_ping_limit 0`
- `sv_antilag_teleport_dist 96`
- `sv_antilag_prefilter_pvs 1`
- `sv_antilag_prefilter_team 1`
- `sv_antilag_ray_narrow 1`
- `sv_antilag_ray_narrow_pad 24`
- `sv_antilag_frame_budget_ms 1.50`

Why: prioritizes accessibility for higher latency players.

### Experimental Projectile Full (Admin-Only Validation)

- `sv_antilag 2`
- `sv_antilag_rollout_stage 3`
- `sv_antilag_projectile_mode 2`
- `sv_antilag_projectile_playtest_signoff 1`
- `sv_antilag_projectile_full_admin 1`
- keep all other values from the target baseline preset above

Why: only for controlled sessions with explicit monitoring and rollback plan.

## Rollout Runbook

1. Stage 0 baseline
- Set `sv_antilag_rollout_stage 0`.
- Keep intended long-term cvars preconfigured.
- Collect decision metrics and operator observations for representative matches.

2. Stage 1 activation
- Set `sv_antilag_rollout_stage 1`.
- Keep projectile gates disabled.
- Validate with:
  - `sv_antilag_rollout_status`
  - `sv_antilag_dumpall 4`
  - `sv_antilag_replaytests`

3. Stage 2 projectile-limited activation
- Set `sv_antilag_rollout_stage 2`.
- Set `sv_antilag_projectile_playtest_signoff 1`.
- Keep `sv_antilag_projectile_mode 1` and `sv_antilag_projectile_full_admin 0`.
- Run dedicated playtest sessions and compare disputes vs baseline.

4. Stage 3 eligibility (still limited by admin gate)
- Set `sv_antilag_rollout_stage 3`.
- Keep `sv_antilag_projectile_full_admin 0` for normal production.
- Enable `sv_antilag_projectile_full_admin 1` only in controlled validation windows.

Rollback rule:
- For immediate gameplay rollback without losing instrumentation, set `sv_antilag_rollout_stage 0`.

## Live Monitoring Checklist

- `sv_antilag_rollout_status`
  - confirms requested vs effective modes
- `sv_antilag_dump <player> 8`
  - inspect reason flags and timing deltas for specific disputes
- `sv_antilag_dumpall 2`
  - sweep latest decisions across active clients
- `sv_antilag_replaytests`
  - verify deterministic policy invariants after config changes

Watch for:

- Frequent `ping_reject` on target population.
- Frequent `cmd_fallback` / `clamp_unlag` spikes.
- Repeated `rollout_gate` when gameplay anti-lag is expected.
- Clip/build counters growing faster than frame budget permits.

## Config Snippets

Tournament baseline:

```cfg
set sv_antilag 1
set sv_antilag_rollout_stage 1
set sv_antilag_projectile_mode 0
set sv_antilag_projectile_playtest_signoff 0
set sv_antilag_projectile_full_admin 0
set sv_antilag_maxunlag 0.25
set sv_antilag_max_cmd_delta 0.20
set sv_antilag_ping_limit 400
set sv_antilag_teleport_dist 64
set sv_antilag_prefilter_pvs 1
set sv_antilag_prefilter_team 1
set sv_antilag_ray_narrow 1
set sv_antilag_ray_narrow_pad 16
set sv_antilag_frame_budget_ms 0.75
```

Public mixed-ping baseline:

```cfg
set sv_antilag 2
set sv_antilag_rollout_stage 2
set sv_antilag_projectile_mode 1
set sv_antilag_projectile_playtest_signoff 1
set sv_antilag_projectile_full_admin 0
set sv_antilag_maxunlag 0.25
set sv_antilag_max_cmd_delta 0.20
set sv_antilag_ping_limit 500
set sv_antilag_teleport_dist 64
set sv_antilag_prefilter_pvs 1
set sv_antilag_prefilter_team 1
set sv_antilag_ray_narrow 1
set sv_antilag_ray_narrow_pad 16
set sv_antilag_frame_budget_ms 1.00
```

## Change Control Recommendations

- Apply anti-lag changes one dimension at a time.
- Record exact cvar snapshots before and after each change.
- Run at least one deterministic replay check after significant updates.
- Preserve at least one week of operator dispute notes before deciding on stage escalation.
