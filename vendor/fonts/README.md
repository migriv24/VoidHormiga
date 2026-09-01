# Vendored fonts

All fonts here are under the SIL Open Font License 1.1 (OFL) — free to embed and
redistribute, including in this public repo and in generated sites. Per the
"vendor, don't depend" rule they are checked in, not fetched at runtime/CDN.

## App UI + map-export atlas (static TTF, loaded by ImGui)
- `Lato-Regular.ttf` — Łukasz Dziedzic. License: `OFL-Lato.txt`
- `fa-solid-900.ttf` — Font Awesome 6 Free (solid), icon glyphs only. (see IconsFontAwesome6.h)

## Web output (latin-subset woff2, embedded via @font-face in site/style.css)
- `web/inter-{400,600,700}.woff2`        — Inter, Rasmus Andersson. License: `OFL-Inter.txt`
- `web/sourceserif-{400,700}.woff2`      — Source Serif 4, Adobe. License: `OFL-SourceSerif.txt`
- `web/spacegrotesk-{500,700}.woff2`     — Space Grotesk, Florian Karsten. License: `OFL-SpaceGrotesk.txt`
