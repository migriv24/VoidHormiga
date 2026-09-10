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

# A publish target is now EITHER kind of host node (2026-09-02: `hol_github`
# became a real deployer), so the refusal names both rather than the one that
# happened to be implemented first. An operator who has wired a GitHub Pages
# node and is told to add a `hol_static_host` will add a second, wrong node.
out=$("$CLI" --allow-effects=deploy-site effect deploy-site 2>&1)
echo "$out" | grep -q "no publish target"
check $? "deploy with no host node says so"
echo "$out" | grep -q "hol_github"
check $? "...and names both kinds of host, not only the first one implemented"

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


# ── the Click LaFont field report, 2026-09-02 ────────────────────────────────
# Six defects and six absences, of which these are the ones a headless run can
# see. Each check names the report item it pins, because a test whose reason
# lives only in a commit message is a test somebody deletes.
mkdir -p report && cd report

cat > site.txt <<'EOF'
mantle new demo-org
use demo-org
mantle new report-site
use report-site
rune new page home
set home slug home
set home title_en Home
set home title_es Inicio
set home order 0
rune new hero r-hero
set r-hero title_en 'Riverton Network'
set r-hero row 0
rune new narrative r-nar
set r-nar text_en 'Line one.
Line two, see https://example.org for more.'
set r-nar row 1
rune new divider r-bar
set r-bar divider_style bar
set r-bar colors '#ff5a5f,#3ddc97,;background:url(x)'
set r-bar row 2
rune new image_grid r-gal
set r-gal query 'type:image'
set r-gal columns 2
set r-gal row 3
EOF
"$CLI" --script site.txt --atomic >/dev/null 2>&1
check $? "the field-report fixture applies"
out=$("$CLI" --allow-effects=render-site effect render-site en report-site 2>&1)

# D1 — every page this renderer has ever produced was invisible without JS
grep -q 'noscript' site/index-en.html
check $? "D1: a no-JS visitor gets an unanimated page, not an empty one"

# D4 — `narrative` renders the same way in both output domains
grep -q 'class="prose pre-line reveal"' site/index-en.html
check $? "D4: a narrative keeps the line breaks the author typed"
grep -q 'a href="https://example.org"' site/index-en.html
check $? "...and a bare URL in it is linkified, as AGENT-GUIDE 8 says"
grep -q 'color:inherit' site/index-en.html
[ $? -ne 0 ]
check $? "...without the email domain's inline colour, which a page has CSS for"

# D2/D3 — the two colour rules that were not tokens
grep -q 'site-head{position:sticky;top:0;z-index:20;' site/style.css
check $? "D2: the header rule is still there"
grep -q 'background:color-mix(in srgb,var(--bg) 78%,transparent)' site/style.css
check $? "...and it rides --bg now instead of hardcoding white over a dark page"
grep -q 'meta a{color:var(--accent)}' site/style.css
check $? "D3: a link in a meta line has a colour"

# A6 — a declared field that did nothing, and a bar there was no way to draw
grep -q 'class="gallery grid cols-2 reveal"' site/index-en.html
check $? "A6: image_grid.columns reaches the page"
grep -q 'class="divider bar reveal"' site/index-en.html
check $? "A6: divider_style bar draws a coloured swatch strip"
grep -q 'background:#ff5a5f' site/index-en.html
check $? "...with the colours the operator named"
grep -q 'url(x)' site/index-en.html
[ $? -ne 0 ]
check $? "...and a value that is not a colour is dropped, never escaped into a style attribute"
echo "$out" | grep -q 'divider colour'
check $? "...and the render says which value it dropped"
grep -q 'og:title" content="Home - Riverton Network"' site/index-en.html
check $? "A6: a share card carries the site name, not the bare word Home"

# Part 3 — the operator's two lines of CSS
grep -q 'custom.css' site/index-en.html
[ $? -ne 0 ]
check $? "no custom.css beside the database changes nothing"
printf 'body{outline:0}\n' > custom.css
"$CLI" --allow-effects=render-site effect render-site en report-site >/dev/null 2>&1
grep -q 'href="custom.css' site/index-en.html
check $? "Part 3: custom.css beside the database is staged and linked"
rm custom.css
"$CLI" --allow-effects=render-site effect render-site en report-site >/dev/null 2>&1
[ ! -f site/custom.css ]
check $? "...and deleting it turns the overrides off again"

# A3 — the author declined "publish one language" and asked for translation
# tooling instead. This is that tooling.
out=$("$CLI" --allow-effects=render-site effect render-site es report-site 2>&1)
echo "$out" | grep -q 'fell back to another language'
check $? "A3: a render says how much of the page is in the language asked for"
out=$("$CLI" --allow-effects=translation-report effect translation-report es 2>&1)
echo "$out" | grep -q 'fall back to the language they were written in'
check $? "...and translation-report counts every gap in the database"
grep -q 'set r-nar text_es' exports/translate-es.hormiga
check $? "...and writes a replayable script with the source text in place"
grep -q 'atomic' exports/translate-es.hormiga
check $? "...that says how to replay it"

# D6 — the silent empty render, which cost the report an hour
cat > elsewhere.txt <<'EOF'
mantle new someorg
use someorg
rune new image e-img
set e-img path 'assets/nothing.jpg'
tag e-img +type:image
EOF
"$CLI" --script elsewhere.txt --atomic >/dev/null 2>&1
out=$("$CLI" --allow-effects=render-site effect render-site en report-site 2>&1)
echo "$out" | grep -q "the data mantle 'demo-org' is empty"
check $? "D6: runes in a mantle no block query reads are reported, not ignored"
echo "$out" | grep -q 'someorg'
check $? "...and the message names the mantle they are actually in"
echo "$out" | grep -q 'mantle rename someorg demo-org'
check $? "...and the one command that fixes it"

# A5 — check-host, the sibling of check-store for the other one-way door
"$CLI" --describe 2>/dev/null | grep -q 'check-host'
check $? "A5: check-host is in the briefing an agent reads"
out=$("$CLI" effect check-host 2>&1)
echo "$out" | grep -qi 'refused'
check $? "...and it is an effect, so it is refused by default"
out=$("$CLI" --allow-effects=check-host effect check-host 2>&1)
echo "$out" | grep -q 'no publish target'
check $? "...and with nothing wired it says so in both hosts' vocabulary"

# GitHub Pages: the deployer whose node has been in the palette all along
cat > gh.txt <<'EOF'
mantle new antfarm
use antfarm
rune new hol_github gh1
set gh1 repo 'not-a-repo-spec'
set gh1 token_file 'gh.token'
EOF
"$CLI" --script gh.txt --atomic >/dev/null 2>&1
check $? "a hol_github node configures like any other holiday"
printf 'ghp_0123456789abcdefghijklmnopqrstuvwxyz\n' > gh.token
out=$("$CLI" --allow-effects=deploy-site effect deploy-site gh1 2>&1)
echo "$out" | grep -q "as owner/repo"
check $? "...and a repo spec that names no owner is a refusal, not a guess"
echo "$out" | grep -q "somebody else"
check $? "...because guessing an owner publishes to somebody else's repository"
out=$("$CLI" --allow-effects=rollback-site effect rollback-site gh1 2>&1)
echo "$out" | grep -q 'name the deployment to restore'
check $? "a GitHub rollback needs no rollback_cmd - restoring is a ref move"

# ── a deleted page leaves the site (author, 2026-09-02) ─────────────────────
# `render_site` wrote one file per page and removed nothing, so deleting a page
# left its HTML in the folder the deploy uploads and the old URL stayed live
# forever. `site/` is a mirror of the document.
cat > pages.txt <<'EOF'
use report-site
rune new page gone
set gone slug gone
set gone title_en Gone
set gone order 5
EOF
"$CLI" --script pages.txt --atomic >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site report-site >/dev/null 2>&1
[ -f site/gone-en.html ] && [ -f site/gone-es.html ]
check $? "a page renders to its own file in every language"
printf 'use report-site\nrune rm gone\n' > rmpage.txt
"$CLI" --script rmpage.txt --atomic >/dev/null 2>&1
out=$("$CLI" --allow-effects=render-site effect render-site report-site 2>&1)
[ ! -f site/gone-en.html ] && [ ! -f site/gone-es.html ]
check $? "...and deleting the page removes both, so the old URL stops being live"
echo "$out" | grep -q "removed gone-en.html"
check $? "...and the render says which file it took down"
[ -f site/index-en.html ] && [ -f site/404.html ] && [ -f site/style.css ]
check $? "...while the pages that remain, and the site chrome, are untouched"
# a ONE-LANGUAGE render must not take the other language's pages down: the
# preview loop runs `render-site es` constantly and that is not a deletion.
"$CLI" --allow-effects=render-site effect render-site en report-site >/dev/null 2>&1
[ -f site/index-es.html ]
check $? "...and rendering one language leaves the other language's pages alone"

# the link relation is quoted, not concatenated (author 2026-09-02: "cant link
# images together? or there's a weird error"). The GUI builds this command; the
# defect was that a relation with a space silently truncated and one with an
# apostrophe produced a SPEC 6.1 quoting error.
cat > lk.txt <<'EOF'
use demo-org
rune new image lk-a
rune new image lk-b
EOF
"$CLI" --script lk.txt --atomic >/dev/null 2>&1
printf "use demo-org\nlink lk-a lk-b --relation 'goes with'\n" > lk2.txt
"$CLI" --script lk2.txt --atomic >/dev/null 2>&1
check $? "a quoted multi-word relation applies"
"$CLI" links lk-a 2>&1 | grep -q -- "-goes with->"
check $? "...and survives whole, instead of truncating at the space"

# ── the `audio` block (field report A1, first rung; 2026-09-02) ─────────────
# "a music artist's website cannot play the artist's music" -- and an
# organization with a podcast or a recorded meeting has the same absence.
python - <<'PYEOF'
import struct, math, io, os
os.makedirs('assets', exist_ok=True)
sr = 8000; n = int(sr * 0.4)
d = b''.join(struct.pack('<h', int(12000 * math.sin(2 * math.pi * 440 * i / sr)))
             for i in range(n))
io.open('assets/tone.wav', 'wb').write(
    b'RIFF' + struct.pack('<I', 36 + len(d)) + b'WAVEfmt ' +
    struct.pack('<IHHIIHH', 16, 1, 1, sr, sr * 2, 2, 16) +
    b'data' + struct.pack('<I', len(d)) + d)
PYEOF
check $? "a test recording exists to render"
cat > audio.txt <<'EOF'
use report-site
rune new audio a-good
set a-good src 'assets/tone.wav'
set a-good title_en 'Track One'
set a-good artist 'The Organization'
set a-good duration '0:24'
set a-good caption_en 'From the meeting.'
set a-good row 20
rune new audio a-missing
set a-missing src 'assets/not-here.mp3'
set a-missing title_en 'A recording nobody moved over'
set a-missing row 21
EOF
"$CLI" --script audio.txt --atomic >/dev/null 2>&1
check $? "audio blocks apply"
out=$("$CLI" --allow-effects=render-site effect render-site en report-site 2>&1)

grep -q 'class="audio-player" controls preload="none" src="assets/tone.wav"' site/index-en.html
check $? "A1: the page carries a real player for the organization's own file"
[ -f site/assets/tone.wav ]
check $? "...and the file is staged into the site like any other asset"
grep -q 'preload="none"' site/index-en.html
check $? "...fetching nothing until somebody presses play"
grep -q 'audio.*<a href="assets/tone.wav">' site/index-en.html
check $? "...with a download link inside it for a browser that cannot play it"
grep -q 'class="audio-title">Track One<' site/index-en.html
check $? "...and the title, artist and duration on the card"
grep -q 'class="audio-dur">0:24<' site/index-en.html
check $? "...including the duration as the operator wrote it"

# a file that is not on this machine must SAY so, not render a dead player
grep -q 'This recording is not available' site/index-en.html
check $? "a missing audio file gets a sentence, not a play button that does nothing"
echo "$out" | grep -q "is not on this machine"
check $? "...and the render names the file and how to fix it"

# EMAIL: no client plays audio, so the newsletter links it -- which needs an
# absolute address, which only site.base_url can supply.
out=$("$CLI" --allow-effects=render effect render en report-site 2>&1)
echo "$out" | grep -q "site.base_url is unset"
check $? "without a base URL the newsletter says why it cannot link the recording"
"$CLI" config set site.base_url 'https://example.org' >/dev/null 2>&1
"$CLI" --allow-effects=render effect render en report-site >/dev/null 2>&1
ls exports/*.html >/dev/null 2>&1
check $? "the newsletter renders"
grep -q 'https://example.org/assets/tone.wav' exports/*.html
check $? "...and with one, the Listen button points at a real address"
grep -q '<audio' exports/*.html
[ $? -ne 0 ]
check $? "...and never emits <audio>, which Gmail and Outlook both strip"

# the block is in the briefing, so an agent can place it too
"$CLI" --describe 2>/dev/null | grep -q '"audio"'
check $? "the audio glyph is in the briefing an agent reads"

# ── the `download` block (portfolio agent, 2026-09-02) ─────────────────────
# "A Hormiga website cannot publish a file a visitor can download" -- which was
# blocking that client's first deploy. A resume is what surfaced it; a flier
# PDF, the bylaws and an annual report are the same ask for an organization.
mkdir -p files
printf '%%PDF-1.4\n' > files/handbook.pdf
head -c 9000 /dev/urandom >> files/handbook.pdf
printf '<html>a page on our own origin</html>' > files/page.html
cat > dl.txt <<'EOF'
use demo-org
rune new resource r-handbook
set r-handbook path 'files/handbook.pdf'
set r-handbook topic 'governance'
use report-site
rune new download dl-btn
set dl-btn file 'files/handbook.pdf'
set dl-btn label_en 'Download the handbook'
set dl-btn caption_en 'PDF, updated September'
set dl-btn row 30
rune new download dl-viaresource
set dl-viaresource file r-handbook
set dl-viaresource label_en 'Our bylaws'
set dl-viaresource download_style card
set dl-viaresource row 31
rune new download dl-refused
set dl-refused file 'files/page.html'
set dl-refused label_en 'Should not publish'
set dl-refused row 32
rune new download dl-absent
set dl-absent file 'files/nowhere.pdf'
set dl-absent label_en 'Not here'
set dl-absent row 33
EOF
"$CLI" --script dl.txt --atomic >/dev/null 2>&1
check $? "download blocks apply"
out=$("$CLI" --allow-effects=render-site effect render-site en report-site 2>&1)

grep -q 'href="assets/handbook.pdf" download' site/index-en.html
check $? "A1: a file the organization owns is published with a download link"
[ -f site/assets/handbook.pdf ]
check $? "...and staged into the site like any other asset"
grep -q 'class="dl-meta">PDF' site/index-en.html
check $? "...with the type and size a visitor wants before they tap it"
grep -q 'class="dl-card' site/index-en.html
check $? "...and `download_style card` gives the wider shape"

# `file` takes a resource RUNE name too -- which is what finally makes the
# `resource` glyph reachable. It was declared, editable and rendered by nothing.
grep -q 'dl-label">Our bylaws<' site/index-en.html
check $? "...and `file` accepts a `resource` rune name, not just a path"

# the security refusal: model data can arrive by import or merge, and a .html
# served from our own origin acts with our own authority
grep -q 'page.html' site/index-en.html
[ $? -ne 0 ]
check $? "a .html is never published by this block"
[ ! -f site/assets/page.html ]
check $? "...and is not even staged"
echo "$out" | grep -q "refusing to publish"
check $? "...and the render says so out loud, with the reason"
echo "$out" | grep -q "is not on this machine"
check $? "a file that is absent is reported, not published broken"

# EMAIL: a newsletter cannot carry the file, so it links -- which needs an
# absolute address
"$CLI" config set site.base_url 'https://example.org' >/dev/null 2>&1
"$CLI" --allow-effects=render effect render en report-site >/dev/null 2>&1
grep -q 'https://example.org/assets/handbook.pdf' exports/*.html
check $? "the newsletter links the file at an absolute address"

"$CLI" --describe 2>/dev/null | grep -q '"download"'
check $? "the download glyph is in the briefing an agent reads"

# ── PLATFORM SETS: the download for the visitor's computer (2026-09-08) ────
# The author asked for OS detection; the Click LaFont report is why it is
# renderer-owned rather than an author `<script>`, and why it may only reorder
# and mark. The rule under test is the one that makes it safe: NOTHING IS EVER
# HIDDEN -- not by the renderer, and not by app.js, which does not run at all
# for the visitor who has scripting off.
cat > plat.txt <<'EOF'
use report-site
rune new link dl-win
set dl-win label_en 'Download for Windows'
set dl-win label_es 'Descargar para Windows'
set dl-win target 'https://github.com/migriv24/VoidHormiga/releases/latest/download/VoidHormiga-windows-x64-setup.exe'
set dl-win link_style 'button'
set dl-win platform 'windows-x64'
set dl-win row 34
set dl-win col 0
set dl-win span 4
rune new link dl-mac
set dl-mac label_en 'macOS - not yet'
set dl-mac target 'roadmap'
set dl-mac platform 'macos'
set dl-mac row 34
set dl-mac col 4
set dl-mac span 4
rune new download dl-linux
set dl-linux file 'files/handbook.pdf'
set dl-linux label_en 'Linux - build it yourself'
set dl-linux download_style card
set dl-linux platform 'linux-x64'
set dl-linux row 34
set dl-linux col 8
set dl-linux span 4
rune new link dl-typo
set dl-typo label_en 'Typo'
set dl-typo target 'https://example.org/x'
set dl-typo platform 'win64'
set dl-typo row 35
set dl-typo col 0
set dl-typo span 6
rune new link dl-plain
set dl-plain label_en 'Read the roadmap'
set dl-plain target 'roadmap'
set dl-plain row 35
set dl-plain col 6
set dl-plain span 6
rune new link dl-lonely
set dl-lonely label_en 'Download for Windows'
set dl-lonely target 'https://example.org/setup.exe'
set dl-lonely platform 'windows-x64'
set dl-lonely row 36
set dl-lonely col 0
set dl-lonely span 6
rune new link dl-nearby
set dl-nearby label_en 'Release notes'
set dl-nearby target 'https://example.org/notes'
set dl-nearby row 36
set dl-nearby col 6
set dl-nearby span 6
EOF
"$CLI" --script plat.txt --atomic >/dev/null 2>&1
check $? "platform-set blocks apply"
out=$("$CLI" --allow-effects=render-site effect render-site en report-site 2>&1)

grep -q 'class="wrow platform-set"' site/index-en.html
check $? "a row with two or more platform blocks is marked a platform set"
grep -q 'data-yours="For your computer"' site/index-en.html
check $? "...and carries the badge text, so app.js never has to know a language"
grep -q 'data-platform="windows-x64"' site/index-en.html   && grep -q 'data-platform="macos"' site/index-en.html   && grep -q 'data-platform="linux-x64"' site/index-en.html
check $? "...with each of the three candidates naming its own computer"
grep -q 'btn[^>]*data-platform="windows-x64"' site/index-en.html
check $? "...the installer button among them, because it is a link and not a file"
grep -q 'dl-card[^>]*data-platform="linux-x64"' site/index-en.html
check $? "...and a download block can name one too"

# the whole design in one assertion: every platform is in the markup. app.js
# reorders and labels; it has nothing to hide with, by construction.
grep -q 'macOS - not yet' site/index-en.html
check $? "...and no platform is left out of the page, which is the whole rule"

# a typo must not read as `any` in silence -- the image_grid.columns failure
echo "$out" | grep -q "dl-typo: platform 'win64' is not a value"
check $? "an unknown platform is reported rather than silently treated as any"
grep -q 'data-platform="win64"' site/index-en.html
[ $? -ne 0 ]
check $? "...and never reaches the markup"

# one platform block on a row is not a set: there is nothing to choose between
grep -q 'class="wrow platform-set" data-yours[^>]*>.*dl-lonely' site/index-en.html
[ $? -ne 0 ]
check $? "a row with only one platform block is not a platform set"
grep -c 'platform-set' site/index-en.html | grep -qx '1'
check $? "...so exactly one row on this page is one"

"$CLI" --allow-effects=render-site effect render-site es report-site >/dev/null 2>&1
grep -q 'data-yours="Para tu computadora"' site/index-es.html
check $? "the badge is in the language of the page it is on"

"$CLI" --describe 2>/dev/null | grep -q '"platform"'
check $? "the platform field is in the briefing an agent reads"

# ── D1: `site.languages` was accepted, stored and ignored ──────────────────
# The author declined a one-language switch; the fix is that the key stops
# pretending to be one.
"$CLI" config set site.languages 'en' >/dev/null 2>&1
out=$("$CLI" --allow-effects=render-site effect render-site report-site 2>&1)
echo "$out" | grep -q "site.languages"
check $? "D1: setting site.languages is reported as a key nothing reads"
echo "$out" | grep -q "translation-report"
check $? "...and names the thing that does exist"
[ -f site/index-es.html ]
check $? "...and both languages are still published, which is the decision"

# ── the portfolio's second round (2026-09-03) ──────────────────────────────

# D2: `pack-database` bundled only assets/, so a file a BLOCK points at from
# anywhere else was silently absent from every bundle -- and a credential must
# still never be in one.
mkdir -p papers
printf 'PDFBYTES' > papers/annual-report.pdf
printf 'SECRET-DEPLOY-TOKEN' > cf.key
cat > packref.txt <<'EOF'
use antfarm
set gh1 token_file 'cf.key'
use report-site
rune new download dl-outside
set dl-outside file 'papers/annual-report.pdf'
set dl-outside label_en 'Annual report'
set dl-outside row 40
EOF
"$CLI" --script packref.txt --atomic >/dev/null 2>&1
out=$("$CLI" --allow-effects=pack-database effect pack-database packtest.miga 2>&1)
echo "$out" | grep -q "bundled papers/annual-report.pdf"
check $? "D2: a file a block points at from outside assets/ is bundled"
python - <<'PYEOF'
import io, json, sys
d = json.load(io.open('packtest.miga', encoding='utf-8'))
keys = list(d.get('assets', {}))
ok = any(k.endswith('papers/annual-report.pdf') for k in keys)
leak = any('key' in k or 'token' in k or 'secret' in k for k in keys)
sys.exit(0 if (ok and not leak) else 1)
PYEOF
check $? "...and it is really in the envelope, with no credential beside it"

# A5: a data rune's prose is per-language now. `directory` publishes an
# organization's own description, and it was the one text a bilingual site
# could not translate.
cat > bilingual.txt <<'EOF'
use demo-org
rune new organization o-bi
set o-bi display_name 'Nomad'
set o-bi bio_en 'A geometry-nodes experiment.'
set o-bi bio_es 'Un experimento de nodos de geometria.'
tag o-bi +type:organization +clearance:public
rune new organization o-legacy
set o-legacy display_name 'Older Entry'
set o-legacy bio 'Written before the fields were split.'
tag o-legacy +type:organization +clearance:public
use report-site
rune new directory dir-bi
set dir-bi kind organization
set dir-bi query 'type:organization'
set dir-bi row 41
EOF
"$CLI" --script bilingual.txt --atomic >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site report-site >/dev/null 2>&1
grep -q 'A geometry-nodes experiment' site/index-en.html
check $? "A5: an organization's description reaches the English page"
grep -q 'Un experimento de nodos de geometria' site/index-es.html
check $? "...and the Spanish page carries the SPANISH one, which it could not before"
grep -q 'A geometry-nodes experiment' site/index-es.html
[ $? -ne 0 ]
check $? "...and not the English one alongside it"
grep -q 'Written before the fields were split' site/index-es.html
check $? "...while a legacy `bio` with no language still publishes, on both pages"

# A5.1: an email address has no Spanish, so counting it made 100% unreachable
# and the warning permanent.
cat > untrans.txt <<'EOF'
mantle new solo
use solo
rune new page home
set home slug home
set home order 0
rune new link l-mail
set l-mail label_en 'someone@example.org'
set l-mail row 0
rune new link l-tel
set l-tel label_en '541-555-0100'
set l-tel row 1
rune new narrative n-prose
set n-prose text_en 'Real prose.'
set n-prose text_es 'Prosa real.'
set n-prose row 2
EOF
"$CLI" --script untrans.txt --atomic >/dev/null 2>&1
out=$("$CLI" --allow-effects=render-site effect render-site es solo 2>&1)
echo "$out" | grep -q "fell back"
[ $? -ne 0 ]
check $? "A5: a site whose only untranslated values are an address and a number is SILENT"
out=$("$CLI" --allow-effects=translation-report effect translation-report es 2>&1)
echo "$out" | grep -q "someone@example.org"
[ $? -ne 0 ]
check $? "...and translation-report does not ask anyone to translate an email"

# A6 + A9: one field each, and each deletes a client's CSS hack
cat > heroparts.txt <<'EOF'
use report-site
rune new hero h-portrait
set h-portrait title_en 'Miguel Rivas'
set h-portrait portrait 'assets/tone-cover-missing.png'
set h-portrait row 42
rune new narrative n-head
set n-head heading_en 'Void Hormiga'
set n-head text_en 'Developer, 2026 to present'
set n-head row 43
EOF
"$CLI" --script heroparts.txt --atomic >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en report-site >/dev/null 2>&1
grep -q '<h3 class="prose-heading' site/index-en.html
check $? "A9: a narrative can carry a heading, instead of a secretly-bold first line"
grep -q 'prose-heading' site/style.css
check $? "...and the stylesheet sizes it below the page's own section headings"
grep -q 'hero-portrait' site/style.css
check $? "A6: the hero has a round portrait slot in front of its banner"
cd ..

# A failed command must exit non-zero: a shell and an agent both branch on it.
"$CLI" rune new no-such-glyph x >/dev/null 2>&1
[ $? -ne 0 ]
check $? "a failed command exits non-zero"

if [ "$fail" = "0" ]; then echo "OK - headless: brief, batch, review, gate, render"; fi
exit $fail
