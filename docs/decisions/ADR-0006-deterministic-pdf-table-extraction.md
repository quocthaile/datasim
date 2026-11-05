# ADR-0006: Deterministic PDF table extraction

Date: 2026-11-05

Status: **IMPLEMENTED and MERGED in PR #7 (`1704f726d901f3e1d00f9cdc918e80d352b443ac`),
VERIFIED on synthetic fixtures in hosted Windows CI runs `42000652152` and
`42017272043`.** Project licensing and binary redistribution decisions remain
**PROPOSED**.

## Context

Phase 4B provides an in-memory, candidate-only `datasim_intake` boundary. It has
no PDF processing dependency. Phase 4C needs to produce candidate observations
from a deliberately small, text-selectable PDF subset while preserving all
existing component, intake, and simulation contracts.

The adapter must obtain the actual PDF page count, use explicit zero-based page
selection, require an explicit table region and column layout, preserve raw text
and source-cell provenance, and route every result through
`validate_intake_manifest()` and `build_candidate_bundle()`. It must fail closed
on unsupported or ambiguous geometry and cannot create approved observations.

## Decision

1. Keep PDF processing outside `datasim_intake`. The separate `datasim_pdf`
   adapter depends on the established intake API and does not link simulation,
   persistence, OCR or external services.
2. Use Poppler's `pdfinfo` and `pdftotext -bbox-layout` as external local tools.
   The adapter consumes page count and word bounding boxes, then applies explicit
   rectangle and column-span configuration. It does not parse arbitrary table
   structure or call an LLM.
3. Require a caller-supplied source document record, exact manufacturer/MPN,
   rights metadata, selected PDF page, printed label (if known), table identity,
   region, category and column spans. The PDF index never supplies the printed
   label or MPN.
4. Preserve original cell strings and units. Parse only unambiguous scalar text
   using the Phase 4B numeric parser. Distinct min/typ/max cells produce distinct
   candidate observations; empty cells are absent. Conditions and footnote text
   are source evidence, not inferred engineering facts.
5. Make candidate generation deterministic from explicit request inputs and
   source layout. IDs contain stable source coordinates and no time/random data.
6. Use fictional generated PDFs only in tests and examples. No vendor PDF, table,
   model file or extracted copyrighted dataset enters the repository.

## Backend evaluation

- **Poppler**: MSYS2 provides a UCRT package with `pdfinfo.exe` and
  `pdftotext.exe`; `-bbox-layout` exposes word coordinate boxes. The package is
  declared GPL-2.0-or-later. The project does not redistribute this package in
  the MVP, and the repository's missing license remains an unresolved owner
  decision. CI checks the expected Poppler version and runs the real extraction
  path.
- **PDFium**: Google's public C API exposes page counts, Unicode text, and text
  coordinates, and its source tree documents dedicated text-extraction code.
  Windows deployment would require an independently pinned compatible build and
  third-party notices, so it adds more build/release machinery than needed here.
- **MuPDF**: offers a capable text API, but its AGPL/commercial licensing paths
  are a poor fit while distribution and project licensing are undecided.

These are technology selection observations, not legal conclusions. Poppler's
license obligations must be reviewed before bundling or distributing its
executables or libraries.

## Consequences

- `datasim_intake` public APIs remain unchanged.
- Text extraction and geometry are deterministic for a pinned Poppler build and
  explicit input configuration; layout grouping is deliberately narrow and
  version-sensitive.
- The adapter cannot authenticate publisher identity, MPN, PDF integrity, or
  source rights. A content hash identifies bytes only.
- Complex merged cells, multi-page tables, rotated text, image-only pages,
  unusual reading order, overlapping columns, and ambiguous conditions fail
  closed or remain unresolved with diagnostics.
- Real-world accuracy is not verified without a labeled corpus that is legally
  usable for evaluation.

## References

- Poppler project and package: <https://poppler.freedesktop.org/> and
  <https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-poppler>
- CI package checked: `mingw-w64-ucrt-x86_64-poppler 26.10.0-1`, SHA-256
  `29971b323692b327aeacbf643180c129af23219b8aaa0b5cbcffc05e95a6334a`.
- Poppler page text layout API: <https://poppler.freedesktop.org/api/glib/poppler-Poppler-Page.html>
- PDFium text API: <https://pdfium.googlesource.com/pdfium/+/master/public/fpdf_text.h>
- PDFium text subsystem: <https://pdfium.googlesource.com/pdfium/+/master/core/fpdftext/README.md>
- MuPDF licensing: <https://mupdf.readthedocs.io/en/latest/license.html>
