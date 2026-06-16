#!/usr/bin/env bash
set -euo pipefail

component="${1:?usage: prepare-sdk-source.sh <component> [expected-file]}"
expected="${2:-}"
repo_root="$(git rev-parse --show-toplevel)"
sdk_dir="${SDK_DIR:-${repo_root}/SDK}"
target="${sdk_dir}/github_source/${component}"

if [[ -n "${expected}" && -f "${target}/${expected}" ]]; then
  exit 0
fi

if [[ -z "${expected}" && -e "${target}" ]]; then
  exit 0
fi

export SDK_PATH="${sdk_dir}"
export GIT_REPO_PATH="${sdk_dir}/github_source"
export CONFIG_REPO_AUTOMATION="${CONFIG_REPO_AUTOMATION:-y}"

"${PYTHON:-python3}" - "${component}" <<'PY'
import os
import sys
from pathlib import Path

component = sys.argv[1]
sdk = Path(os.environ["SDK_PATH"])
source_list = sdk / "github_source" / "source-list.sh"

if not source_list.exists():
    raise SystemExit(f"SDK source list not found: {source_list}")

class Env(dict):
    def Fatal(self, message):
        raise RuntimeError(message)

env = Env()
env["PROJECT_TOOL_S"] = str(sdk / "tools" / "scons" / "SConstruct_tool.py")
env["component_dir"] = os.getcwd()
env["GIT_REPO_LISTS"] = {}
env["COMPONENTS"] = []

for line in source_list.read_text().splitlines():
    stripped = line.strip()
    if not stripped or stripped.startswith("#"):
        continue
    parts = stripped.split()
    if len(parts) < 3 or parts[0] != "git_clone_and_checkout_commit":
        continue
    url, commit = parts[1], parts[2]
    name = url.rstrip("/").removesuffix(".git").split("/")[-1]
    env["GIT_REPO_LISTS"][name] = {
        "url": url,
        "commit": commit,
        "path": str(sdk / "github_source" / name),
    }

def Dir(path):
    return path

def File(path):
    return path

def Glob(path):
    return []

with open(env["PROJECT_TOOL_S"], encoding="utf-8") as f:
    exec(f.read(), globals(), globals())

check_component(component)
PY
