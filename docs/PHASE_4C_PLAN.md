# Phase 4C plan: Deterministic datasheet table extraction MVP

Status: **IMPLEMENTED and MERGED in PR #7 (`1704f726d901f3e1d00f9cdc918e80d352b443ac`),
VERIFIED on synthetic fixtures by hosted Windows CI runs `42000652152` and
`42017272043`.** Open licensing and redistribution policy questions remain
**PROPOSED**. No extraction accuracy claim is made for real vendor
datasheets, and no physical measurements were taken.

## Objective and boundaries

Phase 4C adds a PDF adapter above the stable Phase 4B intake boundary:

```text
local selectable-text PDF -> bounded table adapter -> IntakeManifest
    -> validate_intake_manifest() -> build_candidate_bundle()
```

The adapter must preserve the established Phase 2/3/4B contracts and link only to
`datasim_intake`. It accepts a local PDF, explicit document identity, exact MPN,
zero-based PDF page, table identifier, table rectangle and explicit column spans.
It does not authenticate document identity or rights. Those fields are caller
declarations.

The MVP supports simple, single-page electrical-characteristics tables whose text
cells occupy stable horizontal bands. It does not claim general PDF table
recognition. OCR, image-only content, automatic MPN recovery, unit conversion,
engineering interpretation, absolute-maximum promotion, model selection and
approval are excluded.

## Backend evaluation

| Candidate | Windows UCRT fit | Layout access | Licensing/distribution | Decision |
| --- | --- | --- | --- | --- |
| Poppler `pdfinfo` + `pdftotext -bbox-layout` | Packaged for MSYS2 UCRT; exercised in hosted Windows CI | Page count plus word bounding boxes in HTML | Poppler package declares GPL-2.0-or-later; the project has no license and makes no binary redistribution decision | Selected as an external executable boundary, version checked in CI |
| PDFium C API | Good C API for page/text access, but a project-local Windows build/package needs Chromium GN/tooling or separately managed binaries | Character text and coordinate APIs | PDFium uses a BSD-style license with third-party notices required for binary distributions | Not selected for this narrow MVP because its build/distribution footprint is larger |
| MuPDF | Mature C API | Text page extraction and structured text | AGPL or commercial licensing | Not selected while project distribution policy is undecided |

The adapter launches the local Poppler tools and does not link Poppler into
`datasim_intake`. CI checks the MSYS2 UCRT package `26.10.0-1` against SHA-256
`29971b323692b327aeacbf643180c129af23219b8aaa0b5cbcffc05e95a6334a` and checks
the executable-reported version before running extraction tests. The selected package and its dependencies
must not be copied into the repository. The GPL package license and obligations
remain visible for owner review; this decision does not determine the project
license or authorize redistribution.

## Deterministic table rules

1. Obtain page count from `pdfinfo`; reject a page index outside `[0, count)`.
2. Request only the selected page with `pdftotext -bbox-layout`.
3. Restrict words to the caller-provided page-coordinate rectangle.
4. Group words into rows using an explicit vertical tolerance; order words by
   horizontal coordinate, with stable lexical tie-breaking.
5. Assign each word to explicit non-overlapping column spans. Words on a span
   boundary, overlapping spans, repeated/merged cells and rows with ambiguous
   assignments fail closed with diagnostics.
6. Use the caller's explicit parameter, unit, min, typ and max column spans.
   Each non-empty numeric qualifier cell yields a separate candidate. Blank or
   dash cells remain absent. Numeric parsing accepts only the existing
   `parse_unambiguous_scalar` grammar; source notation and units are retained.
7. Preserve all source locations as table/row/column text in `EvidenceLocation`
   and each `ParameterObservation::evidence_refs`. Printed page labels, MPN,
   publisher, rights, conditions and category are supplied by explicit selection
   or captured verbatim; none are inferred from the zero-based page number.
8. Build an `IntakeManifest`, invoke `validate_intake_manifest()`, then call
   `build_candidate_bundle()`. No bypass to approved knowledge exists.

Any unsupported geometry or uncertain cell mapping must produce diagnostics and
no candidate for the affected row. Output ordering and IDs derive only from the
document ID, selected page, table ID, row order, column name and source text; no
timestamps or random values participate.

## Fixture and validation approach

Tests use a locally generated fictional PDF with selectable vector text. The
fixture contains no vendor data. It exercises two component families, min/typ/max
columns, blank cells, ambiguous literals, units, conditions, footnote markers,
repeated extraction, page-vs-printed-page distinction and rejected mappings.
The hosted test installs Poppler and runs the extraction target; missing tools are
test failures in CI, not skips.

## Status vocabulary

- **IMPLEMENTED**: code and docs merged into `main` via PR #7 (`1704f726d901f3e1d00f9cdc918e80d352b443ac`).
- **VERIFIED**: only a command that actually completed with its result recorded.
- **PROPOSED**: a decision or capability awaiting review.
- **NOT VERIFIED**: no passing execution evidence, including any skipped test.
