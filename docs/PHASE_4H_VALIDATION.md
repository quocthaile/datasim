# Phase 4H validation

Status: IMPLEMENTED and locally VERIFIED within the explicitly selected scope.
Hosted Phase 4H CI: pending PR execution. No Phase 4I or 4J work has started.
Branch: `feat/phase-4h-hierarchical-table-context`.
Date: 2026-11-05.

## Baseline and environment

`git fetch origin --prune` and `git pull --ff-only origin main` confirmed
`357dc52aa809` (Phase 4G, PR #11). The working tree was clean. The GitHub connector
reported every substantive step successful in post-merge
[run 42052973122](https://github.com/quocthaile/datasim/actions/runs/42052973122),
including required real integration and convergence/repeatability.

Local Windows x64 build: MSYS2 UCRT GCC 16.2.0 Rev6, MinGW Makefiles, Release;
Poppler 26.10.0; ngspice-47; CTest used MSYS2 Python 3.14.7. The new
`build/phase4h` directory was configured from scratch; subsequent scoped edits were
rebuilt before final CTest. This is local execution, distinct from hosted CI.

| Executable under `C:/msys64/ucrt64/bin/` | SHA-256 |
| --- | --- |
| `c++.exe` | `5f30e02408ab97050fcbfa39bb391b0923cf9402063920b849ab8491e63334b3` |
| `pdfinfo.exe` | `93d7582a332354a7bd5a7d38f682fb238b37e70f0493ce751fe60af6d5f4598a` |
| `pdftotext.exe` | `1137cb55f6b0a2ac1ed73920952ef08ca74563280ccf3ee6db4a2f0dd5453cfe` |
| `ngspice_con.exe` | `dc31be5d2f1095e4777b7ec6c462dfdc66737da9688ae211fe5020c80f5fcb67` |

## Executed regression gates

Commands from the repository root (all final commands exited 0):

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
cmake -S . -B build/phase4h -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release -DDATASIM_NGSPICE=C:/msys64/ucrt64/bin/ngspice_con.exe
cmake --build build/phase4h -j 4
ctest --test-dir build/phase4h --output-on-failure --output-junit phase4h.xml
python -m unittest discover -s tests/ci -v
build/phase4h/rc_study.exe C:/msys64/ucrt64/bin/ngspice_con.exe build/phase4h-study
git diff --check
```

- CTest: **9/9 passed, zero skips**, including real Poppler, real ngspice,
  PDF-to-simulation integration, backend failure/timeout tests, all domain/intake
  suites, and corpus evaluation. Final log: `build/phase4h/Testing/Temporary/LastTest.log`.
- PDF extraction: **182 always-active Release checks**, versus the baseline 113.
- Corpus evaluator: **8 unittest methods passed**, including all earlier fixtures
  and the six-observation hierarchy/merged-cell fixture with zero FP/FN.
- CI runtime configuration: **5/5 passed**.
- RC study: six runs passed. Refinement maximum errors were `6.31106e-05`,
  `1.78925e-05`, `6.00291e-06`, and `2.66873e-06` V; repeated coarse runs retained
  `6.31106e-05` V. The existing 1% step-amplitude tolerance is unchanged.
- All four pre-existing PDF fixtures were compared byte-for-byte with Git HEAD
  and are unchanged. No source changes in simulation, component, vertical, or intake.

## Positive and negative coverage

The new [fictional fixture](../tests/fixtures/make_hierarchy_pdf.py) and
[C++ tests](../tests/unit/pdf_extraction_tests.cpp) verify two adjacent parent
groups, two children under one heading, no numeric observation from the heading,
unselected siblings, unchanged complete neighboring labels, exact composition,
bounded two-row merged parameter cells, deterministic bundles/diagnostics/sidecars,
candidate-only output, and separately resolvable parent/child evidence.

Atomic rejection checks cover missing/altered parent text, clipped words, invalid
or NaN geometry, nonexistent children, parent-only rows selected as children,
child-text mismatches, duplicate/conflicting assignments, crossing a neighboring
heading, overlapping parent/merged boxes, overlapping source text, competing
parameter labels in a merged row, partial row coverage, unassigned merged value
rows, and a full side-by-side multi-MPN row. CLI malformed indices/coordinates and
corrupted parent/child provenance are rejected by the evaluation tests.

Default extraction is exercised on both the new fixture and all old fixtures;
original value provenance and physical row IDs are unchanged.

## Local corpus reruns

The five original vendor PDF files and both existing annotation sets were available
under ignored `build/corpus/`. The evaluator verified all five PDF SHA-256 digests.
Annotations, eligibility, selected table geometry, and denominators were not
changed. The only enabled configuration addition is the exact, locally verified
parent box/text and explicitly selected child rows in the MRS16 sub-band.

The baseline executable retained in `build/phase4g` reproduced the historical
metrics; the freshly built Phase 4H executable with bindings disabled reproduced
them again. The baseline executable was reused, not claimed to be a new clean
Phase 4G build. Both enabled comparisons use the same annotation bytes as their
corresponding baseline. Exact observed hashes and per-case results are in the
public, text-free [aggregate snapshot](../evaluation/phase4h_results.json).

| Subset and configuration | TP | FP | FN | Precision | Recall | F1 | Parameter labels | Evidence agreement |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| Same 15, Phase 4G baseline / 4H disabled | 14 | 1 | 1 | 0.9333 | 0.9333 | 0.9333 | 14/15 | 15/15 |
| Same 15, Phase 4H enabled | 15 | 0 | 0 | 1.0000 | 1.0000 | 1.0000 | 15/15 | 15/15 |
| P2-expanded 22, Phase 4G baseline / 4H disabled | 20 | 2 | 2 | 0.9091 | 0.9091 | 0.9091 | 20/22 | 22/22 |
| Same expanded 22, Phase 4H enabled | 22 | 0 | 0 | 1.0000 | 1.0000 | 1.0000 | 22/22 | 22/22 |

All six reruns: **4/4 unsupported layouts fail closed, zero false accepts, 8/8
repeat checks identical, candidate-only boundary verified**. Enabled evidence
agreement additionally checks configured parent/child sidecars and references.
This does not independently authenticate the pre-existing single-annotator review
metadata. No new human annotation or engineering approval is claimed.

Each pair was executed with `scripts/eval/evaluate_pdf_corpus.py`, passing
`--eval-dump-bin build/phase4h/datasheet_eval_dump.exe` (or `build/phase4g/...` for
the baseline), `--manifest`, `--annotations`, `--output-json`, and `--output-md`.
Local artifacts are retained in `build/phase4h-evidence/`:

| Comparison | Baseline manifest | Enabled manifest | Unmodified annotations |
| --- | --- | --- | --- |
| 15 | `evaluation/real_corpus_manifest_phase4g.json` | `build/phase4h-evidence/manifest15.json` | `build/corpus/real_ground_truth_phase4g.json` |
| 22 | `build/phase4g/real_corpus_manifest_phase4g_p2_expanded.json` | `build/phase4h-evidence/manifest22.json` | `build/corpus/real_ground_truth_phase4g_p2_expanded.json` |

Report prefixes are `baseline15`, `disabled15`, `after15`, `baseline22`,
`disabled22`, and `after22`. Historical reports and documents were not overwritten.
Some historical prose hashes differ from actual retained files; this record uses
observed file hashes, also matching the retained Phase 4G JSON report inputs.
No annotation adjudication was performed to improve these results.

## Scope and review gate

Real-corpus results are local only and cannot be independently rerun in public CI
without the controlled vendor PDFs and annotation files. Public CI uses fictional
fixtures; the workflow does not upload vendor files. No vendor text, PDFs, raw
annotations, or local enabled manifests are added to Git. The public snapshot
contains counts, identifiers, environment metadata, and hashes only.

General merged-cell interpretation, arbitrary multi-MPN/multi-axis tables, OCR,
baseline-aware subscript/superscript reconstruction, and numerical interpretation
of scientific typography remain unsupported. Phase 4I must supply its own ADR,
bounded reconstruction tests, and comparable local evaluation after Phase 4H is
merged and post-merge CI passes. No Phase 4I or Phase 4J branch is created here.

Poppler remains an external GPL-2.0-or-later executable, not bundled. No project
license or distribution approval is adopted. Stop at the Phase 4H PR review gate;
do not automatically merge.
