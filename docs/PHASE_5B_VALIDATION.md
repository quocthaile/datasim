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
| Linux core / POSIX process tests | VERIFIED | Ubuntu 24.04 hosted run [42170539048](https://github.com/quocthaile/datasim/actions/runs/42170539048): 11/11 passed, 0 failed, 0 skipped; includes Poppler, ngspice, timeout, process group, quotas, parser and corpus tests. |
| ASan/UBSan | VERIFIED | Same run: 11/11 passed, 0 failed, 0 skipped; ASan leak detection and UBSan halt-on-error were enabled. |
| Hosted hardening-branch CI | VERIFIED | Same run: Windows UCRT, Linux core and Linux sanitizer jobs all succeeded on commit `1dfc510a71b44369b24eeb34cb171f863ec69331`. |

The Qt-enabled build was rerun after closing the manually launched viewer, which
had locked the executable during the first link attempt. The successful rerun
and all 14 desktop tests completed without skips.

## PR #17 Linux CI correction

Hosted run [42165152332](https://github.com/quocthaile/datasim/actions/runs/42165152332)
was the initial integrated baseline: `windows-ucrt` succeeded, while
`linux-core` and `linux-sanitizers` each failed 2 of 11 tests. Both failures
were reproducible portability/contract defects:

- `pdf_extraction` could not find `pdfinfo` because its test helper used a
  Windows-only `;` PATH separator and unconditional `.exe` suffixes.
- `pdf_process` reported “missing executable was not reported” because the
  POSIX child returned 127 after `execv()` failed while Windows threw from
  `CreateProcessW()`.

The fix uses platform-specific PATH parsing and executable names, prevalidates
regular executable files in `run_capture()`, and throws consistently for a
missing path (plus non-executable POSIX files). Launched process exit codes,
timeouts, process-tree cleanup and output quotas retain their prior semantics.
Additional regression coverage checks both stdout and stderr over-quota output.

Corrective local Windows UCRT64 validation on the PR update:

| Check | Result |
|---|---|
| Core Release build / CTest | VERIFIED; 11/11 passed, 0 failed, 0 skipped. |
| Qt-enabled desktop build / CTest | VERIFIED; 14/14 passed, 0 failed, 0 skipped. |
| `pdf_extraction` | VERIFIED; real Poppler synthetic extraction passed using Windows PATH discovery. |
| `pdf_process` | VERIFIED; exact quota, excess stdout/stderr, missing executable, nonzero exit, timeout, and concurrent isolation passed. |
| Real ngspice and six-run refinement/repeatability | VERIFIED; included in both core/desktop CTest and prior branch study. |
| Linux core / ASan/UBSan | VERIFIED in hosted run [42170539048](https://github.com/quocthaile/datasim/actions/runs/42170539048); each CTest suite 11/11 passed, 0 failed, 0 skipped. |

Hosted correction run [42170539048](https://github.com/quocthaile/datasim/actions/runs/42170539048)
completed successfully on `1dfc510a71b44369b24eeb34cb171f863ec69331`:

| Hosted job | Result |
|---|---|
| `windows-ucrt` | VERIFIED; unit/process CTest 8/8 passed, real integration 3/3 passed, Qt-enabled CTest 14/14 passed. `pdf_extraction` and `pdf_process` passed. |
| `linux-core` | VERIFIED; 11/11 passed, 0 failed, 0 skipped. Real Poppler and ngspice integration passed. |
| `linux-sanitizers` | VERIFIED; 11/11 passed, 0 failed, 0 skipped with ASan and UBSan enabled. No sanitizer report occurred. |

Both Ubuntu jobs passed the workflow's JUnit `notrun` rejection guard. On Linux,
`pdf_process` verified exact-limit output, excessive stdout and stderr limits,
missing and non-executable path reporting, nonzero exit preservation, timeout,
descendant cleanup, and concurrent process isolation. `pdf_extraction` found
unsuffixed Poppler executables using colon-separated PATH and passed real
synthetic extraction.
