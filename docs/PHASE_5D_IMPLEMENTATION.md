# Phase 5D implementation record

**Status: IMPLEMENTED on `feat/phase-5d-evidence-review`.** Verification status
is tracked separately in [validation](PHASE_5D_VALIDATION.md).

## Implemented

- Application-owned, Qt-free `ReviewSession` holding copies of extracted
  observations and evidence locations; source extraction values remain intact.
- Stable `ObservationId` mapping, session ownership and evidence ownership
  checks, duplicate decision ID rejection, and replacement context validation.
- Review transitions via existing domain validation and
  `apply_review_decision()`, with strong exception safety.
- Review panel with explicit reviewer ID, rationale, observation evidence,
  allowed next state, replacement ID for supersession, confirmation, and
  history; status text remains visible with state icons.
- Edited-session warnings on re-extraction, configuration/document change, and
  closure. Review controls are unavailable during extraction. Current file hash
  and complete configuration key are rechecked before worker results are used.
- Six supplied SVGs embedded at the documented Qt resource paths. The existing
  logo was preserved byte-for-byte; ZIP logo differed from repository logo and
  was not copied. The supplied `viewer.qrc` and `assets/README.md` were reviewed
  and applied.
- Qt Svg is linked only to the PDF workbench application/test targets. CI's
  Windows UCRT setup now installs the matching Qt Svg package.

## Explicitly not implemented

Authentication, durable audit, persistence/export, collaboration, automatic
conflict resolution, `ComponentKnowledge` promotion, model creation/validation,
and simulation routing remain out of scope. Human GUI lifecycle evidence and
hosted branch CI are not claimed until recorded in validation.
