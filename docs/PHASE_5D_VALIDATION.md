# Phase 5D validation

**Status: local Windows UCRT PDF-enabled build and CTest VERIFIED; hosted CI and
manual GUI lifecycle NOT VERIFIED.** This status applies to the current local
feature-branch source and synthetic fixtures only.

## Local Windows configuration

- GCC 16.1.0 (MSYS2 UCRT64), CMake 4.3.2, Qt 6.11.1, Poppler from MSYS2 UCRT64,
  ngspice-47.
- Qt Svg package was installed locally after the first SVG-render test showed
  that this runtime did not load SVG through `QIcon` without an SVG backend.
- PDF-enabled Release configure and build succeeded:

```powershell
cmake -S . -B build/phase5d-review -G 'MinGW Makefiles' `
  -DCMAKE_BUILD_TYPE=Release -DDATASIM_BUILD_DESKTOP=ON `
  -DDATASIM_BUILD_PDF_WORKBENCH=ON `
  -DDATASIM_NGSPICE=C:/msys64/ucrt64/bin/ngspice_con.exe
cmake --build build/phase5d-review -j 4
ctest --test-dir build/phase5d-review --output-on-failure
```

CTest result: 15 passed, 0 failed, 0 skipped. The `evidence_workbench` test
covered candidate-only synthetic extraction, original manifest retention,
stable ID mapping, successful candidate-to-needs_review transition, failed
decision exception safety, source/config invalidation, no mutation of extracted
observations, all six resource paths, representative SVG rendering through Qt
Svg, invalid resource lookup, and GUI-thread worker delivery. Existing
`ngspice_rc`, Poppler integration, and PDF corpus tests also passed locally.

The Phase 5B regression configuration with desktop enabled and PDF workbench
disabled also built successfully and passed CTest: 14 passed, 0 failed, 0
skipped. The GUI interaction attempt could not start: the Computer Use native
pipe was unavailable in this environment, so the manual GUI lifecycle remains
NOT VERIFIED.

## Not verified

- Clean core-only Linux and Linux sanitizer configurations on this feature
  branch.
- Hosted Windows UCRT PDF workbench, Ubuntu Linux, and Linux ASan/UBSan jobs.
- Interactive GUI lifecycle: complete form extraction, approval/rejection and
  supersession dialogs, edited-session discard, source replacement during a
  running worker, and close during extraction. Widget code is implemented, but
  these interactions were not manually exercised.
- Real manufacturer datasheet accuracy, authenticated human identity, durable
  audit, persistence, or physical measurements.

No numerical solver or tolerance code changed. The local real ngspice test
passed; hosted numerical non-regression remains pending branch CI.
