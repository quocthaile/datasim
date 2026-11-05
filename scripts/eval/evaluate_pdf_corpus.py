#!/usr/bin/env python3
"""Offline deterministic evaluator for the Datasim Phase 4C/4E PDF extraction pipeline.

Exercises the real C++ datasim::pdf::extract_electrical_table() implementation via
the development-only datasheet_eval_dump adapter executable. Does not reimplement
PDF parsing in Python and never modifies production library contracts.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

CORPUS_SCHEMA_VERSION = "datasim.phase4e.corpus.v1"
ANNOTATIONS_SCHEMA_VERSION = "datasim.phase4e.annotations.v1"
REPORT_SCHEMA_VERSION = "datasim.phase4e.eval_report.v1"

LAYOUT_CATEGORIES = (
    "supported_single_page_table",
    "partially_supported_ambiguous",
    "unsupported_complex_layout",
    "image_only_or_scanned",
    "missing_or_inaccessible_document",
    "rights_unestablished",
)

ERROR_TAXONOMY_LABELS = (
    "unsupported_layout",
    "missing_selectable_text",
    "invalid_table_region",
    "column_assignment_error",
    "row_segmentation_error",
    "missing_observation",
    "spurious_observation",
    "raw_value_mismatch",
    "unit_mismatch",
    "qualifier_mismatch",
    "condition_mismatch",
    "evidence_location_mismatch",
    "ambiguous_numeric",
    "source_identity_mismatch",
    "backend_or_environment_failure",
    "rights_or_document_unavailable",
)

CELL_ROW_RE = re.compile(r"^row=(\d+);column=(min|typ|max)$")


class EvaluationConfigError(Exception):
    """Raised when the corpus manifest, ground-truth annotations, or CLI config is invalid."""


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def find_executable(explicit: str | None, name: str) -> Path:
    if explicit:
        p = Path(explicit)
        if p.is_file():
            return p.resolve()
        raise EvaluationConfigError(f"Configured executable for {name} does not exist: {explicit}")
    found = shutil.which(name)
    if found:
        return Path(found).resolve()
    exe_name = f"{name}.exe" if os.name == "nt" else name
    repo_root = Path(__file__).resolve().parents[2]
    for candidate in (
        repo_root / "build" / "phase4g" / exe_name,
        repo_root / "build" / "phase4f" / exe_name,
        repo_root / "build" / "phase4e" / exe_name,
        repo_root / "build" / "ci" / exe_name,
        repo_root / "build" / exe_name,
        Path("C:/msys64/ucrt64/bin") / exe_name,
    ):
        if candidate.is_file():
            return candidate.resolve()
    raise EvaluationConfigError(f"Required executable '{name}' not found in PATH, build/, or C:/msys64/ucrt64/bin")


def get_poppler_version(exe_path: Path) -> str:
    proc = subprocess.run(
        [str(exe_path), "-v"],
        capture_output=True,
        text=True,
        check=False,
    )
    combined = (proc.stdout or "") + "\n" + (proc.stderr or "")
    for line in combined.splitlines():
        if "version" in line.lower():
            return line.strip()
    raise EvaluationConfigError(f"Unable to determine version from {exe_path}")


def validate_corpus_manifest(manifest: dict[str, Any]) -> None:
    if not isinstance(manifest, dict):
        raise EvaluationConfigError("Corpus manifest must be a JSON object")
    if manifest.get("schema_version") != CORPUS_SCHEMA_VERSION:
        raise EvaluationConfigError(
            f"Unsupported corpus manifest schema_version: {manifest.get('schema_version')!r}"
        )
    if not manifest.get("corpus_id") or not isinstance(manifest.get("corpus_id"), str):
        raise EvaluationConfigError("Corpus manifest requires non-empty string 'corpus_id'")
    docs = manifest.get("documents")
    if not isinstance(docs, list) or not docs:
        raise EvaluationConfigError("Corpus manifest requires non-empty list 'documents'")

    seen_doc_ids: set[str] = set()
    for idx, doc in enumerate(docs):
        if not isinstance(doc, dict):
            raise EvaluationConfigError(f"documents[{idx}] must be an object")
        doc_id = doc.get("document_id")
        if not doc_id or not isinstance(doc_id, str):
            raise EvaluationConfigError(f"documents[{idx}] missing valid 'document_id'")
        if doc_id in seen_doc_ids:
            raise EvaluationConfigError(f"Duplicate document_id in manifest: {doc_id}")
        seen_doc_ids.add(doc_id)

        for req_str in ("manufacturer", "title", "revision", "source_url", "eligibility_status", "layout_category"):
            if not doc.get(req_str) or not isinstance(doc.get(req_str), str):
                raise EvaluationConfigError(f"Document {doc_id} missing required string '{req_str}'")
        if doc["eligibility_status"] not in ("eligible", "excluded"):
            raise EvaluationConfigError(f"Document {doc_id} has invalid eligibility_status")
        if doc["layout_category"] not in LAYOUT_CATEGORIES:
            raise EvaluationConfigError(f"Document {doc_id} has invalid layout_category: {doc['layout_category']}")
        part_numbers = doc.get("part_numbers")
        if not isinstance(part_numbers, list) or not part_numbers or not all(isinstance(p, str) and p for p in part_numbers):
            raise EvaluationConfigError(f"Document {doc_id} requires non-empty string list 'part_numbers'")
        rights = doc.get("usage_rights")
        if not isinstance(rights, dict):
            raise EvaluationConfigError(f"Document {doc_id} missing 'usage_rights' object")
        if not isinstance(rights.get("redistribution_permitted"), bool) or not isinstance(
            rights.get("evaluation_permitted"), bool
        ):
            raise EvaluationConfigError(f"Document {doc_id} usage_rights requires boolean flags")
        if not rights.get("evidence_notes") or not isinstance(rights.get("evidence_notes"), str):
            raise EvaluationConfigError(f"Document {doc_id} usage_rights requires 'evidence_notes'")
        if doc["eligibility_status"] == "eligible":
            sha = doc.get("expected_sha256")
            if not isinstance(sha, str) or len(sha) != 64:
                raise EvaluationConfigError(f"Eligible document {doc_id} requires 64-hex 'expected_sha256'")

    cases = manifest.get("cases")
    if not isinstance(cases, list):
        raise EvaluationConfigError("Corpus manifest requires list 'cases'")
    seen_case_ids: set[str] = set()
    for idx, case in enumerate(cases):
        if not isinstance(case, dict):
            raise EvaluationConfigError(f"cases[{idx}] must be an object")
        case_id = case.get("case_id")
        if not case_id or not isinstance(case_id, str):
            raise EvaluationConfigError(f"cases[{idx}] missing valid 'case_id'")
        if case_id in seen_case_ids:
            raise EvaluationConfigError(f"Duplicate case_id in manifest: {case_id}")
        seen_case_ids.add(case_id)
        doc_id = case.get("document_id")
        if doc_id not in seen_doc_ids:
            raise EvaluationConfigError(f"Case {case_id} references unknown document_id: {doc_id}")
        if case.get("layout_category") not in LAYOUT_CATEGORIES:
            raise EvaluationConfigError(f"Case {case_id} has invalid layout_category")
        if case.get("expected_outcome") not in ("extract_candidates", "fail_closed"):
            raise EvaluationConfigError(f"Case {case_id} has invalid expected_outcome")
        if not isinstance(case.get("pdf_page_index"), int) or case["pdf_page_index"] < 0:
            raise EvaluationConfigError(f"Case {case_id} requires non-negative integer 'pdf_page_index'")
        if not isinstance(case.get("table_id"), str) or not case["table_id"]:
            raise EvaluationConfigError(f"Case {case_id} requires non-empty 'table_id'")
        if not isinstance(case.get("component"), dict) or not isinstance(case.get("table_geometry"), dict):
            raise EvaluationConfigError(f"Case {case_id} requires 'component' and 'table_geometry' objects")


def validate_annotations(annotations: dict[str, Any], manifest: dict[str, Any]) -> None:
    if not isinstance(annotations, dict):
        raise EvaluationConfigError("Annotations file must be a JSON object")
    if annotations.get("schema_version") != ANNOTATIONS_SCHEMA_VERSION:
        raise EvaluationConfigError(
            f"Unsupported annotations schema_version: {annotations.get('schema_version')!r}"
        )
    annotator_count = annotations.get("annotator_count")
    if not isinstance(annotator_count, int) or annotator_count < 1:
        raise EvaluationConfigError("Annotations require positive integer 'annotator_count'")
    if annotator_count == 1 and annotations.get("inter_annotator_agreement_claimed") is not False:
        raise EvaluationConfigError(
            "Single-annotator ground truth must set 'inter_annotator_agreement_claimed' to false"
        )
    cases = annotations.get("cases")
    if not isinstance(cases, list):
        raise EvaluationConfigError("Annotations require list 'cases'")

    manifest_cases = {c["case_id"]: c for c in manifest["cases"]}
    seen_cases: set[str] = set()
    for idx, ann_case in enumerate(cases):
        if not isinstance(ann_case, dict):
            raise EvaluationConfigError(f"annotations.cases[{idx}] must be an object")
        cid = ann_case.get("case_id")
        if cid not in manifest_cases:
            raise EvaluationConfigError(f"Annotation case_id {cid!r} not present in corpus manifest")
        if cid in seen_cases:
            raise EvaluationConfigError(f"Duplicate annotation case_id: {cid}")
        seen_cases.add(cid)
        for req_field in ("document_id", "table_id", "annotation_author", "review_status"):
            if not ann_case.get(req_field) or not isinstance(ann_case.get(req_field), str):
                raise EvaluationConfigError(f"Annotation case {cid} missing string '{req_field}'")
        if not isinstance(ann_case.get("adjudication_history"), list) or not ann_case["adjudication_history"]:
            raise EvaluationConfigError(f"Annotation case {cid} requires non-empty 'adjudication_history'")
        obs_list = ann_case.get("observations")
        if not isinstance(obs_list, list):
            raise EvaluationConfigError(f"Annotation case {cid} requires list 'observations'")
        for oidx, obs in enumerate(obs_list):
            if not isinstance(obs, dict):
                raise EvaluationConfigError(f"Annotation {cid}.observations[{oidx}] must be an object")
            for req in ("entry_id", "cell_or_region", "parameter_name", "original_value_text", "qualifier", "category", "expected_extraction_outcome"):
                if not isinstance(obs.get(req), str) or not obs.get(req):
                    raise EvaluationConfigError(f"Annotation {cid}.observations[{oidx}] missing '{req}'")
            if not isinstance(obs.get("row_index"), int) or obs["row_index"] < 1:
                raise EvaluationConfigError(f"Annotation {cid}.observations[{oidx}] requires positive 'row_index'")
            if not isinstance(obs.get("in_supported_subset"), bool):
                raise EvaluationConfigError(f"Annotation {cid}.observations[{oidx}] requires boolean 'in_supported_subset'")


def resolve_pdf_path(doc: dict[str, Any], repo_root: Path, corpus_dir: Path | None) -> Path | None:
    rel_repo = doc.get("repo_relative_path")
    if rel_repo:
        return (repo_root / rel_repo).resolve()
    local_name = doc.get("local_filename")
    if local_name and corpus_dir is not None:
        return (corpus_dir / local_name).resolve()
    return None


def build_dump_command(
    eval_dump_bin: Path,
    pdfinfo_bin: Path,
    pdftotext_bin: Path,
    pdf_path: Path,
    doc: dict[str, Any],
    case: dict[str, Any],
) -> list[str]:
    comp = case["component"]
    geom = case["table_geometry"]
    region = geom["region"]
    header = geom.get("header", [0.0, 0.0])

    cmd = [
        str(eval_dump_bin),
        "--pdf",
        str(pdf_path),
        "--pdfinfo",
        str(pdfinfo_bin),
        "--pdftotext",
        str(pdftotext_bin),
        "--page-index",
        str(case["pdf_page_index"]),
        "--component-id",
        comp["component_id"],
        "--manufacturer",
        comp["manufacturer"],
        "--part-number",
        comp["part_number"],
        "--family",
        comp.get("family", "other"),
        "--synthetic",
        "true" if doc.get("is_synthetic", False) else "false",
        "--document-id",
        doc["document_id"],
        "--publisher",
        doc.get("publisher", doc["manufacturer"]),
        "--title",
        doc["title"],
        "--revision",
        doc["revision"],
        "--sha256",
        case.get("override_sha256", doc["expected_sha256"]),
        "--redistribution-permitted",
        "true" if doc["usage_rights"].get("redistribution_permitted", False) else "false",
        "--restriction-notes",
        doc["usage_rights"].get("evidence_notes", "Local evaluation only."),
        "--table-id",
        case["table_id"],
        "--region",
        f"{region[0]},{region[1]},{region[2]},{region[3]}",
        "--row-y-tolerance",
        str(geom.get("row_y_tolerance", 2.0)),
        "--header",
        f"{header[0]},{header[1]}",
        "--parameter-column",
        f"{geom['parameter_column'][0]},{geom['parameter_column'][1]}",
        "--unit-column",
        f"{geom['unit_column'][0]},{geom['unit_column'][1]}",
        "--min-column",
        f"{geom['minimum_column'][0]},{geom['minimum_column'][1]}",
        "--typ-column",
        f"{geom['typical_column'][0]},{geom['typical_column'][1]}",
        "--max-column",
        f"{geom['maximum_column'][0]},{geom['maximum_column'][1]}",
        "--category",
        geom.get("category", "electrical_characteristic"),
    ]
    if case.get("printed_page_label"):
        cmd.extend(["--printed-page", str(case["printed_page_label"])])
    if comp.get("part_number_suffix"):
        cmd.extend(["--part-suffix", str(comp["part_number_suffix"])])
    if comp.get("package_id"):
        cmd.extend(["--package-id", str(comp["package_id"])])
    if comp.get("subtype"):
        cmd.extend(["--subtype", str(comp["subtype"])])
    if doc.get("source_url"):
        cmd.extend(["--original-url", str(doc["source_url"])])
    if doc.get("retrieval_date"):
        cmd.extend(["--retrieval-date", str(doc["retrieval_date"])])
    if doc["usage_rights"].get("license_identifier"):
        cmd.extend(["--license-id", str(doc["usage_rights"]["license_identifier"])])
    for mpn in case.get("override_doc_part_numbers", doc["part_numbers"]):
        cmd.extend(["--doc-part-number", str(mpn)])
    if geom.get("condition_column"):
        cc = geom["condition_column"]
        cmd.extend(["--condition-column", f"{cc[0]},{cc[1]}"])
    for ub in geom.get("header_unit_bindings", []):
        box = ub["header_box"]
        spec = (
            f"{ub['qualifier']}|{box[0]},{box[1]},{box[2]},{box[3]}|"
            f"{ub['unit_symbol']}|{ub['expected_header_text']}"
        )
        cmd.extend(["--header-unit-binding", spec])
    if geom.get("header_condition_binding"):
        cb = geom["header_condition_binding"]
        box = cb["header_box"]
        spec = (
            f"{box[0]},{box[1]},{box[2]},{box[3]}|"
            f"{cb['parameter_name']}|{cb['unit_symbol']}|{cb['expected_header_text']}"
        )
        cmd.extend(["--header-condition-binding", spec])
    for ib in geom.get("inline_value_unit_bindings", []):
        units_csv = ",".join(str(u) for u in ib["allowed_unit_symbols"])
        spec = f"{ib['qualifier']}|{units_csv}"
        cmd.extend(["--inline-unit-binding", spec])
    for binding in geom.get("parent_context_bindings", []):
        fields = [",".join(str(v) for v in binding["parent_box"]), binding["expected_parent_text"]]
        fields += [f"{c['row_index']}:{c['expected_parameter_text']}" for c in binding["children"]]
        if any("|" in field for field in fields):
            raise EvaluationConfigError("Parent context CLI text cannot contain pipes")
        cmd.extend(["--parent-context", "|".join(fields)])
    return cmd


def context_provenance_matches(dump: dict, obs: dict, case: dict, row_index: int | None) -> bool:
    """Check configured context evidence separately from composed label agreement."""
    expected = [(b, child) for b in case["table_geometry"].get("parent_context_bindings", [])
                for child in b["children"] if child["row_index"] == row_index]
    actual = [p for p in dump.get("parameter_context_provenance", [])
              if p.get("observation_id") == obs["id"]]
    if not expected:
        return not actual
    if len(expected) != 1 or len(actual) != 1:
        return False
    binding, child = expected[0]
    p = actual[0]
    evidence = {e["id"]: e for e in dump.get("evidence", [])}
    parent = evidence.get(p.get("parent_evidence_id"), {})
    child_ev = evidence.get(p.get("child_evidence_id"), {})
    refs = obs.get("evidence_refs", [])
    row_box = p.get("child_row_box", [])
    if (len(row_box) != 4 or not all(isinstance(v, (int, float)) and math.isfinite(v) for v in row_box)
            or row_box[0] >= row_box[2] or row_box[1] >= row_box[3]):
        return False
    return (p.get("row_index") == row_index
            and p.get("parent_box") == binding["parent_box"]
            and p.get("raw_parent_text") == binding["expected_parent_text"]
            and p.get("raw_child_parameter_text") == child["expected_parameter_text"]
            and bool(p.get("raw_child_row_text"))
            and parent.get("id") != child_ev.get("id")
            and all(e.get("id") in refs
                    and e.get("source_document_id") == case["document_id"]
                    and e.get("pdf_page_index") == case["pdf_page_index"]
                    and e.get("printed_page_label") == case.get("printed_page_label")
                    and e.get("table_or_figure_id") == case["table_id"] for e in (parent, child_ev))
            and parent.get("row_label") == binding["expected_parent_text"]
            and child_ev.get("row_label") == child["expected_parameter_text"]
            and str(parent.get("cell_or_region", "")).startswith("parent_context=")
            and child_ev.get("cell_or_region") == f"row={row_index};column=parameter/unit")


def classify_diagnostic_message(msg: str) -> str:
    lower = msg.lower()
    if "no selectable text; image-only" in lower or "no selectable words" in lower or "ocr" in lower:
        return "missing_selectable_text"
    if (
        "conflicts with bound header unit" in lower
        or "conflicts with explicit row unit" in lower
        or "inline unit token" in lower
        or "not permitted by configured inline value/unit binding" in lower
        or "header unit" in lower
    ):
        return "unit_mismatch"
    if "conflicts with bound condition header" in lower or "header condition" in lower:
        return "condition_mismatch"
    if (
        "explicit table region contains no selectable text" in lower
        or "only the configured header row" in lower
        or "no supported min/typ/max value cells" in lower
        or "invalid explicit table region" in lower
        or "header binding text mismatch" in lower
        or "header binding box" in lower
        or "header word straddles" in lower
    ):
        return "invalid_table_region"
    if "outside the document" in lower:
        return "evidence_location_mismatch"
    if "column span" in lower or "unassigned word" in lower or "missing parameter cell" in lower or "overlap" in lower:
        return "column_assignment_error"
    if "page count" in lower or "pdfinfo" in lower or "pdftotext" in lower or "digest mismatch" in lower or "cannot read pdf" in lower:
        return "backend_or_environment_failure"
    if (
        "unit cell is blank" in lower
        or "numeric value was not asserted" in lower
        or "unsupported value representation" in lower
    ):
        return "ambiguous_numeric"
    if "operating condition" in lower:
        return "condition_mismatch"
    return "unsupported_layout"


def classify_intake_code(code: str) -> str:
    if code in ("ambiguous_numeric_text", "asserted_ambiguous_numeric_value", "raw_numeric_mismatch"):
        return "ambiguous_numeric"
    if code in (
        "manufacturer_publisher_mismatch",
        "part_number_suffix_mismatch",
        "inconsistent_synthetic_markers",
        "synthetic_masquerading_as_manufacturer",
    ):
        return "source_identity_mismatch"
    if code in ("page_index_out_of_range", "derived_page_index"):
        return "evidence_location_mismatch"
    if code == "qualifier_text_mismatch":
        return "qualifier_mismatch"
    if code == "unsupported_unit_conversion":
        return "unit_mismatch"
    return "unsupported_layout"


def classify_domain_code(code: str) -> str:
    if code in ("incompatible_source_document_part_number", "synthetic_provenance_mismatch"):
        return "source_identity_mismatch"
    if code in ("invalid_sha256_digest",):
        return "backend_or_environment_failure"
    if code in ("missing_required_unit", "contradictory_unitless_flag", "incompatible_parameter_units"):
        return "unit_mismatch"
    if code in ("empty_evidence_locator", "missing_evidence_reference"):
        return "evidence_location_mismatch"
    return "unsupported_layout"


def parse_row_index(cell_or_region: str | None) -> int | None:
    if not cell_or_region:
        return None
    m = CELL_ROW_RE.match(cell_or_region)
    if not m:
        return None
    return int(m.group(1))


def normalize_conditions(obs: dict[str, Any]) -> list[tuple[str, str, str, float | None]]:
    result: list[tuple[str, str, str, float | None]] = []
    for cond in obs.get("operating_conditions", []):
        num = cond.get("numeric_value")
        result.append(
            (
                str(cond.get("parameter_name", "")),
                str(cond.get("original_value_text", "")),
                str(cond.get("unit_symbol", "")),
                float(num) if num is not None else None,
            )
        )
    return result


def floats_equal(a: float | None, b: float | None, rel_tol: float = 1e-12) -> bool:
    if a is None and b is None:
        return True
    if a is None or b is None:
        return False
    return math.isclose(float(a), float(b), rel_tol=rel_tol, abs_tol=1e-15)


def safe_ratio(numerator: int, denominator: int) -> float | None:
    if denominator == 0:
        return None
    return round(numerator / denominator, 6)


def evaluate_corpus(
    manifest_path: Path,
    annotations_path: Path,
    repo_root: Path,
    corpus_dir: Path | None,
    eval_dump_bin: Path,
    pdfinfo_bin: Path,
    pdftotext_bin: Path,
    repeat_check: bool = True,
) -> dict[str, Any]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    annotations = json.loads(annotations_path.read_text(encoding="utf-8"))
    validate_corpus_manifest(manifest)
    validate_annotations(annotations, manifest)

    manifest_sha256 = sha256_file(manifest_path)
    annotations_sha256 = sha256_file(annotations_path)
    pdfinfo_version = get_poppler_version(pdfinfo_bin)
    pdftotext_version = get_poppler_version(pdftotext_bin)

    docs_by_id = {d["document_id"]: d for d in manifest["documents"]}
    ann_by_case_id = {c["case_id"]: c for c in annotations["cases"]}

    taxonomy_counts: dict[str, int] = {label: 0 for label in ERROR_TAXONOMY_LABELS}

    # Section A: Corpus eligibility & layout coverage
    eligible_doc_count = 0
    excluded_doc_count = 0
    exclusion_reasons: dict[str, int] = {}
    doc_layout_counts: dict[str, int] = {cat: 0 for cat in LAYOUT_CATEGORIES}
    document_summaries: list[dict[str, Any]] = []

    for doc in sorted(manifest["documents"], key=lambda d: d["document_id"]):
        doc_id = doc["document_id"]
        cat = doc["layout_category"]
        doc_layout_counts[cat] += 1
        pdf_path = resolve_pdf_path(doc, repo_root, corpus_dir)
        actual_sha256: str | None = None
        file_present = False
        sha256_verified = False

        if pdf_path is not None and pdf_path.is_file():
            file_present = True
            actual_sha256 = sha256_file(pdf_path)
            sha256_verified = actual_sha256.lower() == (doc.get("expected_sha256") or "").lower()

        if doc["eligibility_status"] == "eligible":
            if file_present and sha256_verified and doc["usage_rights"].get("evaluation_permitted", False):
                eligible_doc_count += 1
            else:
                excluded_doc_count += 1
                reason = "local_pdf_missing_or_sha256_mismatch"
                exclusion_reasons[reason] = exclusion_reasons.get(reason, 0) + 1
                taxonomy_counts["rights_or_document_unavailable"] += 1
        else:
            excluded_doc_count += 1
            reason = doc.get("exclusion_reason") or "excluded_by_protocol"
            exclusion_reasons[reason] = exclusion_reasons.get(reason, 0) + 1
            if cat in ("missing_or_inaccessible_document", "rights_unestablished"):
                taxonomy_counts["rights_or_document_unavailable"] += 1

        document_summaries.append(
            {
                "document_id": doc_id,
                "manufacturer": doc["manufacturer"],
                "part_numbers": doc["part_numbers"],
                "title": doc["title"],
                "revision": doc["revision"],
                "source_url": doc["source_url"],
                "is_synthetic": bool(doc.get("is_synthetic", False)),
                "eligibility_status": doc["eligibility_status"],
                "exclusion_reason": doc.get("exclusion_reason"),
                "layout_category": cat,
                "redistribution_permitted": doc["usage_rights"]["redistribution_permitted"],
                "evaluation_permitted": doc["usage_rights"]["evaluation_permitted"],
                "expected_sha256": doc.get("expected_sha256"),
                "actual_sha256": actual_sha256,
                "file_present": file_present,
                "sha256_verified": sha256_verified,
                "page_count": doc.get("page_count"),
                "annotation_status": doc.get("annotation_status", "unannotated"),
            }
        )

    # Section B/C/D accumulators
    case_layout_counts: dict[str, int] = {cat: 0 for cat in LAYOUT_CATEGORIES}
    total_tp = 0
    total_fp = 0
    total_fn = 0

    field_totals = {
        "raw_value": {"matched": 0, "total": 0},
        "original_unit": {"matched": 0, "total": 0},
        "qualifier": {"matched": 0, "total": 0},
        "parameter_label": {"matched": 0, "total": 0},
        "operating_conditions": {"matched": 0, "total": 0},
        "evidence_location": {"matched": 0, "total": 0},
        "footnote_refs": {"matched": 0, "total": 0},
        "parsed_scalar": {"matched": 0, "total": 0},
    }

    expected_fail_closed_cases = 0
    actual_fail_closed_cases = 0
    false_accept_cases = 0
    diagnostic_matched_fail_closed_cases = 0

    repeat_runs_verified = 0
    repeat_run_mismatches = 0
    candidate_only_verified = True
    total_candidate_observations_inspected = 0

    case_reports: list[dict[str, Any]] = []

    for case in sorted(manifest["cases"], key=lambda c: c["case_id"]):
        case_id = case["case_id"]
        doc_id = case["document_id"]
        doc = docs_by_id[doc_id]
        ann_case = ann_by_case_id.get(case_id, {"observations": []})
        cat = case["layout_category"]
        case_layout_counts[cat] += 1

        case_taxonomy: dict[str, int] = {}

        def bump_tax(label: str) -> None:
            taxonomy_counts[label] += 1
            case_taxonomy[label] = case_taxonomy.get(label, 0) + 1

        pdf_path = resolve_pdf_path(doc, repo_root, corpus_dir)
        override_pdf_path = case.get("override_pdf_path")
        if override_pdf_path:
            pdf_path = (repo_root / override_pdf_path).resolve()

        if doc["eligibility_status"] != "eligible" and not override_pdf_path:
            bump_tax("rights_or_document_unavailable")
            case_reports.append(
                {
                    "case_id": case_id,
                    "document_id": doc_id,
                    "table_id": case["table_id"],
                    "pdf_page_index": case["pdf_page_index"],
                    "printed_page_label": case.get("printed_page_label"),
                    "layout_category": cat,
                    "expected_outcome": case["expected_outcome"],
                    "status": "skipped_ineligible_or_unavailable",
                    "taxonomy_counts": case_taxonomy,
                }
            )
            continue

        if pdf_path is None or not pdf_path.is_file():
            if case.get("allow_missing_pdf_for_fault_test", False):
                # Deliberate missing-file fault test executed through datasheet_eval_dump
                pass
            else:
                bump_tax("rights_or_document_unavailable")
                case_reports.append(
                    {
                        "case_id": case_id,
                        "document_id": doc_id,
                        "table_id": case["table_id"],
                        "pdf_page_index": case["pdf_page_index"],
                        "printed_page_label": case.get("printed_page_label"),
                        "layout_category": cat,
                        "expected_outcome": case["expected_outcome"],
                        "status": "missing_local_pdf",
                        "taxonomy_counts": case_taxonomy,
                    }
                )
                continue
        else:
            actual_sha = sha256_file(pdf_path)
            expected_doc_sha = doc.get("expected_sha256", "")
            if actual_sha.lower() != expected_doc_sha.lower() and not case.get("override_sha256"):
                bump_tax("backend_or_environment_failure")
                case_reports.append(
                    {
                        "case_id": case_id,
                        "document_id": doc_id,
                        "table_id": case["table_id"],
                        "pdf_page_index": case["pdf_page_index"],
                        "printed_page_label": case.get("printed_page_label"),
                        "layout_category": cat,
                        "expected_outcome": case["expected_outcome"],
                        "status": "pdf_sha256_mismatch",
                        "expected_sha256": expected_doc_sha,
                        "actual_sha256": actual_sha,
                        "taxonomy_counts": case_taxonomy,
                    }
                )
                continue

        cmd = build_dump_command(
            eval_dump_bin=eval_dump_bin,
            pdfinfo_bin=pdfinfo_bin,
            pdftotext_bin=pdftotext_bin,
            pdf_path=pdf_path if pdf_path is not None else Path("nonexistent.pdf"),
            doc=doc,
            case=case,
        )
        proc1 = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", check=False)
        if proc1.returncode != 0:
            raise EvaluationConfigError(
                f"datasheet_eval_dump failed on case {case_id} (exit {proc1.returncode}): {proc1.stderr}"
            )
        raw_json_1 = proc1.stdout
        dump1 = json.loads(raw_json_1)

        repeat_identical = True
        if repeat_check:
            proc2 = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", check=False)
            if proc2.returncode != 0 or proc2.stdout != raw_json_1:
                repeat_identical = False
                repeat_run_mismatches += 1
            else:
                repeat_runs_verified += 1

        # Verify candidate-only invariant on all extracted observations
        for ext_obs in dump1.get("observations", []):
            total_candidate_observations_inspected += 1
            if ext_obs.get("review_state") != "candidate" or ext_obs.get("review_history_count", 0) != 0:
                candidate_only_verified = False

        # Classify parser/intake/domain diagnostics
        for dmsg in dump1.get("extraction_diagnostics", []):
            bump_tax(classify_diagnostic_message(dmsg))
        for idiag in dump1.get("intake_diagnostics", []):
            if idiag.get("code") != "conflict_candidate":
                bump_tax(classify_intake_code(idiag.get("code", "")))
        for dissue in dump1.get("domain_validation_issues", []):
            bump_tax(classify_domain_code(dissue.get("code", "")))

        expected_outcome = case["expected_outcome"]
        has_bundle = bool(dump1.get("has_candidate_bundle", False))

        if expected_outcome == "fail_closed":
            expected_fail_closed_cases += 1
            if not has_bundle:
                actual_fail_closed_cases += 1
            else:
                false_accept_cases += 1
                bump_tax("spurious_observation")

            expected_subs = case.get("expected_diagnostic_substrings", [])
            expected_icodes = set(case.get("expected_intake_codes", []))
            expected_dcodes = set(case.get("expected_domain_codes", []))
            all_msgs = " | ".join(dump1.get("extraction_diagnostics", []))
            actual_icodes = {d["code"] for d in dump1.get("intake_diagnostics", [])}
            actual_dcodes = {d["code"] for d in dump1.get("domain_validation_issues", [])}

            subs_ok = all(sub in all_msgs for sub in expected_subs)
            icodes_ok = expected_icodes.issubset(actual_icodes)
            dcodes_ok = expected_dcodes.issubset(actual_dcodes)
            diag_matched = (not has_bundle) and subs_ok and icodes_ok and dcodes_ok
            if diag_matched:
                diagnostic_matched_fail_closed_cases += 1

            case_reports.append(
                {
                    "case_id": case_id,
                    "document_id": doc_id,
                    "table_id": case["table_id"],
                    "pdf_page_index": case["pdf_page_index"],
                    "printed_page_label": case.get("printed_page_label"),
                    "layout_category": cat,
                    "expected_outcome": expected_outcome,
                    "status": "fail_closed_verified" if diag_matched else "fail_closed_mismatch",
                    "has_candidate_bundle": has_bundle,
                    "diagnostic_matched": diag_matched,
                    "repeat_identical": repeat_identical,
                    "extraction_diagnostics": dump1.get("extraction_diagnostics", []),
                    "intake_diagnostics": dump1.get("intake_diagnostics", []),
                    "domain_validation_issues": dump1.get("domain_validation_issues", []),
                    "taxonomy_counts": case_taxonomy,
                }
            )
            continue

        # Supported or partially supported extraction case (expected_outcome == "extract_candidates")
        evidence_by_id = {ev["id"]: ev for ev in dump1.get("evidence", [])}
        gt_observations = [
            g
            for g in ann_case.get("observations", [])
            if str(g.get("expected_extraction_outcome", "")).startswith("candidate_")
        ]

        gt_by_key: dict[tuple[str, int, str, int, str], dict[str, Any]] = {}
        for g in gt_observations:
            key = (
                doc_id,
                int(case["pdf_page_index"]),
                str(case["table_id"]),
                int(g["row_index"]),
                str(g["qualifier"]),
            )
            gt_by_key[key] = g

        ext_by_key: dict[tuple[str, int, str, int, str], list[ tuple[dict[str, Any], dict[str, Any] | None] ]] = {}
        unkeyed_extracted: list[dict[str, Any]] = []
        if has_bundle:
            for ext_obs in dump1.get("observations", []):
                ev_refs = ext_obs.get("evidence_refs", [])
                ev: dict[str, Any] | None = None
                row_idx: int | None = None
                for ref_id in ev_refs:
                    cand_ev = evidence_by_id.get(ref_id)
                    if cand_ev is not None:
                        cand_row = parse_row_index(cand_ev.get("cell_or_region"))
                        if cand_row is not None:
                            ev = cand_ev
                            row_idx = cand_row
                            break
                if ev is None or row_idx is None:
                    unkeyed_extracted.append(ext_obs)
                    continue
                key = (
                    str(ev.get("source_document_id", "")),
                    int(ev["pdf_page_index"]) if ev.get("pdf_page_index") is not None else -1,
                    str(ev.get("table_or_figure_id", "")),
                    row_idx,
                    str(ext_obs.get("qualifier", "")),
                )
                ext_by_key.setdefault(key, []).append((ext_obs, ev))

        case_tp = 0
        case_fp = 0
        case_fn = 0
        matched_details: list[dict[str, Any]] = []

        # Unkeyed extracted observations are spurious
        for uobs in unkeyed_extracted:
            case_fp += 1
            bump_tax("spurious_observation")
            bump_tax("evidence_location_mismatch")

        all_keys = sorted(set(gt_by_key.keys()) | set(ext_by_key.keys()))
        for key in all_keys:
            g = gt_by_key.get(key)
            ext_list = ext_by_key.get(key, [])

            if g is not None and not ext_list:
                case_fn += 1
                bump_tax("missing_observation")
                matched_details.append(
                    {
                        "entry_id": g["entry_id"],
                        "key": list(key),
                        "match_status": "missing_observation",
                    }
                )
                continue

            if g is None and ext_list:
                for _ in ext_list:
                    case_fp += 1
                    bump_tax("spurious_observation")
                matched_details.append(
                    {
                        "entry_id": None,
                        "key": list(key),
                        "match_status": "spurious_observation",
                        "count": len(ext_list),
                    }
                )
                continue

            # Both g and at least one extracted candidate exist at this location key
            assert g is not None
            ext_obs, ev = ext_list[0]
            if len(ext_list) > 1:
                for _ in ext_list[1:]:
                    case_fp += 1
                    bump_tax("spurious_observation")

            expected_raw_cell = g.get("expected_raw_cell_text")
            raw_cell_ok = True
            if expected_raw_cell is not None:
                raw_cell_ok = ext_obs.get("raw_cell_text") == expected_raw_cell
            raw_val_ok = (ext_obs.get("original_value_text") == g.get("original_value_text")) and raw_cell_ok
            unit_ok = ext_obs.get("original_unit_symbol") == g.get("original_unit_symbol")
            qual_ok = ext_obs.get("qualifier") == g.get("qualifier")
            param_ok = ext_obs.get("parameter_name") == g.get("parameter_name")

            expected_conds = normalize_conditions(g)
            actual_conds = normalize_conditions(ext_obs)
            raw_cond_ok = (ext_obs.get("raw_conditions_text") or None) == (g.get("raw_conditions_text") or None)
            cond_ok = (expected_conds == actual_conds) and raw_cond_ok

            expected_hdr_regions = g.get("expected_header_evidence_regions")
            hdr_ev_ok = True
            if expected_hdr_regions is not None:
                actual_hdr_regions: list[str] = []
                for ref_id in ext_obs.get("evidence_refs", []):
                    ref_ev = evidence_by_id.get(ref_id)
                    if ref_ev is None:
                        hdr_ev_ok = False
                        break
                    region_str = str(ref_ev.get("cell_or_region") or "")
                    if region_str.startswith("header_"):
                        if (
                            ref_ev.get("pdf_page_index") != case["pdf_page_index"]
                            or ref_ev.get("printed_page_label") != case.get("printed_page_label")
                            or ref_ev.get("table_or_figure_id") != case["table_id"]
                        ):
                            hdr_ev_ok = False
                            break
                        actual_hdr_regions.append(region_str)
                if sorted(actual_hdr_regions) != sorted(expected_hdr_regions):
                    hdr_ev_ok = False

            ev_loc_ok = (
                ev is not None
                and ev.get("pdf_page_index") == case["pdf_page_index"]
                and ev.get("printed_page_label") == case.get("printed_page_label")
                and ev.get("table_or_figure_id") == case["table_id"]
                and ev.get("cell_or_region") == g.get("cell_or_region")
                and hdr_ev_ok
                and context_provenance_matches(dump1, ext_obs, case, g.get("row_index"))
            )
            footnotes_ok = (ev.get("footnote_refs", []) if ev else []) == g.get("footnote_refs", [])
            scalar_ok = floats_equal(ext_obs.get("parsed_numeric_value"), g.get("expected_parsed_numeric_value"))

            field_checks = {
                "raw_value": raw_val_ok,
                "original_unit": unit_ok,
                "qualifier": qual_ok,
                "parameter_label": param_ok,
                "operating_conditions": cond_ok,
                "evidence_location": ev_loc_ok,
                "footnote_refs": footnotes_ok,
                "parsed_scalar": scalar_ok,
            }
            for fname, ok in field_checks.items():
                field_totals[fname]["total"] += 1
                if ok:
                    field_totals[fname]["matched"] += 1

            if not raw_val_ok:
                bump_tax("raw_value_mismatch")
            if not unit_ok:
                bump_tax("unit_mismatch")
            if not qual_ok:
                bump_tax("qualifier_mismatch")
            if not param_ok:
                bump_tax("column_assignment_error")
            if not cond_ok:
                bump_tax("condition_mismatch")
            if not ev_loc_ok or not footnotes_ok:
                bump_tax("evidence_location_mismatch")
            if not scalar_ok:
                bump_tax("ambiguous_numeric")

            core_observation_ok = (
                raw_val_ok and unit_ok and qual_ok and param_ok and ev_loc_ok and scalar_ok
            )
            if core_observation_ok:
                case_tp += 1
            else:
                case_fp += 1
                case_fn += 1

            matched_details.append(
                {
                    "entry_id": g["entry_id"],
                    "observation_id": ext_obs.get("id"),
                    "key": list(key),
                    "match_status": "true_positive" if core_observation_ok else "field_mismatch",
                    "field_agreement": field_checks,
                }
            )

        total_tp += case_tp
        total_fp += case_fp
        total_fn += case_fn

        case_reports.append(
            {
                "case_id": case_id,
                "document_id": doc_id,
                "table_id": case["table_id"],
                "pdf_page_index": case["pdf_page_index"],
                "printed_page_label": case.get("printed_page_label"),
                "layout_category": cat,
                "expected_outcome": expected_outcome,
                "status": "evaluated",
                "has_candidate_bundle": has_bundle,
                "repeat_identical": repeat_identical,
                "tp": case_tp,
                "fp": case_fp,
                "fn": case_fn,
                "precision": safe_ratio(case_tp, case_tp + case_fp),
                "recall": safe_ratio(case_tp, case_tp + case_fn),
                "f1": safe_ratio(2 * case_tp, 2 * case_tp + case_fp + case_fn),
                "extraction_diagnostics": dump1.get("extraction_diagnostics", []),
                "intake_diagnostics": dump1.get("intake_diagnostics", []),
                "domain_validation_issues": dump1.get("domain_validation_issues", []),
                "taxonomy_counts": case_taxonomy,
                "observations": matched_details,
            }
        )

    field_agreement_summary: dict[str, Any] = {}
    for fname, counts in field_totals.items():
        field_agreement_summary[fname] = {
            "matched": counts["matched"],
            "total": counts["total"],
            "agreement_rate": safe_ratio(counts["matched"], counts["total"]),
        }

    report: dict[str, Any] = {
        "schema_version": REPORT_SCHEMA_VERSION,
        "corpus_id": manifest["corpus_id"],
        "annotation_set_id": annotations.get("annotation_set_id", ""),
        "annotator_count": annotations["annotator_count"],
        "inter_annotator_agreement_claimed": annotations.get("inter_annotator_agreement_claimed", False),
        "manifest_sha256": manifest_sha256,
        "annotations_sha256": annotations_sha256,
        "environment": {
            "python_version": sys.version.split()[0],
            "platform": sys.platform,
            "pdfinfo_version": pdfinfo_version,
            "pdftotext_version": pdftotext_version,
        },
        "section_a_corpus_eligibility_and_coverage": {
            "total_candidate_documents": len(manifest["documents"]),
            "eligible_documents": eligible_doc_count,
            "excluded_documents": excluded_doc_count,
            "exclusion_reasons": exclusion_reasons,
            "document_layout_category_counts": doc_layout_counts,
            "total_configured_cases": len(manifest["cases"]),
            "case_layout_category_counts": case_layout_counts,
            "documents": document_summaries,
        },
        "section_b_supported_extraction_quality": {
            "tp": total_tp,
            "fp": total_fp,
            "fn": total_fn,
            "precision_numerator": total_tp,
            "precision_denominator": total_tp + total_fp,
            "precision": safe_ratio(total_tp, total_tp + total_fp),
            "recall_numerator": total_tp,
            "recall_denominator": total_tp + total_fn,
            "recall": safe_ratio(total_tp, total_tp + total_fn),
            "f1_numerator": 2 * total_tp,
            "f1_denominator": 2 * total_tp + total_fp + total_fn,
            "f1": safe_ratio(2 * total_tp, 2 * total_tp + total_fp + total_fn),
            "field_level_agreement": field_agreement_summary,
        },
        "section_c_fail_closed_behavior": {
            "expected_fail_closed_cases": expected_fail_closed_cases,
            "actual_fail_closed_cases": actual_fail_closed_cases,
            "false_accept_cases": false_accept_cases,
            "diagnostic_matched_fail_closed_cases": diagnostic_matched_fail_closed_cases,
            "fail_closed_rate": safe_ratio(actual_fail_closed_cases, expected_fail_closed_cases),
            "diagnostic_match_rate": safe_ratio(
                diagnostic_matched_fail_closed_cases, expected_fail_closed_cases
            ),
        },
        "section_d_reproducibility_and_diagnostics": {
            "repeat_check_enabled": repeat_check,
            "repeat_runs_verified": repeat_runs_verified,
            "repeat_run_mismatches": repeat_run_mismatches,
            "total_candidate_observations_inspected": total_candidate_observations_inspected,
            "candidate_only_verified": candidate_only_verified,
            "error_taxonomy_counts": taxonomy_counts,
        },
        "cases": case_reports,
    }
    return report


def format_metric(val: float | None, num: int, den: int) -> str:
    if val is None or den == 0:
        return f"undefined ({num}/{den})"
    return f"{val:.4f} ({num}/{den})"


def render_markdown_summary(report: dict[str, Any]) -> str:
    sec_a = report["section_a_corpus_eligibility_and_coverage"]
    sec_b = report["section_b_supported_extraction_quality"]
    sec_c = report["section_c_fail_closed_behavior"]
    sec_d = report["section_d_reproducibility_and_diagnostics"]

    lines: list[str] = [
        f"# Datasim Phase 4E Evaluation Summary — `{report['corpus_id']}`",
        "",
        f"- **Report Schema**: `{report['schema_version']}`",
        f"- **Manifest SHA-256**: `{report['manifest_sha256']}`",
        f"- **Annotations SHA-256**: `{report['annotations_sha256']}`",
        f"- **Annotator Count**: `{report['annotator_count']}` (`inter_annotator_agreement_claimed = {str(report['inter_annotator_agreement_claimed']).lower()}`)",
        f"- **Poppler `pdfinfo`**: `{report['environment']['pdfinfo_version']}`",
        f"- **Poppler `pdftotext`**: `{report['environment']['pdftotext_version']}`",
        "",
        "## A. Corpus Eligibility and Layout Coverage",
        "",
        f"- **Total Candidate Documents**: `{sec_a['total_candidate_documents']}`",
        f"- **Eligible Documents**: `{sec_a['eligible_documents']}`",
        f"- **Excluded Documents**: `{sec_a['excluded_documents']}`",
        f"- **Total Configured Table Cases**: `{sec_a['total_configured_cases']}`",
        "",
        "| Document ID | Manufacturer | Part Numbers | Revision | Eligibility | Layout Category | SHA-256 Verified | Redistribution Permitted |",
        "| --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    for doc in sec_a["documents"]:
        mpns = ", ".join(doc["part_numbers"])
        lines.append(
            f"| `{doc['document_id']}` | {doc['manufacturer']} | `{mpns}` | `{doc['revision']}` | "
            f"`{doc['eligibility_status']}` | `{doc['layout_category']}` | "
            f"`{str(doc['sha256_verified']).lower()}` | `{str(doc['redistribution_permitted']).lower()}` |"
        )

    lines.extend(
        [
            "",
            "## B. Extraction Quality Conditional on Supported / Partially Supported Layouts",
            "",
            f"- **True Positives (`TP`)**: `{sec_b['tp']}`",
            f"- **False Positives (`FP`)**: `{sec_b['fp']}`",
            f"- **False Negatives (`FN`)**: `{sec_b['fn']}`",
            f"- **Precision**: `{format_metric(sec_b['precision'], sec_b['precision_numerator'], sec_b['precision_denominator'])}`",
            f"- **Recall**: `{format_metric(sec_b['recall'], sec_b['recall_numerator'], sec_b['recall_denominator'])}`",
            f"- **F1 Score**: `{format_metric(sec_b['f1'], sec_b['f1_numerator'], sec_b['f1_denominator'])}`",
            "",
            "### Field-Level Agreement (Matched Observations)",
            "",
            "| Field | Matched | Total | Agreement Rate |",
            "| --- | --- | --- | --- |",
        ]
    )
    for fname, fstats in sec_b["field_level_agreement"].items():
        rate_str = format_metric(fstats["agreement_rate"], fstats["matched"], fstats["total"])
        lines.append(f"| `{fname}` | `{fstats['matched']}` | `{fstats['total']}` | `{rate_str}` |")

    lines.extend(
        [
            "",
            "## C. Fail-Closed Behavior for Unsupported / Negative Layouts",
            "",
            f"- **Expected Fail-Closed Cases**: `{sec_c['expected_fail_closed_cases']}`",
            f"- **Actual Fail-Closed Cases**: `{sec_c['actual_fail_closed_cases']}`",
            f"- **False Accept Cases**: `{sec_c['false_accept_cases']}`",
            f"- **Diagnostic-Matched Fail-Closed Cases**: `{sec_c['diagnostic_matched_fail_closed_cases']}`",
            f"- **Fail-Closed Rate**: `{format_metric(sec_c['fail_closed_rate'], sec_c['actual_fail_closed_cases'], sec_c['expected_fail_closed_cases'])}`",
            "",
            "## D. Reproducibility, Trust Boundary, and Error Taxonomy",
            "",
            f"- **Repeat Runs Verified**: `{sec_d['repeat_runs_verified']}` (`mismatches = {sec_d['repeat_run_mismatches']}`)",
            f"- **Total Candidate Observations Inspected**: `{sec_d['total_candidate_observations_inspected']}`",
            f"- **Candidate-Only Boundary Verified (`review_state == candidate`)**: `{str(sec_d['candidate_only_verified']).lower()}`",
            "",
            "### Error Taxonomy Counts",
            "",
            "| Taxonomy Label | Count |",
            "| --- | --- |",
        ]
    )
    for label, count in sec_d["error_taxonomy_counts"].items():
        lines.append(f"| `{label}` | `{count}` |")

    lines.extend(
        [
            "",
            "## Per-Case Evaluation Results",
            "",
            "| Case ID | Document ID | Table ID | Page | Category | Expected | Status | TP | FP | FN | Precision | Recall |",
            "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |",
        ]
    )
    for c in report["cases"]:
        tp = c.get("tp", "-")
        fp = c.get("fp", "-")
        fn = c.get("fn", "-")
        prec = f"{c['precision']:.4f}" if isinstance(c.get("precision"), float) else str(c.get("precision", "-"))
        rec = f"{c['recall']:.4f}" if isinstance(c.get("recall"), float) else str(c.get("recall", "-"))
        lines.append(
            f"| `{c['case_id']}` | `{c['document_id']}` | `{c['table_id']}` | `{c['pdf_page_index']}` | "
            f"`{c['layout_category']}` | `{c['expected_outcome']}` | `{c['status']}` | "
            f"`{tp}` | `{fp}` | `{fn}` | `{prec}` | `{rec}` |"
        )
    lines.append("")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Datasim Phase 4E PDF Corpus Evaluator")
    parser.add_argument("--manifest", required=True, help="Path to corpus manifest JSON")
    parser.add_argument("--annotations", required=True, help="Path to ground-truth annotations JSON")
    parser.add_argument("--repo-root", default=None, help="Repository root path")
    parser.add_argument("--corpus-dir", default=None, help="Untracked local real-PDF corpus directory")
    parser.add_argument("--eval-dump-bin", default=None, help="Path to datasheet_eval_dump executable")
    parser.add_argument("--pdfinfo", default=None, help="Path to Poppler pdfinfo executable")
    parser.add_argument("--pdftotext", default=None, help="Path to Poppler pdftotext executable")
    parser.add_argument("--output-json", required=True, help="Output JSON report path")
    parser.add_argument("--output-md", required=True, help="Output Markdown summary path")
    parser.add_argument("--no-repeat-check", action="store_true", help="Disable second-run determinism check")

    args = parser.parse_args(argv)
    try:
        repo_root = Path(args.repo_root).resolve() if args.repo_root else Path(__file__).resolve().parents[2]
        manifest_path = Path(args.manifest).resolve()
        annotations_path = Path(args.annotations).resolve()
        if not manifest_path.is_file():
            raise EvaluationConfigError(f"Manifest file does not exist: {manifest_path}")
        if not annotations_path.is_file():
            raise EvaluationConfigError(f"Annotations file does not exist: {annotations_path}")

        corpus_dir = Path(args.corpus_dir).resolve() if args.corpus_dir else (repo_root / "build" / "corpus")
        eval_dump_bin = find_executable(
            args.eval_dump_bin or os.environ.get("DATASIM_EVAL_DUMP_BIN"), "datasheet_eval_dump"
        )
        pdfinfo_bin = find_executable(args.pdfinfo, "pdfinfo")
        pdftotext_bin = find_executable(args.pdftotext, "pdftotext")

        report = evaluate_corpus(
            manifest_path=manifest_path,
            annotations_path=annotations_path,
            repo_root=repo_root,
            corpus_dir=corpus_dir,
            eval_dump_bin=eval_dump_bin,
            pdfinfo_bin=pdfinfo_bin,
            pdftotext_bin=pdftotext_bin,
            repeat_check=not args.no_repeat_check,
        )

        out_json = Path(args.output_json)
        out_md = Path(args.output_md)
        out_json.parent.mkdir(parents=True, exist_ok=True)
        out_md.parent.mkdir(parents=True, exist_ok=True)

        json_text = json.dumps(report, indent=2, sort_keys=False) + "\n"
        md_text = render_markdown_summary(report)
        out_json.write_text(json_text, encoding="utf-8", newline="\n")
        out_md.write_text(md_text, encoding="utf-8", newline="\n")
        return 0
    except (EvaluationConfigError, json.JSONDecodeError, KeyError, ValueError) as exc:
        print(f"evaluate_pdf_corpus error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
