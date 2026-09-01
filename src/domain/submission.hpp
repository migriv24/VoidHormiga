/* submission.hpp — the gate a proposed command transcript must pass.
 *
 * ── WHAT THIS GUARDS ─────────────────────────────────────────────────────────
 *
 * okf/concepts/platform/web-platform.md §4: a website visitor does not write, they
 * PROPOSE. A proposal is a command transcript; approving it is dispatching that
 * transcript in an admin's session, as one attributed, undoable batch. The same
 * seam carries Void Reyna's harvested datasets and Allomone's Weaver, which is
 * three independent arrivals at one shape and the reason to trust it.
 *
 * The text inside a submission is written by a stranger. That is not an edge
 * case, it is the feature.
 *
 * ── WHY THIS IS NOT A PARSER WE WROTE ────────────────────────────────────────
 *
 * On 2026-08-21 we demonstrated that a newline inside a quoted value ended the
 * command and the remainder EXECUTED: one submitted `bio` rewrote a different
 * contact's email. Our verb filter did not help, because the injected verb was
 * `set` — exactly what a submission is supposed to contain.
 *
 * Void Core 0.2.7 closed it, corrected our diagnosis (their argv tokenizer had
 * always carried newlines; the fault was in three OTHER scanners), and found two
 * further routes we had not thought to test — an escaped apostrophe followed by
 * `}`, and `$(…)` expanding inside single quotes. Five implementations of SPEC
 * §6.1 have now been wrong, including ours twice and including the reference
 * core's own script reader.
 *
 * So the rule this file exists to hold is: **we do not parse a transcript. We
 * ask the engine that will run it what it will do.** `vc_transcript_split_json`
 * is the decoder half of the codec, exported for exactly this purpose. Any
 * check we performed on text we tokenized ourselves would be a check on a
 * different program than the one that runs.
 *
 * ── THE THREE QUESTIONS, IN ORDER ────────────────────────────────────────────
 *
 * 1. Does it parse at all? An unterminated quote is now an ERROR (§6.1 rule 5,
 *    changed in 0.2.7) rather than something that silently runs to end of
 *    input. A submission that does not parse is refused, not repaired.
 * 2. Is it FLAT? Core's word: no statement opens a block or begins with a §8
 *    control word. A flat transcript's effect can be read off its statements
 *    without simulating it — which is precisely what a reviewer, human or
 *    machine, is assuming when they read one.
 * 3. Is every verb allowed? Model-building only. `use`, `config`, `effect`,
 *    `deploy` and `script` are refused, so a stranger's proposal structurally
 *    cannot become an effect.
 *
 * Order matters: 3 alone is what we had, and it is what the injection walked
 * straight past. 1 and 2 are what make 3 mean anything, because they are what
 * guarantee the verbs we inspect are the verbs that will run.
 */
#pragma once

extern "C" {
#include "voidcore.h"
}

#include "json.hpp"

#include <cstdlib>
#include <set>
#include <string>
#include <vector>

namespace hormiga::submission {

/* One statement, as Void Core will actually run it. */
struct Command {
    int line = 0;
    std::string verb;
    std::vector<std::string> argv;
    std::string text;   // the source, for showing a reviewer
};

struct Review {
    bool ok = false;
    std::string refusal;          // why, in a sentence for a person
    std::vector<Command> commands;
};

/* The verbs a stranger's proposal may contain: model-building only.
 *
 * A deliberately SHORT allow-list rather than a deny-list. A deny-list is wrong
 * for the same reason everywhere: it fails open on the verb somebody adds next
 * year, and the person adding it will not be thinking about submissions. */
inline const std::set<std::string>& allowed_verbs() {
    static const std::set<std::string> v{
        "rune", "set", "setjson", "tag", "untag", "link", "unlink", "relate",
    };
    return v;
}

/* A LOCAL import — Void Reyna's harvested dataset, a CSV-shaped transcript — is
 * the same gate with one more verb.
 *
 * It may create MANTLES, because importing a dataset legitimately means "here is
 * a new body of data"; a website visitor proposing one would be proposing a
 * place to put things nobody asked for. `place` is here for the same reason: a
 * local import may position what it creates.
 *
 * Two policies, ONE gate. web-platform.md §4 already claimed these were the same
 * seam — *"Allomone's Weaver, Void Reyna, and a website visitor: same seam, three
 * sources"* — and until 2026-08-25 that was true of the design and false of the
 * code, which had two implementations. This is the claim made honest. */
inline const std::set<std::string>& local_import_verbs() {
    static const std::set<std::string> v{
        "mantle", "rune", "set", "setjson", "tag", "untag", "link", "unlink",
        "relate", "place",
    };
    return v;
}

/* Review a proposed transcript. Never dispatches; never repairs. */
inline Review review(const std::string& transcript,
                     const std::set<std::string>& allowed = allowed_verbs()) {
    Review out;
    char* raw = vc_transcript_split_json(transcript.c_str());
    if (!raw) {
        out.refusal = "the transcript could not be read at all";
        return out;
    }
    std::string json(raw);
    std::free(raw);

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
    } catch (...) {
        out.refusal = "the transcript could not be read at all";
        return out;
    }

    /* 1. DOES IT PARSE. Core reports the line, so the refusal can too — a
     * submitter who mis-quoted something deserves to know where.
     *
     * ── THE LINE NUMBER IS APPROXIMATE, AND SAYS SO (2026-08-25) ────────────
     *
     * Void Maiz found that `vc_transcript_split_json` advances its line counter
     * only at a statement boundary, so newlines consumed INSIDE a quoted value
     * are never counted and every later number drifts low by exactly that many.
     * Reported upstream by them; the split itself is correct, only the number
     * attached to it is off.
     *
     * That matters here specifically, because this refusal is shown to a
     * person. A submission carrying a multi-line bio — which is the ordinary
     * case, not the exotic one — will report every later problem against the
     * wrong line. So the wording hedges: "at or after". A number presented as
     * exact and quietly wrong is worse than one presented as a starting point,
     * and the alternative (counting lines ourselves) would be a sixth
     * implementation of the thing this whole file exists to stop us writing. */
    if (!j.value("ok", false)) {
        out.refusal = j.value("error", std::string("the transcript is malformed"));
        if (j.contains("line"))
            out.refusal +=
                " (at or after line " + std::to_string(j.value("line", 0)) + ")";
        return out;
    }

    /* 2. IS IT FLAT. Control flow in a proposal is not a feature we have
     * declined to implement — it is a thing that makes the other checks
     * meaningless, because a loop or a conditional decides at RUN time what
     * runs, and a reviewer read it at REVIEW time. */
    if (!j.value("flat", false)) {
        out.refusal =
            "this proposal contains control flow, so what it does cannot be "
            "read from what it says. Only plain statements are accepted.";
        return out;
    }

    for (const auto& c : j.value("commands", nlohmann::json::array())) {
        Command cmd;
        cmd.line = c.value("line", 0);
        cmd.text = c.value("text", std::string());
        for (const auto& a : c.value("argv", nlohmann::json::array()))
            cmd.argv.push_back(a.get<std::string>());
        if (cmd.argv.empty()) continue;    // a blank or comment-only statement
        cmd.verb = cmd.argv.front();

        /* 3. IS THE VERB ALLOWED. Read off the argv Core produced, never off
         * the source text — the whole point of asking the engine. */
        if (!allowed.count(cmd.verb)) {
            out.refusal = "at or after line " + std::to_string(cmd.line) +
                          ": `" + cmd.verb +
                          "` is not something a proposal may do. A submission "
                          "may add and describe things; it may not use, "
                          "configure, deploy, or cause an effect.";
            out.commands.clear();
            return out;
        }
        out.commands.push_back(std::move(cmd));
    }

    if (out.commands.empty()) {
        out.refusal = "this proposal contains nothing to do";
        return out;
    }
    out.ok = true;
    return out;
}

} // namespace hormiga::submission
