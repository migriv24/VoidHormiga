#!/bin/sh
# review-pr.sh <number> — check a pull request's tree before merging it.
#
# okf/concepts/contributing/maintainers.md. CI checks a fork's pull request for
# PATTERNS only, because a fork cannot see the PRIVATE_TERMS secret. This is the
# other half: it puts the PR's head in a separate worktree (your own checkout is
# not touched), runs the privacy lint over it WITH the names, and builds nothing
# and runs nothing from it. Any maintainer, on any machine with `gh auth login`
# and either a local .private-terms or HORMIGA_PRIVATE_TERMS(_TEXT) set.
#
#     tools/maintainer/review-pr.sh 12
set -eu
[ $# -eq 1 ] || { echo "usage: $0 <pull-request-number>" >&2; exit 2; }
n=$1
root=$(git rev-parse --show-toplevel)
py=$(command -v python3 || command -v python)
wt="${TMPDIR:-/tmp}/hormiga-pr-$n"

git -C "$root" fetch --quiet origin "pull/$n/head:refs/remotes/pr/$n"
rm -rf "$wt"
git -C "$root" worktree add --quiet --detach "$wt" "refs/remotes/pr/$n"
trap 'git -C "$root" worktree remove --force "$wt" >/dev/null 2>&1 || true' EXIT

echo "PR #$n, $(git -C "$wt" log -1 --format='%h %s')"
echo "files changed:"
git -C "$root" diff --stat "origin/main...refs/remotes/pr/$n" | tail -n 25
echo
# The lint from YOUR checkout, over THEIR tree: a PR that edits the lint cannot
# weaken the check that reviews it.
"$py" "$root/tools/lint_public.py" --root "$wt" --strict
