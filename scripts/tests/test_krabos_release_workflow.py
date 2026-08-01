"""Fail-closed contracts for the KrabOS exact-device release workflow."""

from __future__ import annotations

import re
import unittest
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"
ACTION_REF = re.compile(r"uses:\s+[^.\s][^@\s]+@([^\s]+)")


class KrabosReleaseWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.text = (WORKFLOWS / "krabos-edge.yml").read_text(encoding="utf-8")
        cls.workflow = yaml.safe_load(cls.text)
        cls.pi5 = cls.workflow["jobs"]["pi5"]
        cls.steps = cls.pi5["steps"]
        cls.step_by_name = {step["name"]: step for step in cls.steps}

    def test_workflow_is_manual_only_and_uses_main_definition(self) -> None:
        triggers = self.workflow["on"]
        self.assertNotIn("push", triggers)
        self.assertNotIn("pull_request", triggers)
        self.assertIn("workflow_dispatch", triggers)
        job_if = self.pi5["if"]
        self.assertIn("github.repository == 'n30nex/KrabDeck'", job_if)
        self.assertIn("github.ref == 'refs/heads/main'", job_if)
        self.assertEqual(
            self.pi5["runs-on"],
            ["self-hosted", "Linux", "ARM64", "krabdeck-pi5"],
        )
        self.assertEqual(
            self.workflow["concurrency"]["group"],
            "meshcore-hardware-krabdeck-pi5",
        )

    def test_manual_stable_release_is_exactly_v1_and_sha_bound(self) -> None:
        inputs = self.workflow["on"]["workflow_dispatch"]["inputs"]
        self.assertEqual(
            inputs["release_channel"]["options"],
            ["validation", "edge", "stable-v1.0.0"],
        )
        self.assertTrue(inputs["candidate_sha"]["required"])
        request = self.step_by_name["Admit exact trusted candidate request"]["run"]
        self.assertIn('test "$candidate_branch" = "$GITHUB_REF_NAME"', request)
        self.assertIn('test "$candidate_sha" = "$GITHUB_SHA"', request)
        self.assertIn('test "$candidate_branch" = "main"', request)
        bind = self.step_by_name["Bind release identity before any build"]["run"]
        self.assertIn('tag="v1.0.0"', bind)
        self.assertIn("KRABOS_RELEASE_VERSION=$tag", bind)
        build_index = next(
            index
            for index, step in enumerate(self.steps)
            if step["name"].startswith("Build and stage exact")
        )
        bind_index = next(
            index
            for index, step in enumerate(self.steps)
            if step["name"] == "Bind release identity before any build"
        )
        self.assertLess(bind_index, build_index)

    def test_validation_channel_builds_exact_internal_branch_without_hardware(self) -> None:
        request = self.step_by_name["Admit exact trusted candidate request"]
        self.assertIn("validation)", request["run"])
        checkout = self.step_by_name["Check out exact candidate"]
        self.assertEqual(checkout["with"]["ref"], "${{ steps.request.outputs.candidate_sha }}")
        source_gate = self.step_by_name["Verify exact trusted source state"]["run"]
        self.assertIn("git diff --quiet refs/remotes/origin/main", source_gate)
        self.assertIn(".github/actions/cache-platformio", source_gate)
        hardware = self.step_by_name[
            "Run autonomous exact-device flash and recovery gate"
        ]
        self.assertIn("steps.request.outputs.release_mode == 'true'", hardware["if"])
        self.assertIn("steps.request.outputs.candidate_branch == 'main'", hardware["if"])
        validation_upload = self.step_by_name["Upload validation-only build artifacts"]
        self.assertEqual(
            validation_upload["if"], "steps.request.outputs.release_mode == 'false'"
        )

    def test_stable_release_has_evidence_priority_and_environment_gates(self) -> None:
        gate = self.step_by_name[
            "Enforce stable source evidence and open-priority gate"
        ]["run"]
        self.assertIn("scripts/verify_release_evidence.py", gate)
        self.assertIn("--paginate --slurp", gate)
        self.assertIn("p[01]", gate)
        postbuild = self.step_by_name[
            "Bind stable evidence to produced public bytes"
        ]["run"]
        self.assertIn("--write-attestation", postbuild)
        self.assertIn("--attestation", postbuild)

        approval = self.workflow["jobs"]["stable_approval"]
        self.assertEqual(approval["environment"]["name"], "krabos-v1-production")
        approval_step = approval["steps"][0]
        self.assertEqual(
            approval_step["env"]["APPROVED_VERSION"],
            "${{ vars.KRABOS_STABLE_RELEASE_VERSION }}",
        )

    def test_builds_genuine_production_recovery_and_debug_images(self) -> None:
        build = self.step_by_name[
            "Build and stage exact production, recovery, and debug images"
        ]["run"]
        for environment in (
            "KrabOS_TDeckPlus",
            "KrabOS_TDeckPlus_recovery",
            "KrabOS_TDeckPlus_debug",
        ):
            self.assertIn(f"pio run -j 2 -e {environment}", build)
        self.assertIn(
            ".pio/build/KrabOS_TDeckPlus_debug/firmware-merged.bin"
            ' "$KRABOS_ARTIFACTS/firmware-debug.bin"',
            build,
        )
        self.assertNotIn(
            "KrabOS_TDeckPlus_recovery/firmware-merged.bin"
            ' "$KRABOS_ARTIFACTS/firmware-debug.bin"',
            build,
        )

    def test_hardware_gate_uses_hardened_exact_device_commands(self) -> None:
        hardware = self.step_by_name[
            "Run autonomous exact-device flash and recovery gate"
        ]
        self.assertTrue(hardware["continue-on-error"])
        self.assertIn("exact_device_release.py release", hardware["run"])
        public = self.step_by_name[
            "Validate canonical redacted publication receipt"
        ]["run"]
        self.assertIn("exact_device_release.py check-public", public)
        failure = self.step_by_name[
            "Fail closed when exact hardware evidence is incomplete"
        ]
        self.assertIn("steps.hardware.outcome != 'success'", failure["if"])
        self.assertIn(
            "steps.public_gate.outputs.release_eligible != 'true'", failure["if"]
        )

    def test_publication_is_least_privilege_signed_and_restart_safe(self) -> None:
        publish = self.workflow["jobs"]["publish"]
        self.assertEqual(
            publish["permissions"],
            {"attestations": "write", "contents": "write", "id-token": "write"},
        )
        publish_text = "\n".join(
            str(step.get("run", "")) for step in publish["steps"]
        )
        self.assertIn("cosign sign-blob --yes", publish_text)
        self.assertIn("cosign verify-blob", publish_text)
        self.assertIn("gh attestation verify", publish_text)
        self.assertIn("--deny-self-hosted-runners", publish_text)
        self.assertIn("Existing immutable release asset differs", publish_text)
        self.assertIn("cmp --silent", publish_text)
        self.assertNotIn("--clobber", publish_text)
        self.assertIn("already_complete=true", publish_text)
        self.assertIn("github.ref == 'refs/heads/main'", publish["if"])
        self.assertIn("needs.pi5.outputs.candidate_branch == 'main'", publish["if"])

    def test_every_third_party_action_is_immutable(self) -> None:
        refs = ACTION_REF.findall(self.text)
        self.assertGreater(len(refs), 0)
        for ref in refs:
            self.assertRegex(ref, r"^[0-9a-f]{40}$")

    def test_legacy_tag_workflow_excludes_all_krabos_tags(self) -> None:
        legacy = yaml.safe_load(
            (WORKFLOWS / "build-release.yml").read_text(encoding="utf-8")
        )
        tags = legacy["on"]["push"]["tags"]
        self.assertIn("!edge-*", tags)
        self.assertIn("!v1.0.0", tags)

    def test_optional_krabos_bootstrap_builds_skip_only_when_env_is_absent(self) -> None:
        for name in ("pr-ci.yml", "security.yml"):
            text = (WORKFLOWS / name).read_text(encoding="utf-8")
            self.assertIn("grep -Fqx '[env:KrabOS_TDeckPlus]' platformio.ini", text)
        matrix = (WORKFLOWS / "build-validation-matrix.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("case '${{ matrix.env }}' in", matrix)
        self.assertIn("grep -Fqx '[env:${{ matrix.env }}]' platformio.ini", matrix)
        self.assertIn("KrabOS_*)", matrix)
        self.assertIn("Required PlatformIO environment is missing", matrix)

    def test_workflow_only_prs_do_not_queue_firmware_builds(self) -> None:
        ignored = {
            ".github/workflows/**",
            "scripts/tests/test_krabos_release_workflow.py",
            "scripts/tests/test_meshcore_pin.py",
            "scripts/tests/test_security_workflow.py",
            "scripts/tests/test_validation_matrix.py",
        }
        for name in ("pr-ci.yml", "build-validation-matrix.yml", "security.yml"):
            workflow = yaml.safe_load(
                (WORKFLOWS / name).read_text(encoding="utf-8")
            )
            self.assertEqual(
                set(workflow["on"]["pull_request"]["paths-ignore"]),
                ignored,
                f"{name} must skip the dedicated workflow-only PR",
            )

    def test_every_platformio_build_or_test_runs_only_on_pi5(self) -> None:
        pi5_runner = ["self-hosted", "Linux", "ARM64", "krabdeck-pi5"]
        command = re.compile(r"\bpio\s+(?:run|test|check)\b")
        for path in WORKFLOWS.glob("*.yml"):
            workflow = yaml.safe_load(path.read_text(encoding="utf-8"))
            for job_name, job in workflow.get("jobs", {}).items():
                runs = "\n".join(
                    str(step.get("run", "")) for step in job.get("steps", [])
                )
                if command.search(runs):
                    self.assertEqual(
                        job.get("runs-on"),
                        pi5_runner,
                        f"{path.name}:{job_name} can build outside the Pi 5",
                    )
        nightly = yaml.safe_load(
            (WORKFLOWS / "nightly-smoke.yml").read_text(encoding="utf-8")
        )
        self.assertEqual(nightly["jobs"]["smoke"]["runs-on"], pi5_runner)

    def test_pi5_jobs_use_the_runner_python_in_an_isolated_environment(self) -> None:
        pi5_runner = ["self-hosted", "Linux", "ARM64", "krabdeck-pi5"]
        for path in WORKFLOWS.glob("*.yml"):
            workflow = yaml.safe_load(path.read_text(encoding="utf-8"))
            for job_name, job in workflow.get("jobs", {}).items():
                if job.get("runs-on") != pi5_runner:
                    continue
                steps = job.get("steps", [])
                external_actions = "\n".join(
                    str(step.get("uses", "")) for step in steps
                )
                commands = "\n".join(str(step.get("run", "")) for step in steps)
                self.assertNotIn(
                    "actions/setup-python@",
                    external_actions,
                    f"{path.name}:{job_name} cannot install a hosted-image "
                    "Python build on Debian ARM64",
                )
                self.assertIn(
                    'python3 -m venv "$RUNNER_TEMP/',
                    commands,
                    f"{path.name}:{job_name} must isolate the runner Python",
                )
                self.assertIn(
                    "assert sys.version_info[:2] == (3, 13)",
                    commands,
                    f"{path.name}:{job_name} must match the Pi lock runtime",
                )
                self.assertRegex(
                    commands,
                    r"ci/requirements-(?:platformio|coverage)-pi\.txt",
                    f"{path.name}:{job_name} must install the ARM64 lock",
                )

    def test_untrusted_forks_cannot_reach_self_hosted_pr_jobs(self) -> None:
        for name in ("pr-ci.yml", "build-validation-matrix.yml", "security.yml"):
            workflow = yaml.safe_load(
                (WORKFLOWS / name).read_text(encoding="utf-8")
            )
            for job_name, job in workflow["jobs"].items():
                if job.get("runs-on") == [
                    "self-hosted",
                    "Linux",
                    "ARM64",
                    "krabdeck-pi5",
                ]:
                    self.assertIn(
                        "github.event.pull_request.head.repo.full_name == github.repository",
                        str(job.get("if", "")),
                        f"{name}:{job_name} lacks the same-repository PR guard",
                    )


if __name__ == "__main__":
    unittest.main()
