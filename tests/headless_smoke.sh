#!/usr/bin/env bash
# The headless front-end, end to end: an agent adds data, builds a newsletter,
# and the newsletter contains the data. Run by ctest as hormiga_headless_smoke.
#
# WHY A SHELL TEST AND NOT A C++ ONE. What is under test is the BINARY and its
# argument surface — the state-file default, the effect gate, the exit codes an
# agent branches on. A C++ test linking the same objects would exercise none of
# that, and those are exactly the parts that broke during adoption.
set -u
CLI="$1"
# resolved BEFORE the cd below, for the same reason $CLI must be absolute
LINT_NFC="$(cd "$(dirname "$0")/.." && pwd)/tools/lint_nfc.py"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cd "$WORK" || exit 2
fail=0
check() { if [ "$1" = "0" ]; then echo "  ok   $2"; else echo "  FAIL $2"; fail=1; fi }

"$CLI" --describe > brief.json 2>/dev/null
check $? "--describe runs"
grep -q '"app"' brief.json && grep -q '"glyphs"' brief.json \
  && grep -q '"predicates"' brief.json && grep -q '"effects"' brief.json
check $? "the briefing carries identity, glyphs, predicates and effects"

# The state file must be the one the GUI opens, not <id>.state.json.
cat > work.txt <<'EOF'
mantle new demo-org
use demo-org
rune new contact maria-lopez
set maria-lopez role 'Outreach Coordinator'
rune new event posada-2026
set posada-2026 date '2026-12-12'
set posada-2026 venue 'Community Center'
tag posada-2026 +type:event +community-outreach
EOF
"$CLI" --script work.txt --atomic --actor test >/dev/null 2>&1
check $? "an atomic script batch applies"
[ -f demo-org.json ]
check $? "it writes demo-org.json (the file the GUI opens), not hormiga.state.json"
[ -f demo-org.json.log ]
check $? "a journal survives the process"

"$CLI" status 2>/dev/null | grep -q "added 2"
check $? "a later session sees the work as unsaved changes against the baseline"

# The effect gate: refused by default, and the refusal names the consequence.
out=$("$CLI" effect render en 2>&1)
echo "$out" | grep -qi "refused"
check $? "effects are refused by default"
echo "$out" | grep -q "nothing is sent to anyone"
check $? "...and the refusal quotes the op's consequence"
"$CLI" --dry-run-effects effect render en 2>&1 | grep -qi "dry run"
check $? "--dry-run-effects rehearses without running"

# A newsletter, rendered from a document the agent builds, filled by a query.
cat > news.txt <<'EOF'
mantle new outreach-dec
rune new hero masthead
set masthead title_en 'Riverton Community'
rune new section_header h
set h title_en 'Upcoming Events'
rune new event_grid ev
set ev query 'type:event AND community-outreach'
link masthead h --relation 1:1
link h ev --relation 2:1
EOF
"$CLI" --script news.txt --atomic --actor test >/dev/null 2>&1
check $? "a newsletter document builds"
"$CLI" --allow-effects=render effect render en outreach-dec >/dev/null 2>&1
check $? "--allow-effects permits the render"
NL=exports/preview-en.html
[ -f "$NL" ] && grep -q "Riverton Community" "$NL"
check $? "the newsletter lands in exports/ (what --describe has always claimed)"
# the date is both PRESENT (the query worked) and READABLE (2026-08-20: an ISO
# string is the right thing to store and the wrong thing to print)
grep -q "Dec 12" "$NL"
check $? "...and the QUERY BLOCK pulled the agent's event into it, dated legibly"

# ── the newsletter is a newsletter (2026-08-19) ─────────────────────────────
# Everything below was zero or wrong in a real issue a person actually read.
cat > look.txt <<'EOF'
use demo-org
set posada-2026 title_en 'Riverton Winter Posada'
set posada-2026 start_time '6:00 PM'
set posada-2026 end_time '9:00 PM'
set posada-2026 color '#16a34a'
set posada-2026 summary_en 'Details and RSVP at https://example.org/rsvp or write to team@example.org'
config set theme.accent '#2e6b4f'
use outreach-dec
rune new link cta
set cta label_en 'Reserve a seat'
set cta target 'https://example.org/rsvp'
set cta link_style 'button'
set cta row '9'
EOF
"$CLI" --script look.txt --atomic >/dev/null 2>&1
check $? "the aesthetics fixture applies"
"$CLI" --allow-effects=render effect render en outreach-dec > render.out 2>&1
check $? "it renders"

grep -q '<a href="https://example.org/rsvp"' "$NL"
check $? "a link block renders in EMAIL (it used to be dropped silently)"
grep -q 'text-decoration:none;font-weight:bold' "$NL"
check $? "...as a bulletproof table button, not a bare anchor"
grep -q 'mailto:team@example.org' "$NL"
check $? "a bare email address in prose is linkified"
grep -q '>https://example.org/rsvp</a>' "$NL"
check $? "a bare URL in prose is linkified"
grep -q '6:00 PM' "$NL" && grep -q '9:00 PM' "$NL"
check $? "start_time/end_time render on a DATED event"
grep -q '#16a34a' "$NL"
check $? "event.color reaches the card"
grep -q '#2e6b4f' "$NL"
check $? "the CONFIG theme accent reaches the email render"
grep -q 'Riverton Winter Posada' "$NL"
check $? "title_en is the display title (the rune name mangled acronyms)"
grep -qi 'words, about' render.out
check $? "the render reports word count and read time"

# A .miga bundle handed to --state must be REFUSED, not merged into. Regression
# guard for the P0 found by the first real agent run: the CLI saw no `mantles`
# key, called the document empty, and wrote a phantom empty document into the
# bundle's top level beside the envelope.
printf '{"magic":"MIGA","version":3,"meta":{},"assets":[],"state":{"mantles":[]}}' > b.miga
before=$(cat b.miga)
"$CLI" --state b.miga mantles >/dev/null 2>&1
[ $? -eq 2 ]
check $? "a .miga bundle is refused as a state document"
[ "$(cat b.miga)" = "$before" ]
check $? "...and the bundle is left byte-identical"

# The query grammar is EXACT and supports AND/OR/NOT via `ls --tag`. Pinned
# because a real run mistook `find` (substring search) for the query language
# and rebuilt a whole newsletter around single-purpose tags to dodge it.
# `ls --tag` lists the ACTIVE mantle, and the newsletter above left its own
# mantle active — which is itself worth knowing: an agent that builds a document
# and then queries the data will look in the wrong place.
"$CLI" use demo-org >/dev/null 2>&1
n=$("$CLI" ls --tag 'type:event AND community-outreach' 2>/dev/null | grep -c posada)
[ "$n" = "1" ]
check $? "ls --tag evaluates AND"
"$CLI" ls --tag 'type:event AND month:never' 2>/dev/null | grep -q "no matches"
check $? "...and a failing conjunct really excludes"

# ── the deploy holiday (2026-08-19) ────────────────────────────────────────
# Deploy is the one-way door. Every refusal below must be VISIBLE: a caller that
# cannot see why nothing happened is how "printed done, did nothing" started.
out=$("$CLI" effect deploy-site 2>&1)
echo "$out" | grep -qi "refused"
check $? "deploy-site is refused by default"
echo "$out" | grep -q "PUBLISHES THE WEBSITE"
check $? "...and the refusal says what publishing means"

"$CLI" --allow-effects=deploy-site effect deploy-site 2>&1 | grep -q "no hol_static_host"
check $? "deploy with no host node says so"

cat > farm.txt <<'EOF'
mantle new antfarm
rune new hol_static_host h1
set h1 provider 'cloudflare-pages'
set h1 project 'p'
EOF
"$CLI" --script farm.txt --atomic >/dev/null 2>&1
"$CLI" --allow-effects=deploy-site effect deploy-site 2>&1 | grep -q "no index"
check $? "deploy before render-site says to render first"

# ── PUBLISHING IS NOT A PER-LANGUAGE OPERATION (2026-08-20) ────────────────
# The bug: the operator edited both halves of a bilingual site, ran
# `effect render-site` (which defaulted to English), and published. The upload
# is the whole site/ FOLDER, so a stale Spanish page went live under the same
# URL while the English page was current -- and it is invisible from the English
# page, which is the one a person checks afterwards.
#
# Rendering ONE language and then publishing is now a refusal, in both of the
# shapes it comes in: the other language missing, and the other language present
# but built at a different time.
"$CLI" --allow-effects=render-site effect render-site en outreach-dec >/dev/null 2>&1
out=$("$CLI" --allow-effects=deploy-site effect deploy-site 2>&1)
echo "$out" | grep -q "no index for es"
check $? "publishing with only English built is refused, naming the missing language"
echo "$out" | grep -q "not a per-language operation"
check $? "...and says why, because the operator's mental model is the bug"

# The bare verb, with no language argument, builds EVERY language. This is the
# exact command from the operator's console that silently did half the job.
"$CLI" --allow-effects=render-site effect render-site outreach-dec >/dev/null 2>&1
[ -f site/index-en.html ] && [ -f site/index-es.html ]
check $? "\`effect render-site\` with no language argument builds all of them"

# ...and the document may be the FIRST argument now that the language is
# optional. It used to be read from slot 1 unconditionally, so this form
# rendered an empty mantle and reported success.
grep -q "Diciembre\|diciembre\|<html lang=\"es\"" site/index-es.html
check $? "...and the Spanish page is really Spanish, not a copy of the English one"

# Present but STALE is the harder half: the file exists and is valid.
python -c "import os,time; t=time.time()-3*3600; os.utime('site/index-es.html',(t,t))" 2>/dev/null   || touch -d '3 hours ago' site/index-es.html 2>/dev/null
"$CLI" --allow-effects=deploy-site effect deploy-site 2>&1 | grep -q "not built in one pass"
check $? "publishing a site whose Spanish page is stale is refused too"

# Back to a site built in one pass, so the assertions below are about the
# credential and the vendor rather than about languages.
"$CLI" --allow-effects=render-site effect render-site outreach-dec >/dev/null 2>&1
# The message names BOTH credential routes since 2026-08-20: a token in the
# encrypted vault (which travels with the .miga - the author's point about what
# a .miga is for) or a token file beside the database.
out=$("$CLI" --allow-effects=deploy-site effect deploy-site 2>&1)
echo "$out" | grep -q "no credential"
check $? "deploy with no credential says so"
echo "$out" | grep -q "token_key"
check $? "...and names the vault route, which travels with the database"

printf 'tok
' > cf.key
"$CLI" use antfarm >/dev/null 2>&1
printf "use antfarm
set h1 token_file 'cf.key'
" > f2.txt
"$CLI" --script f2.txt --atomic >/dev/null 2>&1
# NATIVE SINCE 2026-08-20. This used to assert that a deploy with no
# `deploy_cmd` REFUSED and handed over a wrangler command to paste. Hormiga now
# speaks the Cloudflare Pages Direct Upload protocol itself, so with a token and
# a project it tries — and the assertion becomes that it gets far enough to talk
# to the vendor and reports the vendor's own words when it is turned away.
#
# The account id is unset/bogus here, so this exercises the whole LOCAL half of
# the flow — walk the folder, hash every file, ask for an upload token — without
# needing a real credential in the test environment. The remote half is the part
# only a live account can prove.
out=$("$CLI" --allow-effects=deploy-site effect deploy-site 2>&1)
echo "$out" | grep -qE "hashed [0-9]+ file"
check $? "the native deploy hashes the site before asking for anything"
echo "$out" | grep -q "upload token"
check $? "...then asks Cloudflare for an upload token"
echo "$out" | grep -qE "7003|Could not route|Authentication|Invalid"
check $? "...and reports the vendor's own error verbatim, not a summary"
[ ! -f .deploy-curl.cfg ]
check $? "the curl config holding the token never outlives the call"

# ── THE WEBSITE IS A WEBSITE (2026-08-19) ──────────────────────────────────
# Every assertion below is a defect the field agent reported after building a real
# five-page bilingual site with `render-site`. The pattern in all of them is the
# same: work that shipped for the EMAIL renderer and stopped there, or a field
# that was declared, advertised by --describe, and rendered by nothing.
cat > web.txt <<'EOF'
use demo-org
rune new contact ana-ruiz
set ana-ruiz display_name 'Ana Ruiz'
set ana-ruiz role 'Housing Lead'
set ana-ruiz email 'ana@example.org'
set ana-ruiz notes 'INTERNAL never publish'
tag ana-ruiz +type:contact +clearance:public +clearance:contact
rune new contact quiet-volunteer
set quiet-volunteer role 'Volunteer'
set quiet-volunteer email 'quiet@example.org'
tag quiet-volunteer +type:contact
rune new contact listed-only
set listed-only display_name 'Listed Only'
set listed-only email 'listed@example.org'
tag listed-only +type:contact +clearance:public
mantle new org-site
use org-site
rune new page home
set home slug home
set home title_en Home
set home order 0
rune new hero mast
set mast title_en 'Riverton Community Network'
set mast subtitle_en '1234 Main St'
set mast row 0
rune new event_grid evw
set evw query 'type:event AND community-outreach'
set evw sort date
set evw caption_en 'Everything coming up.'
set evw row 1
rune new calendar_embed calw
set calw query 'type:event'
set calw row 2
rune new directory dirw
set dirw kind contact
set dirw query 'type:contact'
set dirw caption_en 'Our members.'
set dirw row 3
EOF
"$CLI" --script web.txt --atomic >/dev/null 2>&1
check $? "the website fixture applies"
"$CLI" --allow-effects=render-site effect render-site en org-site > site.out 2>&1
check $? "render-site runs"
"$CLI" --allow-effects=render-site effect render-site es org-site >/dev/null 2>&1
W=site/index-en.html

grep -q 'Riverton Winter Posada' "$W"
check $? "web: title_en is the display title (11 of 13 cards printed a slug)"
grep -q 'Details and RSVP' "$W"
check $? "web: summary_en renders (0 of 13 cards had a summary)"
grep -q '#16a34a' "$W"
check $? "web: event.color reaches the card and the calendar"
grep -q 'Everything coming up.' "$W"
check $? "web: a declared caption_en on event_grid is rendered"
grep -q 'Our members.' "$W"
check $? "web: a declared caption_en on directory is rendered"
grep -q '1234 Main St' "$W"
check $? "web: hero subtitle renders"

# The DIRECTORY and its consent gate. A block that published everything its
# query matched would publish a phone book the first time somebody wrote
# `type:contact` — which is the query a person writes when they want a
# directory. So the query is not sufficient and cannot be made sufficient.
grep -q 'Ana Ruiz' "$W"
check $? "directory: a clearance:public contact is published"
grep -q 'Listed Only' "$W"
check $? "directory: ...including one with no clearance:contact"
! grep -q 'quiet@example.org' "$W" && ! grep -qi 'quiet-volunteer' "$W"
check $? "directory: an UNTAGGED contact is withheld entirely"
grep -q 'ana@example.org' "$W"
check $? "directory: clearance:contact releases the email"
! grep -q 'listed@example.org' "$W"
check $? "directory: clearance:public alone does NOT release an email"
! grep -rq 'INTERNAL never publish' site/
check $? "directory: internal notes reach no output at any clearance (rule 6)"
grep -q 'withheld' site.out
check $? "directory: the render says how many runes it withheld"

# The .ics twin. Every time in a real community database is 12-hour with a
# meridiem, because that is what a flier prints. The shipped writer sliced
# fixed offsets: "8:00 AM" became `T8:0 00` and a 3:00 PM meeting would have
# landed at three in the morning.
grep -q 'DTSTART:.*T180000' site/calendar.ics
check $? "ics: '6:00 PM' is 18:00, not 06:00 and not '6:0 00'"
grep -q 'DTEND:.*T210000' site/calendar.ics
check $? "ics: the end time carries its meridiem too"
grep -q '^DTSTAMP:' site/calendar.ics
check $? "ics: DTSTAMP is present (RFC 5545 requires it; some importers refuse)"
[ -f site/calendar-en.ics ] && [ -f site/calendar-es.ics ]
check $? "ics: one calendar per language (a SUMMARY is prose)"

# A static host serves index.html at a directory root. None of them serve
# index-en.html, so a complete site answered / with a 404.
[ -f site/index.html ] && grep -q 'index-es.html' site/index.html
check $? "a root index.html exists and can reach BOTH languages"
grep -q 'langswitch' "$W" && grep -q 'langswitch' site/index-es.html
check $? "the generated chrome has a door between the languages"
grep -q 'hreflang="es"' "$W"
check $? "each page points at its twin with rel=alternate hreflang"

# Sitemaps 0.9 requires an absolute <loc>. Emitting a relative one produces a
# file Google rejects, which is worse than not emitting one.
[ ! -f site/sitemap.xml ]
check $? "no site.base_url: no sitemap (rather than an invalid one)"
grep -qi 'base_url is unset' site.out
check $? "...and the render says why"
"$CLI" config set site.base_url 'https://example.org' >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en org-site >/dev/null 2>&1
grep -q '<loc>https://example.org/index-en.html</loc>' site/sitemap.xml
check $? "with a base_url the sitemap is absolute"
grep -q '<loc>https://example.org/index-es.html</loc>' site/sitemap.xml
check $? "...and the SPANISH half of a bilingual site is discoverable"

# The deployed site carries its own fonts, no CDN. This staged them from the
# DATA folder, which for a real caller is somebody's org folder with no vendor/,
# so the guard stepped over it in silence.
ls site/fonts/*.woff2 >/dev/null 2>&1
check $? "webfonts are staged from where the BINARY ships, not the data folder"

# Once staged, a corrected flier used to have no effect on the site, forever.
mkdir -p assets
printf 'AAAAAAAAAAAAAAAAAAAA' > assets/f.txt
cp assets/f.txt site/assets/f.txt 2>/dev/null || { mkdir -p site/assets; cp assets/f.txt site/assets/f.txt; }
printf 'BBBBBBBBBBBBBBBBBBBBBBBBBBBBBB' > assets/f.txt
cat > img.txt <<'EOF'
use org-site
rune new image_grid gw
set gw query 'type:flier'
set gw row 4
use demo-org
rune new image f-img
set f-img path 'assets/f.txt'
tag f-img +type:flier
EOF
"$CLI" --script img.txt --atomic >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en org-site >/dev/null 2>&1
grep -q 'BBBB' site/assets/f.txt
check $? "a changed source asset is re-staged (it used to be copied once, ever)"

# ── PHASE 0 BUGS, from the field reports of 2026-08-20 ───────────────────────

# A REAL NEWLINE must survive to the page. `field_value` stripped a value's
# quotes and never decoded its escapes, so a genuine U+000A in a job description
# reached a LIVE public page as the two characters `\` and `n` — 32 of them in
# one card. The decoder existed; it was applied by hand at five call sites and
# nowhere else. Fixed in the primitive (src/scene_value.hpp), which is now the
# only one — there were two copies of field_value.
cat > nl.txt <<'EOF'
use demo-org
rune new job newline-probe
set newline-probe org 'Probe Co'
tag newline-probe +type:job
EOF
"$CLI" --script nl.txt --atomic >/dev/null 2>&1
# a real newline cannot travel through a line-oriented script file, so write it
# straight into the state document the way an importer would
python - <<'PY'
import json, io
p = 'demo-org.json'
d = json.load(io.open(p, encoding='utf-8'))
for m in d.get('mantles', []):
    for r in m.get('runes', []):
        if r.get('spirit', {}).get('name') == 'newline-probe':
            r.setdefault('content', {})['description'] = 'LINE-ONE\nLINE-TWO'
json.dump(d, io.open(p, 'w', encoding='utf-8'), ensure_ascii=False)
PY
cat > nlsite.txt <<'EOF'
mantle new nl-site
use nl-site
rune new page home
set home slug home
set home title_en Home
set home order 0
rune new job_grid nlg
set nlg query 'type:job'
set nlg detail full
set nlg row 0
EOF
"$CLI" --script nlsite.txt --atomic >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en nl-site >/dev/null 2>&1
! grep -q 'LINE-ONE\\nLINE-TWO' site/index-en.html
check $? "a stored newline is NOT rendered as a literal backslash-n"
grep -q 'LINE-ONE' site/index-en.html && grep -q 'LINE-TWO' site/index-en.html
check $? "...and both halves of the value still reach the page"

# EMPTY STATES have a language. Reported three times: a Spanish page apologised
# in English, on the half of a bilingual site whose readers may not read it.
cat > es.txt <<'EOF'
mantle new es-site
use es-site
rune new page home
set home slug home
set home title_en Home
set home order 0
rune new event_grid noev
set noev query 'type:nothing-matches-this'
set noev row 0
rune new directory nodir
set nodir kind contact
set nodir query 'type:nothing-matches-this'
set nodir row 1
EOF
"$CLI" --script es.txt --atomic >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site es es-site >/dev/null 2>&1
grep -q 'No hay nada programado' site/index-es.html
check $? "the empty event grid speaks Spanish on the Spanish page"
grep -q 'Este directorio esta vacio' site/index-es.html
check $? "...and so does the empty directory"
"$CLI" --allow-effects=render-site effect render-site en es-site >/dev/null 2>&1
grep -q 'Nothing scheduled right now' site/index-en.html
check $? "...and English on the English one"

# THE GOOGLE CALENDAR LINK is built host-side now. The version in app.js parsed
# the time itself and got it wrong for every 12-hour value — which is what a
# flier prints, so that was every event, and it failed silently (Google opens on
# the wrong date rather than erroring).
cat > gc.txt <<'EOF'
use demo-org
rune new event gcal-probe
set gcal-probe title_en 'Noon Crossing'
set gcal-probe date '2026-09-16'
set gcal-probe start_time '11:00 AM'
set gcal-probe end_time '1:00 PM'
tag gcal-probe +type:gcalprobe
use es-site
rune new calendar_embed gcal
set gcal query 'type:gcalprobe'
set gcal row 2
EOF
"$CLI" --script gc.txt --atomic >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en es-site >/dev/null 2>&1
grep -q '20260916T110000/20260916T130000' site/index-en.html
check $? "an 11 AM - 1 PM event builds a correct Google Calendar range"
! grep -q 'dates=20260916T1100 AM' site/index-en.html
check $? "...with no meridiem left inside the value"
! grep -q "e.s.replace" site/app.js
check $? "...and the second time parser in the JS is gone"

# JOB_GRID was the only grid with no clipping: one real posting ran 1,150
# characters in a card.
grep -q 'detail' <("$CLI" --describe 2>/dev/null)
check $? "job_grid declares a detail axis like event_grid"

# MAP MARKERS print the display name, like every other output.
"$CLI" use demo-org >/dev/null 2>&1
cat > mk.txt <<'EOF'
use demo-org
rune new organization lcog
set lcog display_name 'County Council of Governments'
set lcog geo '44.05,-123.09'
use es-site
rune new map_embed mp
set mp row 3
EOF
"$CLI" --script mk.txt --atomic >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en es-site >/dev/null 2>&1
grep -q 'County Council of Governments' site/index-en.html
check $? "map markers use display_name, not the humanized slug"
! grep -q '&quot;n&quot;:&quot;Lcog&quot;' site/index-en.html
check $? "...so an acronym is not title-cased into nonsense"

# ── THE STYLE AXES (2026-08-20) ────────────────────────────────────────────
# Every one of these is a `config set`, which is the point: the Style tab has no
# private state, so an agent and a person are making the same change. Testing
# them headless is therefore testing the GUI's behaviour too.

# CONTRAST. The bug: `color:#fff` was compiled into a dozen rules that sit on
# var(--accent) — right for a navy brand, invisible for a yellow one, and
# unfixable by the person who picked the yellow.
"$CLI" config set theme.accent '#eab308' >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en org-site >/dev/null 2>&1
grep -q -- '--on-accent:#111111' site/style.css
check $? "a LIGHT accent yields DARK label text"
"$CLI" config set theme.accent '#1e3a8a' >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en org-site >/dev/null 2>&1
grep -q -- '--on-accent:#ffffff' site/style.css
check $? "a DARK accent yields light label text"
grep -q 'color:var(--on-accent)' site/style.css
check $? "...and buttons/bands read the token instead of a literal"
grep -q -- '--accent-lite:#' site/style.css && grep -q -- '--accent-dark:#' site/style.css
check $? "lighter/darker partners are derived from the accent"
"$CLI" config set theme.accent_lite '#abcdef' >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en org-site >/dev/null 2>&1
grep -q -- '--accent-lite:#abcdef' site/style.css
check $? "...and an explicitly stated partner wins over the derived one"
grep -q '.prose{max-width:68ch;color:var(--prose)' site/style.css
check $? "body prose follows the theme (it was a hardcoded near-black)"

# OVERFLOW. A real page leaked an event summary containing a long URL out past
# its card and under the neighbouring one.
grep -q 'overflow-wrap:anywhere' site/style.css
check $? "long unbreakable runs are allowed to break inside their card"
grep -q 'html,body{max-width:100%;overflow-x:hidden}' site/style.css
check $? "the page itself can never scroll sideways"
grep -q '.card table,.prose table{display:block;max-width:100%;overflow-x:auto}' site/style.css
check $? "a wide table scrolls inside its frame rather than widening the page"

# BANNER TREATMENT.
"$CLI" config set theme.banner_filter '2' >/dev/null 2>&1
"$CLI" config set theme.banner_dim '70' >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en org-site >/dev/null 2>&1
grep -q '.hero-bg.f-mono{filter:grayscale(1)' site/style.css
check $? "banner treatments are defined"
grep -q 'style="--bdim:0.7"' site/index-en.html
check $? "the scrim strength reaches the page"
"$CLI" config set theme.banner_filter '0' >/dev/null 2>&1

# GRID RHYTHM — one spacing step, equal heights.
"$CLI" config set theme.grid_gap '4' >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en org-site >/dev/null 2>&1
grep -q -- '--gap:40px' site/style.css
check $? "the spacing step is a theme axis, not a literal per component"
grep -q 'body.even-grid .cards{grid-auto-rows:1fr}' site/style.css
check $? "equal-height rows (the 'distribute' alignment) are available"
grep -q '<body class="clean even-grid' site/index-en.html
check $? "...and on by default"
"$CLI" config set theme.grid_even '0' >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en org-site >/dev/null 2>&1
! grep -q '<body class="clean even-grid' site/index-en.html
check $? "...and can be turned off"
"$CLI" config set theme.grid_even '1' >/dev/null 2>&1
"$CLI" config set theme.grid_gap '2' >/dev/null 2>&1

# ICONS. "events are literally just text" — a date, a time and a place are
# three kinds of fact and were printed as one grey run.
"$CLI" --allow-effects=render-site effect render-site en org-site >/dev/null 2>&1
grep -q '<p class="meta-row"><svg class="ico"' site/index-en.html
check $? "event cards carry inline SVG icons beside each fact"
grep -q 'stroke="currentColor"' site/index-en.html
check $? "...which inherit the text colour (so contrast applies to them too)"
! grep -q 'http' <(grep -o '<svg class="ico"[^>]*>' site/index-en.html)
check $? "...and reference no external host"
"$CLI" config set theme.icons '0' >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en org-site >/dev/null 2>&1
! grep -q 'class="ico"' site/index-en.html
check $? "icons can be turned off entirely"
"$CLI" config set theme.icons '1' >/dev/null 2>&1

# HUMAN DATES. An ISO string is the right thing to store and the wrong thing to
# print, which is the `title_en`-over-a-slug argument one field along.
"$CLI" --allow-effects=render-site effect render-site en org-site >/dev/null 2>&1
grep -qE '<span>(Mon|Tue|Wed|Thu|Fri|Sat|Sun), ' site/index-en.html
check $? "dates print as a person reads them, not as 2026-08-19"
"$CLI" --allow-effects=render-site effect render-site es org-site >/dev/null 2>&1
grep -qE '<span>(lun|mar|mie|jue|vie|sab|dom), [0-9]+ de ' site/index-es.html
check $? "...in the page's own language"

# MOBILE. A seven-column month grid on a 340px screen is ~48px a cell.
"$CLI" --allow-effects=render-site effect render-site en org-site >/dev/null 2>&1
grep -q '.calwidget table{display:none}' site/style.css
check $? "the month grid is hidden on a phone rather than crushed"
grep -q '.calwidget .agenda .ag-day' site/style.css
check $? "...and an agenda list takes its place"
grep -q "mode==='month'&&narrow" site/app.js
check $? "...which the widget actually renders at that breakpoint"
grep -q 'min-height:44px' site/style.css
check $? "nav tap targets meet the 44px platform minimum"

# ── PUBLISHING IS A VERB, AND THE HISTORY IS A RUNE (2026-08-20) ───────────
# `deploy_site` used to be defined in the headless translation unit only, so
# the desktop application could not publish at all and every publish of a real
# site went through an agent. It now lives in `src/publish.cpp`, which both
# main()s compile. These assertions pin the BEHAVIOUR that made that fix worth
# making: a publish records itself through the dispatcher, so the operator's
# history is Hormiga's own rather than a query against a vendor who may be gone.
"$CLI" use antfarm >/dev/null 2>&1
cat > host.txt <<'EOF'
use antfarm
set h1 account_id 'acct-1'
set h1 deploy_cmd 'echo {"success":true,"url":"https://v1.example.pages.dev"}'
EOF
"$CLI" --script host.txt --atomic >/dev/null 2>&1
check $? "a host node can carry a deploy_cmd"

"$CLI" --allow-effects=deploy-site effect deploy-site h1 > d1.out 2>&1
check $? "deploy-site runs with a deploy_cmd set"
grep -q 'v1.example.pages.dev' d1.out
check $? "...and reports the URL the host returned"

"$CLI" use antfarm >/dev/null 2>&1
"$CLI" ls 2>/dev/null | grep -q 'deploy-'
check $? "the publish RECORDED itself as a deployment rune"
n=$("$CLI" ls 2>/dev/null | grep -c 'deploy-')
[ "$n" = "1" ]
check $? "exactly one deployment after one publish"

# A second publish supersedes the first: "which one is live" is a FACT IN THE
# LOG, set by a command, not something inferred from timestamps afterwards.
cat > host2.txt <<'EOF'
use antfarm
set h1 deploy_cmd 'echo {"success":true,"url":"https://v2.example.pages.dev"}'
EOF
"$CLI" --script host2.txt --atomic >/dev/null 2>&1
sleep 1
"$CLI" --allow-effects=deploy-site effect deploy-site h1 >/dev/null 2>&1
"$CLI" use antfarm >/dev/null 2>&1
live=$("$CLI" ls 2>/dev/null | grep -c 'deploy-')
[ "$live" = "2" ]
check $? "a second publish adds a second deployment (history, not overwrite)"
# rollback: declared, gated, and refuses precisely rather than guessing
out=$("$CLI" effect rollback-site h1 dep-1 2>&1)
echo "$out" | grep -qi "refused"
check $? "rollback-site is refused by default like every one-way door"
echo "$out" | grep -q "CHANGES THE LIVE WEBSITE"
check $? "...and its refusal says what it would change"
"$CLI" --describe 2>/dev/null | grep -q 'rollback-site'
check $? "rollback-site is in the briefing an agent reads"
"$CLI" --allow-effects=rollback-site effect rollback-site h1 dep-1 2>&1 \
  | grep -q 'no rollback_cmd'
check $? "rollback with no rollback_cmd names the field to set"
"$CLI" --allow-effects=rollback-site effect rollback-site h1 2>&1 \
  | grep -q 'no rollback_cmd'
check $? "...and does so before anything else can go wrong"

cat > host3.txt <<'EOF'
use antfarm
set h1 rollback_cmd 'echo rolled back to {deployment}'
EOF
"$CLI" --script host3.txt --atomic >/dev/null 2>&1
"$CLI" --allow-effects=rollback-site effect rollback-site h1 2>&1 \
  | grep -q 'name the deployment'
check $? "rollback without a deployment refuses rather than guessing which"
"$CLI" --allow-effects=rollback-site effect rollback-site h1 dep-7 2>&1 \
  | grep -q 'restored dep-7'
check $? "rollback with a named deployment runs the host's own command"
[ ! -f .rollback-curl.cfg ]
check $? "the rollback token file never outlives the call"

# ── --state PINS THE DATA FOLDER (2026-08-21) ──────────────────────────────
# `g_base_dir` was the working directory outright, so a session on a document
# somewhere else wrote its journal, its SQLite mirror, site/ and assets/ beside
# the CALLER. Two days were lost to the GUI half of this bug: the operator and
# the agent edited two different copies of one database, because "which document
# is this application editing" had no answer either of them could see.
mkdir -p sub/orgA
printf 'mantle new demo-org
use demo-org
rune new contact ana
' > sub/w.txt
( cd sub && "$CLI" --state orgA/orgA_01.state.json --script w.txt --atomic ) >/dev/null 2>&1
[ -f sub/orgA/orgA_01.state.json ]
check $? "--state writes the document where it was named, not beside the caller"
[ ! -f sub/demo-org.json ]
check $? "...and does NOT leave a stray document in the working directory"
[ -f sub/orgA/orgA_01.state.json.log ]
check $? "...and the journal follows the document"
( cd sub && "$CLI" --state orgA/orgA_01.state.json --allow-effects=save effect save ) >/dev/null 2>&1
[ -f sub/orgA/orgA_01.state.db ]
check $? "the SQLite mirror is named after the document it mirrors"
[ ! -f sub/orgA/demo-org.db ]
check $? "...not after a different database that happens to be the default"

# -- UNICODE NORMALIZATION IS BYTE-TRANSPARENT (2026-08-21) -----------------
# Void Palabra SPEC 6.1 (normative 2026-08-21) puts the NFC precondition on the
# CALLER: "callers supply NFC; Palabra does not normalize." That was Hormiga's
# own proposal -- the library has a hash function, we have the keyboard -- so
# holding it is ours.
#
# The hazard is silent: composed "Campana" (with a tilde, U+00F1) and decomposed
# (n + U+0303) are one word to a reader and two different hashes to the system,
# and the symptom is a merge that does nothing rather than an error. Our live
# database is Spanish and the volunteers are not all on one platform.
#
# What is asserted here is what was MEASURED: Hormiga passes text through
# unchanged in both directions. It does not corrupt (which would be a bug) and
# it does not normalize (which is why tools/lint_nfc.py exists). If Core ever
# starts rewriting text under us, this fails and we find out here rather than
# from two peers who cannot agree on a name.
python - <<'PYEOF'
import io, unicodedata
# its OWN mantle: `mantle new demo-org` would fail here (it exists by now) and
# --atomic would roll the whole batch back, leaving nothing to assert on
w = 'mantle new uni-check\nuse uni-check\n'
comp = unicodedata.normalize('NFC', 'Campa\u00f1a')   # n-tilde as ONE codepoint
deco = unicodedata.normalize('NFD', 'Campa\u00f1a')   # n + combining tilde
w += "rune new event u-nfc\nset u-nfc title_en '%s'\n" % comp
w += "rune new event u-nfd\nset u-nfd title_en '%s'\n" % deco
io.open('uni.txt', 'w', encoding='utf-8').write(w)
PYEOF
"$CLI" --script uni.txt --atomic >/dev/null 2>&1
python - <<'PYEOF'
import io, json, sys, unicodedata
d = json.load(io.open('demo-org.json', encoding='utf-8'))
got = {}
for m in d['mantles']:
    for r in m.get('runes', []):
        t = r.get('content', {}).get('title_en')
        if t:
            got[r['spirit']['name']] = t
nfc, nfd = got.get('u-nfc', ''), got.get('u-nfd', '')
composed_intact   = bool(nfc) and unicodedata.normalize('NFC', nfc) == nfc
decomposed_intact = bool(nfd) and unicodedata.normalize('NFC', nfd) != nfd
sys.exit(0 if (composed_intact and decomposed_intact) else 1)
PYEOF
check $? "text is byte-transparent: composed stays composed, decomposed is NOT normalized"

# The corpus checker must catch what it is for, or a clean report on the real
# database means nothing.
python "$LINT_NFC" --strict demo-org.json >/dev/null 2>&1
[ $? -ne 0 ]
check $? "lint_nfc flags that decomposed document (it is not vacuously clean)"

# -- publish-index: THE PUBLISHED SUBSET AS DATA (2026-08-21) ---------------
# The website should respond to database changes without a full redeploy, so the
# published subset gets a second FORMAT: rows, not pages. The whole safety
# argument is the word SAME -- it goes through render/published.hpp, the one
# clearance gate the HTML directory also uses. A second exporter with its own
# copy of the check would be a second privacy surface that drifts silently,
# because a website showing too much still looks correct.
#
# These assertions are CLAUDE.md rule 6 at the new seam. If any of them fails,
# somebody's private data is on a public website.
mkdir -p idx && cd idx
cat > pub.txt <<'PUBEOF'
mantle new demo-org
use demo-org
rune new contact ana-ruiz
set ana-ruiz display_name 'Ana Ruiz'
set ana-ruiz email 'ana@example.org'
set ana-ruiz phone '555-0100'
set ana-ruiz notes 'INTERNALSECRET do not publish'
tag ana-ruiz +type:contact +clearance:public +clearance:contact
rune new contact bo-quiet
set bo-quiet display_name 'Bo Quiet'
set bo-quiet email 'bo@example.org'
set bo-quiet notes 'INTERNALSECRET also'
tag bo-quiet +type:contact +clearance:public
rune new contact cy-private
set cy-private display_name 'Cy Private'
set cy-private email 'cy@example.org'
tag cy-private +type:contact
rune new organization lcog
set lcog display_name 'County Council of Governments'
set lcog email 'desk@example.org'
tag lcog +type:organization +clearance:public
PUBEOF
"$CLI" --script pub.txt --atomic >/dev/null 2>&1
"$CLI" --allow-effects=publish-index effect publish-index >/dev/null 2>&1
IDX=site/index/directory.json
[ -f "$IDX" ]
check $? "publish-index writes site/index/directory.json"

# RULE 6. An internal-notes-class field must never reach an Output holiday, and
# the check must be testable. This is that check.
! grep -q 'INTERNALSECRET' "$IDX"
check $? "no internal note reaches the index (CLAUDE.md rule 6)"
! grep -q '"notes"' "$IDX"
check $? "...and the field is ABSENT from the shape, not blanked"

# a rune with no clearance is not in the index at all
! grep -q 'Cy Private' "$IDX"
check $? "a contact with no clearance tag is absent entirely"
! grep -q 'cy@example.org' "$IDX"
check $? "...and so is their email"

# clearance:public alone lists you; it does NOT release contact details
grep -q 'Bo Quiet' "$IDX"
check $? "clearance:public lists a contact"
! grep -q 'bo@example.org' "$IDX"
check $? "...but does NOT release their email - that is a second consent"

# clearance:contact releases them
grep -q 'ana@example.org' "$IDX" && grep -q '555-0100' "$IDX"
check $? "clearance:contact releases the email and phone"

# an ORGANISATION's email waits for the tag too: a small group's "front desk"
# is very often one volunteer's personal inbox
grep -q 'County Council of Governments' "$IDX"
check $? "an organization is listed on clearance:public"
! grep -q 'desk@example.org' "$IDX"
check $? "...and its email waits for clearance:contact like anyone else's"

# DETERMINISTIC: no timestamp, so an unchanged database produces identical
# bytes. That is what lets a push skip an unchanged index and lets a reviewer
# diff one.
cp "$IDX" first.json
"$CLI" --allow-effects=publish-index effect publish-index >/dev/null 2>&1
cmp -s first.json "$IDX"
check $? "the index is deterministic - unchanged data, identical bytes"
cd ..

# -- A LIVE DIRECTORY REFRESHES WITHOUT A REDEPLOY (2026-08-21) -------------
# The operator's complaint was redeploying the WHOLE site whenever one contact
# changed. A directory marked `live` also writes its cards as a standalone
# fragment under site/index/, and app.js re-fetches that on load -- so a change
# costs one small file republished.
#
# The safety properties, in order of how badly they fail:
#   1. the fragment goes through the SAME clearance gate as the page
#   2. the page still carries the cards, so JS-off is unaffected
#   3. the fragment and the page cannot disagree, because one buffer wrote both
mkdir -p live && cd live
cat > lv.txt <<'LVEOF'
mantle new demo-org
use demo-org
rune new contact ana-ruiz
set ana-ruiz display_name 'Ana Ruiz'
tag ana-ruiz +type:contact +clearance:public
rune new contact secret-sam
set secret-sam display_name 'SECRETSAM'
tag secret-sam +type:contact
mantle new site-doc
use site-doc
rune new page home
set home slug home
set home title_en Home
set home order 0
rune new directory dir
set dir kind contact
set dir query 'type:contact'
set dir live on
set dir row 0
LVEOF
"$CLI" --script lv.txt --atomic >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site site-doc >/dev/null 2>&1
FRAG=site/index/dir-dir.html
[ -f "$FRAG" ]
check $? "a live directory writes its cards as a refreshable fragment"
grep -q 'data-live="index/dir-dir.html"' site/index-en.html
check $? "...and the page points at it"

# 1. THE SAME GATE. A fragment that bypassed the clearance check would be a
# second privacy surface serving a file nobody looks at.
! grep -q 'SECRETSAM' "$FRAG"
check $? "the fragment obeys clearance:public (an unclearanced contact is absent)"
! grep -q 'SECRETSAM' site/index-en.html
check $? "...and so does the page, as before"

# 2. PROGRESSIVE ENHANCEMENT. The cards are in the page already; the fetch only
# ever makes them fresher. A visitor with no JavaScript is unaffected.
grep -q 'Ana Ruiz' site/index-en.html
check $? "the page still carries the cards (JavaScript-off is unaffected)"

# 3. THEY CANNOT DISAGREE, because one buffer wrote both.
python - <<'PYEOF'
import io, sys
page = io.open('site/index-en.html', encoding='utf-8').read()
frag = io.open('site/index/dir-dir.html', encoding='utf-8').read().strip()
sys.exit(0 if frag and frag in page else 1)
PYEOF
check $? "the fragment is embedded verbatim in the page - they cannot drift"

# a directory that is NOT live gets no fragment and no pointer: the feature is
# opt-in, and a site that needs no freshness should not pay for a fetch
"$CLI" use site-doc >/dev/null 2>&1
"$CLI" set dir live off >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site site-doc >/dev/null 2>&1
! grep -q 'data-live' site/index-en.html
check $? "live is opt-in: off means no fetch pointer at all"
grep -q 'Ana Ruiz' site/index-en.html
check $? "...and the directory still renders normally"
cd ..

# -- THE ENCRYPTED BACKUP, AND ITS RECOVERY PATH (2026-08-21) ---------------
# A whole database as a blob the store cannot read (okf/concepts/platform/data-planes.md
# §3), so S3 or a USB stick are equally acceptable places to keep it.
#
# The assertions that matter are about the DISASTER, not the happy path -- and
# the disaster is what found the design flaw this now guards: `restore-database`
# is an effect, effects run inside a session, a session refuses to start on a
# corrupt document, so the recovery tool was gated behind the thing being
# broken. A recovery tool that requires a working system is not a recovery tool.
mkdir -p bkp && cd bkp
printf 'mantle new demo-org
use demo-org
rune new contact ana
set ana notes "INTERNALBACKUPSECRET"
' > b.txt
"$CLI" --script b.txt --atomic >/dev/null 2>&1

# the passphrase is NOT an argument: argv is readable in a process listing
"$CLI" --allow-effects=backup-database effect backup-database 2>&1   | grep -q "HORMIGA_BACKUP_PASSPHRASE"
check $? "a backup with no passphrase refuses and names the variable"

HORMIGA_BACKUP_PASSPHRASE='a real passphrase'   "$CLI" --allow-effects=backup-database effect backup-database >/dev/null 2>&1
[ -f demo-org.json.bkp ]
check $? "backup-database seals the database"
! grep -q 'INTERNALBACKUPSECRET' demo-org.json.bkp
check $? "...and the store cannot read what is inside it"
grep -q 'INTERNALBACKUPSECRET' demo-org.json
check $? "...while the live database still has it (the backup is a COPY)"

# THE DISASTER. Corrupt the database completely, then recover.
echo 'corrupted junk' > demo-org.json
HORMIGA_BACKUP_PASSPHRASE='a real passphrase'   "$CLI" --restore-backup demo-org.json.bkp >/dev/null 2>&1
check $? "--restore-backup works on a database too corrupt to open"
grep -q 'INTERNALBACKUPSECRET' demo-org.json
check $? "...and the data is actually back"
"$CLI" ls 2>/dev/null | grep -q ana
check $? "...and the restored document loads as a real database"

# A FAILED restore must not make a bad situation worse.
echo 'corrupted junk again' > demo-org.json
HORMIGA_BACKUP_PASSPHRASE='wrong'   "$CLI" --restore-backup demo-org.json.bkp >/dev/null 2>&1
[ $? -ne 0 ]
check $? "a wrong passphrase fails the restore"
grep -q 'corrupted junk again' demo-org.json
check $? "...and leaves the file exactly as it was, rather than half-written"
cd ..

# -- A NEWLINE IN A VALUE MUST NOT BECOME A COMMAND (closed 2026-08-25) -----
# Reported as MESSAGE_FOR_VOIDCORE_hormiga-string-carrying-subsystem, fixed in
# Void Core 0.2.7. Our diagnosis was wrong -- their argv tokenizer had always
# carried newlines correctly -- but the injection was real and they found two
# more routes we had not tested. The fix was one line of SPEC 6.1: an
# unterminated quoted run is now an ERROR rather than running to end of input.
#
# This block used to assert the BROKEN behaviour so the suite stayed green while
# the gap was open. It failed the day 0.2.7 landed, printed what to do, and this
# is that. Keeping the test rather than deleting it: the property is permanent
# even though the bug is not.
mkdir -p inj && cd inj
python - <<'PYEOF'
import io
NL = chr(10); SQ = chr(39); BS = chr(92)
L = ['mantle new demo-org', 'use demo-org',
     'rune new contact treasurer', 'set treasurer email real@org.example',
     'rune new contact visitor']
# route 1 -- a newline inside a quoted value (what we reported)
L.append('set visitor bio ' + SQ + 'weekends.' + NL
         + 'set treasurer email attacker@evil.example' + SQ)
# route 2 -- an escaped apostrophe then a brace (Core found this one)
L.append('set visitor d ' + SQ + 'don' + BS + SQ + 't } set treasurer f BREACHED' + SQ)
# route 3 -- command substitution inside single quotes (Core found this too)
L.append('set visitor e ' + SQ + 'a stranger wrote $(rune ls)' + SQ)
io.open('inj.txt', 'w', encoding='utf-8').write(NL.join(L) + NL)
PYEOF
# ONE run, output captured -- a second run would halt at `mantle exists` before
# ever reaching the line under test, which is exactly the kind of assertion that
# passes for the wrong reason Core warned us about in their section 8.
"$CLI" --script inj.txt > inj.out 2>&1

# THE PROPERTY, checked on the FIELD rather than on the file.
#
# Grepping demo-org.json for the attacker's address was wrong and passed only by
# accident: once Void Maiz's --script started reading whole transcripts through
# vc_transcript_split_json (2026-08-25), the hostile text is STORED AS DATA
# inside the bio -- which is correct behaviour -- and a file-level grep cannot
# tell that apart from the text having been executed. The only question that
# matters is whether the OTHER rune changed.
python - <<'PYEOF'
import io, json, sys
d = json.load(io.open('demo-org.json', encoding='utf-8'))
f = {}
for m in d.get('mantles', []):
    for r in m.get('runes', []):
        f[r.get('spirit', {}).get('name')] = r.get('content', {})
t = f.get('treasurer', {})
v = f.get('visitor', {})
problems = []
# route 1 -- a newline in a value must not rewrite another rune's field
if t.get('email') != 'real@org.example':
    problems.append('treasurer.email was changed to ' + repr(t.get('email')))
# route 2 -- an escaped apostrophe then a brace must not set a field
if t.get('f'):
    problems.append('treasurer.f was set to ' + repr(t.get('f')))
# and the hostile text SHOULD be present, as data, where a reviewer sees it
if 'attacker@evil.example' not in (v.get('bio') or ''):
    problems.append('the submitted bio did not survive as data')
for p in problems:
    sys.stderr.write(p + chr(10))
sys.exit(1 if problems else 0)
PYEOF
check $? "a hostile value is stored as DATA and executes nothing"

# route 3 -- command substitution must stay literal
python - <<'PYEOF'
import io, json, sys
d = json.load(io.open('demo-org.json', encoding='utf-8'))
for m in d.get('mantles', []):
    for r in m.get('runes', []):
        if r.get('spirit', {}).get('name') == 'visitor':
            e = r.get('content', {}).get('e') or ''
            sys.exit(0 if e == 'a stranger wrote $(rune ls)' else 1)
sys.exit(1)
PYEOF
check $? "...and $(...) inside single quotes stays literal, unexpanded"

# A MULTI-LINE VALUE IS NOW SCRIPTABLE, which is Void Maiz adopting Core's
# transcript splitter (2026-08-25). Before that the run halted here with an
# unterminated quote -- safe, but it meant two paragraphs could not be scripted
# at all, and Hormiga has three multiline: field editors.
python - <<'PYEOF'
import io, json, sys
d = json.load(io.open('demo-org.json', encoding='utf-8'))
for m in d.get('mantles', []):
    for r in m.get('runes', []):
        if r.get('spirit', {}).get('name') == 'visitor':
            sys.exit(0 if chr(10) in (r.get('content', {}).get('bio') or '') else 1)
sys.exit(1)
PYEOF
check $? "...and the newline itself survives, so a two-paragraph value works"
cd ..

# A failed command must exit non-zero: a shell and an agent both branch on it.
"$CLI" rune new no-such-glyph x >/dev/null 2>&1
[ $? -ne 0 ]
check $? "a failed command exits non-zero"

if [ "$fail" = "0" ]; then echo "OK - headless: brief, batch, review, gate, render"; fi
exit $fail
