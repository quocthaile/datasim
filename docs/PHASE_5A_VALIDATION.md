# Phase 5A validation

**Status:** Local Windows x64 validation complete. Hosted feature-branch workflow
status is tracked on the review PR and is separate from these local results.

## Baseline

- `git fetch origin` completed before implementation.
- Clean starting worktree; `origin/main` and the local starting commit both resolved
  to `3fb583a0eb81640de421bc9049db9c0b80102e50` (Phase 4J merge PR #14).
- Hosted main run [42150487373](https://github.com/quocthaile/datasim/actions/runs/42150487373)
  reports **Success** on the Phase 4J merge commit.
- GitHub's workflow-run connector returned no records for this push-triggered run;
  the run's GitHub Actions page was separately opened and showed `Status Success`.

## Environment

| Item | Observed value |
| --- | --- |
| OS / architecture | Microsoft Windows NT 10.0.26300.0, AMD64 / x64 |
| C++ compiler | MSYS2 UCRT64 GCC/G++ 16.1.0 Rev5 (`C:/msys64/ucrt64/bin/c++.exe`) |
| CMake / generator | CMake 4.3.2, `MinGW Makefiles`, Release |
| Qt | Qt 6.11.1-1 MSYS2 UCRT64 `qt6-base`; `qt6-pdf` 6.11.1-1 is installed but not linked or used |
| Simulation backend | `C:/msys64/ucrt64/bin/ngspice_con.exe`, ngspice-47, KLU Direct Linear Solver |
| Python | 3.13.12 |

No Qt, PDF, or other binary dependency was added to the repository. `DATASIM_BUILD_DESKTOP`
is `OFF` in the core build cache and `ON` in the separate desktop build cache. The
core build cache contains no `Qt6_DIR`; the default build did not discover Qt.

## Build and test commands

The existing PDF synthetic fixtures were regenerated from repository scripts before
the clean builds; `git status` confirms those tracked fixtures are unchanged.

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;C:\msys64\usr\bin;' + $env:PATH
python -B tests/fixtures/make_synthetic_datasheet_pdf.py
python -B tests/fixtures/make_hierarchy_pdf.py
python -B tests/fixtures/make_baseline_pdf.py
cmake --fresh -S . -B build/phase5a-core -G 'MinGW Makefiles' `
  -DCMAKE_BUILD_TYPE=Release `
  -DDATASIM_NGSPICE=C:/msys64/ucrt64/bin/ngspice_con.exe
cmake --build build/phase5a-core -j 4
cmake --fresh -S . -B build/phase5a-desktop -G 'MinGW Makefiles' `
  -DCMAKE_BUILD_TYPE=Release `
  -DDATASIM_BUILD_DESKTOP=ON `
  -DDATASIM_NGSPICE=C:/msys64/ucrt64/bin/ngspice_con.exe
cmake --build build/phase5a-desktop -j 4
New-Item -ItemType Directory -Force -Path `
  build/phase5a-core/test-temp,build/phase5a-desktop/test-temp | Out-Null
$env:TEMP = (Resolve-Path build/phase5a-core/test-temp).Path
$env:TMP = $env:TEMP
ctest --test-dir build/phase5a-core --output-on-failure
$env:TEMP = (Resolve-Path build/phase5a-desktop/test-temp).Path
$env:TMP = $env:TEMP
ctest --test-dir build/phase5a-desktop --output-on-failure
build/phase5a-core/rc_study.exe `
  C:/msys64/ucrt64/bin/ngspice_con.exe build/phase5a-core/phase5a-study
```

For the final CTest runs, process `TMP` and `TEMP` were directed into the respective
build trees; test-specific PDF temporary directories were already isolated under
their build tree. Build, simulation, test fixtures, and test-generated temporary
files were confined to `build/` or the existing tracked synthetic fixture paths.
No fixture bytes or other source-tree inputs changed.

## Results

| Check | Result | Evidence |
| --- | --- | --- |
| Default Qt-off clean Release configure/build | **PASS** | `build/phase5a-core`; `DATASIM_BUILD_DESKTOP=OFF`; no `Qt6_DIR` in cache. |
| Core CTest suites | **9 passed, 0 failed, 0 skipped** | Unit, component model, vertical slice, intake, intake handoff, PDF extraction, backend errors, real `ngspice_rc`, and PDF corpus evaluation. |
| Qt-enabled clean Release configure/build | **PASS** | `build/phase5a-desktop`; `datasim_viewer.exe` linked against Qt 6.11.1 Widgets and `datasim`. |
| Desktop CTest suites | **10 passed, 0 failed, 0 skipped** | The same nine core suites plus `viewer_sample_loading`; the real `ngspice_rc` path passed. |
| Loader behavior | **PASS** | Automated test accepted a valid 2-row CSV; rejected malformed/non-finite `nan` and a missing file with explicit error strings; valid source bytes were identical before/after loading. |
| Fresh ngspice reference run | **PASS** | `rc_demo.exe` wrote 640 samples to `build/phase5a-desktop/viewer-demo/results.csv`; run validation passed with max absolute error `6.31105602105e-05 V` against the existing ideal-RC analytical reference. This is simulated output and a software validation result, not a physical measurement. |
| RC convergence/repeatability | **PASS** | `rc_study.exe`: step sizes `1e-5`, `5e-6`, `2.5e-6`, `1.25e-6` seconds had max errors `6.31106e-05`, `1.78925e-05`, `6.00291e-06`, `2.66873e-06 V`; two repeated `1e-5` runs both reported `6.31106e-05 V`. |
| GUI launch | **VERIFIED** | Windows window title `Datasim Viewer — Read-only Results` appeared and remained interactive. |
| Interactive valid display | **VERIFIED** | Opened the fresh 640-sample ngspice run; inspected curve, file/sample/time summary, `Time (s)`, `Voltage (V)`, and `Vout` legend. Toggled `Vin` on the same axes and inspected its legend/trace. |
| Interactive malformed CSV error | **VERIFIED** | Opened the `nan` fixture. Plot cleared, visible error text showed `Invalid results CSV: malformed numerical CSV cell`, and the same diagnostic appeared in the status bar. |
| Source artifact mutation | **PASS** | Automated loader test compared bytes before and after read. GUI reported read-only loading; no save/write path is present. |
| Unintended source/output writes | **PASS** | CTest temp roots and all generated runs were under `build/`; regenerated tracked synthetic PDF fixture files remained unchanged; no new persistent output outside selected build/output paths. |
| `git diff --check` | **PASS** | No whitespace errors. |

The local Qt-enabled build verifies that the app compiles in this specific UCRT64
environment. The GUI checks are actual launch and interactive visual inspection,
not automated screenshot assertions. They do not establish behavior on other Qt,
Windows, DPI, accessibility, or graphics-driver configurations.

## Dependency, licensing, and limits

The optional path was built with MSYS2 packages `mingw-w64-ucrt-x86_64-qt6-base`
and `mingw-w64-ucrt-x86_64-qt6-pdf` version 6.11.1-1. The viewer links Widgets
only; Qt PDF was not needed for the waveform prototype. Qt remains an external
runtime/package requirement when `DATASIM_BUILD_DESKTOP=ON`. The repository has
no adopted license, and this validation does not resolve Qt LGPL obligations or
any project binary-redistribution policy. The owner must decide those points
before shipping bundled binaries.

The plot displays existing simulation samples. It neither establishes physical
accuracy nor changes the numerical solver. A clean hosted feature-branch run is
still required for repository CI acceptance; PR status is reported separately
from the local results above.
