"""Source-selection safety checks using real Git repositories, without GitHub writes."""
from pathlib import Path
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
OFFICIAL = "https://github.com/Shawna7han/mumble-funk.git"


class SourceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / "checkout"
        self.remote = self.root / "published.git"
        self.run_git("clone", "--shared", "--no-checkout", str(REPO), str(self.repo), cwd=self.root)
        self.git("config", "user.name", "Source guard test")
        self.git("config", "user.email", "test@example.invalid")
        self.git("checkout", "-B", "stable", "HEAD")
        self.head = self.git("rev-parse", "HEAD").stdout.strip()
        self.git("update-ref", "refs/remotes/origin/stable", self.head)
        self.git("update-ref", "refs/remotes/origin/funk-v0.1", self.head)
        self.run_git("init", "--bare", str(self.remote), cwd=self.root)
        self.git("remote", "set-url", "origin", OFFICIAL)
        # Only this disposable repository redirects the public URL to its local
        # publication fixture. Production validation still checks the raw URL.
        self.git("config", f"url.{self.remote}.insteadOf", OFFICIAL)

    def run_git(self, *args, cwd=None):
        # A detached CI checkout can trigger auto-maintenance after our local
        # fixture commits. Keep it synchronous/off so no Git child recreates
        # files while TemporaryDirectory removes its disposable repository.
        return subprocess.run(["git", "-c", "gc.auto=0", "-c", "maintenance.auto=false",
                               "-c", "gc.autoDetach=false", *args], cwd=cwd or self.repo, check=True,
                              capture_output=True, text=True)

    def git(self, *args):
        return self.run_git(*args)

    def guard(self, allowed, experimental=False):
        result = subprocess.run(["bash", "-eu", "-c",
                                 'source "$1"; funk_require_supported_source "$2" "$3"', "source-test",
                                 str(REPO / "scripts/funk/source.sh"), str(self.repo), ("--experimental-squelch" if experimental is True else experimental) if experimental else ""],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, allowed, result.stdout + result.stderr)
        if allowed:
            self.assertIn(self.head, result.stdout)

    def test_stable_and_legacy_branch(self):
        self.guard(True)
        self.git("checkout", "-B", "funk-v0.1", self.head)
        self.guard(True)

    def test_unknown_branch_rejected(self):
        for branch in ("dev", "feature/audio-engine", "feature/audio-feedback"):
            with self.subTest(branch=branch):
                self.git("checkout", "-B", branch, self.head)
                self.guard(False)

    def feature(self):
        self.git("checkout", "-B", "feature/squelch-audio", self.head)
        self.git("update-ref", "refs/remotes/origin/feature/squelch-audio", self.head)
        self.git("push", str(self.remote), "HEAD:refs/heads/feature/squelch-audio")

    def test_experimental_branch_requires_explicit_opt_in(self):
        self.feature()
        self.guard(False)
        self.guard(True, experimental=True)

    def test_experimental_publication_must_match_cached_and_live_revision(self):
        self.feature()
        self.git("update-ref", "refs/remotes/origin/feature/squelch-audio", "dfbac8d3d9c967003e74b4ffc5610f7d45e8f666")
        self.guard(False, experimental=True)
        self.git("update-ref", "refs/remotes/origin/feature/squelch-audio", self.head)
        self.run_git("--git-dir", str(self.remote), "update-ref", "refs/heads/feature/squelch-audio", "dfbac8d3d9c967003e74b4ffc5610f7d45e8f666")
        self.guard(False, experimental=True)

    def test_experimental_flag_does_not_whitelist_other_branches_or_dirty_source(self):
        self.git("checkout", "-B", "dev", self.head)
        self.guard(False, experimental=True)
        self.feature()
        (self.repo/"README.md").write_text("uncommitted change")
        self.guard(False, experimental=True)

    def test_audio_volume_requires_its_own_flag_and_published_clean_head(self):
        self.git("checkout", "-B", "feature/audio-volume", self.head)
        self.git("update-ref", "refs/remotes/origin/feature/audio-volume", self.head)
        self.git("push", str(self.remote), "HEAD:refs/heads/feature/audio-volume")
        self.guard(False)
        self.guard(False, experimental=True)  # Old Squelch opt-in is not interchangeable.
        self.guard(True, experimental="--experimental-audio-volume")
        self.git("update-ref", "refs/remotes/origin/feature/audio-volume", "dfbac8d3d9c967003e74b4ffc5610f7d45e8f666")
        self.guard(False, experimental="--experimental-audio-volume")
        self.git("update-ref", "refs/remotes/origin/feature/audio-volume", self.head)
        self.run_git("--git-dir", str(self.remote), "update-ref", "refs/heads/feature/audio-volume", "dfbac8d3d9c967003e74b4ffc5610f7d45e8f666")
        self.guard(False, experimental="--experimental-audio-volume")
        self.git("push", str(self.remote), "HEAD:refs/heads/feature/audio-volume")
        (self.repo/"README.md").write_text("uncommitted source")
        self.guard(False, experimental="--experimental-audio-volume")

    def test_local_branch_divergence_rejected(self):
        self.git("commit", "--allow-empty", "-m", "Unpublished local change")
        self.guard(False)

    def test_missing_remote_branch_rejected(self):
        self.git("update-ref", "-d", "refs/remotes/origin/stable")
        self.guard(False)

    def test_tracked_edits_rejected(self):
        with (self.repo / "README.md").open("a") as target:
            target.write("\nLocal change\n")
        self.guard(False)

    def test_foreign_origin_rejected(self):
        self.git("remote", "set-url", "origin", str(self.remote))
        self.guard(False)

    def test_wrong_mumble_base_rejected(self):
        self.git("checkout", "--orphan", "unrelated")
        self.git("commit", "--allow-empty", "-m", "Unrelated source")
        self.git("branch", "-M", "stable")
        self.git("update-ref", "refs/remotes/origin/stable", "HEAD")
        self.guard(False)

    def tag(self, name="funk-v99.0.0", annotated=True, publish=True):
        if annotated:
            self.git("tag", "-a", name, "-m", "Disposable local test tag")
        else:
            self.git("tag", name)
        if publish:
            self.git("push", str(self.remote), f"refs/tags/{name}")
        self.git("checkout", "--detach", self.head)

    def test_published_annotated_version(self):
        self.tag()
        self.guard(True)

    def test_unpublished_version_rejected(self):
        self.tag(publish=False)
        self.guard(False)

    def test_lightweight_version_rejected(self):
        self.tag(annotated=False)
        self.guard(False)

    def test_archive_tag_rejected(self):
        self.tag(name="archive/mumble/old")
        self.guard(False)

    def test_remote_tag_commit_mismatch_rejected(self):
        self.tag()
        self.run_git("--git-dir", str(self.remote), "update-ref", "refs/tags/funk-v99.0.0",
                     "dfbac8d3d9c967003e74b4ffc5610f7d45e8f666")
        self.guard(False)

    def test_remote_unavailable_fails_closed(self):
        self.tag()
        self.git("config", f"url.{self.root / 'missing.git'}.insteadOf", OFFICIAL)
        self.git("config", "--unset", f"url.{self.remote}.insteadOf")
        self.guard(False)

    def test_worktree_lock_path_is_absolute_from_another_directory(self):
        worktree = self.root / "linked-checkout"
        self.git("worktree", "add", "--detach", str(worktree), self.head)
        result = self.run_git("-C", str(worktree), "rev-parse", "--path-format=absolute",
                              "--git-path", "funk-setup.lock", cwd=self.root)
        path = Path(result.stdout.strip())
        self.assertTrue(path.is_absolute())
        self.assertTrue(path.parent.is_dir())
        path.touch()


if __name__ == "__main__":
    unittest.main(verbosity=2)
