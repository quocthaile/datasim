#!/usr/bin/env python3
"""Automated regression and determinism tests for the Phase 4E PDF corpus evaluator."""

from __future__ import annotations

import copy
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(os.environ.get("DATASIM_REPO_ROOT", Path(__file__).resolve().parents[2])).resolve()
if str(REPO_ROOT / "scripts" / "eval") not in sys.path:
    sys.path.insert(0, str(REPO_ROOT / "scripts" / "eval"))

import evaluate_pdf_corpus as eval_mod  # noqa: E402


class TestPdfCorpusEvaluator(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.repo_root = REPO_ROOT
        cls.manifest_path = cls.repo_root / "tests" / "evaluation" / "synthetic_corpus_manifest.json"
        cls.annotations_path = cls.repo_root / "tests" / "evaluation" / "synthetic_ground_truth.json"
        cls.real_manifest_path = cls.repo_root / "evaluation" / "real_corpus_manifest.json"
        cls.eval_dump_bin = eval_mod.find_executable(
            os.environ.get("DATASIM_EVAL_DUMP_BIN"), "datasheet_eval_dump"
        )
        cls.pdfinfo_bin = eval_mod.find_executable(None, "pdfinfo")
        cls.pdftotext_bin = eval_mod.find_executable(None, "pdftotext")

    def test_synthetic_corpus_metrics_and_determinism(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            json1 = tmp / "run1.json"
            md1 = tmp / "run1.md"
            json2 = tmp / "run2.json"
            md2 = tmp / "run2.md"

            rc1 = eval_mod.main(
                [
                    "--manifest",
                    str(self.manifest_path),
                    "--annotations",
                    str(self.annotations_path),
                    "--repo-root",
                    str(self.repo_root),
                    "--eval-dump-bin",
                    str(self.eval_dump_bin),
                    "--pdfinfo",
                    str(self.pdfinfo_bin),
                    "--pdftotext",
                    str(self.pdftotext_bin),
                    "--output-json",
                    str(json1),
                    "--output-md",
                    str(md1),
                ]
            )
            self.assertEqual(rc1, 0)

            rc2 = eval_mod.main(
                [
                    "--manifest",
                    str(self.manifest_path),
                    "--annotations",
                    str(self.annotations_path),
                    "--repo-root",
                    str(self.repo_root),
                    "--eval-dump-bin",
                    str(self.eval_dump_bin),
                    "--pdfinfo",
                    str(self.pdfinfo_bin),
                    "--pdftotext",
                    str(self.pdftotext_bin),
                    "--output-json",
                    str(json2),
                    "--output-md",
                    str(md2),
                ]
            )
            self.assertEqual(rc2, 0)

            # Byte-identical outputs across repeated evaluations
            self.assertEqual(json1.read_bytes(), json2.read_bytes())
            self.assertEqual(md1.read_bytes(), md2.read_bytes())

            report = json.loads(json1.read_text(encoding="utf-8"))
            self.assertEqual(report["schema_version"], eval_mod.REPORT_SCHEMA_VERSION)
            self.assertEqual(report["annotator_count"], 1)
            self.assertFalse(report["inter_annotator_agreement_claimed"])

            # Verify stable case ordering
            case_ids = [c["case_id"] for c in report["cases"]]
            self.assertEqual(case_ids, sorted(case_ids))
            self.assertEqual(len(case_ids), 7)

            # Section B: Supported / partially supported extraction metrics
            sec_b = report["section_b_supported_extraction_quality"]
            self.assertEqual(sec_b["tp"], 20)
            self.assertEqual(sec_b["fp"], 0)
            self.assertEqual(sec_b["fn"], 0)
            self.assertAlmostEqual(sec_b["precision"], 1.0)
            self.assertAlmostEqual(sec_b["recall"], 1.0)
            self.assertAlmostEqual(sec_b["f1"], 1.0)

            for field_name, stats in sec_b["field_level_agreement"].items():
                self.assertEqual(stats["matched"], 20, f"Field {field_name} mismatch")
                self.assertEqual(stats["total"], 20)
                self.assertAlmostEqual(stats["agreement_rate"], 1.0)

            # Section C: Fail-closed behavior
            sec_c = report["section_c_fail_closed_behavior"]
            self.assertEqual(sec_c["expected_fail_closed_cases"], 5)
            self.assertEqual(sec_c["actual_fail_closed_cases"], 5)
            self.assertEqual(sec_c["false_accept_cases"], 0)
            self.assertEqual(sec_c["diagnostic_matched_fail_closed_cases"], 5)
            self.assertAlmostEqual(sec_c["fail_closed_rate"], 1.0)

            # Section D: Reproducibility & candidate-only boundary
            sec_d = report["section_d_reproducibility_and_diagnostics"]
            self.assertEqual(sec_d["repeat_runs_verified"], 7)
            self.assertEqual(sec_d["repeat_run_mismatches"], 0)
            self.assertTrue(sec_d["candidate_only_verified"])
            self.assertGreaterEqual(sec_d["total_candidate_observations_inspected"], 20)

    def test_case_order_independence(self) -> None:
        manifest = json.loads(self.manifest_path.read_text(encoding="utf-8"))
        annotations = json.loads(self.annotations_path.read_text(encoding="utf-8"))

        reversed_manifest = copy.deepcopy(manifest)
        reversed_manifest["documents"] = list(reversed(reversed_manifest["documents"]))
        reversed_manifest["cases"] = list(reversed(reversed_manifest["cases"]))

        reversed_annotations = copy.deepcopy(annotations)
        reversed_annotations["cases"] = list(reversed(reversed_annotations["cases"]))
        for c in reversed_annotations["cases"]:
            c["observations"] = list(reversed(c["observations"]))

        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            m_rev = tmp / "manifest_rev.json"
            a_rev = tmp / "annotations_rev.json"
            m_rev.write_text(json.dumps(reversed_manifest), encoding="utf-8")
            a_rev.write_text(json.dumps(reversed_annotations), encoding="utf-8")

            r1 = eval_mod.evaluate_corpus(
                manifest_path=self.manifest_path,
                annotations_path=self.annotations_path,
                repo_root=self.repo_root,
                corpus_dir=None,
                eval_dump_bin=self.eval_dump_bin,
                pdfinfo_bin=self.pdfinfo_bin,
                pdftotext_bin=self.pdftotext_bin,
                repeat_check=False,
            )
            r2 = eval_mod.evaluate_corpus(
                manifest_path=m_rev,
                annotations_path=a_rev,
                repo_root=self.repo_root,
                corpus_dir=None,
                eval_dump_bin=self.eval_dump_bin,
                pdfinfo_bin=self.pdfinfo_bin,
                pdftotext_bin=self.pdftotext_bin,
                repeat_check=False,
            )
            self.assertEqual(r1["section_b_supported_extraction_quality"], r2["section_b_supported_extraction_quality"])
            self.assertEqual(r1["section_c_fail_closed_behavior"], r2["section_c_fail_closed_behavior"])
            self.assertEqual(r1["cases"], r2["cases"])

    def test_changed_or_missing_pdf_rejected(self) -> None:
        manifest = json.loads(self.manifest_path.read_text(encoding="utf-8"))
        annotations = json.loads(self.annotations_path.read_text(encoding="utf-8"))

        # 1. Corrupted SHA-256 expectation -> pdf_sha256_mismatch
        bad_sha_manifest = copy.deepcopy(manifest)
        bad_sha_manifest["documents"][0]["expected_sha256"] = "0" * 64
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            m_bad = tmp / "bad_sha.json"
            m_bad.write_text(json.dumps(bad_sha_manifest), encoding="utf-8")
            rep = eval_mod.evaluate_corpus(
                manifest_path=m_bad,
                annotations_path=self.annotations_path,
                repo_root=self.repo_root,
                corpus_dir=None,
                eval_dump_bin=self.eval_dump_bin,
                pdfinfo_bin=self.pdfinfo_bin,
                pdftotext_bin=self.pdftotext_bin,
                repeat_check=False,
            )
            rc_cases = [c for c in rep["cases"] if c["document_id"] == "doc.synthetic.rc"]
            for c in rc_cases:
                self.assertEqual(c["status"], "pdf_sha256_mismatch")
            self.assertGreater(
                rep["section_d_reproducibility_and_diagnostics"]["error_taxonomy_counts"][
                    "backend_or_environment_failure"
                ],
                0,
            )

            # 2. Missing local PDF -> missing_local_pdf
            missing_manifest = copy.deepcopy(manifest)
            missing_manifest["documents"][0]["repo_relative_path"] = "tests/fixtures/does_not_exist.pdf"
            m_miss = tmp / "missing_pdf.json"
            m_miss.write_text(json.dumps(missing_manifest), encoding="utf-8")
            rep_miss = eval_mod.evaluate_corpus(
                manifest_path=m_miss,
                annotations_path=self.annotations_path,
                repo_root=self.repo_root,
                corpus_dir=None,
                eval_dump_bin=self.eval_dump_bin,
                pdfinfo_bin=self.pdfinfo_bin,
                pdftotext_bin=self.pdftotext_bin,
                repeat_check=False,
            )
            rc_miss_cases = [c for c in rep_miss["cases"] if c["document_id"] == "doc.synthetic.rc"]
            for c in rc_miss_cases:
                self.assertEqual(c["status"], "missing_local_pdf")
            self.assertGreater(
                rep_miss["section_d_reproducibility_and_diagnostics"]["error_taxonomy_counts"][
                    "rights_or_document_unavailable"
                ],
                0,
            )

    def test_empty_ground_truth_and_spurious_observations(self) -> None:
        manifest = json.loads(self.manifest_path.read_text(encoding="utf-8"))
        annotations = json.loads(self.annotations_path.read_text(encoding="utf-8"))

        # Empty ground truth for SYN-CASE-01-TABLE-R causes all 9 extracted candidates to be FP (spurious)
        empty_ann = copy.deepcopy(annotations)
        empty_ann["cases"] = [
            c for c in empty_ann["cases"] if c["case_id"] == "SYN-CASE-01-TABLE-R"
        ]
        empty_ann["cases"][0]["observations"] = []

        single_manifest = copy.deepcopy(manifest)
        single_manifest["cases"] = [
            c for c in single_manifest["cases"] if c["case_id"] == "SYN-CASE-01-TABLE-R"
        ]

        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            m_path = tmp / "m.json"
            a_path = tmp / "a.json"
            m_path.write_text(json.dumps(single_manifest), encoding="utf-8")
            a_path.write_text(json.dumps(empty_ann), encoding="utf-8")

            rep = eval_mod.evaluate_corpus(
                manifest_path=m_path,
                annotations_path=a_path,
                repo_root=self.repo_root,
                corpus_dir=None,
                eval_dump_bin=self.eval_dump_bin,
                pdfinfo_bin=self.pdfinfo_bin,
                pdftotext_bin=self.pdftotext_bin,
                repeat_check=False,
            )
            sec_b = rep["section_b_supported_extraction_quality"]
            self.assertEqual(sec_b["tp"], 0)
            self.assertEqual(sec_b["fp"], 9)
            self.assertEqual(sec_b["fn"], 0)
            self.assertEqual(sec_b["precision"], 0.0)
            self.assertIsNone(sec_b["recall"])
            self.assertEqual(
                rep["section_d_reproducibility_and_diagnostics"]["error_taxonomy_counts"][
                    "spurious_observation"
                ],
                9,
            )

    def test_malformed_manifest_and_invalid_iaa_claim_return_nonzero(self) -> None:
        annotations = json.loads(self.annotations_path.read_text(encoding="utf-8"))
        bad_ann = copy.deepcopy(annotations)
        bad_ann["annotator_count"] = 1
        bad_ann["inter_annotator_agreement_claimed"] = True

        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            bad_ann_path = tmp / "bad_ann.json"
            bad_ann_path.write_text(json.dumps(bad_ann), encoding="utf-8")
            rc = eval_mod.main(
                [
                    "--manifest",
                    str(self.manifest_path),
                    "--annotations",
                    str(bad_ann_path),
                    "--output-json",
                    str(tmp / "out.json"),
                    "--output-md",
                    str(tmp / "out.md"),
                ]
            )
            self.assertEqual(rc, 2)

        # Also validate that the real-corpus manifest adheres strictly to schema
        real_manifest = json.loads(self.real_manifest_path.read_text(encoding="utf-8"))
        eval_mod.validate_corpus_manifest(real_manifest)
        real_p4f_path = self.repo_root / "evaluation" / "real_corpus_manifest_phase4f.json"
        if real_p4f_path.is_file():
            real_p4f = json.loads(real_p4f_path.read_text(encoding="utf-8"))
            eval_mod.validate_corpus_manifest(real_p4f)
        real_p4g_path = self.repo_root / "evaluation" / "real_corpus_manifest_phase4g.json"
        if real_p4g_path.is_file():
            real_p4g = json.loads(real_p4g_path.read_text(encoding="utf-8"))
            eval_mod.validate_corpus_manifest(real_p4g)

    def test_phase4f_header_unit_and_condition_bindings(self) -> None:
        hb_manifest = self.repo_root / "tests" / "evaluation" / "synthetic_header_binding_manifest.json"
        hb_annotations = self.repo_root / "tests" / "evaluation" / "synthetic_header_binding_ground_truth.json"
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            json1 = tmp / "hb1.json"
            md1 = tmp / "hb1.md"
            rc = eval_mod.main(
                [
                    "--manifest",
                    str(hb_manifest),
                    "--annotations",
                    str(hb_annotations),
                    "--repo-root",
                    str(self.repo_root),
                    "--eval-dump-bin",
                    str(self.eval_dump_bin),
                    "--pdfinfo",
                    str(self.pdfinfo_bin),
                    "--pdftotext",
                    str(self.pdftotext_bin),
                    "--output-json",
                    str(json1),
                    "--output-md",
                    str(md1),
                ]
            )
            self.assertEqual(rc, 0)
            report = json.loads(json1.read_text(encoding="utf-8"))
            sec_b = report["section_b_supported_extraction_quality"]
            self.assertEqual(sec_b["tp"], 13)
            self.assertEqual(sec_b["fp"], 0)
            self.assertEqual(sec_b["fn"], 0)
            self.assertAlmostEqual(sec_b["precision"], 1.0)
            self.assertAlmostEqual(sec_b["recall"], 1.0)
            self.assertAlmostEqual(sec_b["f1"], 1.0)
            for field_name, stats in sec_b["field_level_agreement"].items():
                self.assertEqual(stats["matched"], 13, f"Field {field_name} mismatch")
                self.assertEqual(stats["total"], 13)

            sec_c = report["section_c_fail_closed_behavior"]
            self.assertEqual(sec_c["expected_fail_closed_cases"], 2)
            self.assertEqual(sec_c["actual_fail_closed_cases"], 2)
            self.assertEqual(sec_c["false_accept_cases"], 0)
            self.assertEqual(sec_c["diagnostic_matched_fail_closed_cases"], 2)

            sec_d = report["section_d_reproducibility_and_diagnostics"]
            self.assertEqual(sec_d["repeat_runs_verified"], 5)
            self.assertEqual(sec_d["repeat_run_mismatches"], 0)
            self.assertTrue(sec_d["candidate_only_verified"])

    def test_phase4g_inline_value_unit_parsing(self) -> None:
        iu_manifest = self.repo_root / "tests" / "evaluation" / "synthetic_inline_unit_manifest.json"
        iu_annotations = self.repo_root / "tests" / "evaluation" / "synthetic_inline_unit_ground_truth.json"
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp = Path(tmpdir)
            json1 = tmp / "iu1.json"
            md1 = tmp / "iu1.md"
            rc = eval_mod.main(
                [
                    "--manifest",
                    str(iu_manifest),
                    "--annotations",
                    str(iu_annotations),
                    "--repo-root",
                    str(self.repo_root),
                    "--eval-dump-bin",
                    str(self.eval_dump_bin),
                    "--pdfinfo",
                    str(self.pdfinfo_bin),
                    "--pdftotext",
                    str(self.pdftotext_bin),
                    "--output-json",
                    str(json1),
                    "--output-md",
                    str(md1),
                ]
            )
            self.assertEqual(rc, 0)
            report = json.loads(json1.read_text(encoding="utf-8"))
            sec_b = report["section_b_supported_extraction_quality"]
            self.assertEqual(sec_b["tp"], 21)
            self.assertEqual(sec_b["fp"], 0)
            self.assertEqual(sec_b["fn"], 0)
            self.assertAlmostEqual(sec_b["precision"], 1.0)
            self.assertAlmostEqual(sec_b["recall"], 1.0)
            self.assertAlmostEqual(sec_b["f1"], 1.0)
            for field_name, stats in sec_b["field_level_agreement"].items():
                self.assertEqual(stats["matched"], 21, f"Field {field_name} mismatch")
                self.assertEqual(stats["total"], 21)

            sec_c = report["section_c_fail_closed_behavior"]
            self.assertEqual(sec_c["expected_fail_closed_cases"], 4)
            self.assertEqual(sec_c["actual_fail_closed_cases"], 4)
            self.assertEqual(sec_c["false_accept_cases"], 0)
            self.assertEqual(sec_c["diagnostic_matched_fail_closed_cases"], 4)

            sec_d = report["section_d_reproducibility_and_diagnostics"]
            self.assertEqual(sec_d["repeat_runs_verified"], 8)
            self.assertEqual(sec_d["repeat_run_mismatches"], 0)
            self.assertTrue(sec_d["candidate_only_verified"])


    def test_phase4h_context_and_provenance(self) -> None:
        import subprocess
        manifest_path = self.repo_root / "tests/evaluation/synthetic_hierarchy_manifest.json"
        annotations_path = self.repo_root / "tests/evaluation/synthetic_hierarchy_ground_truth.json"
        report = eval_mod.evaluate_corpus(
            manifest_path=manifest_path, annotations_path=annotations_path, repo_root=self.repo_root,
            corpus_dir=None, eval_dump_bin=self.eval_dump_bin, pdfinfo_bin=self.pdfinfo_bin,
            pdftotext_bin=self.pdftotext_bin, repeat_check=True)
        quality = report["section_b_supported_extraction_quality"]
        self.assertEqual((quality["tp"], quality["fp"], quality["fn"]), (6, 0, 0))
        self.assertEqual(quality["field_level_agreement"]["evidence_location"]["matched"], 6)
        self.assertEqual(report["section_d_reproducibility_and_diagnostics"]["repeat_run_mismatches"], 0)
        m = json.loads(manifest_path.read_text())
        case, doc = m["cases"][0], m["documents"][0]
        cmd = eval_mod.build_dump_command(self.eval_dump_bin, self.pdfinfo_bin, self.pdftotext_bin,
            self.repo_root / doc["repo_relative_path"], doc, case)
        dump = json.loads(subprocess.run(cmd, check=True, capture_output=True, encoding="utf-8").stdout)
        obs = dump["observations"][0]
        self.assertTrue(eval_mod.context_provenance_matches(dump, obs, case, 2))
        p = dump["parameter_context_provenance"][0]
        self.assertEqual(p["raw_child_row_text"], "short V 12")
        self.assertGreater(p["child_row_box"][1], 140)
        self.assertLess(p["child_row_box"][3], 155)
        for field, value in [("parent_evidence_id", "missing"), ("child_evidence_id", "missing"),
                             ("raw_parent_text", "altered"), ("raw_child_parameter_text", "altered"),
                             ("parent_box", [0, 0, 1, 1]), ("child_row_box", [0, 0, 0, 0])]:
            corrupted = copy.deepcopy(dump)
            corrupted["parameter_context_provenance"][0][field] = value
            self.assertFalse(eval_mod.context_provenance_matches(corrupted, obs, case, 2), field)
        corrupted = copy.deepcopy(dump)
        corrupted["parameter_context_provenance"].append(copy.deepcopy(p))
        self.assertFalse(eval_mod.context_provenance_matches(corrupted, obs, case, 2))
        for malformed in ("20,115,210,130|Pulse rating:|-1:short", "20,115,210,130|Pulse rating:|2x:short",
                          "20,115,210,130,7|Pulse rating:|2:short"):
            bad_cmd = cmd[:cmd.index("--parent-context")] + ["--parent-context", malformed]
            self.assertNotEqual(subprocess.run(bad_cmd, capture_output=True).returncode, 0)


    def test_phase4i_baseline_provenance(self) -> None:
        import subprocess
        manifest_path = self.repo_root / "tests/evaluation/synthetic_baseline_manifest.json"
        annotations_path = self.repo_root / "tests/evaluation/synthetic_baseline_ground_truth.json"
        report = eval_mod.evaluate_corpus(
            manifest_path=manifest_path, annotations_path=annotations_path, repo_root=self.repo_root,
            corpus_dir=None, eval_dump_bin=self.eval_dump_bin, pdfinfo_bin=self.pdfinfo_bin,
            pdftotext_bin=self.pdftotext_bin, repeat_check=True)
        quality = report["section_b_supported_extraction_quality"]
        self.assertEqual((quality["tp"], quality["fp"], quality["fn"]), (4, 0, 0))
        self.assertEqual(quality["field_level_agreement"]["evidence_location"]["matched"], 4)
        self.assertEqual(report["section_d_reproducibility_and_diagnostics"]["repeat_run_mismatches"], 0)
        m = json.loads(manifest_path.read_text())
        case, doc = m["cases"][0], m["documents"][0]
        cmd = eval_mod.build_dump_command(self.eval_dump_bin, self.pdfinfo_bin, self.pdftotext_bin,
            self.repo_root / doc["repo_relative_path"], doc, case)
        dump = json.loads(subprocess.run(cmd, check=True, capture_output=True, encoding="utf-8").stdout)
        self.assertTrue(eval_mod.baseline_provenance_valid(dump, case))
        p = dump["baseline_text_provenance"][0]
        self.assertEqual(p["display_expression"], "P_{70}")
        self.assertEqual(p["source_text"], "P 70")
        self.assertEqual(p["host_box"], [25, 116.384, 33.004, 127.484])
        self.assertEqual(p["scripts"][0]["source_box"], [34, 125.974, 41.784, 132.449])
        for field, value in [("evidence_id", "missing"), ("raw_host_text", "altered"),
                             ("host_box", [0, 0, 1, 1]), ("scripts", []),
                             ("source_text", "P70"), ("display_expression", "P^{70}")]:
            corrupted = copy.deepcopy(dump)
            corrupted["baseline_text_provenance"][0][field] = value
            self.assertFalse(eval_mod.baseline_provenance_valid(corrupted, case), field)
        corrupted = copy.deepcopy(dump)
        corrupted["baseline_text_provenance"] = []
        self.assertFalse(eval_mod.baseline_provenance_valid(corrupted, case))
        corrupted = copy.deepcopy(dump)
        corrupted["baseline_text_provenance"].append(copy.deepcopy(p))
        self.assertFalse(eval_mod.baseline_provenance_valid(corrupted, case))
        for malformed in ("24,116,34,128|P|above|34,125,42,133|70",
                          "24x,116,34,128|P|sub|34,125,42,133|70",
                          "24,116,34,128|P|sub|34,125,42,133|"):
            bad_cmd = cmd[:cmd.index("--baseline-text")] + ["--baseline-text", malformed]
            self.assertNotEqual(subprocess.run(bad_cmd, capture_output=True).returncode, 0)


if __name__ == "__main__":
    unittest.main()

