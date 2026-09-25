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

Nothing is delegated until a row here shows what the single-context baseline
costs. Delegating before that measurement is how the same-model delegation trap
goes unnoticed.
