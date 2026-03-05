# Anti-Lag Troubleshooting Matrix

Use this matrix to triage anti-lag complaints quickly and consistently.

## Quick Triage Workflow

1. Confirm active rollout and effective modes.
- Run `sv_antilag_rollout_status`.

2. Inspect per-client decision history.
- Run `sv_antilag_dump <userid/name> 8`.

3. Check whether behavior is policy-expected or anomalous.
- Compare reason flags and timing deltas against the matrix below.

4. Validate baseline invariants.
- Run `sv_antilag_replaytests`.

## Troubleshooting Matrix

| Symptom | Typical cause(s) | Verify with | Immediate mitigation | Long-term fix |
|---|---|---|---|---|
| "Shot around corner" reports | Expected bounded rewind paradox; high shooter latency; larger unlag window | `sv_antilag_dump` reason flags and rewind ms | Reduce `sv_antilag_maxunlag`; enable/adjust `sv_antilag_ping_limit` | Keep tournament profile for competitive play; communicate player fairness guide |
| High-jitter player gets inconsistent hit registration | command-time fallback and clamp behavior due jitter/loss | `cmd_fallback`, `clamp_unlag`, `clamp_future` in dump | Raise network quality expectations; cap extreme ping with `sv_antilag_ping_limit` | Improve network path/QoS; keep anti-spoof thresholds tuned |
| Frequent false misses for attacker | stale/no bracket history, severe packet loss, rapid discontinuities | `history_miss`, fallback(o/n/b), decision timing fields | Verify `sv_antilag_maxunlag` not too low; verify teleport/discontinuity behavior | Stabilize packet quality; keep history/discontinuity invariants |
| Projectile feels unfair/inconsistent | projectile mode too aggressive for environment; owner context staleness | projectile mode effective value, owner stale rejects | Force limited projectile mode (`mode 1`) or disable (`mode 0`) | Keep full projectile mode admin-only and playtest-gated |
| Projectile full mode appears not to engage | rollout/admin gates are suppressing mode 2 | `sv_antilag_rollout_status` requested vs effective mode | Set stage/signoff/admin gates explicitly for validation windows | Maintain staged rollout with explicit change control |
| Anti-lag appears "off" despite `sv_antilag 1/2` | rollout stage 0 instrumentation-only | `rollout_gate` reason flag and rollout status command | Set `sv_antilag_rollout_stage 1` (or higher as needed) | Standardize startup config and verification checklist |
| Server frame spikes when many players are fighting | broad candidate set and expensive clip checks | dump totals clip/build counters; frame budget alarms | tighten `sv_antilag_frame_budget_ms`; keep `prefilter_*` and `ray_narrow` enabled | keep ray narrowing tuned; avoid full projectile in large public sessions |
| Team mode shows unnecessary anti-lag candidate work | team filtering not active or unsupported team metadata | dump team prefilter counters | set `sv_antilag_prefilter_team 1` | ensure ruleset/team metadata is consistent |
| Complaints spike after config deploy | multi-variable change without baseline comparison | compare pre/post dumps and rollout status | rollback stage to 0 or revert last config change | apply one-dimensional config changes with replay test gate |

## Reason Flag Interpretation

- `ping_reject`: command path exceeded ping gate; anti-lag reduced/disabled for that request.
- `cmd_fallback`: command-derived target was implausible; latency-derived target used.
- `clamp_unlag`: rewind depth hit hard cap.
- `clamp_future`: target clamped to present/future boundary protection.
- `oldest` / `newest` / `bad_interval` / `history_miss`: history bracket quality issues.
- `rollout_gate`: gameplay rewind prevented by rollout stage policy.

## Suggested Escalation Policy

- Severity 1 (single anecdotal report)
  - collect `sv_antilag_dump` evidence before changes
- Severity 2 (repeated reproducible behavior)
  - temporary mitigation by narrowing cap or forcing projectile limited/off
- Severity 3 (widespread regressions)
  - set `sv_antilag_rollout_stage 0` immediately, keep instrumentation on, triage offline

## Operator Checklist for Match Days

- Run `sv_antilag_rollout_status` before first match.
- Verify expected effective modes.
- Keep a known-good preset snapshot ready.
- If anomalies emerge:
  - capture dumps for involved clients
  - avoid multiple simultaneous cvar changes
  - rerun deterministic replay checks before re-enabling escalated modes
