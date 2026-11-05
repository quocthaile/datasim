# Phase 4C validation

Status: **VERIFIED** on synthetic fixtures by local checks and hosted Windows CI,
merged into `main` via PR #7 (`1704f726d901f3e1d00f9cdc918e80d352b443ac`).
Hosted run [42000652152](https://github.com/quocthaile/datasim/actions/runs/42000652152)
completed successfully on the Phase 4C PR head (`8291401`), and post-merge
hosted run [42017272043](https://github.com/quocthaile/datasim/actions/runs/42017272043)
completed successfully on `main` (`1704f726d901f3e1d00f9cdc918e80d352b443ac`).
Their clean build, unit
and process tests, required real ngspice integration, and convergence/repeatability
steps all passed. In the initial Phase 4C local run below, the local `ngspice_rc`
target was skipped because ngspice was not configured locally; that local skip is
not a pass.

## Local environment and commands

Windows x64, MSYS2 UCRT GCC 16.1.0, CMake 4.3.2, MinGW Makefiles, Poppler
26.04.0-3. A new build directory was used for the Release build.

```text
python tests/fixtures/make_synthetic_datasheet_pdf.py
cmake -S . -B build/phase4c-clean -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build/phase4c-clean -j 4
ctest --test-dir build/phase4c-clean --output-on-failure
```

Outcome: build succeeded; CTest ran 8 targets, 7 passed, 0 failed, and
`ngspice_rc` was skipped because no local ngspice executable was configured.
The `pdf_extraction` integration target passed with 38 always-active checks. It
exercised Poppler against the synthetic PDFs rather than skipping the extraction
path.

The PDF suite covers actual two-page count and zero-based selection; distinct
printed labels; separate min/typ/max candidates; missing cells; original units;
strict and ambiguous numeric text; conditions at different temperatures;
footnote markers; multiple source references; duplicate/conflicting values;
repeat extraction equivalence for observations, order, provenance and
diagnostics; incorrect MPN; out-of-range page; unsupported region; image-only
PDF rejection; blank units remaining unknown; and candidate-only review state
with no knowledge/model records.
The Phase 4B `intake`, `intake_handoff`, `unit`, `component_model`,
`vertical_slice`, and `backend_errors` suites also passed unchanged.

## Hosted verification

The merged Phase 4B main baseline's hosted run
[41995458137](https://github.com/quocthaile/datasim/actions/runs/41995458137)
passed all steps, including real ngspice integration and convergence. The Phase
4C hosted run [42000652152](https://github.com/quocthaile/datasim/actions/runs/42000652152)
(on PR #7 head `8291401`) and the post-merge `main` run
[42017272043](https://github.com/quocthaile/datasim/actions/runs/42017272043)
(on `1704f726d901f3e1d00f9cdc918e80d352b443ac`)
also installed and verified Poppler `26.10.0`, regenerated and exercised the
fictional PDF fixtures through the integration CTest label, and passed the
required ngspice-47 integration and convergence/repeatability steps. This
verifies the implementation on that hosted Windows environment within its
bounded synthetic-fixture scope; it does not
establish real-datasheet extraction accuracy or physical measurement.

No vendor PDF, vendor table, or vendor model file is used. The generated fixture
contains only fictional identities and values. No real-world extraction
accuracy is claimed without a labeled, legally usable evaluation corpus.
