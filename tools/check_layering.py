"""check_layering.py — the folder boundaries, enforced.

`src/` grew folders on 2026-08-20. Folders that are only a convention drift back
into a pile within a month, so the rules that matter are checked here and run in
CI alongside the tests.

THE RULE THAT EARNS ITS KEEP is the first one. `render/` and `domain/` contain
no ImGui, which is what makes a headless render possible at all — measured on
2026-08-18, when adopting the CLI was cheap *because* the Output domain had been
view-free since it was written. That property was true by luck and by care; it
is now true by construction, and a `#include "imgui.h"` in a renderer fails the
build instead of quietly costing the next person their headless mode.

    python tools/check_layering.py          # non-zero on a violation
"""
import os
import re
import sys

# folder -> (things it may not include, why)
FORBIDDEN = {
    'render': ([r'imgui', r'GLFW', r'glad'],
               'the Output domain must stay view-free — it is what makes a '
               'headless render possible, and what keeps the newsletter the '
               'button produces identical to the one an agent produces'),
    'domain': ([r'imgui', r'GLFW', r'glad'],
               'glyphs, temper passes and importers are Scene-in / commands-out '
               'and are tested without a window'),
    'platform': ([r'\.\./ui/', r'"ui/'],
                 'the machine underneath does not reach up into the GUI'),
    # THE STRICTEST FOLDER IN THE TREE, and deliberately so. src/gis/ is the map
    # ENGINE, kept separable from the application that currently hosts it
    # (okf/concepts/foundation/application-boundaries.md, Q42). It may reach NOTHING but
    # itself and the standard library -- not the app, not Void Maiz, not Void
    # Core, not a window. A folder that merely happens not to depend on the app
    # today will depend on it by Thursday; this is the difference between a
    # boundary and a good intention.
    'gis': ([r'imgui', r'GLFW', r'glad', r'voidmaiz', r'voidcore', r'json\.hpp',
             r'"app/', r'"ui/', r'"render/', r'"domain/', r'"platform/',
             r'"publish/', r'\.\./app/', r'\.\./ui/'],
            'the map engine must stay liftable: it is the one part of the map '
            'that is genuinely separable, and it is only separable while it '
            'depends on nothing'),
    # src/sync/ IS WRITTEN TO BE DELETED (2026-08-27). Void Palabra's second
    # pillar is device-to-device transport and it is not built yet; this is the
    # stand-in, and okf/concepts/platform/collaboration.md §3 commits to
    # replacing it rather than growing it. A layer that is meant to be displaced
    # only stays cheap to displace while it knows nothing about the application:
    # it finds an address, proves an identity, moves bytes. The moment something
    # in here knows what a rune is, the swap becomes a refactor.
    # src/update/ IS THE FOLDER THAT HAS TO WORK WHEN NOTHING ELSE DOES. The
    # one machine you cannot attach a debugger to is the one an update broke,
    # and the ordinary reason somebody wants a newer Hormiga is that this one
    # will not open their database. So it depends on the standard library,
    # libsodium and the vendored JSON, and on no part of this application: no
    # window, no Void Core, no session, no org. Every decision in it is
    # reachable from `voidhormiga-cli update` and from tests/update_smoke.cpp.
    'update': ([r'imgui', r'GLFW', r'glad', r'voidmaiz', r'voidcore',
                r'"app/', r'"ui/', r'"render/', r'"domain/', r'"platform/',
                r'"publish/', r'"sync/', r'\.\./app/', r'\.\./ui/'],
               'the update client must stay runnable on a broken install: it '
               'is what a person reaches for when the application itself is '
               'the thing that is wrong'),
    'sync': ([r'imgui', r'GLFW', r'glad', r'"app/', r'"ui/', r'"render/',
              r'"domain/', r'"platform/', r'"publish/', r'\.\./app/', r'\.\./ui/'],
             'the sync layer is a stand-in for Void Palabra Phase 4 and must '
             'stay ignorant of the application, so replacing it is two call '
             'sites rather than a refactor'),
}

# layers a folder may depend on, beyond its own and the vendored/library ones
ALLOWED = {
    # gis depends on ITSELF ONLY -- see FORBIDDEN above. Everything else may use
    # it, because a world's projection and metric are facts the whole
    # application needs and should ask one place for.
    'gis': {'gis'},
    # sync depends on ITSELF ONLY, for the reason in FORBIDDEN above.
    'sync': {'sync'},
    # update depends on ITSELF ONLY -- see FORBIDDEN above.
    'update': {'update'},
    'domain': {'domain', 'gis'},
    'render': {'render', 'domain', 'app', 'gis'},
    'platform': {'platform', 'domain', 'gis'},
    'publish': {'publish', 'render', 'domain', 'app', 'gis'},
    'ui': {'ui', 'render', 'domain', 'app', 'platform', 'gis', 'sync', 'update'},
    'app': {'app', 'domain', 'render', 'platform', 'ui', 'gis', 'sync', 'update'},
    # the two front-ends: adapters, so they may reach anything. There are
    # exactly two and they are peers — founding commitment 1, in the tree.
    'main': {'main', 'app', 'domain', 'render', 'platform', 'publish', 'ui',
             'gis', 'sync', 'update'},
}


def main() -> int:
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src')
    root = os.path.normpath(root)
    bad = []
    for dirpath, _dirs, files in os.walk(root):
        rel = os.path.relpath(dirpath, root).replace('\\', '/')
        layer = rel.split('/')[0]
        if layer in ('.', ''):
            continue
        for fn in files:
            if not fn.endswith(('.cpp', '.hpp')):
                continue
            path = os.path.join(dirpath, fn)
            where = '%s/%s' % (rel, fn)
            with open(path, encoding='utf-8', errors='replace') as f:
                text = f.read()
            includes = re.findall(r'#\s*include\s*[<"]([^">]+)[">]', text)

            pats, why = FORBIDDEN.get(layer, ([], ''))
            for inc in includes:
                for p in pats:
                    if re.search(p, inc, re.I):
                        bad.append('%s includes %s\n      %s' % (where, inc, why))

            allowed = ALLOWED.get(layer)
            if allowed:
                for inc in includes:
                    top = inc.split('/')[0]
                    if top in ALLOWED and top not in allowed:
                        bad.append('%s includes %s — %s may not depend on %s'
                                   % (where, inc, layer, top))

    if bad:
        sys.stderr.write('layering violations:\n')
        for b in bad:
            sys.stderr.write('  - %s\n' % b)
        return 1
    print('layering ok')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
