#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$repo_root"

nix develop . -c make debug
exec nix develop . -c ./build/bin/vicinae server --replace
