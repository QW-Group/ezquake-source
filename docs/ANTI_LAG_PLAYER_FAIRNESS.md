# Anti-Lag Fairness Guide for Players

This page explains what anti-lag does, why paradoxes happen, and what to expect in play.

## What Anti-Lag Does

When you fire, the server estimates where opponents were at your shot time and evaluates the hit against that rewound state.

This helps players with non-zero latency register hits closer to what they saw.

## What Anti-Lag Does Not Do

- It does not make ping irrelevant.
- It does not make every duel look identical for both players.
- It does not let clients decide hits. The server remains authoritative.

## Why "Shot Around Corner" Can Happen

Two facts can both be true:

- You reached cover on your screen.
- The shooter fired earlier on their screen, before your cover transition.

Server anti-lag can legitimately resolve that as a hit from the shooter perspective. This is a normal tradeoff in client/server shooters.

## Fairness Tradeoffs

No anti-lag model satisfies every perspective at the same time. The engine chooses a bounded compromise:

- Better shot validation for moderate latency.
- Bounded rewind caps to reduce abuse and extreme paradoxes.
- Optional ping limits and projectile policy tiers.

## Hitscan, Melee, and Projectile Differences

- Hitscan and melee can be compensated directly against historical target positions.
- Projectiles are harder because they persist through time and involve movement overlap, ownership timing, and dodge perception.
- For this reason, many servers keep projectile compensation limited or disabled.

## Rollout and Policy You May See

Server operators can run staged rollout:

- Stage 0: instrumentation only (no gameplay anti-lag effect).
- Stage 1: hitscan/melee anti-lag.
- Stage 2: limited projectile anti-lag (after playtest signoff).
- Stage 3: full projectile mode eligibility (admin-gated).

Even when `sv_antilag_projectile_mode` is set to full, server gates may keep effective behavior limited.

## What Improves Your Experience

- Stable ping matters more than absolute minimum ping.
- Lower jitter and packet loss reduce fallback behavior.
- Correct rates/network settings reduce interpolation and command-timing mismatch.

## Common Misconceptions

- "Anti-lag is cheating for high ping."
  - Anti-lag is bounded and server-controlled; it is designed to reduce, not create, unfairness.
- "If I die behind cover, anti-lag is broken."
  - That can be an expected paradox under valid rewind rules.
- "Projectile anti-lag should always be full."
  - Full projectile rewind has the highest paradox risk and is usually the most restricted mode.

## When to Report an Issue

Report if behavior is frequent, reproducible, and outside expected tradeoffs:

- repeated impossible-looking outcomes in low-latency sessions
- sustained false misses with clear line of fire
- visible mismatch bursts correlated with server instability

Useful report details:

- approximate timestamp and map
- your ping/jitter/loss at the moment
- demo if available
- weapon class (hitscan, melee, projectile)

## Summary

Anti-lag improves fairness overall, but cannot eliminate all perspective conflicts. Bounded rewinds plus staged rollout are intentional safeguards, not failures.
