# Orchestration

Part of the constitution; see [README.md](README.md) for how these documents fit
together. [README.md](README.md) owns *what* gets specified. This document owns
*who executes it, under what boundaries, and what decides the outcome*.

Status as of 2026-09-22.

The durable part of an agent workflow is not the model. It is the contract:
written constraints, an executable verdict, and an explicit line between
planning and implementation. Those live here and in the specs, so swapping the
model behind any role is a configuration change rather than a rewrite.

## Contract artifacts

The spec-driven layout already supplies three of the four. Nothing new is
introduced; this table just names which artifact plays which role.

| Contract role | This repository | Owned by |
|---|---|---|
| Intent | [mission.md](mission.md) | Specifier |
| Constraints | [tech-stack.md](tech-stack.md) + [AGENTS.md](../AGENTS.md) | Specifier |
| Roadmap | [roadmap.md](roadmap.md), decomposed into packets | Specifier writes phases; planner decomposes |
| Acceptance | the scoreboard below + each packet's `validation.md` | Specifier |

The planner consumes approved phases. It does not rewrite an approved spec to
match what got built.

## Scoreboard — every phase

- **Command (repo-relative)**: `cmake --workflow --preset scoreboard`
- **Adjudicates**:
  - the project configures from a clean tree with the pinned dependency refs
  - every target compiles, including `trackers`, the CLI, and all test binaries
  - all registered ctest suites pass
- **Run per attempt**: exactly once, as the final action
- **Recorded with**: the attempt's exit status, in the attempt log below

Why a workflow preset and not `ctest --test-dir build`: the workflow preset
configures, builds, and tests in one invocation, so it cannot return green over
stale binaries. `ctest --test-dir build` — the command CI used, and the command
that passed for the whole of Phase 0 because nothing was registered — can.
`noTestsAction: error` in the test preset makes an empty suite a failure rather
than a pass.

### Known defect: the scoreboard is not hermetic on a developer machine

`CMakeLists.txt` force-sets `FETCHCONTENT_SOURCE_DIR_NEURIPLO-TASKS` and
`FETCHCONTENT_SOURCE_DIR_NEURIPLO` whenever `../neuriplo-tasks` or `../neuriplo`
exists, with no opt-out. On a machine with sibling checkouts the scoreboard
therefore builds against **whatever is in those trees**, not the refs in
`versions.env` — verified on 2026-09-22, where the sibling was at
`v0.8.2-2-gd320092` while the pin said `v0.8.1`.

Consequences, until an opt-out exists:

- A green scoreboard on a machine with siblings is evidence about *that* tree,
  not about the pinned build. Record which it was in the attempt-log `notes`.
- CI has no sibling checkouts, so CI results are the pinned ones. Treat CI as
  the authority when the two disagree.
- Check with `grep FETCHCONTENT_SOURCE_DIR build/scoreboard/CMakeCache.txt`
  before trusting a local adjudication.

This is tracked as an in-scope fix in
[2026-09-22-integrations](2026-09-22-integrations/requirements.md): a package
config cannot promise a dependency version the build does not actually honour.

Supporting presets, which are *targeted checks* and not the scoreboard:

| Preset | Invocation | Guards |
|---|---|---|
| `strict` | `cmake --build --preset strict` | `-Wall -Wextra -Wpedantic -Werror`, the CI Debug job |
| `lib-only` | `cmake --workflow --preset lib-only` | `trackers` must not depend on anything in `app/` |

`scoreboard` and `strict` use `DEFAULT_BACKEND=OPENCV_DNN` deliberately: it needs
no downloaded runtime, so adjudication works offline and on a worker with no
network.

Presets require CMake ≥ 3.25 (workflow presets). The project itself still builds
with 3.20 as [tech-stack.md](tech-stack.md) states — the presets are an
execution convenience, not a new build requirement.

## Targeted checks vs the scoreboard

|  | Targeted checks | Scoreboard |
|---|---|---|
| Frequency | as often as useful | exactly once, as the final action |
| Scope | the target being changed | the whole contract |
| Examples | `cmake --build --preset scoreboard --target test_trackers`, `ctest --preset scoreboard -R trackers`, `cmake --build --preset strict` | `cmake --workflow --preset scoreboard` |
| Permission | always allowed, encouraged | the end condition |

The compiler is the implementer's fastest feedback; restricting it makes the
work worse. What is restricted is *re-adjudication*: a worker that reacts to a
red scoreboard by editing and rerunning turns a cheap failure into an expensive
spiral. Report the result, pass or fail, and stop. The planner decides what
happens next.

## Roles

Routed by capability tier and role, not by model name — names age, roles
survive replacement. Definitions live in `~/.claude/agents/`; assignments are
configuration, never prompt text.

| Role | Work it owns | Tier |
|---|---|---|
| `architect` | Decisions expensive to reverse: `BaseTracker` / `TrackedObject` / `TrackConfig` surface, ABI and install boundaries, ownership and lifetime contracts, roadmap shaping | strongest |
| `planner` | Decomposing one roadmap phase into packets, writing each handoff, deciding what happens after a red scoreboard | mid |
| `implementer` | One packet against named paths, with an objective verdict | cheap or local |
| `reviewer` | Read-only judgement of a diff against its packet; may reject | strongest |

### When not to delegate

Delegation is an investment that pays when the worker role *changes model or
location*. Splitting across subagents on the same model costs more than it
saves — every handoff re-reads the same headers and rediscovers the same
conventions, and the orchestrator pays to absorb the summary. Same-model
delegation is justified for isolation (fresh context, a reviewable boundary),
never for token savings.

Work in this repository that should stay in one context, not be delegated:

- a subtle identity-switch or Kalman-drift bug, where judgement is the bottleneck
- anything touching the association cost matrix, where decision 10 in the
  [OC-SORT packet](2026-08-18-ocsort-cbiou/requirements.md) is the kind of
  non-obvious invariant a cheap worker will not infer
- an interface change to `BaseTracker`, which is `architect` work

Work that delegates well: a new ctest suite against a fixed schema, a CMake
target with named sources, a flag threaded through `AppConfig` →
`CommandLineParser` → `makeTrackConfig`, a signature change applied across call
sites.

### Context relocation

Moving work to many small workers does not reduce total context; it relocates
it to the planner, which is the expensive role. Prefer fewer, better-briefed
workers over maximal decomposition, and point packets at artifacts rather than
pasting their contents. Measure planner and worker separately or the relocation
is invisible.

## Enforced vs advisory

Agent-definition prose is persuasion. Only the harness-applied part is
enforcement. For Claude Code specifically:

| Boundary | Status here | Backstop |
|---|---|---|
| Reachable tools per role | **enforced** — `tools` / `disallowedTools` in the agent definition | — |
| Reviewer cannot write | **enforced** — `reviewer` has no `Write`/`Edit` tool | — |
| Web fetch and search off *for implementers* | **enforced** — `disallowedTools: WebFetch, WebSearch` in the `implementer` definition | — |
| No `sudo`, no package installs | **enforced** — denied in `.claude/settings.json` | — |
| No destructive git (`reset --hard`, `clean`, `stash`, `checkout --`, `push`) | **enforced** — denied in `.claude/settings.json` | — |
| Build trees are read-only | **enforced** — `Edit(./build*/**)` denied | — |
| **"Touch only the packet's writable paths"** | **advisory** | reviewer must diff against the packet and reject on any path outside it |
| **"Do not edit `specs/`, `tests/`, `CMakePresets.json`, `versions.env`"** | **advisory** | same — see below |
| **"Stop after the scoreboard run"** | **advisory** | `maxTurns` is per-invocation and returns partial output; the parent owns termination |

Why the acceptance suite is not write-denied in settings: Claude Code permission
rules are session-wide and inherited by subagents rather than being per-agent
allowlists. A deny on `Edit(./specs/**)` would also stop the planner from
writing the packet it exists to write. The immutability of the acceptance suite
is therefore a **review obligation** in this repository, and is named as one
rather than dressed up as a setting. Concretely, every review runs:

```bash
git diff --name-only <base>..HEAD
```

and rejects the diff if it touches `specs/`, `tests/` (beyond suites the packet
explicitly authorises), `CMakePresets.json`, `versions.env`, `cmake/versions.cmake`,
`.github/`, or `.claude/`.

If a run needs the acceptance suite genuinely immutable, run it in a harness
with per-path write rules (OpenCode) or in a worktree the worker cannot reach.

## Handoff packet template

The packet is the interface between planner and worker. Size it for the
worker's capacity, not the planner's.

```markdown
## Delegation packet — <phase name>

Writable paths (nothing beyond these):
  - <repo/path — new | modify: what change is expected>

Read-only — read these, never modify them:
  - include/BaseTracker.hpp, include/TrackConfig.hpp — the interface
  - tests/<suite>.cpp — the scoreboard that judges you
  - specs/tech-stack.md — constraints (do not open unless a clause is unclear)

Permitted commands:
  - cmake --build --preset scoreboard [--target <t>]
  - ctest --preset scoreboard -R <suite>
  - cmake --build --preset strict
  - cmake --workflow --preset scoreboard   (once, at the end)

Required final state:
  - <public surface, behaviour, error handling>
  - <exact identifiers and strings where tests assert on them>
  - <existing behaviour that must survive unchanged>
  - Inherited constraints: C++20; no new dependency; clean under
    -Wall -Wextra -Wpedantic -Werror; .clang-format (4-space, 120 col);
    detections stay neuriplo_tasks::Detection; trackers/ must not
    depend on app/.

Working method:
  - For every writable file, write the complete final content; never patch by anchor.
  - Read the real file before changing it. Run targeted checks as often as useful.

Scoreboard, run once as your final action:
  cmake --workflow --preset scoreboard
  Stop after it, pass or fail, and report the exit status.
```

Leave out: pointers into `specs/` that the worker must chase, pasted whole
files, and finished code — otherwise the expensive model wrote it and the cheap
one transcribed it.

Every clause of *required final state* must be something the scoreboard can
check and the reviewer can reject on. The packet fixes the obligations; the
worker chooses the implementation.

## After a red scoreboard

The failure mode to watch for is the planner quietly rewriting weak output
itself. Cost returns to the single-model baseline while the workflow still
looks delegated.

1. The worker reports the exit status and stops. It does not repair.
2. The planner decides: redelegate, resplit, or escalate to `architect`.
3. Defects go back in a **fresh packet to a fresh worker**, not as a patch.
4. Verify afterwards which role consumed the implementation tokens. The attempt
   log shows it.

## Clean starts

Every attempt starts from a declared revision with fresh context.

- Record `git status --porcelain` and the branch before the run.
- A dirty tree is not the agent's to clean. Use `git worktree add` under a
  run-owned directory, or leave the tree to the human. In-tree runs require a
  clean tree.
- A run may destroy only state it created, in paths it can name. On anything
  unexpected or concurrent: abort and report.
- Never `git reset --hard`, `git checkout -- .`, `git clean`, or an untargeted
  `git stash pop` against pre-existing state. These are denied in
  `.claude/settings.json` rather than merely discouraged.
- Return to the recorded SHA afterwards and verify it, rather than assuming it.

## Attempt log

One row per attempt, appended to `specs/attempts.md`. An attempt is one packet
given to one worker, ending in one scoreboard run.

| Field | Meaning |
|---|---|
| `date` | ISO date |
| `packet` | the spec packet and group, e.g. `2026-08-20-evaluation G3` |
| `base` | starting commit SHA |
| `planner` / `worker` | role and model tier actually used |
| `planner_ctx` / `worker_ctx` | context consumed, split by role |
| `tool_calls`, `turns`, `wall` | per role where the harness reports it |
| `cost` | metered cost, or `local` |
| `outcome` | the scoreboard exit status — pre-repair by construction, since it runs once |
| `interventions` | times a human or the planner had to step in |
| `notes` | what changed versus the previous attempt |

A single run proves nothing; these systems are stochastic. To compare two
configurations: same packet, same base commit, same packet structure, one
variable changed, reasoning effort pinned on both sides, each configuration run
more than once, fresh context and a revert between runs. If changing the model
also required changing the workflow, record that — the row then compares two
ways of working, not two sets of weights.

## Egress audit

Recorded here so a later tool addition cannot silently reopen a channel.
Re-audit whenever tooling is added for a phase.

| Path | Status |
|---|---|
| Web fetch / web search, `implementer` | **denied** — `disallowedTools` in the agent definition |
| Web fetch / web search, `architect` | **allowed on purpose** — reading papers and upstream APIs is architect work |
| Web fetch / web search, `reviewer` | **denied** — the role has no web tools |
| Package installs (`pip`, `apt-get`, `sudo`) | **denied** in `.claude/settings.json` |
| `git push` | **denied**; publishing is a human action |
| CMake `FetchContent` | **allowed, pinned** — every ref fixed in `versions.env`; no floating branches since `d96b8cc` |
| ONNX Runtime download | **not exercised by the scoreboard** — `OPENCV_DNN` needs no runtime download |
| Harness telemetry | accepted risk, not audited here |

The `scoreboard` preset is the only command an implementer must run, and with a
populated `build/scoreboard/_deps` it needs no network at all.
