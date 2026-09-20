#!/usr/bin/env bash
# golden_render.sh — render a fixture site and hash every output file.
#
# THE REFACTOR'S SAFETY NET, and the strongest check available for it: a
# refactor that changes a byte of rendered output is not a refactor. The smoke
# suite asserts that particular things are PRESENT; this asserts that nothing
# ELSE moved — whitespace, attribute order, the order blocks are emitted in.
#
#   tools/golden_render.sh <cli> capture   → writes tests/data/golden.sha256
#   tools/golden_render.sh <cli> check     → diffs against it, non-zero on drift
#
# Deliberately fixture-driven rather than pointing at the developer's own
# database: the whole point is a comparison that means the same thing tomorrow.
set -u
CLI="$1"
MODE="${2:-check}"
GOLDEN="$(cd "$(dirname "$0")/.." && pwd)/tests/data/golden.sha256"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
cd "$WORK" || exit 2

cat > fixture.txt <<'EOF'
mantle new demo-org
use demo-org
rune new event golden-morning
set golden-morning title_en 'Network Meeting'
set golden-morning title_es 'Reunion de la Red'
set golden-morning summary_en 'The monthly all-network meeting, upstairs.'
set golden-morning summary_es 'La reunion mensual de toda la red, arriba.'
set golden-morning date '2026-08-19'
set golden-morning start_time '8:00 AM'
set golden-morning end_time '10:00 AM'
set golden-morning venue 'Community Education Building, 100 Main St, Riverton, OR 97000'
set golden-morning color '#2563eb'
tag golden-morning +type:event +news:golden
rune new event golden-noon
set golden-noon title_en 'Relaunch'
set golden-noon date '2026-09-16'
set golden-noon start_time '11:00 AM'
set golden-noon end_time '1:00 PM'
tag golden-noon +type:event +news:golden
rune new job golden-job
set golden-job org 'Riverton County'
set golden-job pay '$24/hr'
set golden-job location 'Riverton, OR'
set golden-job job_type 'part-time'
set golden-job deadline '2026-10-01'
set golden-job contact_email 'jobs@example.org'
tag golden-job +type:job
rune new contact golden-person
set golden-person display_name 'Ana Ruiz'
set golden-person role 'Housing Lead'
set golden-person bio 'Coordinates the housing working group.'
set golden-person email 'ana@example.org'
tag golden-person +type:contact +clearance:public +clearance:contact
rune new event golden-standing
set golden-standing title_en 'Riverton Community Meeting'
set golden-standing days 'Last Friday of the Month'
tag golden-standing +type:event +news:golden
rune new organization golden-org
set golden-org display_name 'County Council of Governments'
set golden-org geo '44.05,-123.09'
tag golden-org +type:organization
mantle new golden-site
use golden-site
rune new page home
set home slug home
set home title_en Home
set home title_es Inicio
set home order 0
rune new hero golden-hero
set golden-hero title_en 'Riverton Community Network'
set golden-hero subtitle_en '1234 Main St, Springfield OR'
set golden-hero row 0
rune new event_grid golden-ev
set golden-ev query 'type:event AND news:golden'
set golden-ev sort date
set golden-ev caption_en 'Everything coming up.'
set golden-ev row 1
rune new job_grid golden-jg
set golden-jg query 'type:job'
set golden-jg detail compact
set golden-jg row 2
rune new directory golden-dir
set golden-dir kind contact
set golden-dir query 'type:contact'
set golden-dir row 3
rune new event_grid golden-recurring
# A DATE PREDICATE THE CLOCK CANNOT MOVE. `date:recurring` and `date:undated`
# are the two `date:` answers that do not depend on what today is, so pinning
# one of them puts the predicate path through the renderer under the golden
# permanently. `date:future` deliberately is NOT pinned here: it would hash
# differently the day golden-noon's date goes past, and a golden that expires
# is a golden nobody trusts.
set golden-recurring query 'type:event AND date:recurring'
set golden-recurring caption_en 'Standing meetings.'
set golden-recurring row 4
rune new video golden-video
set golden-video url 'https://youtu.be/dQw4w9WgXcQ'
set golden-video caption_en 'From the newsletter.'
set golden-video caption_es 'Del boletin.'
set golden-video row 5
rune new calendar_embed golden-cal
set golden-cal query 'type:event'
set golden-cal row 6
rune new map_embed golden-map
set golden-map row 7
EOF

"$CLI" --script fixture.txt --atomic >/dev/null 2>&1 || { echo "fixture failed"; exit 2; }
"$CLI" config set site.base_url 'https://golden.example.org' >/dev/null 2>&1
"$CLI" config set theme.accent '#2e6b4f' >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site en golden-site >/dev/null 2>&1
"$CLI" --allow-effects=render-site effect render-site es golden-site >/dev/null 2>&1
"$CLI" --allow-effects=render effect render en golden-site >/dev/null 2>&1

# UID joined this list 2026-09-10, for a more interesting reason than the clock.
# A VEVENT's UID is now built from the rune's FROZEN `spirit.id` rather than its
# editable `name` -- a UID from the name tells every subscriber that renaming an
# event DELETED it and created an unrelated new one. An id is minted once and
# never reused, so it is exactly stable where stability matters (inside one
# organization's database, across every publish) and necessarily different here,
# where each run builds the fixture from nothing. Pinning it would pin the mint
# rather than the renderer. The golden still holds that the UID is present,
# well-formed and one per event; the rest of the calendar -- the folding, the
# header, every other property -- is byte-compared as before.
#
# DTSTAMP and the cache-busting query string carry the clock, so they differ on
# every run by design. Neutralise them rather than excluding the files: the rest
# of a calendar and the rest of a page are exactly what we want pinned.
# BYTES, NOT CHARACTERS (2026-09-20). macOS's sed refuses UTF-8 input it cannot
# decode in the runner's locale -- "RE error: illegal byte sequence" -- and this
# fixture is full of Spanish. Every pattern here is ASCII, so reading the file as
# bytes is not a compromise: it is what these substitutions always meant. This is
# why the golden has been red on every macOS CI run since 0.1.2.
norm() {
  LC_ALL=C sed -E -e 's/DTSTAMP:[0-9TZ]+/DTSTAMP:X/' \
         -e 's/UID:[A-Za-z0-9_]+@/UID:X@/' \
         -e 's/\?v=[0-9]+/?v=X/g' \
         -e 's/(--bdim:)[0-9.]+/\1X/g' "$1"
}

sums=""
for f in $(cd site && find . -type f | sort) ; do
  sums="$sums$(norm "site/$f" | sha256sum | cut -d' ' -f1)  site/$f
"
done
for f in $(cd exports 2>/dev/null && find . -type f | sort) ; do
  sums="$sums$(norm "exports/$f" | sha256sum | cut -d' ' -f1)  exports/$f
"
done

if [ "$MODE" = "capture" ]; then
  mkdir -p "$(dirname "$GOLDEN")"
  printf '%s' "$sums" > "$GOLDEN"
  echo "captured $(printf '%s' "$sums" | grep -c . ) file(s) -> $GOLDEN"
  exit 0
fi

if [ ! -f "$GOLDEN" ]; then
  echo "no golden file; run: tools/golden_render.sh <cli> capture"
  exit 2
fi
printf '%s' "$sums" > now.sha256
if diff -u "$GOLDEN" now.sha256 > drift.txt; then
  echo "OK - rendered output is byte-identical to the golden"
  exit 0
fi
echo "DRIFT - rendered output changed:"
cat drift.txt
echo
echo "If the change is intended, re-capture:"
echo "  tools/golden_render.sh <cli> capture"
exit 1
