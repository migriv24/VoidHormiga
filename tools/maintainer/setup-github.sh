#!/bin/sh
# setup-github.sh — the repository settings the contribution pipeline needs.
#
# okf/concepts/contributing/maintainers.md. Idempotent: run it again whenever,
# from any maintainer's machine signed in with `gh auth login`. It acts on the
# repository this checkout's `origin` points at, so it follows the project if it
# moves to another owner or organization. Nothing in it names a device.
#
#     tools/maintainer/setup-github.sh            labels + private advisories
#     tools/maintainer/setup-github.sh --terms    ...and upload .private-terms
#                                                 as the PRIVATE_TERMS secret
#     tools/maintainer/setup-github.sh --protect  ...and require pull requests
#                                                 and green checks on main (Q125)
set -eu
repo=$(gh repo view --json nameWithOwner --jq .nameWithOwner)
root=$(git rev-parse --show-toplevel)
echo "repository: $repo"

label() { gh label create "$1" --repo "$repo" --color "$2" --description "$3" --force >/dev/null; echo "  label $1"; }
label needs-triage   ededed "Not read by a maintainer yet"
label needs-repro    fbca04 "Cannot be shown on the Cat Colony yet"
label bug            d73a4a "Hormiga does something wrong"
label feature        a2eeef "Hormiga cannot do something yet"
label privacy-review b60205 "May contain something that must not be public"
label upstream       5319e7 "Needs a change in a sibling project (Core, Maiz, Palabra, Allomone, Hammer)"
label design         0e8a16 "Needs a decision in okf/developer_questions.md first"
label good-first     7057ff "A small, well-understood change"
label declined       cfd3d7 "Will not be done; the reason is in the thread"
for p in windows linux macos android; do label "platform:$p" c5def5 "Only on $p"; done

# Security reports reach whoever maintains the project, not one inbox.
gh api -X PUT "repos/$repo/private-vulnerability-reporting" >/dev/null && echo "  private vulnerability reporting: on"

for a in "$@"; do
  case "$a" in
    --terms)
      [ -f "$root/.private-terms" ] || { echo "no .private-terms in $root" >&2; exit 1; }
      gh secret set PRIVATE_TERMS --repo "$repo" < "$root/.private-terms"
      echo "  secret PRIVATE_TERMS: set from .private-terms" ;;
    --protect)
      gh api -X PUT "repos/$repo/branches/main/protection" --input - >/dev/null <<'JSON'
{
  "required_status_checks": { "strict": false, "contexts": ["lint-public"] },
  "enforce_admins": false,
  "required_pull_request_reviews": { "required_approving_review_count": 0, "require_code_owner_reviews": false },
  "restrictions": null
}
JSON
      echo "  main: pull requests and the privacy check required (admins may still push)" ;;
    *) echo "unknown option $a" >&2; exit 2 ;;
  esac
done
