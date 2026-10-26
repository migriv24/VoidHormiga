"""intake_scan.py — check a new issue, PR or comment for things that must not be public.

Run by .github/workflows/intake.yml when somebody opens or edits an issue, a
pull request or a comment. People report bugs from inside a real organization's
database, so the most likely thing to land in a public tracker is exactly what
okf/concepts/foundation/what-may-be-public.md forbids: a member's email, a
phone number, a path with a username in it, a pasted token, an organization's
name. The issue form asks them not to; this is the check that a person, in a
hurry, did not.

It reuses tools/lint_public.py's checks, with the private-terms list from the
repository secret (HORMIGA_PRIVATE_TERMS_TEXT). What it prints and returns is
only line numbers and reasons, NEVER the matched text, because a bot comment
quoting a phone number would publish it a second time.

    python tools/intake_scan.py <file>       # exit 1 if anything was found
    python tools/intake_scan.py -            # read standard input
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lint_public  # noqa: E402


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    src = sys.argv[1]
    text = sys.stdin.read() if src == '-' else open(src, encoding='utf-8', errors='replace').read()
    hits = lint_public.scan_text(text, lint_public.private_terms())
    for n, why in hits:
        print(f'line {n}: {why}')
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
