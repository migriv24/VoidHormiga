"""lint_i18n.py — English hiding in a renderer.

THE BUG THIS EXISTS FOR: the Spanish page printed *"This directory is empty.
(Entries appear once they are marked as public.)"* — in English, on the half of a
bilingual site whose readers may not read English. Reported by the field agent
**three times** before it was fixed, because it is invisible to anyone testing
in English and invisible in a diff, and because a string the RENDERER writes
looks nothing like a string the AUTHOR writes.

An empty state is also the page a person sees on the day nothing has been
published yet — exactly when a site is being judged.

So: every sentence a visitor can read must come from a language-selected source
(`ui(...)`, `text(...)`, `text_or(...)`) or be listed here as chrome that has no
language. This flags prose-shaped literals in the render units that reach
neither.

Prose-shaped means: contains a space, contains a letter, is not obviously
markup, CSS, a URL or a format string. Deliberately conservative — a linter that
cries wolf on `"</p>"` is one people switch off.

    python tools/lint_i18n.py           # report
    python tools/lint_i18n.py --strict  # non-zero if anything is unselected
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))
FILES = ['src/render/site.cpp', 'src/render/email.cpp']

# language-selected already, or not visitor-facing
SELECTORS = ('ui(', 'text(', 'text_or(', 'title_of(', 'display_name(',
             'human_date(', 'lang == "es"', 'lang == "en"')

# ONLY text that is streamed into the page body can be read by a visitor. A
# literal anywhere else in these files is a CSS fragment, an attribute name, a
# config key, or a message for the OPERATOR — and an operator message staying in
# English is correct, not a bug. Narrowing to the output streams is what takes
# this from 60 false positives to the handful that matter.
EMITS = re.compile(r'\b(h|html|sm|nf|rt)\s*<<')

# not prose: markup, style, urls, format strings, machine values
NOT_PROSE = re.compile(
    r'^[\s<>/=;:,.\-#%{}()\[\]0-9a-fA-F&;]*$'     # punctuation/markup only
    r'|^\s*(https?|mailto|data):'                  # a URL
    r'|[<>]'                                       # any tag or attribute chunk
    r'|^[a-z-]+:[^ ]*$'                            # a css declaration
    r'|%[sdfx]'                                    # a format string
    r'|^[A-Za-z-]+$'                               # a single word (class, id)
    r'|^[a-z_]+ [a-z_]+$'                          # two identifiers, not prose
)

# visitor-facing English that is deliberately not translated, with the reason
ALLOWED = {
    'Built with Void Hormiga': 'the generator credit, a proper noun',
    'Page not found': '404 chrome; the 404 is emitted once, not per language',
    "That page doesn't exist (or moved).": '404 chrome, as above',
    'Back home': '404 chrome, as above',
    'add to Google Calendar': 'names a Google product',
    'Add to Google Calendar': 'names a Google product',
}


def main():
    strict = '--strict' in sys.argv
    hits = []
    for rel in FILES:
        p = os.path.join(ROOT, rel)
        if not os.path.exists(p):
            continue
        for i, line in enumerate(open(p, encoding='utf-8', errors='replace'), 1):
            st = line.strip()
            if st.startswith(('//', '*', '/*')):
                continue
            if any(sel in line for sel in SELECTORS):
                continue
            if not EMITS.search(line):
                continue
            for lit in re.findall(r'"((?:[^"\\]|\\.){4,})"', line):
                # an escaped quote inside the literal means it is a chunk of an
                # HTML attribute (`\" href=\"`), never a sentence
                if '\\"' in lit:
                    continue
                text = lit.replace('\\n', ' ').strip()
                if not text or ' ' not in text:
                    continue
                if not re.search(r'[A-Za-z]{3}', text):
                    continue
                if NOT_PROSE.search(text):
                    continue
                if text in ALLOWED:
                    continue
                hits.append('%s:%d  %s' % (rel, i, text[:70]))

    if hits:
        print('visitor-facing text that is not language-selected:')
        for h in hits:
            print('  ' + h)
        print('\n%d literal(s). Wrap in ui("en", "es"), or add to ALLOWED with '
              'the reason.' % len(hits))
        return 1 if strict else 0
    print('every visitor-facing string in the renderers is language-selected')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
