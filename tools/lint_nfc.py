"""lint_nfc.py — decomposed text hiding in an organization's database.

THE BUG THIS EXISTS FOR, before it happens rather than after.

Void Palabra's SPEC §6.1 is normative as of 2026-08-21: **callers supply NFC;
Palabra does not normalize.** That is the right split and it was Hormiga's own
proposal — the library has a hash function, we have the keyboard. It also means
the precondition is ours to hold, and a precondition nobody checks is a wish.

What goes wrong if it drifts: the canonical form validates UTF-8 but does not
normalize it, so `café` typed on one platform (NFC, U+00E9) and on another
(NFD, `e` + U+0301) are the same word to a reader and **different bytes with
different hashes** to the system. The failure does not present as an error. It
presents as *"the merge did nothing"* — two peers holding what a human would
call the same state, disagreeing about its name, forever.

Hormiga is the case that will hit this, and we said so upstream: the live
database is Spanish and English, real content carries `á é í ó ú ñ ü`, and the
volunteers who type it are not all on one platform.

Measured 2026-08-21: the live 508 KB database, **19,858 strings, zero
decomposed**. This linter exists so that stays true — it is cheap to keep and
expensive to discover.

PRIVACY: this reads a real organization's database, so it prints **counts and
field paths only**. No rune content, no member name, no email ever reaches
stdout. CLAUDE.md rule 2 is not suspended because a tool is a developer tool.

    python tools/lint_nfc.py <state.json> [...]   # report
    python tools/lint_nfc.py --strict <state.json> # non-zero if anything is NFD

With no path it checks every state document it can find beside the repo, which
on a developer machine is the demo/working copy.
"""
import io
import json
import os
import sys
import unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))


def walk(node, path, out):
    """Collect (field-path, string) for every string in the document."""
    if isinstance(node, dict):
        for k, v in node.items():
            walk(v, f"{path}.{k}" if path else str(k), out)
    elif isinstance(node, list):
        # arrays keep the parent's path: the report is field-shaped, not
        # index-shaped, because "runes[417]" tells a reader nothing
        for v in node:
            walk(v, path, out)
    elif isinstance(node, str):
        out.append((path, node))


def check(path):
    """Returns (strings_scanned, {field_path: count}) for one document."""
    with io.open(path, encoding='utf-8') as f:
        doc = json.load(f)
    strings = []
    walk(doc, '', strings)
    bad = {}
    for field, s in strings:
        # NFC is idempotent on already-composed text, so this is exact rather
        # than a heuristic: if composing changes the bytes, it was not composed.
        if unicodedata.normalize('NFC', s) != s:
            bad[field] = bad.get(field, 0) + 1
    return len(strings), bad


def main():
    args = [a for a in sys.argv[1:] if a != '--strict']
    strict = '--strict' in sys.argv[1:]

    if not args:
        args = [p for p in (os.path.join(ROOT, 'demo-org.json'),)
                if os.path.exists(p)]
    if not args:
        print('lint_nfc: no state document to check (pass one as an argument)')
        return 0

    total_bad = 0
    for path in args:
        if not os.path.exists(path):
            print(f'lint_nfc: no such file: {path}')
            return 2
        try:
            n, bad = check(path)
        except (ValueError, UnicodeDecodeError) as e:
            print(f'lint_nfc: {os.path.basename(path)}: not readable as JSON ({e})')
            return 2
        name = os.path.basename(path)
        if not bad:
            print(f'{name}: {n} strings, all composed (NFC)')
            continue
        count = sum(bad.values())
        total_bad += count
        print(f'{name}: {n} strings, {count} DECOMPOSED (NFD) - field paths only:')
        for field, c in sorted(bad.items(), key=lambda kv: -kv[1]):
            print(f'    {field:<50} {c}')

    if total_bad:
        print()
        print('Decomposed text hashes differently from the composed text a')
        print('reader sees, and the symptom is a merge that silently does')
        print('nothing. Normalize on input, where the keyboard is.')
        print('(Void Palabra SPEC 6.1: callers supply NFC.)')
        return 1 if strict else 0

    print('every string in every checked document is composed (NFC)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
