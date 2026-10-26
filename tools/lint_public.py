"""lint_public.py — nothing from a real database or a local machine is committed.

This repository is public, and the OKF, AGENT-GUIDE.md and okf/ ship inside every
installer and archive. Twice a folder of field reports written against real
databases ended up in it: before publication (a partner organization's, removed
2026-09-01) and again on 2026-09-02, when a second client's and the author's own
portfolio reports were committed, shipped in sixteen releases, and carried a
phone number and a personal email. The rule existed in prose both times. This
is the rule as a mechanism. See okf/concepts/foundation/what-may-be-public.md.

WHAT IS CHECKED, over every tracked file (or, with --staged, every staged one):

  paths      okf/reports/, MESSAGE_FOR_*/REPORT_FROM_* correspondence, databases
             and their sidecars (*.state.json, *.miga, *.db, *.bkp, *.peerkey)
  emails     any address outside the reserved example domains
  phones     any North American number whose exchange is not 555
  home dirs  C:\\Users\\<someone>, /home/<someone>, /Users/<someone>
  tokens     GitHub, Slack and AWS key shapes, other than the test dummy
  terms      every line of the PRIVATE TERMS file, case-insensitively

THE PRIVATE TERMS FILE is the one list that cannot be committed, because the
list is itself the secret: client names, organization names, people, places,
project folders. It is `.private-terms` at the repo root (gitignored), or the
file named by HORMIGA_PRIVATE_TERMS. One term per line, matched as a substring
ignoring case; a line starting `re:` is a regular expression instead (a short
name wants word boundaries: `re:`, then word-boundary escapes around it). `#` starts a comment.
Without one, the generic checks still run and the lint says the terms were not.

    python tools/lint_public.py --strict          # CI and ctest
    python tools/lint_public.py --staged --strict # tools/hooks/pre-commit
    python tools/lint_public.py --root <checkout>  # someone else's tree (a PR)
"""
import os
import re
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
# where this script's own checkout keeps .private-terms, whatever --root says
HERE = ROOT

# vendor/ is third-party text with its own authors' addresses in it.
SKIP_PREFIXES = ('vendor/', 'demo-assets/')

FORBIDDEN_PATHS = [
    (re.compile(r'^okf/reports/'), 'okf/reports/ is for no one: a report about a database stays beside that database'),
    (re.compile(r'(^|/)(MESSAGE_FOR|REPORT_FROM)_[^/]*$'), 'correspondence is gitignored (CLAUDE.md rule 4)'),
    (re.compile(r'\.(state\.json|miga|db|bkp|peerkey|merging)$'), 'a database or its sidecar'),
    (re.compile(r'(^|/)HormigaFiles/'), 'a database folder'),
]

EMAIL = re.compile(r'[A-Za-z0-9._%+-]+@([A-Za-z0-9-]+\.)+[A-Za-z]{2,}')
EMAIL_OK = re.compile(r'@(([A-Za-z0-9-]+\.)*(example\.(org|com|net)|example|invalid|test|localhost))$', re.I)

PHONE = re.compile(r'(?<![\d.])(\(?\d{3}\)?[-. ])(\d{3})[-. ]\d{4}(?![\d])')

HOME = re.compile(r'(?:(?<![A-Za-z0-9])[A-Za-z]:[\\/]+Users[\\/]+'
                  r'|(?<![A-Za-z0-9_.-])/(?:c/Users|home|Users)/)([A-Za-z0-9._-]+)')
HOME_OK = {'', 'name', 'someone', 'somebody', 'you', 'user', 'username', 'me',
           'public', 'default', 'runner', 'shared', 'example', '...'}

TOKEN = re.compile(r'\b(ghp_[A-Za-z0-9]{20,}|gho_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}'
                   r'|xox[abprs]-[A-Za-z0-9-]{10,}|AKIA[0-9A-Z]{16})\b')
TOKEN_OK = {'ghp_0123456789abcdefghijklmnopqrstuvwxyz'}


def git(*args):
    return subprocess.run(['git', *args], cwd=ROOT, capture_output=True).stdout


def private_terms():
    # HORMIGA_PRIVATE_TERMS_TEXT carries the list itself, for CI, where it is a
    # repository secret rather than a file on whichever machine a maintainer
    # happens to use (okf/concepts/contributing/maintainers.md).
    text = os.environ.get('HORMIGA_PRIVATE_TERMS_TEXT')
    if not text:
        path = os.environ.get('HORMIGA_PRIVATE_TERMS') or os.path.join(HERE, '.private-terms')
        if not os.path.isfile(path):
            return None
        with open(path, encoding='utf-8') as f:
            text = f.read()
    terms = []
    for line in text.splitlines():
        line = line.split('#', 1)[0].strip()
        if line.startswith('re:'):
            terms.append(re.compile(line[3:], re.I))
        elif line:
            terms.append(re.compile(re.escape(line), re.I))
    return terms


def files(staged):
    if staged:
        out = git('diff', '--cached', '--name-only', '--diff-filter=ACMR', '-z')
    else:
        out = git('ls-files', '-z')
    return [p for p in out.decode('utf-8', 'replace').split('\0') if p]


def content(path, staged):
    if staged:
        return git('show', ':' + path)
    try:
        with open(os.path.join(ROOT, path), 'rb') as f:
            return f.read()
    except OSError:
        return b''


def scan_text(text, terms):
    """(line number, reason) for everything in `text` that must not be public.
    Shared with tools/intake_scan.py, which runs it over a new issue. It never
    returns the matched text itself: a report that echoed a phone number would
    publish it a second time."""
    hits = []
    for n, line in enumerate(text.splitlines(), 1):
        for m in EMAIL.finditer(line):
            if not EMAIL_OK.search(m.group(0)):
                hits.append((n, 'an email address outside the example domains'))
        for m in PHONE.finditer(line):
            if m.group(2) != '555':
                hits.append((n, 'a phone number (fixtures use 555-01xx)'))
        for m in HOME.finditer(line):
            who = m.group(1).strip('.').lower()
            if who not in HOME_OK and not who.startswith('<'):
                hits.append((n, 'a home directory: write a path relative to the repo or a placeholder'))
        for m in TOKEN.finditer(line):
            if m.group(1) not in TOKEN_OK:
                hits.append((n, 'something shaped like a credential'))
        if terms and any(t.search(line) for t in terms):
            hits.append((n, 'a private term (from the private-terms list)'))
    return hits


def main() -> int:
    global ROOT
    if '--root' in sys.argv:  # another checkout: tools/maintainer/review-pr.sh
        ROOT = os.path.abspath(sys.argv[sys.argv.index('--root') + 1])
    staged = '--staged' in sys.argv
    strict = '--strict' in sys.argv
    terms = private_terms()
    hits = []

    if staged and terms:
        email = git('config', 'user.email').decode().strip()
        if any(t.search(email) for t in terms):
            hits.append(('git config', 0, 'the commit author email names a private term: '
                         'commits carry it publicly. Use a no-reply address'))

    for path in files(staged):
        for pat, why in FORBIDDEN_PATHS:
            if pat.search(path):
                hits.append((path, 0, why))
        if path.startswith(SKIP_PREFIXES) or path == 'tools/lint_public.py':
            continue
        data = content(path, staged)
        if b'\0' in data[:4096]:
            continue
        text = data.decode('utf-8', 'replace')
        hits += [(path, n, why) for n, why in scan_text(text, terms)]

    for path, n, why in hits:
        print(f'{path}:{n}: {why}' if n else f'{path}: {why}')
    if terms is None:
        print('note: no .private-terms file (or HORMIGA_PRIVATE_TERMS) - names were not checked')
    print(f'lint_public: {len(hits)} finding(s)')
    return 1 if (strict and hits) else 0


if __name__ == '__main__':
    sys.exit(main())
