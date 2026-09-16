"""lint_host_seams.py — a replaced core must be given its host seams back.

THE BUG THIS EXISTS FOR (2026-09-16). The author, mid-session: *"error when
trying to build and publish. i was able to build a couple times, but then it
stopped working"*, with a toast reading

    no host effect handler for 'effect' (register one via vc_set_effect_handler)

and — the part that made it hard to place — *"for some reason this error message
isn't in the console log?"*

`HormigaApp::install_host()` puts TWO things on a core: the effect handler and
the log sink. Four helpers (`referenced_files`, `translation_report` and two
sync paths) booted "their own" core by assigning over the RUNNING one and
re-registering the glyphs only. After any of them — bundling a database is
enough — every effect answered that message forever, and the core's own warning
had no sink to reach the console with. Both symptoms, one cause.

Those four now use a local core, which is what their comments always claimed.
This linter is the guard: it is a class of bug that compiles, passes every test,
and only shows up as an application that half works after an unrelated action.

    python tools/lint_host_seams.py           # report
    python tools/lint_host_seams.py --strict  # non-zero if anything is unseamed

THE RULE. An assignment to the app's own `core` must be followed, within a few
lines, by `install_host()`. A helper that wants a core of its own should declare
one (`maiz::Core probe(state_json)`) rather than assign over the member — and
that reads better anyway, which is the point of keeping the rule this narrow.

THE EXCEPTION, written down rather than assumed: a comment carrying `NO HOST
SEAMS` within the few lines above the assignment exempts it, and is expected to
say why that core never serves an effect. The CLI has the only one — it renders
through a throwaway `HormigaApp` whose core is nobody's host.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))
SRC = os.path.join(ROOT, 'src')

# `core = maiz::Core(...)` — an assignment over the member, not a declaration
ASSIGN = re.compile(r'(?<![\w.>])core\s*=\s*maiz::Core\s*\(')
SEAMS = 'install_host()'
ALLOW = 'NO HOST SEAMS'  # an exemption that has to say why, in the code
LOOKAHEAD = 8  # lines: the reinstall belongs with the assignment, not far below
LOOKBEHIND = 6  # lines: where the exemption comment is allowed to sit


def main():
    strict = '--strict' in sys.argv
    checked, exempt, bad = 0, 0, []
    for dirpath, _, files in os.walk(SRC):
        for f in sorted(files):
            if not f.endswith('.cpp'):
                continue
            path = os.path.join(dirpath, f)
            rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
            lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
            for i, line in enumerate(lines):
                code = line.split('//')[0]
                if code.lstrip().startswith('*') or not ASSIGN.search(code):
                    continue
                if ALLOW in '\n'.join(lines[max(0, i - LOOKBEHIND):i + 1]):
                    exempt += 1
                    continue
                checked += 1
                window = '\n'.join(lines[i:i + 1 + LOOKAHEAD])
                if SEAMS not in window:
                    bad.append((rel, i + 1, line.strip()))
    for rel, n, text in bad:
        print('  %s:%d  %s' % (rel, n, text[:90]))
    if bad:
        print('\n%d core replacement(s) without %s within %d lines.' %
              (len(bad), SEAMS, LOOKAHEAD))
        print('A core without its host seams answers "no host effect handler" to\n'
              'every effect, and its log sink is gone too, so nothing says why.\n'
              'Either call install_host() after the assignment, or - usually the\n'
              'right answer - give the helper its own local core instead of\n'
              'assigning over the app\'s. A core that genuinely hosts nothing says\n'
              'so in a comment carrying %s, with the reason.' % ALLOW)
        return 1 if strict else 0
    print('all %d core replacement(s) reinstall the host seams'
          '%s' % (checked, ', %d exempt' % exempt if exempt else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
