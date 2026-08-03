Import("env")

import os
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def _macro_string(value):
    escaped = str(value).replace("\\", "\\\\").replace('"', '\\"')
    return '\\"' + escaped + '\\"'


def _git(args, cwd):
    try:
        completed = subprocess.run(
            ["git", *args],
            cwd=str(cwd),
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return "unknown"
    value = completed.stdout.strip()
    return value if value else "unknown"


def _git_dirty(cwd):
    try:
        completed = subprocess.run(
            ["git", "status", "--porcelain", "--untracked-files=normal"],
            cwd=str(cwd),
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return True
    return bool(completed.stdout.strip())


def _bounded(value, limit):
    return str(value)[:limit]


def _git_describe(cwd):
    try:
        completed = subprocess.run(
            ["git", "describe", "--tags", "--dirty"],
            cwd=str(cwd),
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    value = completed.stdout.strip()
    return value if value else None


def _commit_epoch(cwd):
    raw = _git(["show", "-s", "--format=%ct", "HEAD"], cwd)
    try:
        epoch = int(raw)
    except (TypeError, ValueError) as exc:
        raise RuntimeError("Git commit timestamp is required") from exc
    if epoch < 0:
        raise RuntimeError("Git commit timestamp cannot be negative")
    return epoch


def _build_date(epoch):
    months = (
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    )
    stamp = datetime.fromtimestamp(epoch, timezone.utc)
    return f"{stamp.day:02d}-{months[stamp.month - 1]}-{stamp.year:04d}"


def _actions_provenance(
    release_version,
    head_sha,
    server_url,
    repository,
    run_id,
    run_attempt,
    ref,
):
    if release_version:
        commit_url = (
            f"{server_url.rstrip('/')}/{repository}/commit/{head_sha}"
            if repository
            else ""
        )
        return f"commit-{head_sha[:12]}", "", head_sha, commit_url
    run_url = (
        f"{server_url.rstrip('/')}/{repository}/actions/runs/{run_id}"
        if repository and run_id
        else ""
    )
    return run_id, run_attempt, ref, run_url


project_dir = Path(env.subst("$PROJECT_DIR"))
meshcore_dir = project_dir / "lib" / "meshcore"
board_config = env.BoardConfig()

build_env = env.subst("$PIOENV") or "unknown"
board = env.subst("$BOARD") or "unknown"
mcu = board_config.get("build.mcu", "esp32s3")
partitions = env.GetProjectOption("board_build.partitions", "")
if not partitions:
    partitions = board_config.get("build.partitions", "unknown")

git_sha = _git(["rev-parse", "--short=12", "HEAD"], project_dir)
head_sha = _git(["rev-parse", "HEAD"], project_dir).lower()
meshcore_sha = _git(["rev-parse", "--short=12", "HEAD"], meshcore_dir)
git_dirty = _git_dirty(project_dir)
git_tag = _git_describe(project_dir)
commit_epoch = _commit_epoch(project_dir)
build_date = _build_date(commit_epoch)

# Keep compiler date macros tied to the exact checked-out commit.
os.environ["SOURCE_DATE_EPOCH"] = str(commit_epoch)
env["ENV"]["SOURCE_DATE_EPOCH"] = str(commit_epoch)
if git_tag and git_dirty and not git_tag.endswith("-dirty"):
    git_tag += "-dirty"

build_source = "github_actions" if os.environ.get("GITHUB_ACTIONS") == "true" else "local"
release_version = os.environ.get("KRABOS_RELEASE_VERSION", "")
if release_version:
    actions_sha = os.environ.get("GITHUB_SHA", "").lower()
    if build_source != "github_actions":
        raise RuntimeError("KRABOS_RELEASE_VERSION is accepted only in GitHub Actions")
    if not re.fullmatch(r"[0-9a-f]{40}", actions_sha) or actions_sha != head_sha:
        raise RuntimeError("release build GITHUB_SHA does not match checked-out HEAD")
    if git_dirty:
        raise RuntimeError("release version override requires a clean checkout")
    if not re.fullmatch(
        r"(?:v[1-9][0-9]*\.[0-9]+\.[0-9]+|"
        r"edge-[0-9]{4}-[0-9]{2}-[0-9]{2}-[0-9a-f]{12})",
        release_version,
    ):
        raise RuntimeError("invalid KRABOS_RELEASE_VERSION")
    git_tag = release_version

server_url = os.environ.get("GITHUB_SERVER_URL", "https://github.com")
repository = (
    os.environ.get("GITHUB_REPOSITORY", "")
    if build_source == "github_actions"
    else ""
)
actions_run_id, actions_run_attempt, actions_ref, actions_run_url = (
    _actions_provenance(
        release_version,
        head_sha,
        server_url,
        repository,
        os.environ.get("GITHUB_RUN_ID", "local"),
        os.environ.get("GITHUB_RUN_ATTEMPT", ""),
        os.environ.get("GITHUB_REF_NAME")
        or _git(["rev-parse", "--abbrev-ref", "HEAD"], project_dir),
    )
)

build_source = _bounded(build_source, 32)
actions_run_id = _bounded(actions_run_id, 32)
actions_run_attempt = _bounded(actions_run_attempt, 16)
actions_ref = _bounded(actions_ref, 128)
actions_run_url = _bounded(actions_run_url, 256)

# Set SIGURDOS_VERSION from git describe with tdeck_pins.h define as fallback
sigurdos_version = _macro_string(git_tag) if git_tag else None

env.Append(
    CPPDEFINES=[
        ("SIGURDOS_BUILD_GIT_SHA", _macro_string(git_sha)),
        ("SIGURDOS_BUILD_DATE", _macro_string(build_date)),
        ("SIGURDOS_BUILD_GIT_DIRTY", "1" if git_dirty else "0"),
        ("SIGURDOS_BUILD_MESHCORE_SHA", _macro_string(meshcore_sha)),
        ("SIGURDOS_BUILD_ENV", _macro_string(build_env)),
        ("SIGURDOS_BUILD_PARTITIONS", _macro_string(partitions)),
        ("SIGURDOS_BUILD_BOARD", _macro_string(board)),
        ("SIGURDOS_BUILD_MCU", _macro_string(mcu)),
        ("SIGURDOS_BUILD_SOURCE", _macro_string(build_source)),
        ("SIGURDOS_BUILD_ACTIONS_RUN_ID", _macro_string(actions_run_id)),
        ("SIGURDOS_BUILD_ACTIONS_RUN_ATTEMPT", _macro_string(actions_run_attempt)),
        ("SIGURDOS_BUILD_ACTIONS_REF", _macro_string(actions_ref)),
        ("SIGURDOS_BUILD_ACTIONS_RUN_URL", _macro_string(actions_run_url)),
    ]
)

# Add build-derived SIGURDOS_VERSION if git describe succeeded
# (overrides the static default in tdeck_pins.h)
if sigurdos_version:
    env.Append(CPPDEFINES=[("SIGURDOS_VERSION", sigurdos_version)])

dirty_suffix = "+dirty" if git_dirty else ""
print(
    "KrabOS build metadata: "
    f"env={build_env} git={git_sha}{dirty_suffix} "
    f"meshcore={meshcore_sha} partitions={partitions} "
    f"source={build_source} run={actions_run_id}"
)
