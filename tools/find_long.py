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
    # 2850 -> 2890 (2026-09-02): the unsaved-work counter, at the one door every
    # GUI edit passes through. The author asked for a Save button in the Builder
    # tab; the writing already existed, so what the button needed was an answer
    # to "is there anything to save", and that answer is only trustworthy if it
    # is computed where every edit lands. Twenty of the forty lines are the
    # comment explaining why the count deliberately over-reports. The named
    # split (boot / frame / effects) is still the right next move for this file.
    # 2890 -> 2900 (2026-09-02): icons on the section tabs and the dockable
    # windows, with the `###StableId` note that is the whole reason it did not
    # reset everybody's saved dock layout.
    # 2900 -> 2915 (2026-09-02): the note explaining why window titles carry no
    # icons -- an `###` rename orphaned every `imgui.ini` entry and aborted the
    # app at boot. That is a trap worth twenty lines so nobody re-adds it.
    # 2915 -> 2940 (2026-09-04): the update client's two call sites -- one
    # line in `frame()` that drains a finished check, one at the very end of
    # `init()` that may offer an update. Both are three lines of code and
    # twenty of comment, and the comments are the point: the near-miss they
    # record is that the obvious guard for "is a person looking at this?" was
    # `on_shell_capture`, which the HEADLESS front-end also sets -- an agent
    # asking for a newsletter would have made a network request.
    'src/app/app.cpp': 2940,        # the shell: split boot / frame / effects next
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
    # RAISED 2026-09-02, 1866 -> 2060: the Click LaFont field report's render-seam
    # fixes. D1 (the `<noscript>` without which every page this renderer has ever
    # produced was invisible with JavaScript off), D4 (`narrative` rendered the
    # same way in both output domains — `prose()` plus `pre-line`, so the
    # AGENT-GUIDE's linkification claim is true of the website too), A6's
    # `image_grid.columns` (declared, labelled, offered in the inspector, and
    # read by nothing until now) and its coloured `divider_style bar`, a share
    # title for `og:title`, the `custom.css` link that is Part 3's whole ask, and
    # the per-render translation-coverage count that replaced the report's
    # request for a way to publish one language.
    #
    # WHAT LEFT RATHER THAN GREW: the swatch colour validation went to
    # `render/text.hpp` as `css_colors` (pure, and a test can hand it
    # `#fff; background:url(x)`), the custom-stylesheet staging and the
    # wrong-mantle warning to `render/assets.cpp`. What is left is the emit
    # lambda, which is what the note below has said twice is the real answer to
    # this file and which still does not move a piece at a time.
    #   NEXT SPLIT: render/blocks.cpp (per-glyph emit) away from
    #   render/site.cpp (the page shell). Then ~700.
    # The golden-render test hashes every output file, so that split is
    # verifiable byte-for-byte when it is done on its own.
    # RAISED 2026-09-02, 2060 -> 2115: a deleted page has to leave `site/`. The
    # renderer wrote one file per page and removed nothing, so deleting a `page`
    # rune took it out of the nav, the sitemap and the model and left its HTML
    # in the folder the deploy uploads -- the old page stayed live at its old
    # URL forever. `site/` is a mirror of the document, not an accumulation of
    # every render that ever ran. The named split below is still the answer to
    # this file; this is not a licence.
    # RAISED 2026-09-02, 2115 -> 2160: the `audio` block (field report A1, first
    # rung -- "a music artist's website cannot play the artist's music", and the
    # same absence for an organization with a podcast or a recorded meeting).
    # The MARKUP is not here: it went to `render/audio.hpp` beside its own
    # concern, the precedent `render/video.hpp` set, and what is left is what
    # only this function can do -- resolve the cover rune, stage the file, pick
    # the language, and say which of the two silences it hit. The named split
    # below is still the answer to this file.
    # RAISED 2026-09-02, 2160 -> 2225: the `download` block (portfolio report
    # A1, which was blocking that client's first deploy). The MARKUP and the
    # refusal list are not here -- they went to `render/download.hpp`, and the
    # `site.languages` check to `app/translate.cpp`. What is left is what only
    # this function can do: resolve `file` through a `resource` rune, stage it,
    # measure it, and say which of the three silences it hit.
    # RAISED 2026-09-03, 2225 -> 2270: `hero.portrait` (a round inset in front
    # of the banner, which deletes 23 lines of a client's custom.css),
    # `narrative.heading_*`, and threading the page's LANGUAGE into the two
    # blocks that publish an organization's own prose -- `directory` and the
    # image captions -- so a bilingual site's content can be bilingual at all.
    # The last one is the author's `site.languages` commitment applied one layer
    # down, and the reusable half (`lang_text`) went to render/text.hpp.
    # RAISED 2026-09-08, 2270 -> 2295: platform sets -- reading a block's
    # `platform`, and deciding at build time whether a grid row is a set of
    # per-platform downloads. Twenty-three lines, and everything that COULD
    # leave already did: the vocabulary, the `data-platform` attribute and the
    # complaint about an unknown value all live in render/download.hpp, and the
    # behaviour is app.js's. What is left here is the part only the driver can
    # do, because only the driver knows what shares a row.
    'src/render/site.cpp': 2295,
    'src/domain/allomone_legacy.hpp': 900,  # unshipped, frozen
    # RAISED 2026-08-20 for the publish preflight. The panel and the deploy
    # holiday are two jobs in one file; publish/panel.cpp vs publish/deploy.cpp
    # is the next move here.
    # RAISED 2026-09-02, 1050 -> 1110: the GitHub Pages host (field report A5,
    # and roadmap phase E's second deploy holiday). `deploy_site` and
    # `rollback_site` learned a second kind of host node, and `check_host` — the
    # sibling of `check-store` for the other one-way door — is new. Two helpers
    # were lifted OUT of the two functions that had a copy each (`find_host`,
    # `host_token`), which is also what fixed rollback silently ignoring the
    # vault. The named split (panel.cpp vs deploy.cpp) already happened for the
    # panel; deploy vs rollback vs check is the next one.
    'src/publish/publish.cpp': 1120,
    # NEW ENTRY 2026-09-02 (was on the 1000 default): `stage_custom_css`,
    # `translation_report` and `check_host` are three declarations with the
    # comment each of them needs to be used correctly. This header is the
    # application's whole surface and it grows one method at a time; the split
    # when it is due is by section (render / publish / sync), not by size.
    # 1040 -> 1070 (2026-09-02): document rename/delete, the shared rune-rename
    # control, the org image picker and the rune-minting ingest -- each a
    # declaration plus the comment it needs to be used correctly (the rename one
    # returns a bool that means "your node reference is now dangling", which is
    # not a thing to leave to a caller's memory). This header is the
    # application's whole surface and grows one method at a time; the split when
    # it is due is by section (render / publish / sync), not by size.
    # 1070 -> 1150 (2026-09-04): the update client's state -- a modal enum, a
    # job struct whose entire synchronisation is one atomic, the fields a prompt
    # draws from, and a cached view of the preferences file (the Settings window
    # is open by default, and reading and parsing a file on disk once per frame
    # is not a thing to do to somebody's laptop). It is state and not logic on purpose: every
    # decision the update client makes lives in `src/update/`, which links no
    # window and no Void Core, so that it still works on a machine where those
    # are the thing that is broken. Roughly half the growth is the comment
    # recording a near-miss -- the obvious guard for "is a person looking at
    # this?" was `on_shell_capture`, which the HEADLESS front-end also sets, so
    # an agent asking for a newsletter would have made a network request.
    # 1150 -> 1160 (2026-09-10): the Calendar's UX pass — six members for the
    # quick-add box, the jump-to-date buffer, the two "which cell is expanded"
    # indices that replaced silently-truncated entry lists, and the time grid's
    # full-day toggle. The alternative was clawing four lines back out of
    # unrelated declarations to stay under a number, which makes the file worse
    # rather than smaller. The calendar code itself did NOT take this route:
    # `calendar.cpp` went 898 -> 1339 in the same pass and was split into
    # `calendar_toolbar.cpp` and `calendar_export.cpp` (and a pure, testable
    # `domain/quick_add.hpp`) rather than given a bigger budget — which is the
    # distinction this table is for. A class declaration grows when the class
    # grows; a 1,300-line function file has a seam in it.
    'src/app/app.hpp': 1160,
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
    # 1140 -> 1210 (2026-09-02): two effects the field report asked for --
    # `check-host` (A5: "the sibling of check-store … tell the operator whether
    # the token, the account id and the project name line up, BEFORE a deploy is
    # attempted") and `translation-report`, which is the author's answer to A3.
    # Argument handling and docstrings, which is what this file is for;
    # everything either of them computes lives elsewhere (publish/publish.cpp,
    # app/translate.cpp).
    # 1210 -> 1230 (2026-09-04): the `update` verb's two-line dispatch, plus the
    # comment for why it runs before any session exists -- the same argument as
    # `--restore-backup`: a recovery tool that requires a working system is not
    # a recovery tool. The verb ITSELF went to `src/update/cli.cpp` rather than
    # here, which is the ratchet working: 130 lines landed in the folder they
    # belong to instead of on this file's total.
    'src/main/headless.cpp': 1230,
    # NEW ENTRY 2026-09-02 (was on the 1000 default): the Data tab lost 168 lines
    # to `ui/widgets.cpp` the same afternoon -- the rune-rename control (shared
    # with the Notes tab, which is why a note could not be renamed at all), the
    # org image picker and the rune-minting ingest, none of which are the Data
    # SECTION. What is left grew by the comments on two real defects: the note
    # name that was `TextDisabled`, and the link relation that was concatenated
    # into a command line, where "goes with" silently became "goes" and an
    # apostrophe produced a SPEC §6.1 quoting error at somebody typing a word
    # into a text box.
    'src/ui/data.cpp': 1020,        # icons in "+ New"; the clamp_fit note
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
