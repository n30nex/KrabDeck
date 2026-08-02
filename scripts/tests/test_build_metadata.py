import ast
import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]


class BuildMetadataContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "scripts" / "build_metadata.py").read_text(
            encoding="utf-8"
        )

    def test_untracked_build_inputs_mark_build_dirty(self):
        self.assertIn('"--untracked-files=normal"', self.source)
        self.assertNotIn('"--untracked-files=no"', self.source)

    def test_ci_provenance_fields_are_bounded(self):
        for field in (
            "build_source",
            "actions_run_id",
            "actions_run_attempt",
            "actions_ref",
            "actions_run_url",
        ):
            self.assertIn(f"{field} = _bounded({field},", self.source)

    def test_release_version_override_is_sha_bound_and_ci_only(self):
        self.assertIn('os.environ.get("KRABOS_RELEASE_VERSION", "")', self.source)
        self.assertIn('build_source != "github_actions"', self.source)
        self.assertIn('actions_sha != head_sha', self.source)
        self.assertIn('if git_dirty:', self.source)
        self.assertIn('git_tag = release_version', self.source)

    def test_release_version_override_accepts_only_stable_or_edge_names(self):
        self.assertIn('r"(?:v[1-9][0-9]*\\.[0-9]+\\.[0-9]+|"', self.source)
        self.assertIn('r"edge-[0-9]{4}-[0-9]{2}-[0-9]{2}-[0-9a-f]{12})"', self.source)

    def test_build_date_and_compiler_epoch_are_commit_derived(self):
        companion = (ROOT / "src" / "comms" / "companion_bridge.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn('["show", "-s", "--format=%ct", "HEAD"]', self.source)
        self.assertIn('os.environ["SOURCE_DATE_EPOCH"] = str(commit_epoch)', self.source)
        self.assertIn('env["ENV"]["SOURCE_DATE_EPOCH"] = str(commit_epoch)', self.source)
        self.assertIn('("SIGURDOS_BUILD_DATE", _macro_string(build_date))', self.source)
        self.assertIn("BUILD_DATE = SIGURDOS_BUILD_DATE", companion)
        self.assertNotIn("__DATE__", companion)

    def test_release_provenance_is_invariant_across_actions_retries(self):
        tree = ast.parse(self.source)
        helper = next(
            node
            for node in tree.body
            if isinstance(node, ast.FunctionDef)
            and node.name == "_actions_provenance"
        )
        namespace = {}
        exec(
            compile(
                ast.Module(body=[helper], type_ignores=[]),
                "scripts/build_metadata.py",
                "exec",
            ),
            namespace,
        )
        provenance = namespace["_actions_provenance"]
        sha = "a" * 40
        first = provenance(
            "v1.0.0",
            sha,
            "https://github.com",
            "n30nex/KrabDeck",
            "100",
            "1",
            "main",
        )
        retry = provenance(
            "v1.0.0",
            sha,
            "https://github.com",
            "n30nex/KrabDeck",
            "999",
            "7",
            "different-ref",
        )
        self.assertEqual(first, retry)
        self.assertEqual(
            first,
            (
                "commit-aaaaaaaaaaaa",
                "",
                sha,
                f"https://github.com/n30nex/KrabDeck/commit/{sha}",
            ),
        )
        self.assertNotEqual(
            provenance("", sha, "https://github.com", "n30nex/KrabDeck", "100", "1", "main"),
            provenance("", sha, "https://github.com", "n30nex/KrabDeck", "999", "7", "main"),
        )


if __name__ == "__main__":
    unittest.main()
