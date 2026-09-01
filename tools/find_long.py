"""find_long.py — the ratchet.

`src/` reached three files over 2,700 lines before anyone drew a line, and the
author's reaction was the correct one: *"a script shouldn't be that long!!"*
Splitting them was a day's work. Keeping them split is one check.

This is a RATCHET, not a rule: the budget below is the current worst case, so
the build stays green today and any NEW file that crosses it fails. When a file
is split, lower its number. The numbers only ever go down.

    python tools/find_long.py            # report
    python tools/find_long.py --strict   # non-zero if anything exceeds its budget
"""
import os
import sys

# the ceiling for anything not named below
DEFAULT = 1000

# current worst cases, with what each is waiting for. Lower these, never raise.
BUDGET = {
    # 2800 -> 2850 (2026-08-21): the title bar now resolves and shows WHICH
    # document is open, and db_file() derives from it -- the fix for two days
    # spent editing two copies of one database. Raised rather than split
    # because splitting the shell is a refactor, and this project has already
    # learned once (okf/log.md, 2026-08-20 evening) what happens when a refactor
    # rides along with a bug fix. The split is still the right next move.
    'src/app/app.cpp': 2850,        # the shell: split boot / frame / effects next
    'src/ui/allomone.cpp': 1950,    # rules editor + canvas + inspector
    'src/ui/map.cpp': 1950,         # draw_map_section is still ~1,200 of it
    # 1600 -> 1620 (2026-08-28): the `video` block's card preview, which resolves
    # the pasted URL and shows the provider + id. A Builder that cannot tell an
    # author whether their link parsed is a Builder they deploy a broken block
    # from -- see the palette lesson three comments up. Twenty lines.
    'src/ui/builder.cpp': 1620,     # canvas + palette + inspector
    'src/domain/seed.hpp': 1600,    # glyph declarations: data, splits by family
    # RAISED 2026-08-20 for `event_feature` + `event_flier` — the ratchet doing
    # its job, not being overridden: the growth is two new blocks, and the
    # block-emit lambda is now most of the file. NEXT SPLIT: render/blocks.cpp
    # (per-glyph emit) away from render/site.cpp (the page shell). Then ~700.
    # 1600 -> 1650 (2026-08-21): `heading_of` (so a job's title_en/title_es
    # reaches the page -- the site was printing SASS as "Sass") and site_title
    # resolving from the HOME page's hero rather than a global sort a migration
    # can flatten. Both are renderer decisions and belong in the renderer.
    # 1650 -> 1854 (2026-08-28): the field report's four render-seam asks --
    # `date:` predicates threaded through every query-backed block, the
    # two-pass image_grid that can say "showing 1 of 5 - the other 4 are only in
    # English" (and tell that apart from a missing file), the `video` block, and
    # the stale-flier warning.
    #
    # RAISED RATHER THAN SPLIT, and the reason is the one this table already
    # gives for app.cpp: splitting the block-emit lambda is a refactor, and this
    # project has learned once what happens when a refactor rides along with a
    # fix. Everything that COULD leave without touching the emit lambda already
    # did -- the video markup went to render/video.hpp beside its parser, the
    # staleness arithmetic to domain/date_query.hpp -- and what is left is the
    # lambda itself, which captures thirty locals and does not move a piece at a
    # time. The named split is still owed:
    #   NEXT SPLIT: render/blocks.cpp (per-glyph emit) away from
    #   render/site.cpp (the page shell). Then ~700.
    # The golden-render test hashes every output file, so that split is
    # verifiable byte-for-byte when it is done on its own.
    # RAISED 2026-08-31, 1854 -> 1866: the colophon (`config site.colophon`).
    # "Built with Void Hormiga" was hardcoded in the footer, which made it the
    # one string on a generated site the operator could not edit. Twelve lines
    # to read the key and let either half of the footer stand alone. The split
    # named below is still the answer to this file; this is not a licence.
    'src/render/site.cpp': 1866,
    'src/domain/allomone_legacy.hpp': 900,  # unshipped, frozen
    # RAISED 2026-08-20 for the publish preflight. The panel and the deploy
    # holiday are two jobs in one file; publish/panel.cpp vs publish/deploy.cpp
    # is the next move here.
    'src/publish/publish.cpp': 1050,
    'src/domain/hormiga_allomone.cpp': 900,
    # NEW ENTRY 2026-08-28 (was on the 1000 default): two effects the field
    # report asked for -- `query`, which is `ls --tag` plus the clock because
    # Void Core's verb cannot know what today is, and `read-flier`, which runs
    # the operator's own recognizer and proposes tags without dispatching any.
    # Both are argument handling and refusals, which is what this file is for;
    # everything either of them actually computes lives in a header a test can
    # reach (domain/date_query.hpp, domain/flier_read.hpp).
    # RAISED 2026-09-01, 1060 -> 1120: two things. (1) `render_from_state` no longer inherits
    # the GUI's `cur_doc = "issue-demo"` initialiser when no document is named.
    # Thirteen of the fourteen lines are the comment explaining why, and it
    # earns the room: the old behaviour rendered a newsletter as a website and
    # reported success, which cost the field agent a render and was reported to
    # us as a build regression. (2) `effect pack-database` — the .miga bundle
    # could only be made from the GUI, so an agent-run database could not
    # produce one; the Cat Colony demo folder needed exactly that.
    # 1120 -> 1140 (2026-09-01): the bare run announces that it is creating an
    # empty database. It was the silent half of the two-copies-of-one-database
    # bug — `--state` at least names a path somebody typed.
    'src/main/headless.cpp': 1140,
}


def main() -> int:
    strict = '--strict' in sys.argv
    root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
    over, rows = [], []
    for dirpath, _d, files in os.walk(os.path.join(root, 'src')):
        for fn in sorted(files):
            if not fn.endswith(('.cpp', '.hpp')):
                continue
            p = os.path.join(dirpath, fn)
            rel = os.path.relpath(p, root).replace('\\', '/')
            with open(p, encoding='utf-8', errors='replace') as f:
                n = sum(1 for _ in f)
            budget = BUDGET.get(rel, DEFAULT)
            rows.append((n, rel, budget))
            if n > budget:
                over.append((rel, n, budget))

    rows.sort(reverse=True)
    for n, rel, budget in rows[:12]:
        flag = '  OVER' if n > budget else ''
        print('  %5d / %-5d  %s%s' % (n, budget, rel, flag))

    if over:
        print('\n%d file(s) over budget:' % len(over))
        for rel, n, budget in over:
            print('  %s: %d > %d' % (rel, n, budget))
        print('Split it, or — if the growth is genuinely warranted — raise its '
              'budget in tools/find_long.py and say why in the commit.')
        return 1 if strict else 0
    print('\nall %d source files within budget' % len(rows))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
