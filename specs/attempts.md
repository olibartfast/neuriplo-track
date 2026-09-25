# Attempt log

One row per attempt. An attempt is one delegation packet given to one worker,
ending in exactly one scoreboard run
(`cmake --workflow --preset scoreboard`). The fields and the rules for a
controlled comparison are defined in [orchestration.md](orchestration.md).

`outcome` is the scoreboard exit status. It is pre-repair by construction: the
scoreboard runs once per attempt, so a repaired change is a new row, not an
edit to this one.

| date | packet | base | planner | worker | planner_ctx | worker_ctx | tool_calls | turns | wall | cost | outcome | interventions | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 2026-09-22 | orchestration bootstrap | 95b048e | opus-5 | — | — | — | — | — | — | — | pass (0) | 0 | Baseline: no delegation. Establishes the scoreboard preset and confirms 5/5 suites pass from a cold configure. Everything below is compared against this. **Built against sibling checkouts, not the pins** (`../neuriplo-tasks` at v0.8.2-2-gd320092 vs pinned v0.8.1) -- see the hermeticity defect in orchestration.md. |
| 2026-09-22 | 2026-08-20-evaluation G2-G5 | 95b048e | opus-5 | — (in-context) | — | — | — | — | ~2h incl. two 300-frame model runs | — | pass (0) | 0 | Groups 2-5 delivered in one context, not delegated: see "When not to delegate" — the flag-combination rules and the replay/class-filtering interaction are judgement, not transcription. Scoreboard 7/7. Same sibling-checkout caveat as the row above. |
| 2026-09-22 | mask-cue measurement (Phase 5) | working tree | opus-5 | — (in-context) | — | — | — | — | ~75 min of CPU model runs | — | n/a (measurement, not a build) | 3 | Four live segmentation runs at mask_iou_weight 0/0.25/0.5/1.0, plus one diagnostic capture. Interventions: two experiment redesigns after inconclusive nulls (sparse scene, then class breadth), then the diagnostic that found `ocsort_det_thresh=0.6` was suppressing the measurement. Overturns the PR #4 conclusion. |

Nothing is delegated until a row here shows what the single-context baseline
costs. Delegating before that measurement is how the same-model delegation trap
goes unnoticed.
