# specs/

Project specifications, kept in version control so intent survives outside any
one conversation. The layout follows the
[spec-driven workflow](https://olibartfast.ninja/blog/ai-spec-driven-development-workflow.html).

```
specs/
├── mission.md                   living
├── tech-stack.md                living
├── roadmap.md                   living
└── YYYY-MM-DD-feature-name/     historical
    ├── requirements.md          what and why
    ├── plan.md                  task groups
    └── validation.md            evidence, filled in after the work
```

## Living: the constitution

Three documents that describe the project **as it is now**. They are read before
planning any feature and are updated whenever reality changes.

| File | Answers |
|------|---------|
| [mission.md](mission.md) | Who this is for, what problem it solves, what it deliberately is not |
| [tech-stack.md](tech-stack.md) | Language, dependencies, backends, build options, layout, tooling |
| [roadmap.md](roadmap.md) | What shipped, what is next, in reviewable phases |

If one of these disagrees with the code, the document is wrong and should be
fixed.

## Historical: dated feature packets

One directory per delivered slice, named `YYYY-MM-DD-feature-name`. Each holds
the contract that was agreed **before** the code was written:

- `requirements.md` — the goal, what is in and out of scope, and the decisions
  taken with their rationale.
- `plan.md` — numbered task groups in dependency order, each ending in something
  observable.
- `validation.md` — the checks that define done, written before implementation
  so success cannot be defined to fit whatever got built. Its Results section is
  filled in afterwards, including anything that could not be run.

**A packet is a record, not documentation.** It is accurate as of the day its
branch merged and is not maintained after that. Read it to understand *why* a
decision was made; read the constitution, the code, and the tests to learn what
is true today. The one exception is the workflow's own rule: when a requirement
changes while the branch is still open, the packet is updated in the same branch
as the code, so the two never disagree at merge time.

Packets are kept rather than deleted because they carry reasoning the code
cannot: why `max_age` defaults differ per algorithm, why OC-SORT's re-update
rewinds to a snapshot, why a mask is discarded the moment a detection is missed.
[roadmap.md](roadmap.md) links to them by name.

## Promotion: what must not stay in a packet

Anything durable that a feature discovers belongs in the constitution or in
[AGENTS.md](../AGENTS.md), not only in a dated folder — otherwise the newest
work follows rules that older documents never learned. Examples already promoted:

- fetched targets must be built position independent, because `trackers` is a
  shared library linking a static dependency → [tech-stack.md](tech-stack.md)
- the five-step procedure for adding a tracker → [AGENTS.md](../AGENTS.md)
- per-algorithm defaults resolved in `makeTrackConfig` → [tech-stack.md](tech-stack.md)

## Starting a new packet

1. Pick the next item from [roadmap.md](roadmap.md) and branch for it.
2. `mkdir specs/$(date +%F)-feature-name`.
3. Interview for scope, decisions, and context; check the answers against
   [mission.md](mission.md) and [tech-stack.md](tech-stack.md).
4. Write all three documents before writing code. Validation especially: if a
   check is added after the fact, it tends to describe whatever was built.
5. Implement in task groups, running the closest checks after each.
6. Record results honestly — a check that could not be run is "not executed",
   not a pass — then update [roadmap.md](roadmap.md) and merge spec and code
   together.
