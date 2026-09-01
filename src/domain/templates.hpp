/* templates.hpp — starter TEMPLATES for the Builder (okf/concepts/sections/builder.md
 * "enablers"; author 2026-07-23: "save pages, save layouts… websites will
 * have a much more complex or interesting template").
 *
 * A template is a NAMED TRANSCRIPT: dispatcher commands that build a document's
 * elements (+ pages + links for websites), plus a theme. Applying one clears
 * the current document and replays the transcript — one undoable batch. This
 * is the whole philosophy again: a layout is a replayable command sequence,
 * not a binary blob. Built-ins live here as data; user "Save as template"
 * writes the same shape to templates/*.json.
 *
 * The NEWSLETTER template is a single vertical document (email physics). The
 * WEBSITE template is deliberately richer: multiple pages, horizontal bands,
 * authored navigation links, query-backed grids, a modern theme.
 */
#pragma once

#include <string>
#include <vector>

namespace hormiga {

struct DocTemplate {
    std::string name, kind, desc;   // kind: "newsletter" | "website"
    std::string accent, bg, ink;    // theme colors (hex; empty = keep current)
    int preset = -1, font = -1, dark = -1; // -1 = keep current
    std::vector<std::string> commands;     // build the elements
};

inline std::vector<DocTemplate> builtin_templates() {
    std::vector<DocTemplate> out;

    // ── a NEWSLETTER: one clean vertical column, email-safe ─────────────────
    {
        DocTemplate t;
        t.name = "Monthly Bulletin";
        t.kind = "newsletter";
        t.desc = "A simple vertical newsletter: masthead, a note, an events "
                 "grid, announcements, a sign-off. Email-safe.";
        t.accent = "#b3592e";
        t.preset = 0;
        t.font = 0;
        t.commands = {
            R"(rune new hero nl-hero)",
            R"(set nl-hero title_en "Monthly Bulletin")",
            R"(set nl-hero title_es "Boletin Mensual")",
            R"(set nl-hero row "0")",
            R"(rune new narrative nl-intro)",
            R"(set nl-intro text_en "A quick hello from our team - here is what is happening this month.")",
            R"(set nl-intro text_es "Un saludo de nuestro equipo - esto es lo que sucede este mes.")",
            R"(set nl-intro row "1")",
            R"(rune new section_header nl-events-h)",
            R"(set nl-events-h title_en "Upcoming Events")",
            R"(set nl-events-h title_es "Proximos Eventos")",
            R"(set nl-events-h row "2")",
            R"(rune new event_grid nl-events)",
            R"(set nl-events query "type:event")",
            R"(set nl-events row "3")",
            R"(rune new section_header nl-news-h)",
            R"(set nl-news-h title_en "Announcements")",
            R"(set nl-news-h title_es "Anuncios")",
            R"(set nl-news-h row "4")",
            R"(rune new narrative nl-news)",
            R"(set nl-news text_en "Share a note, a thank-you, or a call for volunteers here.")",
            R"(set nl-news row "5")",
            R"(rune new footer nl-foot)",
            R"(set nl-foot text_en "Thanks for reading. See you next month.")",
            R"(set nl-foot text_es "Gracias por leer. Nos vemos el proximo mes.")",
            R"(set nl-foot row "6")",
        };
        out.push_back(std::move(t));
    }

    // ── a WEBSITE: multi-page, bands, authored nav, modern theme ────────────
    {
        DocTemplate t;
        t.name = "Modern Community Site";
        t.kind = "website";
        t.desc = "A 3-page site (Home / About / Get Involved) with a full-bleed "
                 "hero, a two-column welcome band, a call-to-action button, an "
                 "events grid, a photo carousel, and a bold modern theme.";
        t.accent = "#2e6b4f";
        t.preset = 2;   // bold / maximal
        t.font = 1;     // serif headings
        t.dark = 1;     // honors the visitor's dark mode
        t.commands = {
            // pages (home in the header nav; About/Involved reached via links)
            R"(rune new page home)",
            R"(set home slug "home")", R"(set home title_en "Home")",
            R"(set home order "0")", R"(set home in_nav "1")",
            R"(rune new page about)",
            R"(set about slug "about")", R"(set about title_en "About")",
            R"(set about order "1")", R"(set about in_nav "1")",
            R"(rune new page involved)",
            R"(set involved slug "involved")",
            R"(set involved title_en "Get Involved")",
            R"(set involved order "2")", R"(set involved in_nav "1")",

            // HOME
            R"(rune new hero w-hero)",
            R"(set w-hero title_en "Building community, together")",
            R"(set w-hero title_es "Construyendo comunidad, juntos")",
            R"(set w-hero row "0")",
            // a two-column welcome band (narrative | narrative)
            R"(rune new narrative w-welcome-l)",
            R"(set w-welcome-l text_en "We are a community outreach organization. This is what we do, and how you can be part of it.")",
            R"(set w-welcome-l row "1")", R"(set w-welcome-l col "0")",
            R"(set w-welcome-l span "6")",
            R"(rune new narrative w-welcome-r)",
            R"(set w-welcome-r text_en "Events, resources, and a network of neighbors. Everyone is welcome.")",
            R"(set w-welcome-r row "1")", R"(set w-welcome-r col "6")",
            R"(set w-welcome-r span "6")",
            // a call-to-action button → the Get Involved page
            R"(rune new link w-cta)",
            R"(set w-cta label_en "Get Involved")",
            R"(set w-cta label_es "Participa")",
            R"(set w-cta target "involved")",
            R"(set w-cta link_style "button")",
            R"(set w-cta row "2")",
            R"(rune new section_header w-events-h)",
            R"(set w-events-h title_en "What's Happening")",
            R"(set w-events-h row "3")",
            R"(rune new event_grid w-events)",
            R"(set w-events query "type:event")",
            R"(set w-events row "4")",
            R"(rune new section_header w-photos-h)",
            R"(set w-photos-h title_en "Our Community")",
            R"(set w-photos-h row "5")",
            R"(rune new image_grid w-photos)",
            R"(set w-photos query "type:image")",
            R"(set w-photos display "carousel")",
            R"(set w-photos row "6")",

            // ABOUT
            R"(rune new section_header a-h)",
            R"(set a-h title_en "About Us")",
            R"(set a-h row "0")", R"(set a-h page "about")",
            R"(rune new narrative a-body)",
            R"(set a-body text_en "Tell your story here: who you serve, your mission, your history.")",
            R"(set a-body row "1")", R"(set a-body page "about")",

            // GET INVOLVED
            R"(rune new section_header i-h)",
            R"(set i-h title_en "Get Involved")",
            R"(set i-h row "0")", R"(set i-h page "involved")",
            R"(rune new narrative i-body)",
            R"(set i-body text_en "Volunteer, donate, or join a working group. Reach out and we will connect you.")",
            R"(set i-body row "1")", R"(set i-body page "involved")",
            R"(rune new job_grid i-jobs)",
            R"(set i-jobs query "type:job")",
            R"(set i-jobs row "2")", R"(set i-jobs page "involved")",
            R"(rune new link i-back)",
            R"(set i-back label_en "Back to Home")",
            R"(set i-back target "home")",
            R"(set i-back link_style "text")",
            R"(set i-back row "3")", R"(set i-back page "involved")",
        };
        out.push_back(std::move(t));
    }

    return out;
}

} // namespace hormiga
