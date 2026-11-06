# ADR-0013: Datasheet Evidence Workbench

**Status: Accepted for Phase 5C implementation.**

## Context

The existing `datasim_pdf` adapter already supports bounded extraction from an
explicit page and explicit table geometry. Phase 5B supplies an optional Qt 6
desktop application. An evidence workbench needs PDF display and evidence
configuration without moving PDF processing into domain libraries or changing
their contracts.

## Decision

- Keep the five production libraries and dependency graph unchanged.
- Add an optional `DATASIM_BUILD_PDF_WORKBENCH` application feature, default
  `OFF`, dependent on `DATASIM_BUILD_DESKTOP=ON`.
- Use Qt PDF (`QPdfDocument`, `QPdfView`, `QPdfPageNavigator`) for read-only
  display and navigation. Use existing Poppler-backed `datasim_pdf` for
  selectable-text extraction and source geometry.
- Accept numeric table geometry only in Poppler top-origin points. Defer mouse
  rectangle selection until a tested Qt-to-Poppler transform is available.
- Build a complete explicit `ExtractionRequest`, execute it in an application
  worker, and display only returned candidate evidence and provenance.
- Preserve `ReviewState`; do not apply a review decision, approve knowledge,
  create a model descriptor, or start a simulation from extraction output.
- Bundle only an original small project-owned SVG application mark. Keep user
  PDFs outside the repository and synthetic inputs under `tests/fixtures/`.

## Consequences

Qt PDF and Poppler have separate duties. The optional app configuration requires
Qt 6.11 `Pdf` and `PdfWidgets`; default/core and Phase 5B-only builds remain
independent of those modules. Qt PDF includes PDFium and third-party notices;
distribution must respect the applicable Qt, PDFium, and dependency terms.
Poppler remains an external GPL-2.0-or-later executable. No dependency binaries
are bundled, and repository licensing remains unresolved.

## References

- Qt PDF class documentation: <https://doc.qt.io/qt-6/qpdfdocument.html>,
  <https://doc.qt.io/qt-6/qpdfview.html>.
- Qt PDF licensing and included PDFium notice: <https://doc.qt.io/qt-6/qtpdf-licensing.html>.
- Qt CMake imported targets: <https://doc.qt.io/qt-6/cmake-imported-targets.html>.
- MSYS2 Qt 6 PDF package record (local toolchain reference):
  <https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-qt6-pdf>.
- Existing Poppler and extraction boundary: [ADR-0006](ADR-0006-deterministic-pdf-table-extraction.md).
