"""lint_okf_links.py — every OKF-absolute link resolves to a file that exists.

The OKF grew FOLDERS on 2026-08-27 (`concepts/foundation`, `sections`,
`platform`, `projects`, alongside the `allomone` folder that was already there),
because seventeen concept files in one directory had stopped being a structure
and started being a pile. Seventeen files moved and 419 links had to move with
them.

That is exactly the change that is cheap to do and expensive to do WRONG: a
link that points at a file which no longer exists does not fail anything. It
sits there, and the next agent to read the OKF follows it, finds nothing, and
either invents what the page said or quietly works without it. Ground rule 1
says the OKF is the source of truth; a source of truth with dead pointers in it
is worse than one that is merely incomplete, because it lies with confidence.

So the same discipline `check_layering.py` applies to `src/` applies here: the
structure is checked in CI, and a move that breaks a link fails the build the
same way a bad include does.

WHAT IS CHECKED: markdown links of the OKF-absolute form `](/concepts/x.md)`,
which is this OKF's convention for pointing at itself. Relative links that
escape the repo (`../../VoidReyna/okf/...`) are deliberately NOT checked — that
is a sibling project's tree, it may legitimately be absent from a given
checkout, and failing our build over its layout would be exactly the upstream
coupling ground rule 4 forbids.

    python tools/lint_okf_links.py            # non-zero if any link is dead
"""
import os
import re
import sys

pat = re.compile(r'\]\((/[A-Za-z0-9_./-]+\.md)\)')


def main() -> int:
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
    root = os.path.normpath(root)
    okf = os.path.join(root, 'okf')
    if not os.path.isdir(okf):
        print('no okf/ here; nothing to check')
        return 0

    bad = []
    seen = 0
    for base, dirs, files in os.walk(okf):
        # `reports/` held a partner organization's dated field measurements and
        # was removed before publication (2026-09-01) — the findings live in the
        # log and in the code, the organization does not live in this repo. The
        # skip stays so a future reports/ is again treated as verbatim: you do
        # not rewrite a link inside a measurement somebody else wrote.
        dirs[:] = [d for d in dirs if d != 'reports']
        for fn in files:
            if not fn.endswith('.md'):
                continue
            path = os.path.join(base, fn)
            with open(path, encoding='utf-8', errors='replace') as f:
                text = f.read()
            for m in pat.finditer(text):
                seen += 1
                if not os.path.exists(os.path.join(okf, m.group(1).lstrip('/'))):
                    bad.append('%s -> %s'
                               % (os.path.relpath(path, root).replace(os.sep, '/'),
                                  m.group(1)))

    print('checked %d okf-absolute links' % seen)
    if bad:
        sys.stderr.write('dead OKF links (%d):\n' % len(bad))
        for b in sorted(set(bad)):
            sys.stderr.write('  - %s\n' % b)
        sys.stderr.write('\nA moved concept takes its inbound links with it. '
                         'Fix the link or restore the file.\n')
        return 1
    print('all resolve')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
