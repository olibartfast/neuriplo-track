# Feature Validation — integrations

> **Historical record.** The checks below were written **before**
> implementation, so success could not be defined to fit whatever got built.
> The Results section is filled in afterwards.

Every check is a command whose output is recorded, or an inspection with a
stated pass condition. A check that could not be run is recorded as
**not executed**, never as a pass.

Throughout, `$PREFIX` is a temporary install prefix inside the build tree
(`build/scoreboard/consumer-prefix`), and `$SCRATCH` a throwaway consumer
project. Nothing below requires `sudo` or writes outside the build tree.

## Traceability

| Requirement | Checks |
|---|---|
| R-1 public/private header split | V-1, V-2 |
| R-2 exportable usage requirements | V-3, V-4 |
| R-3 two components | V-5, V-6, V-7 |
| R-4 ByteTrack not in the export set | V-4, V-8 |
| R-5 pins honoured | V-9, V-10 |
| R-6 version and stability policy | V-11, V-12 |
| R-7 ABI hardening | V-13, V-14, V-15 |
| R-8 packaging surfaces | V-6, V-16, V-17, V-18 |
| R-9 consumer smoke test | V-6, V-7 |
| R-10 documentation | V-19, V-20 |
| No behavioural regression (Constraints) | V-21, V-22 |

## Checks

### V-1 — Private headers are not installed

```bash
cmake --install build/scoreboard --prefix "$PREFIX"
find "$PREFIX/include" -name '*.hpp' | sort
```

**Pass:** exactly the contract headers appear — `BaseTracker.hpp`,
`TrackConfig.hpp`, `TrackedObject.hpp` and the wrapper headers. No `OCSort.hpp`,
`CBIoUTracker.hpp`, `Hungarian.hpp`, `MaskOverlap.hpp`, BoTSORT or ByteTrack
header is present anywhere under `$PREFIX/include`.

### V-2 — Private headers are unreachable from a consumer

A consumer translation unit containing `#include "OCSort.hpp"` fails to compile
against `neuriplo-track::core`.

**Pass:** compilation fails with a "file not found" diagnostic. Recorded as a
transcript; not committed as a test.

### V-3 — No build-tree path escapes into the export file

```bash
grep -nE "$(pwd)|/home/|_deps|BUILD_INTERFACE" "$PREFIX"/lib*/cmake/neuriplo-track/*.cmake
```

**Pass:** no match referencing an absolute source, build or `_deps` path.
`$<INSTALL_INTERFACE:>`-derived relative paths only.

### V-4 — No unresolvable target in the export set

```bash
grep -nE "bytetrack|libonnxruntime\.so|ONNX_RUNTIME_DIR" "$PREFIX"/lib*/cmake/neuriplo-track/*.cmake
```

**Pass:** no reference to a `bytetrack` target and no absolute
`libonnxruntime.so` path. An `INTERFACE_LINK_LIBRARIES` entry naming an imported
ONNX Runtime target on the `reid` component only is acceptable and is recorded.

### V-5 — `core` builds and installs with no ONNX Runtime

On an environment with `ONNX_RUNTIME_DIR` unset and no ONNX Runtime present:

```bash
cmake --preset scoreboard -DNEURIPLO_TRACK_USE_SIBLING_CHECKOUTS=OFF
cmake --build --preset scoreboard --target neuriplo-track-core
```

**Pass:** `core` configures and builds; the configure log states that `reid` is
skipped and why. No ONNX Runtime download is attempted.

### V-6 — A downstream project consumes the installed package

```bash
ctest --preset scoreboard -R consumer --output-on-failure
```

Driving `$SCRATCH` containing only:

```cmake
find_package(neuriplo-track 0.1 REQUIRED COMPONENTS core)
target_link_libraries(consumer PRIVATE neuriplo-track::core)
```

**Pass:** the consumer configures, builds, links, runs three scripted frames
through a tracker via `BaseTracker`, and exits 0. The consumer source does not
name any path inside this repository.

### V-7 — `COMPONENTS core` links no ONNX Runtime

```bash
ldd "$SCRATCH/build/consumer" | grep -i onnxruntime
```

**Pass:** no match. Recorded as part of the `consumer` ctest case, so a
regression fails the scoreboard.

### V-8 — ByteTrack symbols are present but private

```bash
nm -D --defined-only "$PREFIX"/lib*/libneuriplo-track-core.so* | grep -ci byte_track
ldd "$PREFIX"/lib*/libneuriplo-track-core.so*
```

**Pass:** ByteTrack functionality works (V-22 covers behaviour) with no
`libbytetrack` entry in `ldd` output — the objects are absorbed, not linked as
a separate artifact.

### V-9 — The sibling opt-out is honoured

```bash
cmake --preset scoreboard -DNEURIPLO_TRACK_USE_SIBLING_CHECKOUTS=OFF 2>&1 | tee cfg.log
grep FETCHCONTENT_SOURCE_DIR build/scoreboard/CMakeCache.txt
```

**Pass:** `cfg.log` states `pinned v0.8.1` for neuriplo-tasks, and the cache
contains no `FETCHCONTENT_SOURCE_DIR_NEURIPLO*` entry — run on this machine,
where `../neuriplo-tasks` exists and is at a different ref.

### V-10 — Installing from a sibling build is refused

```bash
cmake --preset scoreboard -DNEURIPLO_TRACK_USE_SIBLING_CHECKOUTS=ON -DNEURIPLO_TRACK_INSTALL=ON
```

**Pass:** configure fails with a `FATAL_ERROR` naming both sibling paths and how
to proceed. The message, verbatim, is recorded in Results.

### V-11 — Version file rejects an incompatible request

```bash
find_package(neuriplo-track 9.0 REQUIRED COMPONENTS core)   # in $SCRATCH
```

**Pass:** configure fails with CMake's "could not find a configuration file
compatible with requested version" diagnostic. The `0.1` request in V-6
succeeds.

### V-12 — SOVERSION is set on both components

```bash
readelf -d "$PREFIX"/lib*/libneuriplo-track-core.so* | grep SONAME
readelf -d "$PREFIX"/lib*/libneuriplo-track-reid.so* | grep SONAME
```

**Pass:** each reports a versioned `SONAME`, and the symlink chain
`libX.so → libX.so.<major> → libX.so.<full>` exists in `$PREFIX`.

### V-13 — `TrackedObject` stays trivially copyable

**Pass:** `static_assert(std::is_trivially_copyable_v<TrackedObject>)` is
present in `include/TrackedObject.hpp` and the project compiles. Removing a
field's triviality breaks the build — verified once by temporarily adding a
`std::string` member and recording the diagnostic.

### V-14 — `TrackConfig` PImpl preserves every resolved default

```bash
ctest --preset scoreboard -R track_config --output-on-failure
```

**Pass:** `test_track_config` passes **unchanged** — no assertion in it is
edited during Group 8. Any edit to that file in the Group 8 diff is a review
rejection, per [../orchestration.md](../orchestration.md).

### V-15 — The vtable policy is stated

**Pass:** `include/BaseTracker.hpp` carries a comment stating that virtuals are
appended and never inserted or reordered within a major version, and
`tech-stack.md` repeats it in the stability policy.

### V-16 — pkg-config agrees with the CMake target

```bash
PKG_CONFIG_PATH="$PREFIX"/lib*/pkgconfig pkg-config --cflags --libs neuriplo-track-core
```

**Pass:** the emitted include and link flags name the same install prefix as
the CMake target's `INTERFACE_INCLUDE_DIRECTORIES`, and contain no build-tree
or `_deps` path.

### V-17 — The Conan recipe builds

```bash
conan create packaging/conan --build=missing
```

**Pass:** the recipe builds and its `test_package` links. Recorded with the
Conan version used. **If Conan is not available in the environment, this is
recorded as not executed** — not as a pass.

### V-18 — The vcpkg port builds

```bash
vcpkg install neuriplo-track --overlay-ports=packaging/vcpkg
```

**Pass:** the port installs. Same not-executed rule as V-17.

### V-19 — Documentation describes what exists

**Pass:** `tech-stack.md` lists both components, every new option
(`NEURIPLO_TRACK_USE_SIBLING_CHECKOUTS`, `NEURIPLO_TRACK_INSTALL`,
`NEURIPLO_TRACK_INSTALL_CLI`), and the stability policy including the
neuriplo-tasks bound from R-6. `mission.md`'s "Integrators" bullet is amended
per Decision 1. The README snippet is the one in V-6, copied verbatim and
verified to work.

### V-20 — Links resolve

Per the hyperlink rule in `AGENTS.md`: every relative link in the touched
documents resolves to an existing file; every absolute GitHub URL returns a
non-error status.

### V-21 — The scoreboard passes at every group boundary

```bash
cmake --workflow --preset scoreboard
```

**Pass:** exit 0 with all suites passing, recorded once per group in the
attempt log ([../attempts.md](../attempts.md)), not only at the end.

### V-22 — Tracking output is byte-identical across the refactor

Before Group 1, capture a baseline for all five trackers over a fixed scripted
detection sequence. After Group 8, replay and compare.

```bash
diff baseline-<tracker>.txt after-<tracker>.txt
```

**Pass:** empty diff for SORT, ByteTrack, BoTSORT, OC-SORT and C-BIoU.

**Dependency:** this check needs the MOT writer and the `--detections` replay
path from [2026-08-20-evaluation](../2026-08-20-evaluation/requirements.md),
Groups 2-3, which are not yet delivered. If they are still missing when Group 1
starts, V-22 degrades to "the five existing ctest suites pass unchanged", which
is weaker — it covers tracker behaviour on scripted sequences but not full
per-frame output equality. Record which variant was used.

## Results

*Filled in during implementation. Each entry: date, command, outcome, and the
commit it was run at. Not executed is a valid outcome and must be written as
such.*

| Check | Date | Outcome | Evidence |
|---|---|---|---|
| V-1 … V-22 | — | not executed | Packet written 2026-09-22; implementation not started. |
