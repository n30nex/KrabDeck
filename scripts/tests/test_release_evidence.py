import json
import subprocess
import sys
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
PYTHON = sys.executable
sys.path.insert(0, str(ROOT / "scripts"))
import verify_release_evidence as release_evidence  # noqa: E402
REQUIRED_ARTIFACTS = {
    "firmware.bin",
    "firmware-merged.bin",
    "krabos-candidate.bin",
    "krabos-recovery-rf-off.bin",
    "KrabOS-tdeck-plus-launcher.bin",
    "firmware-debug.bin",
    "manifest.json",
    "build-metadata.json",
    "krabos-tdeck-plus-bootloader.bin",
    "krabos-tdeck-plus-partitions.bin",
    "krabos-tdeck-plus-boot_app0.bin",
    "krabos-tdeck-plus-firmware.bin",
    "krabos-tdeck-plus-full.bin",
    "krabos-tdeck-plus-launcher.bin",
}


class ReleaseEvidenceTests(unittest.TestCase):
    def run_script(self, name, *args):
        return subprocess.run(
            [PYTHON, str(ROOT / "scripts" / name), *map(str, args)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )

    def test_golden_frames_match_pinned_stock_source(self):
        result = self.run_script("verify_companion_golden_frames.py", "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["frames"], 19)
        self.assertEqual(
            report["protocol_codes_sha256"],
            "37ed4b4636de7d9535c91b3f3087750d6f788699de180783998f5a35105c22c1",
        )
        self.assertEqual(
            report["reviewed_upstream"],
            "a3a1aa5e3be34b42d8ac8c2cc244d30af6cdd71e",
        )

    def test_candidate_meshcore_revision_is_compared_by_git_object(self):
        result = self.run_script(
            "verify_companion_golden_frames.py",
            "--candidate-ref", "HEAD", "--json",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["candidate"], report["submodule"])
        self.assertEqual(
            report["protocol_codes_sha256"],
            "37ed4b4636de7d9535c91b3f3087750d6f788699de180783998f5a35105c22c1",
        )

    def test_protocol_code_drift_blocks_golden_validation(self):
        corpus = json.loads(
            (ROOT / "test/fixtures/companion_golden_frames.json").read_text()
        )
        corpus["source"]["protocol_codes_sha256"] = "0" * 64
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "drifted-corpus.json"
            path.write_text(json.dumps(corpus))
            result = self.run_script(
                "verify_companion_golden_frames.py", "--corpus", path,
            )
        self.assertEqual(result.returncode, 1)
        self.assertIn(
            "pinned companion protocol code snapshot changed", result.stderr
        )

    def test_release_template_covers_machine_readable_requirements(self):
        result = self.run_script("verify_release_evidence.py")
        self.assertEqual(result.returncode, 0, result.stderr)

    def make_completed_evidence(self, commit, tag):
        requirements = release_evidence.load_requirements()
        tested_at = datetime.now(timezone.utc).date().isoformat()
        records = []
        for requirement in requirements:
            record = {
                "id": requirement["id"],
                "outcome": "pass",
                "evidence_url": f"https://example.invalid/evidence/{requirement['id']}",
                "tested_at": tested_at,
                "firmware_version": tag,
            }
            if requirement["peer_required"]:
                record["peer_version"] = "test-peer-1"
            records.append(record)
        return {
            "schema_version": 1,
            "commit": commit,
            "tag": tag,
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "artifacts": {
                name: f"{index:064x}" for index, name in enumerate(
                    sorted(REQUIRED_ARTIFACTS), start=1
                )
            },
            "requirements": records,
        }

    def make_exact_evidence(
        self, commit, tag, production_digest, artifact_sha256s=None
    ):
        artifact_sha256s = artifact_sha256s or {
            "production": production_digest,
            "recovery": "c" * 64,
            "debug": "d" * 64,
            "ota": "e" * 64,
        }
        evidence = self.make_completed_evidence(commit, tag)
        evidence.pop("commit")
        evidence.pop("artifacts")
        evidence.update({
            "schema_version": 3,
            "kind": "krabos-exact-release-evidence-input",
            "candidate_commit": commit,
            "production_image_sha256": production_digest,
            "artifact_sha256s": artifact_sha256s,
        })
        requirements = {item["id"]: item for item in release_evidence.load_requirements()}
        for record in evidence["requirements"]:
            requirement = requirements[record["id"]]
            record.pop("evidence_url")
            record.update(
                {
                    "evidence_class": requirement["evidence_classes"][0],
                    "evidence_bundle_url": (
                        f"https://example.invalid/evidence/{requirement['id']}.json"
                    ),
                    "evidence_bundle_sha256": "f" * 64,
                    "artifacts": [
                        {"role": role, "sha256": artifact_sha256s[role]}
                        for role in requirement["artifact_roles"]
                    ],
                    "claims": requirement["required_claims"],
                    "metrics": {
                        name: limits["min"] if "min" in limits else limits["max"]
                        for name, limits in requirement["metric_bounds"].items()
                    },
                }
            )
            record["candidate_commit"] = commit
            record["production_image_sha256"] = production_digest
        return evidence

    def run_evidence(self, evidence, commit="a" * 40, tag="beta-test"):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "evidence.json"
            path.write_text(json.dumps(evidence))
            return self.run_script(
                "verify_release_evidence.py",
                "--evidence", path,
                "--commit", commit,
                "--tag", tag,
            )

    def verify_exact(self, evidence, commit="a" * 40, tag="v1.0.0"):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "evidence.json"
            path.write_text(json.dumps(evidence), encoding="utf-8")
            return release_evidence.verify_evidence(
                path,
                tag,
                expected_commit=commit,
                expected_production_image_sha256=evidence[
                    "production_image_sha256"
                ],
                require_exact=True,
            )

    def test_completed_evidence_is_bound_to_tag_and_commit(self):
        commit = "a" * 40
        tag = "beta-test"
        result = self.run_evidence(self.make_completed_evidence(commit, tag), commit, tag)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("completed release evidence OK", result.stdout)

    def test_incomplete_or_failed_evidence_blocks_release(self):
        commit = "a" * 40
        evidence = self.make_completed_evidence(commit, "beta-test")
        evidence["requirements"].pop()
        result = self.run_evidence(evidence, commit)
        self.assertEqual(result.returncode, 1)
        self.assertIn("missing requirement evidence", result.stderr)

        evidence = self.make_completed_evidence(commit, "beta-test")
        evidence["requirements"][0]["outcome"] = "fail"
        result = self.run_evidence(evidence, commit)
        self.assertEqual(result.returncode, 1)
        self.assertIn("outcome must be pass", result.stderr)

    def test_mismatched_commit_and_missing_hashes_block_release(self):
        evidence = self.make_completed_evidence("b" * 40, "beta-test")
        result = self.run_evidence(evidence)
        self.assertEqual(result.returncode, 1)
        self.assertIn("tagged commit", result.stderr)

        evidence = self.make_completed_evidence("a" * 40, "beta-test")
        del evidence["artifacts"]["manifest.json"]
        result = self.run_evidence(evidence)
        self.assertEqual(result.returncode, 1)
        self.assertIn("missing artifact hashes", result.stderr)

    def test_exact_evidence_binds_every_record_to_commit_and_production_bytes(self):
        commit = "a" * 40
        tag = "v1.0.0"
        production_digest = "b" * 64
        evidence = self.make_exact_evidence(commit, tag, production_digest)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "release-evidence-input.json"
            path.write_text(json.dumps(evidence), encoding="utf-8")
            with mock.patch.object(
                release_evidence,
                "_production_hashes",
                return_value=(
                    {
                        filename: evidence["artifact_sha256s"][role]
                        for role, filename in release_evidence.ARTIFACT_ROLE_FILES.items()
                    },
                    production_digest,
                ),
            ):
                count, _ = release_evidence.verify_evidence(
                    path,
                    tag,
                    expected_commit=commit,
                    artifacts_dir=Path(directory),
                )
        self.assertGreater(count, 0)

        evidence["requirements"][0]["candidate_commit"] = "c" * 40
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "release-evidence-input.json"
            path.write_text(json.dumps(evidence), encoding="utf-8")
            with mock.patch.object(
                release_evidence,
                "_production_hashes",
                return_value=(
                    {
                        filename: evidence["artifact_sha256s"][role]
                        for role, filename in release_evidence.ARTIFACT_ROLE_FILES.items()
                    },
                    production_digest,
                ),
            ):
                with self.assertRaisesRegex(ValueError, "candidate_commit"):
                    release_evidence.verify_evidence(
                        path,
                        tag,
                        expected_commit=commit,
                        artifacts_dir=Path(directory),
                    )

    def test_typed_exact_records_reject_free_text_mutable_links_and_wrong_class(self):
        evidence = self.make_exact_evidence("a" * 40, "v1.0.0", "b" * 64)
        self.assertGreater(self.verify_exact(evidence)[0], 0)
        rf_index = next(
            index
            for index, record in enumerate(evidence["requirements"])
            if record["id"] == "RF-END-TO-END"
        )
        mutations = []
        extra = json.loads(json.dumps(evidence))
        extra["requirements"][0]["notes"] = "looks good"
        mutations.append((extra, "fields"))
        mutable_url = json.loads(json.dumps(evidence))
        mutable_url["requirements"][0]["evidence_bundle_url"] += "?token=secret"
        mutations.append((mutable_url, "query"))
        wrong_class = json.loads(json.dumps(evidence))
        wrong_class["requirements"][rf_index]["evidence_class"] = "manual-review"
        mutations.append((wrong_class, "evidence_class"))
        wrong_role = json.loads(json.dumps(evidence))
        wrong_role["requirements"][rf_index]["artifacts"][1]["sha256"] = "0" * 64
        mutations.append((wrong_role, "artifact role digest"))
        bool_confusion = json.loads(json.dumps(evidence))
        bool_confusion["requirements"][rf_index]["claims"][
            "observer_liveness_verified"
        ] = 1
        mutations.append((bool_confusion, "claims"))
        for mutated, message in mutations:
            with self.subTest(message=message), self.assertRaisesRegex(
                ValueError, message
            ):
                self.verify_exact(mutated)

    def test_active_soak_uses_two_hour_and_authoritative_heap_boundaries(self):
        evidence = self.make_exact_evidence("a" * 40, "v1.0.0", "b" * 64)
        record = next(
            item for item in evidence["requirements"] if item["id"] == "SOAK-ACTIVE"
        )
        record["metrics"].update(
            {
                "duration_seconds": 7200,
                "completion_percent": 90,
                "heap_range_bytes": 999,
            }
        )
        self.assertGreater(self.verify_exact(evidence)[0], 0)
        for metric, value, message in (
            ("duration_seconds", 7199, "below its minimum"),
            ("heap_range_bytes", 1000, "exceeds its maximum"),
            ("completion_percent", 89, "below its minimum"),
        ):
            mutated = json.loads(json.dumps(evidence))
            target = next(
                item
                for item in mutated["requirements"]
                if item["id"] == "SOAK-ACTIVE"
            )
            target["metrics"][metric] = value
            with self.subTest(metric=metric), self.assertRaisesRegex(
                ValueError, message
            ):
                self.verify_exact(mutated)

    def test_rf_record_is_independent_and_binds_production_and_recovery(self):
        evidence = self.make_exact_evidence("a" * 40, "v1.0.0", "b" * 64)
        record = next(
            item for item in evidence["requirements"] if item["id"] == "RF-END-TO-END"
        )
        self.assertEqual(record["evidence_class"], "independent-observer")
        self.assertEqual(
            [item["role"] for item in record["artifacts"]],
            ["production", "recovery"],
        )
        mutated = json.loads(json.dumps(evidence))
        target = next(
            item for item in mutated["requirements"] if item["id"] == "RF-END-TO-END"
        )
        target["claims"]["observer_liveness_verified"] = False
        with self.assertRaisesRegex(ValueError, "claims"):
            self.verify_exact(mutated)

    def test_numeric_artifact_digest_is_not_coerced_to_sha256(self):
        with self.assertRaisesRegex(ValueError, "artifact digest"):
            release_evidence._declared_hashes(
                {"artifacts": {"firmware.bin": int("1" * 64)}}
            )

    def test_version_only_schema_two_evidence_is_rejected(self):
        evidence = {
            "schema_version": 2,
            "tag": "v1.0.0",
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "requirements": [],
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "evidence.json"
            path.write_text(json.dumps(evidence), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "version-only evidence"):
                release_evidence.verify_evidence(path, "v1.0.0")

    def test_stable_exact_mode_rejects_legacy_schema_one(self):
        commit = "a" * 40
        evidence = self.make_completed_evidence(commit, "v1.0.0")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "evidence.json"
            path.write_text(json.dumps(evidence), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "requires schema 3 exact evidence"):
                release_evidence.verify_evidence(
                    path,
                    "v1.0.0",
                    expected_commit=commit,
                    artifacts_dir=Path(directory),
                    require_exact=True,
                )

    def test_stable_exact_mode_rejects_legacy_attestation(self):
        attestation = {
            "schema_version": 2,
            "kind": "sigurdos-release-attestation",
            "commit": "a" * 40,
            "tag": "v1.0.0",
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "release-evidence.json"
            path.write_text(json.dumps(attestation), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "requires schema 3 exact attestation"):
                release_evidence.verify_attestation(
                    path,
                    "a" * 40,
                    "v1.0.0",
                    Path(directory),
                    require_exact=True,
                )

    def test_exact_evidence_requires_built_artifacts(self):
        commit = "a" * 40
        evidence = self.make_exact_evidence(commit, "v1.0.0", "b" * 64)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "evidence.json"
            path.write_text(json.dumps(evidence), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "exact production image"):
                release_evidence.verify_evidence(
                    path, "v1.0.0", expected_commit=commit
                )

    def test_github_source_metadata_is_exact_and_completed(self):
        commit = "a" * 40
        now = datetime.now(timezone.utc)
        created = (now - timedelta(minutes=2)).isoformat()
        updated = (now - timedelta(minutes=1)).isoformat()
        source = release_evidence.evidence_source_reference(
            "n30nex/KrabDeck", "123", "456", "sha256:" + "d" * 64, commit
        )
        artifact = {
            "id": 456,
            "name": f"krabos-v1-evidence-{commit}",
            "digest": "sha256:" + "d" * 64,
            "expired": False,
            "created_at": created,
            "updated_at": created,
            "expires_at": (now + timedelta(days=30)).isoformat(),
            "workflow_run": {
                "id": 123,
                "head_sha": commit,
                "head_branch": "main",
            },
        }
        run = {
            "id": 123,
            "head_sha": commit,
            "head_branch": "main",
            "head_repository": {"full_name": "n30nex/KrabDeck"},
            "path": ".github/workflows/krabos-evidence.yml",
            "event": "workflow_dispatch",
            "status": "completed",
            "conclusion": "success",
            "pull_requests": [],
            "created_at": (now - timedelta(minutes=3)).isoformat(),
            "updated_at": updated,
            "repository": {"full_name": "n30nex/KrabDeck"},
        }
        with tempfile.TemporaryDirectory() as directory:
            artifact_path = Path(directory) / "artifact.json"
            run_path = Path(directory) / "run.json"
            artifact_path.write_text(json.dumps(artifact), encoding="utf-8")
            run_path.write_text(json.dumps(run), encoding="utf-8")
            release_evidence.verify_github_source_metadata(
                artifact_path, run_path, source, commit
            )
            artifact["digest"] = "sha256:" + "e" * 64
            artifact_path.write_text(json.dumps(artifact), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "digest"):
                release_evidence.verify_github_source_metadata(
                    artifact_path, run_path, source, commit
                )

            artifact["digest"] = "sha256:" + "d" * 64
            artifact["created_at"] = (now - timedelta(days=31)).isoformat()
            artifact["updated_at"] = artifact["created_at"]
            artifact_path.write_text(json.dumps(artifact), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "freshness window"):
                release_evidence.verify_github_source_metadata(
                    artifact_path, run_path, source, commit
                )

    def test_github_source_metadata_rejects_wrong_workflow(self):
        commit = "a" * 40
        now = datetime.now(timezone.utc)
        source = release_evidence.evidence_source_reference(
            "n30nex/KrabDeck", 123, 456, "sha256:" + "d" * 64, commit
        )
        artifact = {
            "id": 456,
            "name": f"krabos-v1-evidence-{commit}",
            "digest": "sha256:" + "d" * 64,
            "expired": False,
            "created_at": (now - timedelta(minutes=2)).isoformat(),
            "updated_at": (now - timedelta(minutes=2)).isoformat(),
            "expires_at": (now + timedelta(days=30)).isoformat(),
            "workflow_run": {
                "id": 123,
                "head_sha": commit,
                "head_branch": "main",
            },
        }
        run = {
            "id": 123,
            "head_sha": commit,
            "head_branch": "main",
            "head_repository": {"full_name": "n30nex/KrabDeck"},
            "path": ".github/workflows/untrusted.yml",
            "event": "workflow_dispatch",
            "status": "completed",
            "conclusion": "success",
            "pull_requests": [],
            "created_at": (now - timedelta(minutes=3)).isoformat(),
            "updated_at": (now - timedelta(minutes=1)).isoformat(),
            "repository": {"full_name": "n30nex/KrabDeck"},
        }
        with tempfile.TemporaryDirectory() as directory:
            artifact_path = Path(directory) / "artifact.json"
            run_path = Path(directory) / "run.json"
            artifact_path.write_text(json.dumps(artifact), encoding="utf-8")
            run_path.write_text(json.dumps(run), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "untrusted workflow"):
                release_evidence.verify_github_source_metadata(
                    artifact_path, run_path, source, commit
                )

    def test_exact_attestation_retains_immutable_source_identity(self):
        commit = "a" * 40
        tag = "v1.0.0"
        production_digest = "b" * 64
        artifact_hashes = {
            name: (production_digest if name == "firmware-merged.bin" else "c" * 64)
            for name in REQUIRED_ARTIFACTS
        }
        role_hashes = {
            role: artifact_hashes[filename]
            for role, filename in release_evidence.ARTIFACT_ROLE_FILES.items()
        }
        evidence = self.make_exact_evidence(
            commit, tag, production_digest, role_hashes
        )
        source = release_evidence.evidence_source_reference(
            "n30nex/KrabDeck", 123, 456, "sha256:" + "d" * 64, commit
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            evidence_path = root / "release-evidence-input.json"
            attestation_path = root / "release-evidence.json"
            evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
            with mock.patch.object(
                release_evidence,
                "_production_hashes",
                return_value=(artifact_hashes, production_digest),
            ):
                release_evidence.write_attestation(
                    attestation_path,
                    evidence_path,
                    evidence,
                    commit,
                    tag,
                    root,
                    source,
                )
                count = release_evidence.verify_attestation(
                    attestation_path,
                    commit,
                    tag,
                    root,
                    expected_source_reference=source,
                )
                rf_record, admitted_source_digest = (
                    release_evidence.verify_attested_requirement(
                        attestation_path,
                        "RF-END-TO-END",
                        expected_tag=tag,
                        expected_commit=commit,
                        expected_artifact_sha256s={
                            "production": role_hashes["production"],
                            "recovery": role_hashes["recovery"],
                        },
                        expected_source_reference=source,
                    )
                )
                wrong_source = dict(source)
                wrong_source["run_id"] = 999
                with self.assertRaisesRegex(ValueError, "was not admitted"):
                    release_evidence.verify_attested_requirement(
                        attestation_path,
                        "RF-END-TO-END",
                        expected_tag=tag,
                        expected_commit=commit,
                        expected_artifact_sha256s={
                            "production": role_hashes["production"],
                            "recovery": role_hashes["recovery"],
                        },
                        expected_source_reference=wrong_source,
                    )
            attestation = json.loads(attestation_path.read_text(encoding="utf-8"))
        self.assertGreater(count, 0)
        self.assertEqual(rf_record["evidence_class"], "independent-observer")
        self.assertEqual(
            admitted_source_digest, attestation["source_evidence_sha256"]
        )
        self.assertEqual(attestation["source_artifact"], source)
        self.assertEqual(attestation["production_image_sha256"], production_digest)

    def test_soak_report_is_numeric_and_privacy_safe(self):
        with tempfile.TemporaryDirectory() as directory:
            json_out = Path(directory) / "idle.json"
            md_out = Path(directory) / "idle.md"
            result = self.run_script(
                "analyze_soak_log.py", ROOT / "test/fixtures/soak/idle.log",
                "--scenario", "idle", "--json-out", json_out,
                "--markdown-out", md_out, "--min-samples", "4",
                "--min-duration", "15",
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads(json_out.read_text())
            self.assertTrue(report["passed"])
            self.assertEqual(report["sample_count"], 4)
            self.assertEqual(report["heap_bytes"]["range"], 300)
            self.assertNotIn("boot", json_out.read_text().lower())

    def test_soak_threshold_failure_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.run_script(
                "analyze_soak_log.py", ROOT / "test/fixtures/soak/active.log",
                "--scenario", "active", "--json-out", Path(directory) / "active.json",
                "--markdown-out", Path(directory) / "active.md", "--min-samples", "4",
                "--min-duration", "15",
                "--max-heap-range", "1000",
            )
            self.assertEqual(result.returncode, 1)

    def test_warning_budget_accepts_clean_log_and_rejects_delta(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "build.log"
            log.write_text(
                "lib/vendor.cpp:1: warning: ignored [-Wunused-variable]\n"
            )
            result = self.run_script("check_first_party_warnings.py", "--log", log)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            log.write_text(log.read_text() +
                           "src/new.cpp:9:2: warning: new debt [-Wunused-variable]\n")
            result = self.run_script("check_first_party_warnings.py", "--log", log)
            self.assertEqual(result.returncode, 1)
            self.assertIn("src/new.cpp", result.stdout)

    def test_stale_warning_budget_is_informational(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            log = directory / "build.log"
            baseline = directory / "warnings.json"
            log.write_text("")
            baseline.write_text(json.dumps({
                "schema_version": 1,
                "environment": "SigurdOS_TDeck",
                "budgets": {"src/old.cpp|-Wunused-variable": 1},
            }))
            result = self.run_script(
                "check_first_party_warnings.py",
                "--log", log,
                "--baseline", baseline,
            )
            self.assertEqual(result.returncode, 0)
            self.assertIn("Stale warning budgets", result.stdout)
            self.assertIn("src/old.cpp", result.stdout)

    def test_unclassified_first_party_warning_is_not_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "build.log"
            log.write_text("src/new.cpp:9: warning: compiler warning without an option\n")
            result = self.run_script("check_first_party_warnings.py", "--log", log)
            self.assertEqual(result.returncode, 1)
            self.assertIn("unclassified", result.stdout)


if __name__ == "__main__":
    unittest.main()
