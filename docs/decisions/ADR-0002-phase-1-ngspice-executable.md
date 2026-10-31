# ADR-0002: Phase 1 ngspice executable integration

Date: 2026-10-31

Status: selected for Phase 1 implementation; verification evidence is recorded in
[Phase 1 validation](../PHASE_1_VALIDATION.md).

## Scope change and context

The Phase 1 user request explicitly authorizes C++20 orchestration and an RC
transient feasibility spike. It supersedes the initialization-only prohibition
on code/build additions in docs/AGENTS.md and the original charter. The checkout
has no root AGENTS.md, source, build system, or selected compiler. Introduce CMake
and CTest with dependency-free C++ tests. This does not authorize later phases.
ADR-0001's backend boundary and deterministic numerical authority remain in force.

## Investigation

| Concern | Executable / batch | Shared library |
| --- | --- | --- |
| Complexity | Generate netlist, launch a process, read a table. | Load/link library; initialize callbacks and manage library state and lifetime. |
| Portability | Platform process launch differs; backend executable installed separately. | Platform library names/loading and compatible builds/headers required. |
| Results | `wrdata` with `wr_singlescale`, `wr_vecnames`, and precision controls. | `SendData`/`SendInitData` callbacks or `ngGet_Vec_Info` vector access. |
| Errors | Exit code, retained log, output/schema/coverage checks; process timeout. | `SendChar`, `ControlledExit`, return codes and background-run state must be handled. |
| Build | No ngspice headers or link dependency. | `sharedspice.h` plus compatible shared build and linking/loading strategy. |
| Maintenance | Small file contract and independent process; overhead acceptable for one circuit. | Better live control, but callback ownership and synchronization complicate a spike. |

The shared API starts with `ngSpice_Init`; `ngSpice_Circ` supplies a netlist and
`ngSpice_Command` issues commands. It can expose time-point data and background
execution, but Phase 1 does not require streaming or interactive changes.

## Decision

Implement only executable integration. Use a shell-free platform process launcher,
an explicit backend path or PATH discovery, and `-n -b` to suppress user spiceinit
and run batch control commands. Keep ngspice-specific netlist and table handling
inside the backend. Return generic time/voltage samples. Circuit configuration,
analytical reference, validation and CSV export have no ngspice API dependency.

Use a zero-volt operating point and explicit `.ic V(vout)=0` with no UIC, then
transient integration of a delayed pulse. Retain adaptive output points and set
the maximum integration step to the requested 10 us. No resampling is needed.
The user selected C++20; CMake is the initial build system. No GUI, database,
LLM runtime, shared-library integration, or custom solver is introduced.

## Consequences and limits

One process per run and strict table parsing are sufficient here. Reject unknown
backend configurations, bad parameters, diagnostics containing errors/warnings,
incomplete output and numerical validation failure. Fresh output directories
prevent stale results being mistaken for successful output. Platform support is
only VERIFIED where actual build/test evidence is recorded. Revisit shared
integration if interactive control or launch overhead becomes a demonstrated need.

## Original sources consulted

- [Official documentation and manual](https://ngspice.sourceforge.io/docs.html):
  PULSE, capacitor IC, `.ic`, transient analysis, batch flags and `wrdata`.
- [Official shared-library overview](https://ngspice.sourceforge.io/shared.html):
  controlling functions, callbacks and sharedspice.h; manual shared-library chapter.
- [Official downloads](https://ngspice.sourceforge.io/download.html): portable
  Windows executable distribution. Runtime is not vendored into source control.
