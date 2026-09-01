/* submission_smoke.cpp — the gate that stands between a stranger's text and the
 * organization's database.
 *
 * Every case below is either a real injection that worked against Void Core
 * 0.2.6, or the property that stops it. Two of the three were found by Core
 * rather than by us, after we reported the first and got our diagnosis
 * corrected — which is the argument for the gate calling THEIR decoder instead
 * of a parser we wrote: we did not think of routes 2 and 3, and we would not
 * have thought of route 4 either.
 */
#include "domain/submission.hpp"

#include <cstdio>
#include <string>

static int failures = 0;
static void ok(bool cond, const char* what) {
    std::printf("  %s %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond) ++failures;
}

static const char NL = '\n';
static const char SQ = '\'';
static const char BS = '\\';

int main() {
    using namespace hormiga::submission;

    std::printf("submission: an ordinary, honest proposal\n");
    {
        std::string t = "rune new contact ana-visitor\n";
        t += "set ana-visitor display_name 'Ana Ruiz'\n";
        t += "tag ana-visitor +type:contact\n";
        Review r = review(t);
        ok(r.ok, "is accepted");
        ok(r.commands.size() == 3, "...with every statement seen");
        ok(r.commands.size() > 1 && r.commands[1].argv.size() == 4 &&
               r.commands[1].argv[3] == "Ana Ruiz",
           "...and a value with a space is ONE argument, not two");
    }

    std::printf("submission: a value carrying a newline is DATA, not a command\n");
    {
        /* The injection we reported on 2026-08-21. The gate must see TWO
         * commands here, not three — the second line is inside the bio. */
        std::string bio = std::string("I volunteer on weekends.") + NL +
                          "set treasurer email attacker@evil.example";
        std::string t = "rune new contact visitor\n";
        t += std::string("set visitor bio ") + SQ + bio + SQ + "\n";
        Review r = review(t);
        ok(r.ok, "is accepted (a newline in prose is legitimate)");
        ok(r.commands.size() == 2, "...as TWO commands, not three");
        ok(r.commands.size() == 2 && r.commands[1].argv.size() == 4 &&
               r.commands[1].argv[3] == bio,
           "...and the whole bio, newline and all, is one argument");
        // the reviewer must be able to SEE the injected text, in the value
        ok(r.commands.size() == 2 &&
               r.commands[1].argv[3].find("attacker@evil.example") !=
                   std::string::npos,
           "...with the suspicious text visible where a reviewer will read it");
    }

    std::printf("submission: the verb gate\n");
    {
        ok(!review("effect deploy-site org-pages").ok, "`effect` is refused");
        ok(!review("deploy site").ok, "`deploy` is refused");
        ok(!review("use antfarm").ok, "`use` is refused");
        ok(!review("config set site.base_url https://evil.example").ok,
           "`config` is refused");
        ok(!review("script run something").ok, "`script` is refused");
        // and the refusal explains rather than just denying
        Review r = review("effect deploy-site");
        ok(r.refusal.find("effect") != std::string::npos,
           "...and the refusal names what was attempted");
        /* The line number is APPROXIMATE and the wording says so: Void Core's
         * transcript splitter does not count newlines consumed inside a quoted
         * value, so every later number drifts low. Reported upstream by Void
         * Maiz. Presenting it as exact would be quietly wrong. */
        ok(r.refusal.find("at or after line") != std::string::npos,
           "...and hedges the line number, which upstream drifts");
    }

    std::printf("submission: an allowed verb hidden behind a refused one\n");
    {
        /* The gate must refuse the WHOLE transcript, not filter it. Approving
         * "the good parts" of a hostile proposal is how a reviewer ends up
         * dispatching something nobody read. */
        std::string t = "rune new contact ok-guy\n";
        t += "effect deploy-site\n";
        t += "tag ok-guy +type:contact\n";
        Review r = review(t);
        ok(!r.ok, "one bad statement refuses the whole transcript");
        ok(r.commands.empty(), "...and no partial command list is offered");
    }

    std::printf("submission: control flow cannot hide what a proposal does\n");
    {
        /* Core reports `flat`: false when a statement opens a block or begins
         * with a §8 control word. A reviewer reads statements; anything that
         * decides at RUN time what runs makes that reading a guess. */
        Review r = review("if 1 { set treasurer email attacker@evil.example }");
        ok(!r.ok, "a block is refused");
        ok(r.refusal.find("control flow") != std::string::npos,
           "...and the refusal says why that matters");
    }

    std::printf("submission: malformed input is refused, never repaired\n");
    {
        // SPEC §6.1 rule 5, changed in Core 0.2.7: an unterminated quoted run
        // is an ERROR. Before that it ran to end of input, silently.
        Review r = review(std::string("set v bio ") + SQ + "never closed");
        ok(!r.ok, "an unterminated quote is refused");
        ok(r.refusal.find("nterminated") != std::string::npos,
           "...naming the actual fault");
        ok(!review("").ok, "an empty proposal has nothing to do");
        ok(!review("# just a comment\n").ok, "...and so does a comment-only one");
    }

    std::printf("submission: Void Core's two routes, which we did not think of\n");
    {
        /* Route 2 — an escaped apostrophe then a brace. Against 0.2.6 this set
         * a second rune. It must now be ordinary text inside one value. */
        std::string v = std::string("don") + BS + SQ + "t } set treasurer f BREACHED";
        Review r = review(std::string("set visitor d ") + SQ + v + SQ);
        ok(r.ok && r.commands.size() == 1,
           "an escaped apostrophe followed by `}` is one command");
        ok(r.commands.size() == 1 && r.commands[0].argv.size() == 4 &&
               r.commands[0].argv[3].find("BREACHED") != std::string::npos,
           "...with the whole thing kept as the value");

        /* Route 3 — command substitution inside single quotes. */
        Review r2 = review(std::string("set visitor e ") + SQ +
                           "a stranger wrote $(rune ls)" + SQ);
        ok(r2.ok && r2.commands.size() == 1,
           "$(...) inside single quotes is one command");
        ok(r2.commands.size() == 1 && r2.commands[0].argv.size() == 4 &&
               r2.commands[0].argv[3] == "a stranger wrote $(rune ls)",
           "...and stays literal text, unexpanded");
    }

    std::printf("submission: the LOCAL import policy is the same gate\n");
    {
        /* Void Reyna's harvested datasets went through a separate filter until
         * 2026-08-25 -- one that read the verb as text and, worse, read the file
         * LINE BY LINE, so a value containing a newline had its second line
         * checked and approved as its own command. A harvested PDF whose prose
         * happened to contain a line starting `set` would have had it
         * dispatched.
         *
         * One gate now, two policies. The difference is two verbs, not two
         * implementations. */
        const std::string t = std::string("mantle new harvested") + NL +
                              "rune new contact a" + NL;
        ok(!review(t).ok, "a stranger may NOT create a mantle");
        ok(review(t, local_import_verbs()).ok, "...but a local import may");
        ok(!review("effect deploy-site", local_import_verbs()).ok,
           "and a local import still may not cause an effect");

        /* The hole the old line-by-line filter had, in the shape it had it. */
        std::string bio = std::string("Extracted prose.") + NL +
                          "set treasurer email attacker@evil.example";
        Review r = review(std::string("set org bio ") + SQ + bio + SQ,
                          local_import_verbs());
        ok(r.ok && r.commands.size() == 1,
           "a harvested value containing a newline is ONE command, not two");
    }

    std::printf(failures ? "\nFAILED (%d)\n" : "\nOK - the gate holds\n", failures);
    return failures ? 1 : 0;
}
