# Datasim Phase 4 Release Summary

**Release scope:** bounded Phase 4B–4I evidence intake, extraction, evaluation, provenance, and synthetic integration.
**Phase 4J disposition:** local final validation passed, including an isolated temporary-workspace safety fix; hosted branch CI and owner review remain pending.
**Phase 4J branch:** `chore/phase-4j-final-validation`.

## Delivered Phase 4 scope

Phase 4 extends Datasim's ideal-RC/component contracts with a deterministic, candidate-only evidence path:

- `datasim_intake` validates explicitly supplied source/evidence manifests and emits candidate observations.
- `datasim_pdf` extracts selectable PDF text and geometry from caller-selected pages and table regions. Explicit header, inline value/unit, hierarchical parent/child, and baseline host/script bindings preserve source evidence.
- `datasheet_eval_dump` and `scripts/eval/evaluate_pdf_corpus.py` support versioned offline synthetic and controlled real-corpus comparisons.
- The Phase 4D integration fixture connects a synthetic PDF through the existing candidate, review, model-validation, vertical-slice and real ngspice flow, retaining run provenance.
- Existing review, model-validation, execution-policy, and synthetic-classification gates remain intact. There is no automatic approval or model promotion.

Phase 4H merged through PR #12 at `864eb56c1712470180cc1ecb9d9dbef3b5fd05ff`; post-merge Windows CI run `42058944651` succeeded. Phase 4I merged through PR #13 at `fb35ae1fc5a58dcd6b9f89d7a6807c3198ab99a5`; post-merge Windows CI run `42065262568` succeeded. Phase 4J performed a fresh local Windows Release build and full verification at that integrated source baseline. The security audit also fixed a temporary-directory collision in the PDF adapter with exclusive directory creation and a sentinel-preservation regression test; extraction behavior and public APIs are unchanged. Phase 4J's required hosted CI is still a review gate, so Phase 4J is not yet VERIFIED.

## Final local verification snapshot

- Windows 10 x64 / AMD64, MSYS2 UCRT GCC/G++ 16.1.0, CMake 4.3.2, Release / MinGW Makefiles.
- Poppler 26.10.0 and real ngspice-47 (`ngspice_con.exe`).
- Fresh `build/phase4j-release-final`: configure/build exit 0; CTest **9/9 passed, 0 failed, 0 skipped**.
- CI runtime configuration tests **5/5 passed**; corpus evaluator tests **9/9 passed**; PDF extraction **239 checks passed**, including the collision regression.
- Real Poppler and real ngspice were exercised in the synthetic end-to-end gate. The six-run RC convergence/repeatability study passed; repeated coarse runs matched exactly.
- Fresh RC run `results.csv`, `errors.csv`, netlist, and backend log matched retained Phase 1.5 SHA-256 values. Metadata files retained equal numerical results and were not claimed byte-identical.

Full commands, hashes, historical comparisons, architecture matrix, provenance/security review, rights constraints, limitations, and acceptance status are in [PHASE_4_FINAL_VALIDATION.md](PHASE_4_FINAL_VALIDATION.md).

## Historical real-corpus status

Historical reports record the local pilot corpus progressing from Phase 4E TP=9/15, FP=6, FN=6, through Phase 4F/4G gains and Phase 4H/4I comparable subsets of TP=15/15 and TP=22/22. The latter reports also recorded 4/4 unsupported layouts failing closed, zero false accepts, and 8/8 repeat checks. These are historical, single-annotator pilot measurements. The original controlled vendor PDFs and annotation files were unavailable for Phase 4J, so no new real-corpus result is claimed. Synthetic regression tests were rerun successfully.

## Not included in this bounded release

Phase 4 does not deliver OCR, automatic datasheet/table recognition, general PDF parsing, automatic interpretation or model generation, automatic approval, LLM integration, persistent storage, a GUI, semiconductor device models, solver extensions, custom numerical solvers, or physical hardware accuracy. It is not a general-purpose circuit simulator or manufacturer-datasheet processor.

Poppler remains an external executable dependency. No manufacturer PDF or copyrighted table passage is tracked. The repository still has no adopted license; repository licensing, third-party redistribution, rights-cleared corpus access, reviewer identity, and source-authenticity decisions belong to the owner.

## Review and release gate

The validation branch is ready for owner review. Keep the pull request unmerged until the Phase 4J hosted Windows workflow completes successfully and the owner reviews the report. Only then may Phase 4J be marked VERIFIED. This closure does not authorize follow-on product implementation.
