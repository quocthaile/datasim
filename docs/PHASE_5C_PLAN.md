# Phase 5C plan: Datasheet Evidence Workbench

## Objective

Extend the Phase 5B `datasim_viewer` with an optional Qt PDF display and an
explicit, candidate-only evidence extraction workspace. Keep `datasim_pdf`,
`datasim_intake`, `datasim_component`, and the simulation libraries unchanged.

## Incremental scope

1. Retain the Phase 5B manual ideal-RC page and add a separate evidence page.
2. Use `QPdfDocument`, `QPdfView`, and `QPdfPageNavigator` for local read-only
   display. Use Poppler via the existing `datasim::pdf` adapter for extraction.
3. Construct a complete `ExtractionRequest` from the selected local PDF and
   explicit identity, page, table, coordinates, and category input.
4. Run extraction on a worker thread; present returned observations,
   diagnostics, and provenance without applying review decisions.
5. Add small project-owned Qt resources and optional CMake/test configuration.

## Boundaries

No mouse rectangle selection, OCR, automatic table discovery, unit conversion,
persistence, approval, model validation, or simulation handoff is included.
Coordinates remain numeric Poppler top-origin page points. See
[ADR-0013](decisions/ADR-0013-datasheet-evidence-workbench.md).
