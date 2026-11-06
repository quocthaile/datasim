# Phase 5C validation

**Status: local PDF-enabled Windows build and CTest VERIFIED; partial manual
GUI review VERIFIED; full interactive extraction/worker GUI workflow and hosted
CI NOT VERIFIED.** This status is limited to this checkout and the synthetic
fixture scope below.

## Baseline

- Fetched `origin/main` at `5756e53446b7e3b4296a13493ab23b26802a7caa` (Phase 5B
  PR #17 merge). Created `feat/phase-5c-datasheet-evidence-workbench` from it
  in an isolated worktree at `D:\projects\datasim-phase5c`.
- Local Qt headers and CMake package: Qt 6.11.1; installed `Qt6::Pdf` and
  `Qt6::PdfWidgets` targets were confirmed by successful configure and link.
- Local compiler: MSYS2 UCRT64 GCC 16.1.0; CMake 4.3.2; MinGW Makefiles;
  Poppler and ngspice supplied by MSYS2 UCRT64 PATH.

## Executed checks

```powershell
cmake -S . -B build/phase5c-pdf -G 'MinGW Makefiles' `
  -DCMAKE_BUILD_TYPE=Release `
  -DDATASIM_BUILD_DESKTOP=ON `
  -DDATASIM_BUILD_PDF_WORKBENCH=ON `
  -DDATASIM_NGSPICE=C:/msys64/ucrt64/bin/ngspice_con.exe
cmake --build build/phase5c-pdf -j 4
ctest --test-dir build/phase5c-pdf --output-on-failure
```

Result: PDF-enabled application and test targets compiled and linked. CTest:
15 passed, 0 failed, 0 skipped. The added `evidence_workbench` test passed 22
checks for Qt resource loading, valid synthetic PDF load, two-page count,
geometry availability, zero-based page navigation, image-only PDF display,
missing/corrupt PDF errors, workbench form construction, candidate-only
synthetic resistor extraction, source text/unit retention, deterministic repeat,
invalid page rejection, missing Poppler executable handling, and queued result
delivery to the GUI thread from the application `ExtractionWorker`. Existing core,
Poppler integration, real ngspice, corpus, and Phase 5B desktop tests also
passed in this configuration.

## Not verified

- Full form entry and candidate extraction through the interactive UI; the
  partial manual check below covered PDF open, render, and page navigation.
- Hosted Windows PDF-enabled CI; Linux core and sanitizer hosted jobs on this
  branch.
- Dedicated automated UI configuration-error and candidate extraction cases,
  repeated in-UI extraction, document-switch race, and worker close lifecycle.
- Mouse-coordinate conversion (the feature is deferred; no approximate mapping
  is used).
- Real manufacturer datasheet accuracy or any physical measurement.

## Partial manual GUI review

Launched the PDF-enabled `datasim_viewer`, confirmed both existing RC and new
evidence tabs are present, opened `tests/fixtures/synthetic_datasheet.pdf` in
read-only mode, observed the two-page count and rendered table, then navigated
from zero-based page 0 to page 1. The page indicator and extraction page-index
field both updated to 1. This does not verify extraction configuration entry,
worker results in the UI, invalid configuration messaging, or close-during-run.

The local passing synthetic and existing tests do not imply hosted verification
or real-datasheet accuracy. The detailed implementation limits are recorded in
[implementation](PHASE_5C_IMPLEMENTATION.md).
