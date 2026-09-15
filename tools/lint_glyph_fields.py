"""lint_glyph_fields.py — a declared field is a promise; check both domains keep it.

THE BUG THIS EXISTS FOR, in full, because it is the argument for the tool:

On 2026-08-19 the website renderer printed 11 of 13 event cards as rune slugs
and 0 of 13 with a summary, and the Spanish page printed the English cards.
Every field involved — `title_en`, `summary_en`, `detail`, `limit`, `sort`,
`color` — was declared on the glyph, advertised by `--describe`, editable in the
Builder's inspector, and honoured by the EMAIL renderer. The work had landed for
one domain and stopped there, in a file so large nobody noticed the two halves
had diverged. `caption_en`/`caption_es` was the same failure from the other
side: declared on three grids and rendered by none of them.

The rule that came out of it is in `okf/concepts/sections/blocks-and-domains.md`:

    a declared field is a promise made by the glyph, and every domain that
    renders the glyph owes it.

A promise a machine can check should be checked by a machine. This reads the
glyph declarations out of `src/domain/seed.hpp` and asks, for each renderer,
whether every declared field is mentioned somewhere in the code that renders
that glyph.

IT IS A SMELL DETECTOR, NOT A PROVER. A field named in a comment counts as
mentioned, and a field read through a computed key would be missed. It is
looking for the specific failure that actually happened — a whole field
forgotten in one of two places — and for that it is exact enough.

    python tools/lint_glyph_fields.py           # report
    python tools/lint_glyph_fields.py --strict  # non-zero if anything is missing
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))

# the renderers, and the glyph families each is responsible for
# A domain may be SEVERAL files. It became one on 2026-08-21, when the clearance
# gate moved into `published.hpp` so the page renderer and the data index could
# share it, and the index became its own unit. This linter briefly reported
# three false positives for that — `organization.abbreviation` and friends were
# still rendered, just no longer mentioned in `site.cpp` — which is the linter
# working correctly against a stale map of where the web domain lives.
DOMAINS = {
    'email (render/email.cpp)': ['src/render/email.cpp'],
    'web   (render/site.cpp)': ['src/render/site.cpp',
                                'src/render/published.hpp',
                                'src/render/index.cpp'],
}

# fields that are deliberately not rendered anywhere, with the reason
EXEMPT = {
    'notes': 'internal-notes class: never reaches an output (CLAUDE.md rule 6)',
    'row': 'layout, consumed by the grid driver', 'col': 'layout',
    'span': 'layout', 'page': 'layout', 'link_to': 'layout',
    # band_bg / band_full / band_image left this list 2026-09-15: both domains
    # render bands now (render/email_theme.hpp, "A band, in an inbox")
    'band_filter': 'CSS filter over a band photo; email has no filter property',
    'ref': 'editor-only fan-out parent', 'ref_off': 'editor-only',
    'geo': 'map/territory facet', 'geo1': 'map', 'geo2': 'map',
    'image_url': 'legacy', 'icon_url': 'legacy',
    'display': 'web-only presentation mode',
    'transcript': 'submission payload, reviewed not rendered',
    'evidence': 'submission payload', 'actor': 'submission provenance',
    'actor_email': 'submission provenance', 'received': 'submission',
    'decided': 'submission', 'state': 'submission / deployment status',
    'note': 'reviewer note', 'vendor_id': 'deployment bookkeeping',
    'theme_accent': 'document theme socket', 'theme_bg': 'document theme socket',
    # ── web-only by PHYSICS, not by omission ───────────────────────────────
    'image_filter': 'CSS filter over a banner; email has no filter property',
    'image_dim': 'CSS custom property; email has no variables',
    'mode': 'the calendar widget is JS; email renders a static table',
    'path': 'a local file. A mail client cannot fetch one — email uses `url`, '
            'which is why publish_image exists at all',
    'avatar': 'a local image path; the same reason as `path`. A directory in '
              'email prints names and roles, not photographs',
    'phone': 'directory in email is deliberately name+role+bio only — an email '
             'is forwarded far more casually than a page is linked',
    'website': 'same: the email directory stays short by design',
    'search': 'the filter box is injected by app.js; email has no JS',
    # -- the calendar hub bookkeeping fields (X3, 2026-09-11) --------------
    'ext_uid': 'the UID the SOURCE calendar gave an imported entry. It is an '
               'identity key, and the only thing `effect import-ics` matches '
               'on, so that a re-import updates rather than duplicates. '
               'Publishing it would print another system opaque identifier at '
               'a reader, which tells them nothing and tells a scraper which '
               'feed the organization subscribes to',
    'rrule': 'the recurrence rule exactly as the source calendar wrote it, '
             'kept so that an import loses nothing. Not rendered because a '
             'renderer must not print FREQ=MONTHLY;BYDAY=3TU at a reader. '
             'Turning it into occurrences on the grid is C3a; when that lands '
             'what gets rendered is the occurrences, not this string',
}


# glyphs no renderer is expected to draw
SKIP_GLYPHS = {
    'page', 'document', 'submission', 'deployment', 'map', 'mapshape',
    'refpoint', 'note', 'resource', 'incident', 'org_core',
}


def glyph_decls(text):
    """(glyph, [fields]) for every register_glyph / block() in seed.hpp."""
    out = []
    # `"glyph":"x" ... "fields":[...]` — the raw-string concatenation means we
    # work on the file text with the C++ quoting stripped
    flat = re.sub(r'R"__?\(|\)__?"|R"\(|\)"', '', text)
    flat = flat.replace('"\n', '').replace('\n', ' ')
    for m in re.finditer(r'"glyph"\s*:\s*"([a-z_0-9]+)"\s*,.*?"fields"\s*:\s*\[(.*?)\]',
                         flat):
        fields = re.findall(r'"([a-z_0-9]+)"', m.group(2))
        out.append((m.group(1), fields))
    # the block() helper: block("glyph", "label", R"("a","b")", ...)
    for m in re.finditer(r'block\(\s*"([a-z_0-9]+)"\s*,\s*"[^"]*"\s*,\s*(.*?),\s*"#',
                         flat, re.S):
        fields = re.findall(r'"([a-z_0-9]+)"', m.group(2))
        if fields:
            out.append((m.group(1), fields))
    merged = {}
    for g, fs in out:
        merged.setdefault(g, set()).update(fs)
    return merged


def main():
    strict = '--strict' in sys.argv
    seed = open(os.path.join(ROOT, 'src/domain/seed.hpp'),
                encoding='utf-8', errors='replace').read()
    glyphs = glyph_decls(seed)

    sources = {}
    for label, rels in DOMAINS.items():
        text = ''
        for rel in rels:
            p = os.path.join(ROOT, rel)
            if os.path.exists(p):
                text += open(p, encoding='utf-8', errors='replace').read()
        sources[label] = text
    # helpers both renderers share count as "mentioned"
    for extra in ('src/render/text.hpp', 'src/render/theme.cpp',
                  'src/render/assets.cpp'):
        p = os.path.join(ROOT, extra)
        if os.path.exists(p):
            shared = open(p, encoding='utf-8', errors='replace').read()
            for k in sources:
                sources[k] += shared

    problems = 0
    for glyph in sorted(glyphs):
        if glyph in SKIP_GLYPHS:
            continue
        # only report on glyphs a renderer actually handles
        handled = {lab: ('"%s"' % glyph) in src for lab, src in sources.items()}
        if not any(handled.values()):
            continue
        for field in sorted(glyphs[glyph]):
            base = re.sub(r'_(en|es)$', '', field)
            if base in EXEMPT or field in EXEMPT:
                continue
            missing = [lab for lab, src in sources.items()
                       if handled[lab]
                       and ('"%s"' % field) not in src
                       and ('"%s"' % base) not in src]
            if missing:
                problems += 1
                print('  %-14s %-16s not rendered in: %s'
                      % (glyph, field, ', '.join(missing)))

    if problems:
        print('\n%d declared field(s) a renderer does not mention.' % problems)
        print('Either render it, or add it to EXEMPT with the reason.')
        return 1 if strict else 0
    print('every declared field is mentioned by every domain that renders its glyph')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
