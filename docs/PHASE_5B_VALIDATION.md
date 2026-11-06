# Phase 5B validation

**Historical status:** The original Phase 5B implementation passed local checks and hosted Windows run [42158153332](https://github.com/quocthaile/datasim/actions/runs/42158153332). The historical GUI access limitation below remains an accurate record of that run. Current hardening-branch evidence is recorded separately below.

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
| GUI launch / interaction | **NOT VERIFIED** | The user approved launching the local executable, but the computer-use tool still denied launching `datasim_viewer.exe`. Compilation and service tests do not prove visual correctness, GUI responsiveness, or close-during-run behavior. |
| Six-run RC convergence/repeatability study | PASS | Four timestep refinements had max errors `6.31106e-05`, `1.78925e-05`, `6.00291e-06`, `2.66873e-06 V`; runs 0, 4, and 5 at `1e-5 s` matched at `6.31106e-05 V`. |
| Hosted Phase 5B CI | **PASS** | Run [42158153332](https://github.com/quocthaile/datasim/actions/runs/42158153332) succeeded on final code commit `2e59968aa33e524b9fb838aa7473e31fcf05d9c2`; includes Qt-off core gates, real Poppler/ngspice integration, six-run study, Qt-on build, and 12 desktop/core CTest suites. |

## Limits

The automated workbench test exercises the real core execution service twice;
it does not synthesize GUI clicks or prove event-loop behavior. Failure classes
including nonzero backend exit and malformed backend output remain covered by
the existing `backend_errors` suite. Thread close behavior, visible failure
presentation, and interactive parameter entry require the separately recorded
GUI check. All runs are simulations, not physical measurements.

Qt remains external and optional. No Qt binaries are bundled. Repository licensing
and redistribution decisions remain unresolved. See [ADR-0012](decisions/ADR-0012-interactive-rc-workbench.md).

## Hardening branch verification (2026-11-06)

Branch `fix/phase-5b-completion-and-hardening` is based on `origin/main`
`483e4c7f4c43d5e78839dc92a4e1be59ae45660a`. The original Phase 5B changes are
preserved as three commits in its ancestry. The detailed dispositions and
reproduction procedures for F-01 through F-07 are in
[PHASE_5B_HARDENING_AUDIT.md](PHASE_5B_HARDENING_AUDIT.md).

| Check | Result | Scope |
|---|---|---|
| Windows core Release build | VERIFIED | MSYS2 UCRT64 GCC 16.1.0, MinGW Makefiles, CMake 4.3.2, Qt disabled, Poppler 26.10.0, ngspice-47. |
| Windows core CTest | VERIFIED: 11/11 passed, 0 failed, 0 skipped | Includes actual Poppler extraction, synthetic corpus, strict coordinate cases, process quota/timeout/concurrency tests, and real ngspice integration. |
| Windows Qt-enabled Release build | VERIFIED | MSYS2 UCRT64 Qt 6.11.1 Widgets. |
| Windows Qt-enabled CTest | VERIFIED: 14/14 passed, 0 failed, 0 skipped | Includes viewer loading, workbench, worker event-loop, PDF process, and coordinate suites. |
| CSV load benchmark | MEASURED | One million CSV samples loaded in 7,572 ms on this Windows environment; this is an observed local timing, not a cross-machine performance guarantee. |
| Real default workbench run | VERIFIED | 640 samples; max absolute error `6.311056021046646e-05 V`, RMSE `1.1339184082191836e-05 V`, configured tolerance `0.05 V`; simulation only, not a physical measurement. |
| GUI interaction | VERIFIED, bounded | Launched, selected ngspice, ran default RC, inspected metrics/artifacts, toggled Vin, opened generated `results.csv` as read-only. Invalid-input visible state, backend failure presentation, close-during-run, and manual dense repaint remain NOT VERIFIED. |
| Six-run convergence/repeatability | VERIFIED | Current `build/hardening-core/rc_study.exe`: four timestep refinements had max errors `6.31106e-05`, `1.78925e-05`, `6.00291e-06`, `2.66873e-06 V`; runs 0, 4, and 5 at `1e-5 s` repeated at `6.31106e-05 V`. This matches the retained numerical reference. |
| `DATASIM_SOURCE_SHA256` mutation matrix | VERIFIED | `scripts/ci/verify_source_fingerprint.py`: unchanged stable; viewer header mutation and selected synthetic PDF mutation changed the identity; build-directory probe excluded; exact restore returned to baseline `e8d69bb77c84d6dac3f844f379ce0142030868fd63c447d1f804ee0480adc07d`. |
| Linux core / POSIX process tests | NOT VERIFIED locally | WSL environment has no compiler/CMake/ngspice and no noninteractive admin installation. Linux hosted workflow is configured; result is pending branch CI. |
| ASan/UBSan | NOT VERIFIED | Hosted sanitizer workflow is configured; no run result yet. |
| Hosted hardening-branch CI | NOT VERIFIED | Awaiting push and hosted workflow completion. |

The Qt-enabled build was rerun after closing the manually launched viewer, which
had locked the executable during the first link attempt. The successful rerun
and all 14 desktop tests completed without skips.
