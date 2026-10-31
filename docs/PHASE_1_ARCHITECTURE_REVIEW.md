# Phase 1.5 architecture review

Status: IMPLEMENTED corrections; local and hosted Windows verification recorded in
[validation](PHASE_1_VALIDATION.md) and [reproducibility](PHASE_1_REPRODUCIBILITY.md).

## Inspection baseline

Started from clean `021513c` on `feat/phase-1-ngspice-rc`, not from `main`.
That feature branch is still unmerged. Created `codex/phase-1-5-hardening` from it.
Read root/docs agent guidance, architecture, Phase 1 implementation/validation,
both ADRs, and every RC source/test file. Rebuilt the original CMake tree and ran
CTest: 3/3 passed, 59 unit checks, zero skips, actual ngspice-47 integration.
Native-grid baseline maximum error was 1.429824283105674e-5 V. There was no CI.

## Actual boundaries

| Concern | Actual ownership and finding |
| --- | --- |
| Circuit configuration | `circuit/rc` owns numeric SI configuration and supported bounds. No ngspice command strings leak into it. Zero IC remains the only supported IC. |
| Netlist generation | `backend/ngspice` owns node names, PULSE, `.ic`, solver controls and `wrdata`. This is intentionally RC-specific. |
| Execution | `backend/process` owns shell-free argv, cwd, redirected streams, timeout and process lifecycle. `backend/ngspice` owns version/diagnostic checks. |
| Numerical retrieval | Backend-specific table parser returns generic `Sample` values. `io/samples` owns normalized CSV and sample integrity. |
| Reference/validation | `analysis` takes only configuration/time; `validation` compares generic samples. Native acceptance remains available and common-grid acceptance is added. |
| Composition/artifacts | `io/report` coordinates one RC run and writes JSON/CSV. It knows the concrete ngspice adapter; CLI/study clients no longer duplicate backend orchestration. This is a narrow coordinator, not a plugin abstraction. |
| Failures | Exceptions propagate to a nonzero CLI exit. Run JSON records execution and validation separately. Missing/failed output cannot become successful validation. |
| Resources | Atomic directory creation reserves each workspace. Explicit outputs must be new. Processes/handles are cleaned up; artifacts are deliberately retained, including failures. No filesystem deletion routine exists. |
| Testability | Backend-independent mathematics, interpolation, serialization and parsers use unit checks. Process faults use a dedicated helper executable. Real integration requires ngspice and never substitutes synthetic simulator output. |

## Concrete problems and corrections

| Observed gap/problem | Correction | Compatibility and validation |
| --- | --- | --- |
| Adaptive sample weighting makes different timesteps incomparable. | Explicit shared 4001-point grid and piecewise-linear interpolation with no extrapolation. Keep native integrity and tolerance checks too. | Original `validate` retained; new `validate_on_grid` used by run reporting. New interpolation/metric tests and four real refinement runs. |
| Finite-ramp input differs from ideal step even after rise. | Add independent ramp formula for diagnosis only; keep ideal-step acceptance and 1% tolerance unchanged. | No simulator/solver change; tests of ramp endpoint and characteristic ideal values. |
| Metadata was ad hoc text, lacked content/build identity and machine-readable validation. | Central configuration serialization, JSON reports, normalized-LF netlist FNV-1a-64 identity, build source SHA256, Git revision, compiler/platform. | CSV waveform schema unchanged; current JSON replaces old metadata.txt for new runs. Historical evidence untouched. Independently parsed/recomputed JSON, hashes and metrics. |
| Invalid config opened a netlist before rejecting it. | Validate at backend/run entry before any filesystem/process operation. | Invalid-input test asserts no run directory is created. |
| Coverage check accepted a missing zero-time sample. | Require exact t=0 first sample; retain full-duration/max-gap checks. | Existing ngspice output begins at zero; all integration runs pass. |
| Timeout was fixed and untested; Windows error code was read after handle closes. | Configurable bounded timeout with 30 s default; capture launch error immediately; Windows cleanup job, POSIX process group; explicit wait/reaping. | Forced timeout test checks prompt failure and no delayed child artifact. Windows verified; POSIX unverified. |
| Concurrent Windows launches could inherit each other's temporary handles. | Serialize only creation/launch/closure of this launcher's inheritable handles; processes still run concurrently. | Two simultaneous literal-argv/cwd tests pass with distinct atomic workspaces. Arbitrary external inheritable handles are not a supported sandbox contract. |
| Unique paths relied on a timestamp without retry. | Atomic create with collision retries for helper/test workspaces; exact requested run directories reject collisions. | Concurrency and user-sentinel preservation tests. |
| Missing-signal, real invalid-netlist and argument tests absent. | Add strict header fault, real ngspice invalid-netlist test, shell-metacharacter/quote/space/backslash/empty-argument cases. | All pass; invalid ngspice netlist exits 1. No parameter string enters a command shell. |
| No hosted build pipeline. | Windows UCRT GitHub Actions workflow pins ngspice 47-1 archive/hash, configures/tests and collects artifacts. | VERIFIED on hosted Windows after the runtime-path repair; see [run/evidence](PHASE_1_REPRODUCIBILITY.md). No Linux/MSVC support claim. |
| Hosted initialization double-prefixed already native code-model paths. | Token-aware conversion of only MSYS-rooted arguments; preserve native paths and package conditionals; capture configuration and check real initialization twice. | Five regression tests, seven model files checked, idempotent snapshots, strict diagnostics and real RC integration passed in hosted run 41217248663. |

No numerical solver tolerance, component model, supported component family, GUI,
database or LLM dependency changed. [ADR-0002](decisions/ADR-0002-phase-1-ngspice-executable.md)
still governs executable integration. The common-grid addition refines validation,
not the backend selection.

## Exact runtime license review

Inspected `ucrt64/share/licenses/ngspice/LICENSE` from MSYS2 **ngspice 47-1**, the
actual executable package used. Archive SHA256 is recorded in
[implementation](PHASE_1_IMPLEMENTATION.md); the extracted license file SHA256 is
`6d939001e899ede403277fb37124c2af6f1cf92926465cd8f4bf59859e06f579`.

The package record labels it BSD-3-Clause, but its own license notice explicitly
lists exceptions: tclspice and KLU (LGPLv2), numparam (LGPLv2 or newer), OSDI
(MPLv2.0), the XSPICE table model (GPLv2 or newer), Sparse (MIT-style), and
public-domain portions. The notice itself is CC-BY-SA 4.0. The tested banner
includes KLU support and the package supplies code-model binaries; an RC run
using SPARSE does not prove other bundled components can be ignored for distribution.

The actual process also depends on FFTW, readline, compiler/OpenMP runtimes and
other DLLs. Review their exact artifacts and notices before distributing a bundle.
FFTW's package lists GPL-2.0-or-later. Executable separation avoids linking ngspice
into Datasim, but is not by itself a conclusion about every distribution obligation.
A shared-library integration would additionally need review of the exact linked
build, linkage and applicable license requirements. No claim of blanket BSD-only
licensing, proprietary-distribution clearance, or legal equivalence is made.
This task ships neither runtime binaries nor vendor models and leaves the Datasim
repository license decision open.

Primary references: [ngspice project licensing overview](https://ngspice.sourceforge.io/devel.html),
[exact ngspice package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-ngspice),
[FFTW package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-fftw).
The extracted release notice, rather than the abbreviated package label alone,
is the basis of the exception inventory above.

## Deferred and unverified

No generic backend interface/registry is warranted for one RC spike. Full run
manifests of every DLL/environment variable, cryptographic runtime-file integrity,
multi-platform CI, descendant-process fault injection, detailed diagnostic taxonomy,
untrusted-netlist sandboxing and atomic crash recovery are deferred. FNV is a
change detector, not security verification. The POSIX launcher is implemented
but not exercised here. A power loss can leave incomplete artifacts; consumers
must require completed success statuses, not infer success from file presence.
