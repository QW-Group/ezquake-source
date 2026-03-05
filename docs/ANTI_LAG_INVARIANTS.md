# Anti-Lag Invariants and Threat Model

This document defines the non-negotiable correctness and safety rules for server-side anti-lag.

Related operations docs:

- `docs/ANTI_LAG_ADMIN_TUNING.md`
- `docs/ANTI_LAG_PLAYER_FAIRNESS.md`
- `docs/ANTI_LAG_TROUBLESHOOTING.md`

## Scope

- Engine: idTech2 / QuakeWorld-style server-authoritative networking.
- Feature area: rewind-based hit registration for instant-hit/melee (projectiles optional by policy).
- Code owners: server networking + movement/trace maintainers.

## Security and Fairness Threat Model

Primary threats:

- Timestamp abuse: client attempts to force rewind far beyond fair window.
- High-ping exploitation: extreme latency used to gain unfair corner-shot advantage.
- History poisoning: teleports/discontinuities producing invalid interpolation.
- Stance mismatch: bbox/size changes not reflected in rewind state.
- Performance DoS: expensive rewind work amplified by player count/projectile settings.

Non-goals:

- Client-authoritative hit decisions.
- Unlimited rewind in support of arbitrarily high ping.

## Anti-Lag Invariants

1. Server authority
- All hit confirmation stays server-side.
- Client data can influence only bounded rewind targets.

2. Bounded rewind
- Rewind time is capped by `sv_antilag_maxunlag`.
- Additional command-time drift is capped by `sv_antilag_max_cmd_delta`.

3. Ping policy
- Compensation is optionally limited by `sv_antilag_ping_limit`.
- When limit is exceeded, anti-lag is reduced/disabled for that command path.

4. History correctness
- History is maintained as a bounded ring buffer.
- History is additionally pruned by configured max-unlag time window.
- Each record stores at least time, origin, mins, maxs, and flags.

5. Discontinuity safety
- Teleports/discontinuous moves must not be interpolated across.
- Discontinuities are explicitly marked in history.

6. Deterministic interpolation
- Rewind interpolation factor is clamped to `[0,1]`.
- Invalid brackets fall back to nearest valid sample.
- If target time is outside stored history, fallback policy is explicit:
  - older than window => oldest sample
  - newer than window => newest sample

7. Operational safety
- Feature can be disabled via `sv_antilag`.
- Debug traces remain available for post-match verification.
- Server tracks anti-lag fallback/reject clamp counters per client.

8. Projectile policy safety (Phase 7)
- Projectile rewind is controlled by `sv_antilag_projectile_mode`:
  - `0`: off
  - `1`: limited window (`sv_antilag_projectile_nudge`, default 50ms-equivalent)
  - `2`: full owner-latency rewind
- Guardrails:
  - `sv_antilag_projectile_mode` and `sv_antilag_projectile_optin` are clamped to `0..2`.
  - `sv_antilag_projectile_nudge` and `sv_antilag_projectile_owner_stale` are clamped to `>= 0`.
- Legacy compatibility: `sv_antilag_projectiles` is synchronized with mode controls:
  - setting legacy `0` forces mode `0`
  - setting legacy `1` promotes mode `0` to mode `1` (keeps mode `2` unchanged)
  - setting mode `0/1/2` mirrors legacy toggle off/on automatically
- Projectile rewind is rejected when owner lagged context is stale:
  - guard threshold: `sv_antilag_projectile_owner_stale`
  - stale/no-owner/no-history => no projectile rewind
- Projectile rewind can be policy-gated per mod/weapon:
  - `sv_antilag_projectile_optin`
  - `sv_antilag_projectile_allow` / `sv_antilag_projectile_deny`
  - explicit entity opt-in via `FL_LAGGEDMOVE`

9. Debug observability safety (Phase 8)
- Hidden MVD anti-lag debug packets include reason flags and timing/counter fields.
- Server keeps a bounded per-client anti-lag decision history ring for operator inspection.
- Runtime inspection commands:
  - `sv_antilag_dump <userid/name> [count]`
  - `sv_antilag_dumpall [count]`
  - `sv_antilag_rollout_status`
  - Dump output includes both cumulative totals and per-decision snapshots.
- Client overlay can display rewind deltas and fallback reasons via:
  - `cl_debug_antilag_hud` (`1` summary, `2` extended counters)
  - Extended mode also shows command-window and server/target timing values.
  - Line-segment visualizations (`cl_debug_antilag_lines`) remain demo/spectator-oriented.

10. Rollout safety (Phase 10)
- Rollout control is driven by `sv_antilag_rollout_stage` (clamped to `0..3`).
- Stage behavior:
  - `0`: instrumentation-only baseline. Server still records anti-lag decisions, but trace policy never applies `MOVE_LAGGED`.
  - `1`: hitscan/melee anti-lag can be applied.
  - `2`: projectile anti-lag can be applied only if `sv_antilag_projectile_playtest_signoff=1`.
  - `3`: full projectile mode (`sv_antilag_projectile_mode=2`) is allowed only if `sv_antilag_projectile_full_admin=1`.
- Projectile rollout gates are clamped to boolean:
  - `sv_antilag_projectile_playtest_signoff`
  - `sv_antilag_projectile_full_admin`
- Full projectile mode requests are downgraded to limited mode when admin gate is disabled.
- Decision telemetry marks stage-gated instrumentation-only operation with `rollout_gate` reason flag.

11. Validation harness safety (Phase 9)
- Scripted latency/jitter/loss/reorder scenarios are stored in:
  - `tests/antilag/phase9_network_scenarios.csv`
- Golden outcomes are stored in:
  - `tests/antilag/phase9_golden.csv`
- Harness guarantees:
  - scenario names must be unique (no duplicate network/golden keys)
  - every executed scenario must have a golden row
  - every golden row for `network`/`hitreg` must be matched (no stale extras)
- Headless regression runner:
  - executable target: `antilag_phase9_regression`
  - ctest name: `antilag_phase9_regression`
  - pass marker: `PHASE9_REGRESSION_PASS`

## Configuration Profiles

Two startup profiles are recognized:

- Competitive profile: lower rewind cap, stricter drift cap, ping limit enabled.
- Casual profile: larger rewind cap, looser drift cap, ping limit disabled.
- Rollout baseline defaults to `sv_antilag_rollout_stage 0` (instrumentation-only).

Profile selection is based on active `ruleset` name at startup:

- Competitive-like: `smackdown`, `smackdrive`, `qcon`, `thunderdome`, `mtfl`.
- Casual/default: everything else.

## Deterministic Replay Checks (Phase 6/7)

- Command: `sv_antilag_replaytests`
- Scenarios:
  - `peek_shot`
  - `crossing_strafe`
  - `jitter_target`
  - `policy_hitscan_mode1`
  - `policy_melee_mode1`
  - `policy_hitscan_mode0`
  - `policy_hitscan_rollout_stage0`
  - `policy_projectile_mode1_off`
  - `policy_hitscan_bot_bypass`
  - `policy_melee_null_attacker`
  - `policy_projectile_mode2_full`
  - `policy_projectile_mode1_limited`
  - `policy_projectile_rollout_stage2_no_signoff`
  - `policy_projectile_rollout_stage2_signoff`
  - `policy_projectile_rollout_stage0_off`
  - `policy_projectile_mode2_admin_gate`
  - `policy_projectile_mode2_admin_gate_blend`
  - `policy_projectile_mode0_off`
  - `policy_projectile_owner_stale`
  - `policy_projectile_owner_stale_unbounded`
  - `policy_projectile_optin1_requires_optin`
  - `policy_projectile_optin1_flag`
  - `policy_projectile_optin1_allow`
  - `policy_projectile_optin2_flag_only_off`
  - `policy_projectile_optin2_allow`
  - `policy_projectile_deny_overrides_allow`
  - `policy_projectile_owner_bot_bypass`
  - `policy_projectile_owner_state_gate`
  - `policy_projectile_owner_spectator_gate`
  - `policy_projectile_owner_no_history`
  - `policy_projectile_blend_mode1_nudged`
  - `policy_projectile_blend_mode2_full`
  - `policy_projectile_blend_mode1_zero_nudge`
  - `policy_projectile_blend_nonpositive_depth`
- Pass criteria:
  - Resolver mode remains `antilag_resolve_interpolate`.
  - Rewound origin matches deterministic expected value within a small epsilon.
  - Trace-policy scenarios produce exact expected `MOVE_*` flags.
  - Projectile blend scenarios match expected nudge/full fractions within epsilon.
- Output:
  - Success: `sv_antilag_replaytests: all deterministic replay scenarios passed`
  - Failure: per-scenario mismatch details and final failing count.

## Validation Checklist

- Rewind never reads outside history bounds.
- Teleport events produce discontinuity boundaries.
- High jitter/loss does not crash or produce NaN traces.
- Disabling anti-lag immediately clears active history state.
- Competitive defaults are applied only when cvars are unset.
- Command-time outliers increment fallback counters and use latency-derived target time.
