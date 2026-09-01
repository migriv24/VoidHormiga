"""split_tu.py — lift a run of top-level definitions out of a .cpp into a new one.

The mechanical half of a file split, done by a script so it cannot pick up a
typo on the way. It copies the source file's leading include block verbatim
(everything before the first top-level definition), then moves the requested
line ranges.

    python tools/split_tu.py <source.cpp> <dest.cpp> "<header comment>" A:B C:D ...

Ranges are 1-indexed and inclusive, and are taken from the ORIGINAL file — the
script sorts and applies them back-to-front so earlier ranges keep their
numbering.

WHY A TOOL AND NOT AN EDITOR. A 3,000-line file gets split once and then again
six months later, and the risky part is never the design — it is dropping a
closing brace or half a comment while moving 400 lines by hand. This makes the
move reproducible and reviewable: `git diff` shows text that moved, nothing that
changed. Verify with `tools/golden_render.sh` — a split that alters a byte of
output was not a split.
"""
import io
import os
import sys


def main() -> int:
    if len(sys.argv) < 5:
        sys.stderr.write(__doc__)
        return 2
    src, dst, banner = sys.argv[1], sys.argv[2], sys.argv[3]
    ranges = []
    for a in sys.argv[4:]:
        lo, hi = a.split(':')
        ranges.append((int(lo), int(hi)))
    ranges.sort()

    # ── GUARDS, added 2026-08-20 after this tool destroyed a file ────────────
    #
    # An invocation of this script was backgrounded after appearing to hang on
    # shell quoting. It had not hung — it was waiting, and it ran hours later
    # against a source file that had since been split by another route. It did
    # exactly what it was told: moved lines 26-671 into a destination that was
    # no longer empty, overwriting an entire renderer.
    #
    # The tool was not wrong. It was UNGUARDED, and a tool that edits source has
    # to assume it is being run at the wrong moment, because one day it will be.
    if os.path.exists(dst) and os.path.getsize(dst) > 0 and '--force' not in sys.argv:
        sys.stderr.write(
            '%s already exists and is not empty.\n'
            'Refusing to overwrite it — this is exactly how a renderer was lost.\n'
            'Pass --force if replacing it is genuinely what you mean.\n' % dst)
        return 2

    lines = io.open(src, encoding='utf-8').read().split('\n')

    # A range that runs past the end, or an empty one, means the source is not
    # the file this command was written against — most likely because it has
    # already been split. Refuse rather than cut something arbitrary.
    for lo, hi in ranges:
        if lo < 1 or hi > len(lines) or lo > hi:
            sys.stderr.write(
                'range %d:%d does not fit %s (%d lines).\n'
                'The file is not what this command expected — has it already '
                'been split?\n' % (lo, hi, src, len(lines)))
            return 2

    # the include block: everything up to the first line that starts a
    # definition at column 0 and is not a preprocessor line or a comment
    head_end = 0
    for i, ln in enumerate(lines):
        st = ln.strip()
        if not st or st.startswith(('#', '/*', '*', '//')) or ln.startswith(' '):
            continue
        head_end = i
        break
    head = '\n'.join(lines[:head_end]).rstrip() + '\n'
    # keep only the #include lines and the namespace-ish preamble
    keep = [l for l in head.split('\n')
            if l.startswith('#include') or l.startswith('using ')
            or l.startswith('namespace ')]

    taken = []
    for lo, hi in ranges:
        taken.append('\n'.join(lines[lo - 1:hi]))

    out = [banner.rstrip(), '']
    out.extend(keep)
    out.append('')
    out.extend(taken)
    io.open(dst, 'w', encoding='utf-8', newline='').write('\n'.join(out).rstrip() + '\n')

    for lo, hi in reversed(ranges):
        del lines[lo - 1:hi]
    io.open(src, 'w', encoding='utf-8', newline='').write('\n'.join(lines))

    moved = sum(hi - lo + 1 for lo, hi in ranges)
    print('moved %d lines -> %s (%s now %d lines)'
          % (moved, dst, src, len(lines)))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
