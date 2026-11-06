# Phase 5C implementation record

**Status: IMPLEMENTED (initial explicit-coordinate workflow).** Verification
results and remaining gaps are in [validation](PHASE_5C_VALIDATION.md).

## Implemented

- An additional Datasheet Evidence Workbench page integrated into the existing
  `datasim_viewer`; the Phase 5B manual RC page remains present.
- Optional `DATASIM_BUILD_PDF_WORKBENCH`, default `OFF`, with clear invalid
  option rejection and checks for `Qt6::Pdf` / `Qt6::PdfWidgets`.
- Local PDF display, page navigation, zoom controls, page count, and explicit
  zero-based page index using Qt PDF APIs. The source file is opened read-only.
- Explicit identity, rights-notes, page, table, region, header, column,
  category, and tolerance fields. Missing/invalid numeric fields are rejected
  before worker launch. No redistribution permission is asserted by default.
- Application-level construction of complete `datasim::pdf::ExtractionRequest`
  values and off-GUI-thread execution of
  `datasim::pdf::extract_electrical_table()`.
- Candidate observation table with original values/units, parsed number,
  qualifier, category, operating conditions, and exact returned review state.
  Detail view shows source IDs and existing value/context/baseline provenance.
- Result invalidation when a field or document changes; stale in-flight results
  are discarded. Diagnostics remain visible on extraction failure.
- Original project-owned SVG icon in Qt Resource System at
  `:/datasim/branding/datasim-mark.svg`; resource bytes are included by
  `DATASIM_SOURCE_SHA256`.
- Focused desktop tests for resource availability, valid/image-only/missing/
  corrupt PDF loading, page count/navigation, and workbench construction.
- `ExtractionWorker` uses a typed queued result signal and has an event-loop test
  confirming the result callback runs on the GUI thread.

## Deferred

Mouse rectangle selection, automated GUI clicks, complete fault injection of
the UI worker lifecycle, and expanded workbench-specific validation cases are
not implemented. The existing PDF extraction and process suites retain their
own broader synthetic coverage. See validation for the exact tested scope.
