# Phase 4 Final Validation and Release Closure

**Milestone:** Phase 4J, final validation only
**Status:** Local validation completed; hosted branch CI passed; owner review pending.
**Branch:** `chore/phase-4j-final-validation`
**Source baseline tested:** `fb35ae1fc5a58dcd6b9f89d7a6807c3198ab99a5` (`main`)
**Main hosted status:** successful Windows run [42065262568](https://github.com/quocthaile/datasim/actions/runs/42065262568) for that exact commit.
**Scope:** integrated verification, documentation, and one isolated temporary-workspace safety fix with a regression test.

This report distinguishes the Phase 4J local execution from earlier phase evidence and records the completed hosted workflow for the Phase 4J pull request. Phase 4J is **VERIFIED** for the documented validation scope: its own hosted Windows workflow run `42148338325` completed successfully on branch commit `c631abc5f578a269b8748b39c631b7aa06b87768`. Owner review and merge remain separate.

## 1. Baseline and working tree

`git fetch origin --prune` completed. Before edits the checkout was clean, `main` matched `origin/main`, and its exact SHA was `fb35ae1fc5a58dcd6b9f89d7a6807c3198ab99a5`. PR #13 merged Phase 4I at that commit. GitHub Actions run `42065262568` is the latest run shown for `main` in the `RC verification` workflow and reports **Success**. The PR #12 Phase 4H merge is `864eb56c1712470180cc1ecb9d9dbef3b5fd05ff`, verified by post-merge run `42058944651`.

The Phase 4J branch was created from the clean, refreshed `main`. The integrated suite was first run against the Phase 4I source tree; the security audit then found a reproducible temporary-directory collision issue. Phase 4J adds one isolated fix in `datasim_pdf/table_extraction.cpp`, a regression test in `tests/unit/pdf_extraction_tests.cpp`, and CTest environment setup in `CMakeLists.txt` so the regression uses an isolated build-tree temporary root. No fixture, workflow, or historical evidence file is modified. The exact Phase 4J branch commit is recorded in the pull request and final handoff.

## 2. Production inventory and dependency graph

The actual CMake target graph is:

```text
datasim_vertical -> datasim_component + datasim
datasim_intake   -> datasim_component
datasim_pdf      -> datasim_intake
datasim          (independent base target)
datasim_component (independent base target)
```

`datasim_vertical` publicly links `datasim_component` and `datasim`; `datasim_intake` publicly links only `datasim_component`; `datasim_pdf` publicly links only `datasim_intake`. The base targets `datasim` and `datasim_component` are independent. CMake adds executable integration tests and configures the ngspice path through the supported `DATASIM_NGSPICE` cache option. Poppler is launched by `datasim_pdf` as an external executable, not linked into or depended on by the intake/domain layer.

| Production target | Implemented responsibility and public contracts | Dependencies and limits |
| --- | --- | --- |
| `datasim` | C++20 ideal-RC configuration (`RcConfig`), deterministic netlist/run orchestration, external ngspice batch process, sampled results, analytical validation, CSV/JSON output, process errors. | No PDF, component, or intake dependency. Only the bounded ideal-RC transient backend is implemented; no general circuit solver or physical device validation. |
| `datasim_component` | In-memory `ComponentIdentity`, `SourceDocument`, `EvidenceLocation`, `ParameterObservation`, conditions, review decisions, approved `ComponentKnowledge`, conflicts, `ModelArtifactDescriptor`, and `ModelValidationRecord`, with referential/domain validation. | Standard-library C++20 domain contracts; no persistent store, identity provider, cryptographic source authentication, or external model loader. |
| `datasim_vertical` | `CircuitAssembly`, `resolve_rc_parameters`, `evaluate_rc_model_compatibility`, `ExecutionPolicy`/`ExecutionClassification`, RC adapter, and `execute_vertical_rc_run` provenance. | Depends on component domain and `datasim`; requires approved knowledge and validated compatible model metadata before simulation. Metadata is not authentication. |
| `datasim_intake` | `IntakeManifest`, raw numeric interpretation, validated candidate bundle construction and review handoff. | Depends only on `datasim_component`. Output remains candidate-only; no implicit unit conversion, human approval, persistence, or model promotion. |
| `datasim_pdf` | Bounded selectable-text extraction via `pdfinfo` and `pdftotext -bbox-layout`, explicit table/column selection, `ExtractionRequest` / `ElectricalTableLayout`, `ExtractionResult`, candidate manifest, diagnostics, and PDF-local provenance sidecars. | Depends on intake; Poppler is an external executable. No OCR, automatic table discovery, source-authentication, or general PDF interpreter. |

The integrated data flow was verified in the actual synthetic Phase 4D test path:

```text
synthetic selectable-text PDF
 -> real Poppler `pdfinfo` and `pdftotext`
 -> datasim_pdf ExtractionResult and IntakeManifest
 -> datasim_intake candidate ComponentDomainBundle
 -> explicit synthetic engineering review in the test
 -> approved ComponentKnowledge and validated ModelArtifactDescriptor
 -> CircuitAssembly and datasim_vertical
 -> real ngspice process through datasim
 -> simulation samples, validation, and provenance_metadata.json
```

That fixture is deliberately synthetic. Its review step exercises lifecycle contracts; it does not constitute manufacturer validation or human engineering approval. No automatic candidate approval or automatic model promotion is present.

## 3. Phase 4 capability matrix

| Capability | Implementation / public contract | Coverage and actual evidence | Supported input constraints and explicit limits | Status |
| --- | --- | --- | --- | --- |
| 4B validated candidate intake | `src/datasim/intake/`; `IntakeManifest`, candidate bundle builder, strict intake validation. | `intake`, `intake_handoff`; current CTest 9/9; hosted Phase 4B Windows run `41995104973` included non-skipped P-07 ngspice. | Explicit manifest and evidence references; preserves raw values, units, conditions and qualifiers. Candidate-only; no normalization to SI, automatic review, or storage. | VERIFIED |
| 4C explicit single-page table extraction | `src/datasim/pdf/table_extraction.{hpp,cpp}`; explicit page, region and non-overlapping column selections. | `pdf_extraction`; final run passes 239 Release-active PDF assertions with Poppler 26.10.0, including a Phase 4J temporary-workspace collision regression; hosted runs `42000652152` and `42017272043` cover the earlier merged behavior. | Selectable text on an explicitly selected page/region; image-only and unsupported layouts fail closed. No OCR or automatic page/table/column recognition. | VERIFIED |
| 4D synthetic PDF-to-simulation integration | `tests/integration/pdf_simulation_gate.cpp`, intake fixture, integration executable. | Current `ngspice_rc` CTest passes with configured real Poppler and ngspice-47; hosted post-merge run `42025406013`. | Synthetic PDF values and synthetic review/model chain; verifies integration and ideal-RC numerical equivalence, not vendor-data accuracy or physical behavior. | VERIFIED |
| 4E offline corpus and evaluator | `scripts/eval/evaluate_pdf_corpus.py`, versioned corpus protocol, `datasheet_eval_dump`, synthetic evaluator tests and text-free historical summaries. | `pdf_corpus_evaluation` passes in current CTest; hosted run `42037281172`. Historical real-corpus baseline in Phase 4E report was TP=9, FP=6, FN=6 on 15 observations; same historical corpus was later compared under explicitly documented Phase 4F–4I configurations. | Offline fixed manifests, annotations, and selected geometry; hosted CI uses synthetic fixtures. Current machine lacks controlled vendor PDFs and annotation files, so no new Phase 4J vendor rerun is claimed. | VERIFIED |
| 4F explicit header bindings | `HeaderUnitBinding`, `HeaderConditionBinding` and separate header evidence in `datasim_pdf`. | PDF tests and synthetic evaluator regressions pass; Phase 4F historical local same-source comparison reported 14/15 against source-corrected annotation, 4/4 unsupported layouts fail closed, 0 false accepts, 8/8 repeat checks; hosted run `42044651168`. | Caller selects exact header text/box and unit/condition. Header declarations are not inferred or converted; ambiguity/conflict rejects. | VERIFIED |
| 4G inline value/unit parsing | `InlineValueUnitBinding`, allowlisted unit tokens, `ValueCellProvenance`, verbatim `raw_cell_text`. | Current PDF/evaluator regression suites pass; historical local 15-observation result TP=14/15 and 22-observation expanded result TP=20/22, with 4/4 unsupported cases rejected and 8/8 repeat checks; hosted run `42052973122`. | Explicitly bounded value/unit cells and strict allowlist; no broad numeric grammar, unit conversion, or arbitrary cell parsing. | VERIFIED |
| 4H explicit hierarchy | Parent/child source-verified bindings and separate parent/child evidence provenance in `datasim_pdf`. | Current PDF/evaluator regression suites pass; historical comparable pilot subsets TP=15/15 and TP=22/22, 4/4 unsupported layouts fail closed, 0 false accepts, 8/8 repeats; hosted post-merge run `42058944651`. | Exact caller-selected parent/child text and bounded geometry. No arbitrary merged-grid or multi-MPN table interpretation. | VERIFIED |
| 4I baseline-aware script association | `ScriptPosition`, `BaselineScriptToken`, `BaselineTextBinding`, `BaselineTextProvenance`, `ElectricalTableLayout::baseline_text_bindings`, `ExtractionResult::baseline_text_provenance`; ADR-0010. | Current PDF/evaluator regression suites pass; historical comparable local subsets TP=15/15 and TP=22/22, 4/4 unsupported layouts rejected, 0 false accepts, 8/8 repeat checks; hosted post-merge run `42065262568`. | Explicit host/script token binding, source text and exact boxes; only bounded ASCII letters/digits/signs/decimal points. Does not interpret exponent numerically or support multi-frequency matrices. | VERIFIED |

Historical corpus figures above remain historical measurements, not Phase 4J measurements. Phase 4J reran all versioned synthetic evaluation tests, not the vendor corpus.

## 4. Clean Windows Release verification

### Environment

| Item | Observed Phase 4J local environment |
| --- | --- |
| OS / architecture | Microsoft Windows 10.0.26300, AMD64 |
| C++ toolchain | MSYS2 UCRT64 GCC/G++ 16.1.0 (Rev5); CMake selected `C:/msys64/ucrt64/bin/c++.exe` |
| Build generator/configuration | CMake 4.3.2, `MinGW Makefiles`, `Release` |
| PDF tools | Poppler `pdfinfo` and `pdftotext` 26.10.0 |
| Simulation backend | MSYS2 UCRT64 `ngspice_con.exe`, ngspice-47, KLU Direct Linear Solver |
| Python | CMake/CTest discovery: 3.13.12; standalone test command on PATH: 3.14.5 |
| Exact ngspice executable SHA-256 | `dc31be5d2f1095e4777b7ec6c462dfdc66737da9688ae211fe5020c80f5fcb67` |
| Exact C++ compiler SHA-256 | `798a6eb495b30cf771dc759b5508d269ecb0dddac739719e8b30fe0ac0744b69` |
| Exact `pdfinfo.exe` SHA-256 | `9c84cd10990156a19e41c54393e8c1e33e76d607b4d8de9394ffdfa3cd0596a6` |
| Exact `pdftotext.exe` SHA-256 | `1ad43b58a1d7f49929a10c15f7c09bec90a920faf7ec85efb4403ffbd679c00d` |
| Configured Git revision (`datasim/build_info.hpp`) | `fb35ae1-dirty` (Phase 4J source/test changes were present in the validated worktree) |
| Configured source-tree SHA-256 (`datasim/build_info.hpp`) | `af4046515f6ff72e4cbde7e7bb9a1576ad5655a6e19862c7a56a0376cff8e1df` |
| Release `datasheet_eval_dump.exe` SHA-256 | `cc78d3bf6b857da56309a56f8a3b17e75d10c15b01743134681600f8613a32b0` |
| Release `pdf_extraction_tests.exe` SHA-256 | `9a7499d1a8fb0780ff45e906082b8b65dd75c5da9435075311c46d4ed293d64d` |
| Downloaded dependency package SHA-256 | Poppler 26.10.0 package `29971b323692b327aeacbf643180c129af23219b8aaa0b5cbcffc05e95a6334a`; ngspice-47 package `a9f615da61165c79a904232641e5c3e31fcc5b21c01b1018965b95d543faadf4` (both match the pinned workflow package hashes). |

GCC is the currently installed Rev5/16.1.0 package, rather than the historical local reports' Rev6/16.2.0 compiler. The exact local build identity is recorded above; this difference is not hidden as an exact toolchain match. The hosted workflow remains the separately pinned project acceptance path.

### Commands and results

The clean Release tree was newly configured; no prior build directory was reused:

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;C:\msys64\usr\bin;' + $env:PATH
python -B tests/fixtures/make_synthetic_datasheet_pdf.py
python -B tests/fixtures/make_hierarchy_pdf.py
python -B tests/fixtures/make_baseline_pdf.py
cmake --fresh -S . -B build/phase4j-release-final -G 'MinGW Makefiles' `
  -DCMAKE_BUILD_TYPE=Release `
  -DDATASIM_NGSPICE=C:/msys64/ucrt64/bin/ngspice_con.exe
cmake --build build/phase4j-release-final -j 4
ctest --test-dir build/phase4j-release-final --output-on-failure `
  --output-junit ctest-phase4j-final.xml
python -B -m unittest discover -s tests/ci -v
$env:DATASIM_EVAL_DUMP_BIN = (Resolve-Path 'build/phase4j-release-final/datasheet_eval_dump.exe').Path
python -B -m unittest discover -s tests/evaluation -v
build/phase4j-release-final/rc_study.exe `
  C:/msys64/ucrt64/bin/ngspice_con.exe build/phase4j-study-clean
```

The standalone evaluator unittest command requires `DATASIM_EVAL_DUMP_BIN`; CTest configures this automatically. It is set explicitly above for the equivalent standalone run.

| Gate | Result | Exit |
| --- | --- | ---: |
| Fresh CMake Release configure | Success; real ngspice path set | 0 |
| Release build (`-j 4`) | All targets, including `datasheet_eval_dump`, `pdf_extraction_tests`, and `integration_tests`, built | 0 |
| CTest | **9 passed, 0 failed, 0 skipped** (9 total) | 0 |
| `tests/ci` unittest suite | **5 tests passed** | 0 |
| `tests/evaluation` unittest suite | **9 tests passed** | 0 |
| PDF extraction assertion executable | **239 checks passed**, including the new collision regression | 0 |
| ngspice installed runtime initialization | Passed twice; 16 code-model paths checked; idempotent, unchanged `spinit`; real process printed `DATASIM_RUNTIME_READY` | 0 |
| `rc_study` | Six-run refinement/repeatability study passed | 0 |

CTest target inventory: `unit`, `component_model`, `vertical_slice`, `intake`, `intake_handoff`, `pdf_extraction`, `backend_errors`, `ngspice_rc`, and `pdf_corpus_evaluation`. The integration-labeled targets were `pdf_extraction`, `ngspice_rc`, and `pdf_corpus_evaluation`; all passed. `ngspice_rc` exercised the real Poppler extraction and real ngspice transient process through the synthetic PDF-to-simulation gate. No integration was skipped.

Evidence is retained locally under ignored `build/phase4j-release-final/` (`Testing/Temporary/LastTest.log`, `ctest-phase4j-final.xml`, integration run directories), `build/phase4j-evidence/`, and `build/phase4j-study-clean/`. These local artifacts are not committed and contain synthetic fixtures only.

Captured local evidence-file SHA-256: `Testing/Temporary/LastTest.log` `f34685046cd00f2650d41f7c20256d65e0d3551b5fe9fac6be46ffd24ab49f6b`; `ctest-phase4j-final.xml` `496c082e594a3aa6f554642a34e4cff970e0244d38a7c6a5cd7319ea2a9ab725`; `build/phase4j-study-clean/study.json` `c95e0dbe4bc1cfcb8b15e9baeda3ef073e6d6635dca809013edd331d399fecb3`. The study JSON records per-run elapsed times and is therefore an archived observation, not a byte-stable golden file.

## 5. Numerical non-regression and repeatability

The numerical contract remained unchanged: default `RcConfig` (`R=1000 ohm`, `C=1e-6 F`, `V=5 V`), existing netlist generation, 4001-point common validation grid, 0.05 V acceptance bound for the 5 V reference, and the pre-existing 1% step-amplitude constraint. The reference is the ideal first-order RC response; for a voltage step its capacitor voltage is (V(1-e^{-t/(RC)})), with τ = RC. The actual simulation uses the existing 1 ns finite PULSE rise; the code also checks the corresponding finite-rise reference. This is a numerical model check, not physical hardware validation. The standard first-order response is documented in MIT OpenCourseWare, and ngspice transient analysis is documented in the [ngspice 47 manual](https://ngspice.sourceforge.io/docs/ngspice-manual.pdf).

Six runs passed. Maximum errors on the four refinements were `6.311056e-05`, `1.789254e-05`, `6.002912e-06`, and `2.668729e-06` V. Both repeated coarse runs had maximum delta **0 V** versus the initial coarse run (study comparison threshold `1e-12 V`). Each run used the existing 4001-point common grid and 0.05 V tolerance. These values are consistent with the historical Phase 1.5/4I study and retain the prior 1% amplitude check.

| Artifact comparison | Historical Phase 1.5 SHA-256 | Fresh Phase 4J run SHA-256 | Result |
| --- | --- | --- | --- |
| `circuit.cir` | `49eb9569ce51a53f435b33e2a5990385e610ab9f1745c0d7cd3ef50efbe38db7` | same | Byte-identical |
| `results.csv` | `9c923c4d86a96bdf42fe3bbd84a00cc6ae8519f5ac8ea3858b614c30260f26f9` | same | Byte-identical |
| `errors.csv` | `931b1135a687bb22d01a9163a234e120e3ec912db46c4dc2c5d3d766885ab63d` | same | Byte-identical |
| Backend text log | `61c225d0d21c65e2f79b8ae5e6660977ec053c605134cdeb928d218c25523acc` | same | Byte-identical |
| `run_metadata.json`, `validation_report.json` | Historical files retained in `examples/rc_transient/phase15-evidence/run-0/` | Fresh files in `build/phase4j-study-clean/run-0/` | Metadata files are not byte-identical; numerical fields, sample counts, grid, tolerance, status, and checkpoint results agree. |

No historical evidence was overwritten. CSV equality is a stronger artifact comparison for the computed data; the report does not equate matching bytes with physical accuracy.

## 6. Corpus benchmark and fail-closed results

### Historical vendor corpus measurements

The controlled vendor PDFs and annotations expected under ignored `build/corpus/` and `build/phase4i-evidence/` are not present on this machine. No substitute revision was downloaded. The current real-vendor rerun is therefore **NOT VERIFIED**. The committed `evaluation/phase4h_results.json`, `evaluation/phase4i_results.json`, and phase validation reports remain historical records only.

### New Phase 4J synthetic corpus executions

The existing `scripts/eval/evaluate_pdf_corpus.py` runner was rerun against each committed fictional corpus and annotation set with the fresh Release `datasheet_eval_dump.exe`, real Poppler 26.10.0, and repeat checking enabled. Outputs are ignored local artifacts under `build/phase4j-evidence/`; the runner verified each fixture PDF hash. These numbers describe synthetic test fixtures only.

| Synthetic corpus | Cases | TP / FP / FN | F1 | Expected / actual fail-closed | False accepts | Repeat checks / mismatches | Candidate-only |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Phase 4E regression | 7 | 20 / 0 / 0 | 1.0000 | 5 / 5 | 0 | 7 / 0 | verified |
| Phase 4F header binding | 5 | 13 / 0 / 0 | 1.0000 | 2 / 2 | 0 | 5 / 0 | verified |
| Phase 4G inline value/unit | 8 | 21 / 0 / 0 | 1.0000 | 4 / 4 | 0 | 8 / 0 | verified |
| Phase 4H hierarchy | 2 | 6 / 0 / 0 | 1.0000 | 0 / 0 | 0 | 2 / 0 | verified |
| Phase 4I baseline text | 1 | 4 / 0 / 0 | 1.0000 | 0 / 0 | 0 | 1 / 0 | verified |

The exact runner inputs are the tracked manifest and annotation pairs below (SHA-256); output JSON files remain under ignored `build/phase4j-evidence/`.

| Corpus | Manifest / annotations | Manifest SHA-256 | Annotation SHA-256 | Output JSON SHA-256 |
| --- | --- | --- | --- | --- |
| 4E | `synthetic_corpus_manifest.json` / `synthetic_ground_truth.json` | `341db4368cea2284d399caa6f39ef0c1977389af719ac8f3da362de5510d5260` | `c72ec7411e6fff391b24c6f2b27b98d836b0037836caec8fe8aeba8af3101119` | `6bb09aea5849e697bd4a038eb5c2e69b613ea3e528f0e3805897538f8a022bdb` |
| 4F | `synthetic_header_binding_manifest.json` / `synthetic_header_binding_ground_truth.json` | `4e64f1c28543a1b758d1784e979b889e0772a825c2a229ac28ee49633ef643c8` | `5b70028601ee55afb7ba363c084730f58e3ea1c8e26f78220af33ee4a8db6645` | `45b6228b97abe36539660cec25f00b5c81cd5ceaf87a53ba630085374f3f6c89` |
| 4G | `synthetic_inline_unit_manifest.json` / `synthetic_inline_unit_ground_truth.json` | `6ff502fc925e861d04319930994fcf85699af0332aa7d136c7673d23add6e76b` | `674df2bb08987072ede46398603806d2dee2b5532699a1ff44ea7173fe9a7421` | `1822466fa4fef7321f0fab77ac7c0f177aba7de5007f243808e12ba203abc743` |
| 4H | `synthetic_hierarchy_manifest.json` / `synthetic_hierarchy_ground_truth.json` | `c6c71e5f06a424204a047f131e288cdca2132482ca75aacd6f3efeb2b21c24b6` | `6ee0d605f07ca9cb75059574c3a84de15f19ea69f255f3676990014d7f10d760` | `5e1b1266ddfe4b117947906ff9da7fc294f6ff0021ed27560ce392972e75566e` |
| 4I | `synthetic_baseline_manifest.json` / `synthetic_baseline_ground_truth.json` | `421ca6ea03347bf5537a1a663a4d2eed0ecd8447ed519abe200ca948fc8c5dd2` | `aa8a4be8da963f81e4f6d43cbb47295326758604ca6c2d7f8d029df413b92a48` | `a1d09b653b740b4dce9c5099836421467828357ea64c04100fdd03ed7b3b6b6b` |

Reproduction uses the command shape below for each exact manifest/annotation pair and output stem listed in the table (output names `phase4e` through `phase4i`):

```powershell
python -B scripts/eval/evaluate_pdf_corpus.py `
  --manifest tests/evaluation/synthetic_baseline_manifest.json `
  --annotations tests/evaluation/synthetic_baseline_ground_truth.json `
  --repo-root . `
  --eval-dump-bin build/phase4j-release-final/datasheet_eval_dump.exe `
  --pdfinfo C:/msys64/ucrt64/bin/pdfinfo.exe `
  --pdftotext C:/msys64/ucrt64/bin/pdftotext.exe `
  --output-json build/phase4j-evidence/phase4i.json `
  --output-md build/phase4j-evidence/phase4i.md
```

| Comparison | Same-document / same-annotation subset | Historical result | Phase 4J status |
| --- | --- | --- | --- |
| Phase 4E initial pilot baseline | 15 observations | TP=9, FP=6, FN=6; F1=0.6000 | Historical only |
| Phase 4F source-corrected units and header conditions | 15 observations | TP=14, FP=1, FN=1; F1=0.9333 | Historical only |
| Phase 4G inline binding | Same 15; expanded 22 | 15: TP=14/15, F1=0.9333; 22: TP=20, FP=2, FN=2, F1=0.9091 | Historical only |
| Phase 4H hierarchy | 15; P2-expanded 22 | 15: TP=15, FP=0, FN=0; 22: TP=22, FP=0, FN=0 | Historical only |
| Phase 4I baseline-aware association | Comparable 15 and 22 | 15: TP=15, FP=0, FN=0; 22: TP=22, FP=0, FN=0 | Historical only |

### Phase 4J evidence categories

- **A. Same documents, annotations, eligibility, and subset:** NOT VERIFIED in a new vendor rerun because the original controlled files are unavailable here. Historical Phase 4H/4I reports say their local comparisons preserved hashes, labels, denominators, and geometry; those are prior-run claims, not new checks.
- **B. Expanded geometry coverage:** no new real-corpus geometry coverage claimed. Historical Phase 4G/4H/4I expanded 22-observation results are kept separately from the 15-observation subset.
- **C. Unsupported layouts:** current synthetic negative/regression tests pass. Historical vendor negatives report 4/4 fail-closed, no false accepts; no new vendor negative cases were executed in Phase 4J.
- **D. Provenance and repeat determinism:** current synthetic 4E–4I evaluator tests pass, including tampered provenance rejection and repeated synthetic extraction. Historical vendor reports recorded 8/8 repeat checks; no fresh vendor determinism number is claimed.
- **E. Corpus limits:** small controlled sample and single-annotator labels; no inter-annotator agreement, external authenticity verification, broad manufacturer sampling, or representative population claim.

## 7. Phase 4I ADR-0010 contract audit

The current types and configuration in `table_extraction.hpp` match ADR-0010: `ScriptPosition`, `BaselineScriptToken`, `BaselineTextBinding`, `BaselineTextProvenance`, `ElectricalTableLayout::baseline_text_bindings`, and `ExtractionResult::baseline_text_provenance`. The existing `synthetic_baseline_datasheet.pdf`, `make_baseline_pdf.py`, PDF extraction tests, and evaluator regression suite passed in this Release run. The preserved contract is:

1. Poppler source word coordinates and text remain in original provenance; only explicitly selected script-token sorting anchors use the host's row coordinates.
2. Each host/script association requires exact selected source boxes and exact expected text; ambiguous host matches reject atomically.
3. Original space-joined source text remains separate from explicitly reconstructed display notation.
4. Host/script ownership is geometrically bounded to the selected column, unique words and host, role direction, glyph-height ratio, horizontal adjacency, and no unrelated intersecting words.
5. Bindings are opt-in; empty bindings preserve the legacy extraction path.
6. A source string such as `10 -4` remains a string and is not parsed as a numeric exponent.
7. Unsupported multi-frequency tables remain fail-closed even if one token association is locally valid.
8. Evidence identifiers resolve through the manifest, including distinct original page index and printed page label fields.
9. All observations remain candidates; no engineering approval or model promotion occurs in `datasim_pdf`.

No Phase 4I semantics, global row tolerance, or numeric grammar changed in Phase 4J.

## 8. Provenance and trust-boundary audit

The contracts were inspected across `SourceDocument`, `EvidenceLocation`, `ParameterObservation`, `ComponentDomainBundle`, `ComponentKnowledge`, `ModelArtifactDescriptor`, `ModelValidationRecord`, `CircuitAssembly`, `ExecutionPolicy`, and `ExecutionClassification`. Their validating suites passed in this run. The resulting limits are explicit:

- Intake and domain validators check identifiers, references, field constraints, and visible conflicting candidates; they do not prove a document's authorship, revision, cryptographic digest authenticity, or reviewer identity.
- PDF page indices and printed page labels are separate optional source fields, not derived from each other. Source strings, units, qualifiers, and conditions are carried without silent normalization or conversion.
- Candidate observations cannot enter approved knowledge without a valid review transition. The integration fixture simulates the allowed review path; actor-kind metadata is not identity authentication.
- Model selection and validation metadata and operating-envelope checks are required before execution. They are not a formal proof that the model is physically correct.
- Synthetic records are identified as synthetic and the vertical execution policy classifies synthetic runs; the fixture is not described as manufacturer validation.
- `provenance_metadata.json` carries the supported lineage to the run. Existing 64-bit FNV-1a content IDs are deterministic fingerprints, not cryptographic integrity or signatures.
- Backend/process failures are surfaced as failures; integration target passed with the real process. PDF tools are launched with explicit executable/argument handling and no shell, with a 30-second process timeout. The PDF adapter's temporary workspace is now created exclusively and cleaned after extraction.
- Residual resource boundary: the 32 MiB selected-page output limit is checked after `pdftotext` exits. It limits what the parser reads but is not a streaming output quota; the timeout limits elapsed time, not bytes written before termination.
- Some diagnostics can include a source word or local path as error context; there is no general redaction layer. Public CI uses synthetic fixtures only and uploads no controlled vendor PDFs or raw annotations. Treat local diagnostics as document-derived content when handling restricted sources.

## 9. Security, licensing, and data governance

The Git index contains only the project's synthetic PDF fixtures and synthetic labels; no manufacturer PDFs, raw vendor annotation files, or copyrighted table dumps are tracked. `evaluation/phase4h_results.json` and `evaluation/phase4i_results.json` are aggregate summaries with identifiers, counts, and hashes, not document text. No local vendor assets were added in Phase 4J.

Poppler remains an external executable dependency and is not bundled by the project. The repository has no adopted license. This report makes no claim that the project or third-party files may be redistributed, and it does not adopt a license. Owner decisions remain open for repository licensing, third-party binary redistribution, approved corpus rights, source authenticity, and reviewer identity policy. The project documentation identifies Poppler as GPL-2.0-or-later; this is an external dependency fact, not a project redistribution grant. The [Poppler project](https://poppler.freedesktop.org/) and its release materials remain the authoritative source for upstream tool details.

No shell-injection path, restricted asset, or document authenticity misrepresentation was found during this bounded review. F-01 is the confirmed temporary-workspace defect fixed in this branch. The post-extraction output-size check and source-token diagnostic context are residual limitations, not claims of hostile-document sandboxing or broad redaction. No security guarantee beyond the existing argument handling, timeout, validators, and isolated workspace behavior is claimed.

## 10. Findings, known limitations, and owner decisions

**Finding F-01 (fixed in this branch):** the PDF adapter previously formed temporary directory names from a process-local counter and called `create_directories` without requiring new-directory creation. A pre-existing directory with the next name could be reused and later recursively removed by the extraction cleanup; separate processes could also race on the same name and scratch files. The implementation now retries names until `create_directory` atomically creates a new workspace, and cleanup only targets the path created by that invocation. The regression test redirects the test process to an isolated temporary root, pre-creates `datasim_pdf_0` with a sentinel file, performs real Poppler extraction, and verifies the sentinel remains. This is a narrow resource-safety correction; it does not change PDF interpretation or public APIs.

No other demonstrated correctness defect requiring a code fix was found in the integrated Phase 4B–4I contracts or their current tests. Unsupported capabilities remain limitations, not defects.

Known limits include ideal-RC-only simulation; no physical hardware calibration; selected, selectable-text PDF regions only; no OCR or automatic layout recognition; constrained caller-selected header, inline-cell, hierarchy, and script bindings; no automatic unit conversion; no numerical interpretation of baseline-reconstructed exponents; unsupported complex/multi-frequency/multi-MPN layouts; local-only real-datasheet evaluation; single-annotator historical labels; in-memory, unauthenticated review/provenance metadata; and no streaming quota or general redaction for Poppler output/diagnostics.

Open owner decisions:

1. Adopt or decline a repository license and separately determine third-party redistribution obligations.
2. Approve a rights-cleared and reproducible vendor corpus before claiming broad extraction accuracy.
3. Define an identity/authentication and review-accountability model if multi-user approval is required.
4. Authorize any future persistent storage, GUI, broader device/model coverage, or parser expansion as a separately scoped phase.
5. Decide whether future deployments need a streaming Poppler output quota and diagnostic redaction for processing hostile or rights-restricted PDFs.

## 11. Release acceptance checklist

| Criterion | Phase 4J finding |
| --- | --- |
| Main, PR #13 and prior hosted evidence reconciled | PASS; exact merge SHAs and successful post-merge runs recorded. |
| Clean Release build and all mandatory CTest targets | PASS locally; 9/9 with zero skips. |
| Real Poppler and ngspice integration | PASS locally with Poppler 26.10.0 and ngspice-47; hosted Phase 4J run `42148338325` succeeded on commit `c631abc5f578a269b8748b39c631b7aa06b87768`. |
| RC numerical references and repeatability | PASS locally; six runs and retained byte-level reference comparisons recorded. |
| Candidate/review/model trust boundaries | PASS within existing metadata contract and synthetic tests; no authentication claim. |
| Historical fixtures and benchmark evidence preserved | PASS; no source/test fixture or historical aggregate changed. |
| Supported/unsupported PDF limits stated | PASS. |
| New real corpus rerun has provenance, denominator and limits | NOT VERIFIED; files unavailable, historical figures labeled historical. |
| Restricted assets or license decision added | PASS; none added, decisions remain with owner. Public artifacts use synthetic data; local diagnostics may echo source tokens and remain subject to source rights. |
| Documentation consistent | Local link/status/diff review passed; final PR head remains subject to owner review. |
| Phase 4J hosted CI | PASS; run `42148338325` succeeded on the Phase 4J branch commit. |

**Review disposition:** ready for owner review as a documentation and evidence PR. Phase 4J validation is VERIFIED on the tested branch commit. The PR remains open for owner review; do not merge automatically.

## 12. Technical references

- MIT OpenCourseWare, *Lecture 05 – Review: step response of 1st order systems* (RC charging response and τ = RC): [course notes](https://ocw.mit.edu/courses/2-004-systems-modeling-and-control-ii-fall-2007/92b7076d416d5ae7f9a26c100b52b724_lecture05.pdf).
- ngspice, *ngspice User's Manual, release 47* (transient analysis and simulator operation): [manual](https://ngspice.sourceforge.io/docs/ngspice-manual.pdf).
- Poppler, upstream project and release information: [Poppler](https://poppler.freedesktop.org/). The Datasim adapter consumes the tool's selectable text and reported word geometry; it does not infer semantic table structure beyond the explicit bindings described above.
