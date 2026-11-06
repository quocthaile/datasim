# Phase 5B validation

**Status:** Local Windows x64 Release builds and automated tests passed on the implementation worktree. GUI launch and interactive behavior remain pending/NOT VERIFIED until approval and direct inspection complete. Hosted feature-branch CI is pending.

## Baseline

- Phase 5A PR #15 merged into `main` at `483e4c7f4c43d5e78839dc92a4e1be59ae45660a`.
- Post-merge hosted Windows run [42155239727](https://github.com/quocthaile/datasim/actions/runs/42155239727) completed successfully.
- Phase 5B branch was created from refreshed `origin/main` at that commit.

## Environment

| Item | Observed value |
| --- | --- |
| OS / architecture | Windows x64 |
| Compiler | MSYS2 UCRT64 GCC 16.1.0 |
| CMake / generator | CMake 4.3.2, MinGW Makefiles, Release |
| Qt | MSYS2 UCRT64 Qt 6.11.1 Widgets |
| ngspice | MSYS2 ngspice-47 (`ngspice_con.exe`) |
| Poppler | MSYS2 Poppler 26.10.0 |

## Commands and results

Commands executed in `D:\projects\datasim-phase5b`:

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;C:\msys64\usr\bin;' + $env:PATH
cmake --fresh -S . -B build/phase5b-core -G 'MinGW Makefiles' `
  -DCMAKE_BUILD_TYPE=Release `
  -DDATASIM_NGSPICE=C:/msys64/ucrt64/bin/ngspice_con.exe
cmake --build build/phase5b-core -j 4
ctest --test-dir build/phase5b-core --output-on-failure
cmake -S . -B build/phase5b-desktop -G 'MinGW Makefiles' `
  -DCMAKE_BUILD_TYPE=Release `
  -DDATASIM_BUILD_DESKTOP=ON `
  -DDATASIM_NGSPICE=C:/msys64/ucrt64/bin/ngspice_con.exe
cmake --build build/phase5b-desktop -j 4
ctest --test-dir build/phase5b-desktop --output-on-failure
```

| Check | Result | Scope |
| --- | --- | --- |
| Qt-off clean Release build | PASS | Existing five libraries and executables built; Qt not discovered. |
| Qt-off CTest | **9 passed, 0 failed, 0 skipped** | Includes PDF extraction, corpus evaluation, backend errors, and real `ngspice_rc`. |
| Qt-enabled Release build | PASS | `datasim_viewer` and app-specific tests compiled with Qt 6.11.1. |
| Qt-enabled CTest | **12 passed, 0 failed, 0 skipped** | Same nine core suites plus CSV loading, workbench, and worker-event-loop tests; real `ngspice_rc` passed. |
| Workbench automated test | PASS | Default/custom config parsing, malformed/locale-ambiguous/non-finite rejection, `check_config()` constraints, existing-parent enforcement, distinct uncreated run paths, two real ngspice runs, returned waveform vs `results.csv` sample count, artifact presence, validation metric field presence, and existing-directory sentinel preservation. |
| Worker event-loop test | PASS | Real run through `SimulationWorker` on `QThread`; queued callback confirmed on the main event thread after core validation passed. |
| GUI launch / interaction | **NOT VERIFIED** | Awaiting permission to launch the built executable through UI automation. Compilation and service tests do not prove visual correctness, GUI responsiveness, or close-during-run behavior. |
| Six-run RC convergence/repeatability study | PASS | Four timestep refinements had max errors `6.31106e-05`, `1.78925e-05`, `6.00291e-06`, `2.66873e-06 V`; runs 0, 4, and 5 at `1e-5 s` matched at `6.31106e-05 V`. |
| Hosted Phase 5B CI | Pending | Workflow updated to build/test both Qt-off core and optional Qt-on target. |

## Limits

The automated workbench test exercises the real core execution service twice;
it does not synthesize GUI clicks or prove event-loop behavior. Failure classes
including nonzero backend exit and malformed backend output remain covered by
the existing `backend_errors` suite. Thread close behavior, visible failure
presentation, and interactive parameter entry require the separately recorded
GUI check. All runs are simulations, not physical measurements.

Qt remains external and optional. No Qt binaries are bundled. Repository licensing
and redistribution decisions remain unresolved. See [ADR-0012](decisions/ADR-0012-interactive-rc-workbench.md).
