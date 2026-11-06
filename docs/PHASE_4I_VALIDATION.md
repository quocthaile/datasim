# Phase 4I validation

Status: IMPLEMENTED, MERGED, and VERIFIED locally and in hosted Windows CI for
explicitly selected word associations. PR #13 merged into `main` at
`fb35ae1fc5a58dcd6b9f89d7a6807c3198ab99a5`; post-merge Windows CI
[run 42065262568](https://github.com/quocthaile/datasim/actions/runs/42065262568)
completed successfully. The local Phase 4I evidence below remains the original
Phase 4I run and was not rewritten as a Phase 4J measurement. See
[Phase 4 final validation](PHASE_4_FINAL_VALIDATION.md) for the separate closure
run.

Hosted [push run 42063445953](https://github.com/quocthaile/datasim/actions/runs/42063445953)
passed for implementation commit `6564f34b8751cc1fe8ff1f3942d577603038a8bb`.
The connected job endpoint confirmed successful clean build, unit tests, required
real backend integration, and convergence/repeatability. This is branch CI, not
post-merge main verification. The following documentation-only commit records
that evidence without changing tested source or fixtures.
Branch: `feat/phase-4i-baseline-aware-text`. Date: 2026-11-05.

## Verified prerequisite

Phase 4H is merged through PR #12 at
`864eb56c1712470180cc1ecb9d9dbef3b5fd05ff`. Public GitHub Actions metadata and the
connected job endpoint confirmed post-merge
[run 42058944651](https://github.com/quocthaile/datasim/actions/runs/42058944651)
completed successfully, including required integration and convergence/repeatability.
The starting tree was clean; the Phase 4I branch was created from refreshed main.

## Local execution

Windows x64, MSYS2 UCRT GCC 16.2.0 Rev6, Release, MinGW Makefiles, Poppler 26.10.0,
ngspice-47, MSYS2 Python 3.14.7. A new `build/phase4i` directory was configured
from scratch and scoped follow-up changes rebuilt before final CTest.
Executable SHA-256 hashes match the table in
[Phase 4H validation](PHASE_4H_VALIDATION.md#baseline-and-environment), rechecked
during this run. Local execution is separate from hosted execution.

Final commands exited 0:

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
python -B tests/fixtures/make_baseline_pdf.py
cmake -S . -B build/phase4i -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release -DDATASIM_NGSPICE=C:/msys64/ucrt64/bin/ngspice_con.exe
cmake --build build/phase4i -j 4
ctest --test-dir build/phase4i --output-on-failure --output-junit phase4i.xml
python -B -m unittest discover -s tests/ci -v
build/phase4i/rc_study.exe C:/msys64/ucrt64/bin/ngspice_con.exe build/phase4i-study
git diff --check
```

- **9/9 CTest suites passed, zero skips**, including real Poppler, real ngspice,
  PDF-to-simulation integration, all domain/intake gates, and backend fault tests.
- **237 Release-active PDF checks** (Phase 4H baseline: 182).
- **9 evaluator unittest methods passed**, including provenance tampering checks.
- **5/5 CI runtime tests passed**.
- Six-run RC refinement/repeatability passed. Maximum errors at the four timestep
  refinements: `6.31106e-05`, `1.78925e-05`, `6.00291e-06`, `2.66873e-06` V.
  Repeated coarse runs retained `6.31106e-05` V. The 1% amplitude tolerance is unchanged.
- **22 complete legacy dump comparisons** between retained Phase 4H and current
  Phase 4I executables were identical after excluding only the new empty sidecar
  field. These span all Phase 4E/F/G/H synthetic corpus cases, including failures.
- All five pre-existing PDF fixtures were byte-compared against Git HEAD and are
  unchanged. No simulation/component/vertical/intake source changes.

## Synthetic acceptance matrix

The [new fictional fixture](../tests/fixtures/make_baseline_pdf.py),
[C++ tests](../tests/unit/pdf_extraction_tests.cpp), and
[evaluation tests](../tests/evaluation/test_pdf_corpus_evaluator.py) cover:

| Case | Observed result |
| --- | --- |
| Ordinary text and bindings disabled | Existing row/text behavior retained |
| Subscript and superscript | Tokens join host rows with original coordinates retained |
| Two expressions in one row; multiple scripts on one host | Deterministic x ordering, no cross-assignment |
| Scientific-notation value | Source `10 -4` retained, display `10^{-4}` separate, scalar remains null |
| Two geometrically possible hosts on adjacent baselines | Atomic rejection |
| Unrelated adjacent-row/intersecting text | Atomic rejection |
| Configured header token | Rejected; no header-to-data propagation |
| Nearby footnote | Not attached; explicit footnote-marker binding rejected |
| Altered text, clipped/non-finite boxes, wrong role direction | Atomic rejection |
| Duplicate/overlapping bindings and reused source words | Atomic rejection |
| Phase 4H parent plus reconstructed child | Both provenance chains retained |
| Repeated extraction | Bundle, diagnostics, and sidecars identical |
| Missing, duplicated, or corrupted provenance/geometry/roles | Evaluator rejects integrity claim |

The four-observation synthetic evaluator subset is **TP=4, FP=0, FN=0** with
evidence agreement 4/4. Five host/script expressions are checked, including two
on the same row. Exact original word coordinates are checked independently
against the fixture specification. No synthetic fixture is treated as engineering
approval or real-manufacturer knowledge.

## Comparable local vendor reruns

Original local vendor PDFs and pre-existing annotation sets were available. Each
baseline/disabled/enabled comparison retains identical PDF bytes, annotation bytes,
eligibility, table bands, row tolerance, and expected denominators. The only
configuration changes are explicit baseline bindings. The evaluator verifies PDF
hashes; manufacturer metadata and annotator identity are not authenticated.

| Configuration | TP | FP | FN | Precision | Recall | F1 | Label agreement | Evidence agreement |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| Phase 4H, 15 observations | 15 | 0 | 0 | 1.0000 | 1.0000 | 1.0000 | 15/15 | 15/15 |
| Phase 4I disabled, same 15 | 15 | 0 | 0 | 1.0000 | 1.0000 | 1.0000 | 15/15 | 15/15 |
| Phase 4I enabled, same 15 | 15 | 0 | 0 | 1.0000 | 1.0000 | 1.0000 | 15/15 | 15/15 |
| Phase 4H, expanded 22 | 22 | 0 | 0 | 1.0000 | 1.0000 | 1.0000 | 22/22 | 22/22 |
| Phase 4I disabled, same 22 | 22 | 0 | 0 | 1.0000 | 1.0000 | 1.0000 | 22/22 | 22/22 |
| Phase 4I enabled, same 22 | 22 | 0 | 0 | 1.0000 | 1.0000 | 1.0000 | 22/22 | 22/22 |

All six runs: **4/4 unsupported cases rejected, zero false accepts, 8/8 repeat
checks identical, candidate-only verified**. MRS16's three selected expressions
in the expanded band have **3/3 exact-source reconstruction/provenance checks**;
display roles are `P_{70}`, `U_{max.}`, and `U_{ins}`. This is a source-selection
check, not an independent new human annotation exercise. The earlier real corpus
already used 3.5-point row tolerance, so **no F1 improvement is claimed**. Actual
separate-row coalescing is demonstrated by the new synthetic cases at 2 points.

The MKP1840M control was rerun with one locally verified exponent binding while
retaining the full unsupported multi-frequency table selection. It still returns
no candidate bundle. No partial provenance is released from a failed extraction,
and success of one expression is not claimed as support for that matrix.

The [aggregate snapshot](../evaluation/phase4i_results.json) retains counts,
annotation/manifest hashes, rejection and diagnostic counters, and comparison
method. Original annotations were not adjudicated or edited:

- 15-observation annotation SHA-256:
  `20845a11d3b5025d447fcce95b659c241c277c468cb0ac5d5fde7e7cd6d792c3`.
- 22-observation annotation SHA-256:
  `1738c46abbc72d942d5fc06d4ffbedcec923ccf1b0a2c3d66e19351b59cf1a38`.

All reports are retained in ignored `build/phase4i-evidence/`, with prefixes
`baseline15`, `disabled15`, `enabled15`, `baseline22`, `disabled22`, `enabled22`.
Commands use `scripts/eval/evaluate_pdf_corpus.py --manifest ... --annotations ...
--eval-dump-bin ... --output-json ... --output-md ...`. Baselines use
`build/phase4h-evidence/manifest15.json` / `manifest22.json` and retained
`build/phase4h/datasheet_eval_dump.exe`; disabled runs use the same manifests with
the new executable; enabled runs use `build/phase4i-evidence/manifest15.json` /
`manifest22.json`. Annotations remain the Phase 4G files recorded by Phase 4H.
No prior benchmark result is overwritten. These vendor reruns are local only.

## Remaining boundaries

No OCR, automatic host discovery, general merged-grid recognition, arbitrary
Unicode script interpretation, numerical exponent evaluation, persistence, LLM,
GUI, semiconductor models, or custom solver. Raw source strings and roles are
preserved separately; no implicit conversion or approval. No vendor PDF or raw
annotation is committed or uploaded as CI artifacts. Poppler is not bundled, and
the project still has no adopted license.

At the time this validation was recorded, Phase 4J had not started and required
the Phase 4I merge plus successful post-merge CI. That gate is now satisfied;
Phase 4J release closure is documented separately in
[PHASE_4_FINAL_VALIDATION.md](PHASE_4_FINAL_VALIDATION.md).
