# Feature Requirements — integrations

> **Historical record.** Written before the work and not maintained afterwards.
> For what is true today see the constitution in [../mission.md](../mission.md),
> [../tech-stack.md](../tech-stack.md) and [../roadmap.md](../roadmap.md), or
> [specs/README.md](../README.md) for how these documents relate.

Roadmap phase: [Phase 4 — Consumability](../roadmap.md), bullets one and two
("Install and export targets", "Version the tracker library"), plus the ABI
work those two imply but the roadmap does not name. "Broaden backend CI
coverage" stays on the roadmap; see Out of Scope.

## Goal

A downstream C++ project can write

```cmake
find_package(neuriplo-track 0.1 REQUIRED COMPONENTS core)
target_link_libraries(my_app PRIVATE neuriplo-track::core)
```

against an installed copy, with no vendoring of this source tree, and get
SORT / ByteTrack / OC-SORT / C-BIoU without ONNX Runtime anywhere in its link
line.

Today that is impossible, and not only because `install()` is missing. The
`trackers` target's usage requirements are unexportable by construction, which
is what makes this a packet rather than a chore.

## What is actually broken

Established by inspection on 2026-09-22.

1. **`mission.md` promises a user the code cannot serve.** It names
   *"Integrators who want only the `trackers` library and already have their own
   detection source"*, but `BaseTracker::update` takes
   `neuriplo_tasks::Detection`, so that integrator needs neuriplo-tasks anyway.
   Mitigating fact found during the interview: `Detection` is a header-only
   aggregate (`Classification` + `BoundingBox`), so the cost is a header
   dependency, not the detection stack. `InstanceSegmentation` is *not* — it
   holds an `ImageMatrix`, which has a compiled implementation.
2. **Every include directory is `PUBLIC` and build-tree-absolute.**
   `${CMAKE_CURRENT_LIST_DIR}/SORT`, `/OCSORT`, `/CBIoU`, `/common`,
   `/BoTSORT/include`, `${bytetrack_SOURCE_DIR}/include`,
   `${ONNX_RUNTIME_DIR}/include`. None of these survive installation, and
   because all of them are public there is no contract surface to attach a
   version to.
3. **ONNX Runtime is linked `PUBLIC` by absolute path**
   (`${ONNX_RUNTIME_DIR}/lib/libonnxruntime.so`). An exported target would
   carry the build machine's filesystem layout into every consumer, and a
   motion-only integrator would still link ONNX Runtime — it is there solely
   for BoTSORT's Re-ID.
4. **`bytetrack` is fetched, never installed.** `install(EXPORT)` on a target
   that links it fails: the export set references a target no consumer can
   resolve.
5. **The dependency pins are not honoured on a developer machine.**
   `CMakeLists.txt` force-sets `FETCHCONTENT_SOURCE_DIR_*` whenever
   `../neuriplo-tasks` or `../neuriplo` exists, with no opt-out. A package built
   there records a version it was not built against. This contradicts mission
   principle 4 and makes any version promise unverifiable.

## In Scope

### R-1 — An honest public/private header split

`include/` becomes the contract: `BaseTracker.hpp`, `TrackConfig.hpp`,
`TrackedObject.hpp`, and the wrapper headers. Everything under `trackers/SORT`,
`trackers/OCSORT`, `trackers/CBIoU`, `trackers/common`, `trackers/BoTSORT` and
the fetched ByteTrack headers becomes `PRIVATE`. Only `include/` is installed.

### R-2 — Exportable usage requirements

Every `PUBLIC` include directory is wrapped in
`$<BUILD_INTERFACE:>` / `$<INSTALL_INTERFACE:>`. ONNX Runtime becomes an
`IMPORTED` target rather than an absolute path, and `PRIVATE` to the Re-ID
component. Eigen and OpenCV stay `PUBLIC` where the headers genuinely require
them, and become `find_dependency` entries in the package config.

### R-3 — Two components

| Component | Target | Contains | Requires |
|---|---|---|---|
| `core` | `neuriplo-track::core` | SORT, ByteTrack, OC-SORT, C-BIoU, mask overlap, their wrappers | OpenCV, Eigen3, neuriplo-tasks, ByteTrack |
| `reid` | `neuriplo-track::reid` | BoTSORT and its wrapper | `core` + ONNX Runtime |

`find_package(neuriplo-track COMPONENTS core)` must succeed on a machine with no
ONNX Runtime installed and no `ONNX_RUNTIME_DIR` set. `trackers` remains as a
deprecated alias target for one release so existing in-tree consumers and
`BUILD_ONLY_LIB=ON` users do not break.

### R-4 — ByteTrack stops being an export hazard

ByteTrack's objects are absorbed into `core` (via an object library or direct
source inclusion) so no fetched target appears in the export set. Its headers
become private. This is a build-graph change only; `ByteTrackWrapper` behaviour
is untouched.

### R-5 — Dependency pins actually honoured

A `NEURIPLO_TRACK_USE_SIBLING_CHECKOUTS` option, defaulting `ON` to preserve
today's cross-repo workflow, gated so that `-DNEURIPLO_TRACK_USE_SIBLING_CHECKOUTS=OFF`
forces the pinned refs. The install/export path requires it `OFF`: configuring
with installation enabled and siblings in use is a configure-time error naming
both trees. The configure log states which source each dependency came from and
at which ref.

### R-6 — Version and stability policy

`project(neuriplo-track VERSION ...)`, `SOVERSION` on both components, a
`neuriplo-trackConfigVersion.cmake` with `SameMajorVersion` compatibility, and a
written policy in `tech-stack.md` stating what is contract
(`BaseTracker`, `TrackConfig`, `TrackedObject`, the wrapper headers) and what is
not (everything private per R-1).

The policy must state the bound the interview created: because the public
interface carries `neuriplo_tasks::Detection`, this project's SOVERSION cannot
promise more stability than neuriplo-tasks' own result types offer. The pin in
`versions.env` is therefore part of the ABI contract, and a neuriplo-tasks major
bump is a neuriplo-track major bump. See Decisions 1.

### R-7 — ABI hardening of the contract headers

`TrackConfig` grows per-algorithm fields with every tracker added, and it is
passed by reference through `BaseTracker`'s constructor, so today every added
field is an ABI break. PImpl `TrackConfig` behind its accessors, keep
`TrackedObject` a trivially-copyable aggregate and assert that with a
`static_assert`, and give `BaseTracker` a reserved virtual slot policy — stated
in the header — so adding an overload does not reorder the vtable.

### R-8 — Packaging surfaces

- **CMake package config** — `install(TARGETS ... EXPORT)`, a
  `configure_package_config_file`-generated `neuriplo-trackConfig.cmake` with
  component handling and `find_dependency` for OpenCV, Eigen3, glog and
  neuriplo-tasks.
- **pkg-config** — `neuriplo-track-core.pc` and `neuriplo-track-reid.pc`,
  generated from the same variables, for Meson/Make consumers.
- **Conan recipe and vcpkg port**, in-repo under `packaging/`, buildable and
  testable locally. Submission to the upstream registries is out of scope.
- **The CLI** — `install(TARGETS neuriplo-track ...)` to `bin/`, plus shell
  completion, behind `NEURIPLO_TRACK_INSTALL_CLI` (default `ON` when the CLI is
  built).

### R-9 — A consumer smoke test that is not run from this tree

`tests/consumer/` holds a minimal CMake project that `find_package`es the
*installed* package and links `neuriplo-track::core`. It is driven by a ctest
case that installs to a temporary prefix and configures/builds the consumer
against it. A packaging change that breaks a downstream consumer must fail the
scoreboard, not a user's bug report.

### R-10 — Documentation

`tech-stack.md` (components, install options, stability policy), `README.md`
(an "Embedding the library" section), `docs/System_Architecture.md` (the
public/private boundary), and `roadmap.md` status.

## Out of Scope

- **Broadening backend CI coverage** (Phase 4, bullet three). It is a CI-matrix
  question, independent of the install interface, and folding it in would make
  an already large packet unreviewable. Stays on [roadmap.md](../roadmap.md).
- **Submitting the Conan recipe or vcpkg port upstream.** Both are built and
  tested in-repo; publishing is a release decision with an ongoing maintenance
  cost, taken separately.
- **A tracker-native detection type.** Rejected at interview — see Decisions 1.
- **Windows and macOS install layouts.** The policy and rules are written not to
  preclude them, but only the Linux layout is tested here. GNUInstallDirs is
  used so the paths are not hardcoded.
- **Installing neuriplo-tasks or neuriplo from this tree.** Consumers
  `find_package` them; this project does not vend its dependencies' packages.
  See Decisions 4.
- **Phase 2 and Phase 3 leftovers** — `--input_sizes` parsing, BoTSORT config
  fail-fast, and evaluation Groups 2-5, which are their own packets.

## Decisions

1. **The public interface keeps `neuriplo_tasks::Detection`.** The alternative
   considered was a tracker-native value type with a header-only adapter, which
   would serve the standalone integrator named in `mission.md` and match
   principle 2's "parallel types only with an explicit adapter". It was rejected
   because it touches every wrapper and every test for a user who is, in
   practice, adequately served: `Detection` is a header-only aggregate and
   neuriplo-tasks already ships a proper CMake package. The consequence is
   recorded honestly in R-6 — this project cannot promise ABI stability beyond
   its dependency's — and `mission.md` is amended so that it no longer implies
   a dependency-free library.
2. **Two components, not one library and not a build-time switch.** A single
   target with private ONNX Runtime still ships the runtime to every consumer; a
   `WITH_REID=OFF` build option makes the installed package's capabilities
   depend on how it was built, which is unanswerable from a `find_package` call.
   Components let the consumer state what it needs and fail at configure time
   when it is absent.
3. **`trackers` survives as a deprecated alias.** `BUILD_ONLY_LIB=ON` and every
   in-tree reference name it. Renaming and aliasing in one release keeps the
   change reviewable; the alias is removed at the next major.
4. **Dependencies are found, not vendored, by the installed package.** The
   package config emits `find_dependency(neuriplo-tasks)`, which requires
   neuriplo-tasks to be installed. This is a real constraint on consumers, and
   the alternative — installing our fetched copy — would have this project
   shipping another project's package and its headers, with no way to reconcile
   two installs.
5. **Sibling checkouts stay the default `ON`.** They are a deliberate
   cross-repo workflow, documented in `tech-stack.md`, and were not the defect;
   the defect is that they are unconditional and silent. R-5 keeps the
   convenience and removes the silence.
6. **`TrackedObject` stays an aggregate; `TrackConfig` gets PImpl.**
   `TrackedObject` is seven POD fields and is returned by value in a `vector`
   from every `update` — PImpl there would cost an allocation per tracked object
   per frame for no stability gain, since the field set is stable.
   `TrackConfig` is the opposite: it grew per-algorithm fields with OC-SORT,
   C-BIoU and the mask cue, and will grow again with the next tracker.
7. **The consumer smoke test installs to a temporary prefix inside the build
   tree.** No `sudo`, no writes outside the build directory, so it runs in CI
   and under the agent permission rules in
   [../orchestration.md](../orchestration.md).

## Constraints and Context

- C++20, CMake ≥ 3.20 for the build itself; presets need 3.25 but are not
  required to build.
- Clean under `-Wall -Wextra -Wpedantic -Werror`; `.clang-format` (4-space,
  120 columns).
- No new runtime dependency. Conan and vcpkg files are packaging metadata, not
  build dependencies, and nothing in the build may require either.
- `GNUInstallDirs` for every path; no hardcoded `lib/` or `lib64/`.
- `CMAKE_POSITION_INDEPENDENT_CODE ON` must be preserved — `trackers` is SHARED
  and links a fetched static `neuriplo-tasks` ([../tech-stack.md](../tech-stack.md)).
- R-1 changes no behaviour. Any behavioural difference in the tracking output is
  a defect in this packet, and the existing five ctest suites must pass
  unchanged at every group boundary.
- Docs with hyperlinks are subject to the link-verification rule in
  [../../AGENTS.md](../../AGENTS.md).

## Open Questions

- **The installed package's version number.** The project has no `VERSION`
  today and no tags. R-6 needs a starting point; `0.1.0` is assumed, with
  `SameMajorVersion` meaning `0.x` gives no compatibility promise — which is
  accurate for a first packaging release. To be confirmed before Group 6.
- **Whether `reid` should be installed at all in the first release.** BoTSORT's
  INI configuration files and Re-ID ONNX model are runtime assets with no
  install story yet. If that is unresolved when Group 5 starts, `reid` ships as
  a build-tree-only component and the roadmap picks up the asset question.
