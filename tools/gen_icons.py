import io, os, re, glob

import sys
SRC = sys.argv[1] if len(sys.argv) > 1 else 'icons-src'
OUT = 'vendor/icons'
os.makedirs(OUT, exist_ok=True)

# the license, verbatim
lic = io.open(os.path.join(SRC, 'LICENSE'), encoding='utf-8').read()
io.open(os.path.join(OUT, 'LICENSE-lucide.txt'), 'w', encoding='utf-8',
        newline='').write(lic)

rows = []
for path in sorted(glob.glob(os.path.join(SRC, '*.svg'))):
    name = os.path.basename(path)[:-4]
    s = io.open(path, encoding='utf-8').read()
    # keep only what is INSIDE <svg>…</svg>: the wrapper is identical for every
    # icon and is emitted once by the renderer, so storing it 41 times would be
    # 41 copies of the same nine attributes.
    i = s.index('>', s.index('<svg')) + 1
    j = s.rindex('</svg>')
    inner = s[i:j]
    inner = re.sub(r'\s+', ' ', inner).strip()
    inner = inner.replace('" />', '"/>').replace('> <', '><')
    rows.append((name, inner))

hdr = '''/* lucide_icons.hpp — a curated subset of Lucide, vendored.
 *
 * Lucide (https://lucide.dev) under the ISC License — see LICENSE-lucide.txt
 * beside this file. Vendored, not depended on: CLAUDE.md rule 5, and for the
 * web renderer specifically there is a second reason — a deployed Hormiga site
 * carries its own assets and reaches no CDN, so an icon that arrives over
 * somebody else's network is an icon that disappears when they turn it off.
 *
 * WHY PATH DATA AND NOT A FONT. The GUI already ships Font Awesome as a TTF,
 * which is right for ImGui (one glyph, one draw call). It is wrong for a
 * website: an icon font is a download that blocks first paint, renders as a
 * box when it fails, and is invisible to a screen reader. Inline SVG is none of
 * those — it inherits `currentColor`, scales with the type, costs no request,
 * and can be given a title.
 *
 * Stored as the INNER markup only. Every Lucide icon has the identical
 * `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" …>` wrapper, so
 * the renderer emits that once and this table holds only what differs.
 *
 * GENERATED — do not hand-edit. To add an icon, fetch it from the Lucide repo
 * and re-run the generator noted in okf/log.md (2026-08-20).
 */
#pragma once
#include <string>
#include <string_view>

namespace hormiga::icons {

struct Icon {
    const char* name;
    const char* body; // the inner markup of a 24x24 stroke icon
};

inline constexpr Icon kIcons[] = {
'''

for name, inner in rows:
    esc = inner.replace('\\', '\\\\').replace('"', '\\"')
    hdr += '    {"%s", "%s"},\n' % (name, esc)

hdr += '''};
inline constexpr int kNumIcons = (int)(sizeof(kIcons) / sizeof(*kIcons));

/* The inner markup for a name, or "" when we do not carry that icon.
 *
 * An unknown name renders NOTHING rather than a placeholder box, deliberately:
 * a missing icon should cost a website a little decoration, never a visible
 * defect on a page a stranger is reading. */
inline std::string_view body(std::string_view name) {
    for (const Icon& i : kIcons)
        if (name == i.name) return i.body;
    return {};
}

/* One inline `<svg>`, sized in `em` so it rides the type scale rather than
 * fighting it, and coloured by `currentColor` so it inherits whatever the text
 * around it is — which is what makes the contrast work of theme.contrast apply
 * to icons for free.
 *
 * `aria-hidden` because these sit BESIDE their own label in every place Hormiga
 * uses them. An icon that repeats the adjacent word to a screen reader is
 * noise, and one that replaces it is a bug. */
inline std::string svg(std::string_view name, const char* cls = "ico") {
    const std::string_view b = body(name);
    if (b.empty()) return {};
    return std::string("<svg class=\\"") + cls +
           "\\" viewBox=\\"0 0 24 24\\" fill=\\"none\\" stroke=\\"currentColor\\" "
           "stroke-width=\\"2\\" stroke-linecap=\\"round\\" "
           "stroke-linejoin=\\"round\\" aria-hidden=\\"true\\" focusable=\\"false\\">" +
           std::string(b) + "</svg>";
}

} // namespace hormiga::icons
'''

io.open(os.path.join(OUT, 'lucide_icons.hpp'), 'w', encoding='utf-8',
        newline='').write(hdr)
print('wrote %d icons' % len(rows))
