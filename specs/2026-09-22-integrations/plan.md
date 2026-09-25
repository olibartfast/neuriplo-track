# Feature Plan — integrations

> **Historical record.** Written before the work and not maintained afterwards.
> For what is true today see the constitution in [../mission.md](../mission.md),
> [../tech-stack.md](../tech-stack.md) and [../roadmap.md](../roadmap.md).

Ten groups, ordered so the build graph is correct before anything is installed,
and the install is correct before anything is packaged. Each ends in something
observable.

Every group names the role that should execute it, per
[../orchestration.md](../orchestration.md). The scoreboard,
`cmake --workflow --preset scoreboard`, must pass at every group boundary — the
five existing suites are the regression signal for "R-1 changes no behaviour".

## Group 1 — Make the pins honest

*Role: `implementer`. Prerequisite for every install claim.*

1. `NEURIPLO_TRACK_USE_SIBLING_CHECKOUTS` option, default `ON`. The two
   `FETCHCONTENT_SOURCE_DIR_*` force-sets happen only when it is `ON`.
2. A configure-time `message(STATUS ...)` per fetched dependency naming its
   source (`sibling <path>` or `pinned <ref>`) and, for a sibling, the output of
   `git -C <path> describe --always --dirty`.
3. `tests/test_utils.cpp` or a sibling: no test can assert on CMake behaviour,
   so this group's evidence is the configure log, captured in `validation.md`.

*Observable:* `cmake --preset scoreboard -DNEURIPLO_TRACK_USE_SIBLING_CHECKOUTS=OFF`
on a machine with sibling checkouts fetches `v0.8.1` and says so.

## Group 2 — Split public from private

*Role: `architect` decides the boundary; `implementer` applies it.*

4. Move every non-contract header out of the `PUBLIC` include set. `include/`
   stays `PUBLIC`; `trackers/SORT`, `trackers/OCSORT`, `trackers/CBIoU`,
   `trackers/common`, `trackers/BoTSORT/include` and the ByteTrack headers
   become `PRIVATE`.
5. Wrap the surviving public directories in `$<BUILD_INTERFACE:>` and add the
   matching `$<INSTALL_INTERFACE:include>`.
6. Fix the fallout: any `.cpp` that relied on a transitively public path gets an
   explicit private include.

*Observable:* the scoreboard passes, and a target linking `trackers` can no
longer `#include "OCSort.hpp"` — proven by a deliberately failing compile in the
group's notes, not committed.

## Group 3 — Absorb ByteTrack, privatise ONNX Runtime

*Role: `implementer`, with `architect` review of the link interface.*

7. ByteTrack's sources compile into the tracker library directly (object
   library or source list), so no fetched target appears in any export set.
8. ONNX Runtime becomes an `IMPORTED` target (`neuriplo-track::onnxruntime`,
   or a `find_package`-style module) instead of an absolute `.so` path, and is
   `PRIVATE`.
9. Eigen3, OpenCV and glog stay `PUBLIC` only where a public header requires
   them; anything else moves to `PRIVATE`.

*Observable:* `ldd` on the built library still resolves, the scoreboard passes,
and `cmake --workflow --preset lib-only` still builds.

## Group 4 — Two components

*Role: `architect` (target topology), then `implementer`.*

10. Split the library into `neuriplo-track-core` and `neuriplo-track-reid`
    targets with `EXPORT_NAME` `core` / `reid` and `neuriplo-track::` aliases.
    BoTSORT and `BoTSORTWrapper` move to `reid`; everything else to `core`.
11. `trackers` becomes an `ALIAS` (or an interface target linking both) marked
    deprecated in a comment and in `tech-stack.md`, so `BUILD_ONLY_LIB=ON` and
    every in-tree consumer keep working.
12. `tests/CMakeLists.txt` links the narrowest component each suite needs —
    `test_trackers` and `test_mask_overlap` against `core` alone.

*Observable:* configuring with no `ONNX_RUNTIME_DIR` and no ONNX Runtime present
builds `core` and its tests; `reid` is skipped with a clear status message.

## Group 5 — Install rules

*Role: `implementer`.*

13. `project(neuriplo-track VERSION 0.1.0)`, `SOVERSION` on both components.
14. `install(TARGETS ... EXPORT neuriplo-track-targets)` with `GNUInstallDirs`
    destinations; `install(DIRECTORY include/ ...)`; `install(EXPORT ...)` under
    the `neuriplo-track::` namespace.
15. `NEURIPLO_TRACK_INSTALL_CLI`, defaulting to `ON` when the CLI is built,
    installing the binary to `${CMAKE_INSTALL_BINDIR}`.
16. Installing with sibling checkouts active is a configure-time `FATAL_ERROR`
    naming both trees (R-5).

*Observable:* `cmake --install build/scoreboard --prefix /tmp/x` lays down
headers, two versioned shared libraries, the CLI, and an export file.

## Group 6 — The package config

*Role: `implementer`.*

17. `cmake/neuriplo-trackConfig.cmake.in` with component handling: an unknown
    component is an error, a requested-but-unbuilt component is an error naming
    what was missing at build time.
18. `find_dependency` for OpenCV, Eigen3, glog and neuriplo-tasks;
    `write_basic_package_version_file` with `SameMajorVersion`.
19. Confirm the Open Question on the starting version before this group merges.

*Observable:* `find_package(neuriplo-track REQUIRED COMPONENTS core)` succeeds
from a scratch project against the temporary prefix.

## Group 7 — The consumer smoke test

*Role: `implementer`. This is the group that makes every earlier claim checkable.*

20. `tests/consumer/CMakeLists.txt` + a `main.cpp` that constructs a tracker
    through `BaseTracker` and runs three scripted frames.
21. A ctest case that installs to `${CMAKE_BINARY_DIR}/consumer-prefix`, then
    configures and builds the consumer project against it in a nested
    `execute_process` / `ctest --build-and-test` step. No `sudo`, nothing
    written outside the build tree.
22. A second case asserting `COMPONENTS core` links without ONNX Runtime.

*Observable:* `ctest --preset scoreboard -R consumer` fails if any packaging
change breaks a downstream consumer.

## Group 8 — ABI hardening

*Role: `architect`. Do not delegate: this is interface design.*

23. PImpl `TrackConfig` behind accessors, preserving the current field
    semantics and `makeTrackConfig`'s per-algorithm defaults exactly.
24. `static_assert(std::is_trivially_copyable_v<TrackedObject>)` and a comment
    stating it is contract.
25. A documented vtable policy on `BaseTracker`: new virtuals are appended,
    never inserted, for the life of a major version.

*Observable:* the scoreboard passes with no change to any tracker's output, and
`test_track_config` still asserts the same resolved defaults.

## Group 9 — pkg-config, Conan, vcpkg

*Role: `implementer`. Independently revertible — if the packet runs long, this
group is the one to drop.*

26. `neuriplo-track-core.pc.in` / `neuriplo-track-reid.pc.in`, generated from
    the same install variables, installed to `${CMAKE_INSTALL_LIBDIR}/pkgconfig`.
27. `packaging/conan/conanfile.py` and `packaging/vcpkg/` port files, each
    exercised locally once and the command recorded in `validation.md`.
28. Neither is wired into CI, and nothing in the build requires either tool.

*Observable:* `pkg-config --cflags --libs neuriplo-track-core` against the
temporary prefix emits the same include and link flags the CMake target does.

## Group 10 — Documentation and roadmap

*Role: `implementer`, then `reviewer` for the whole packet.*

29. `tech-stack.md`: components, new options, the stability policy from R-6
    including the neuriplo-tasks bound, and the sibling-checkout opt-out.
30. `mission.md`: amend the "Integrators" user per Decision 1, so it no longer
    implies a dependency-free library.
31. `README.md`: an "Embedding the library" section with the `find_package`
    snippet from the Goal.
32. `docs/System_Architecture.md`: the public/private boundary.
33. `roadmap.md`: Phase 4 bullets one and two checked, with a link here; bullet
    three left open.
34. Link verification per `AGENTS.md`.

*Observable:* a reader who has never seen this repository can embed the tracker
library from the README alone.

## Sequencing notes

- Groups 1-4 are build-graph surgery with no new public behaviour. They are
  where regressions hide, and the five existing suites are the only thing
  standing between this packet and a silent behavioural change.
- Group 7 should not slip later. Until it exists, Groups 5 and 6 are unverified
  claims; once it exists, Groups 8-9 are cheap to check.
- Group 8 is `architect` work and should not be delegated to a cheap worker
  (see [../orchestration.md](../orchestration.md), "When not to delegate").
- Group 9 is the drop candidate. Groups 1-8 and 10 deliver the Goal on their
  own; pkg-config and the package-manager recipes are reach.
