# Contributing to Void Hormiga

Thank you for wanting to help. Void Hormiga is the application community
organizations use to keep their people, events and resources, and to publish
newsletters and websites from them. You do not need to be a programmer to help.

**One rule comes before everything else here.** Hormiga holds other people's
information, and this repository is public. Nothing that came from *using*
Hormiga belongs in it: no organization's data, no real names, emails, phone
numbers or addresses, no screenshots of real records, no paths from your
computer, and no names of the organizations you work with. Use the Cat Colony,
the made-up database Hormiga opens on its first run, to show anything.
[`okf/concepts/foundation/what-may-be-public.md`](okf/concepts/foundation/what-may-be-public.md)
explains why, and what to write instead.

## Ways to help

| you want to | do this |
|---|---|
| report something wrong | run `voidhormiga-cli report`, open the link it prints, fill in the form |
| ask for something new | [the feature form](https://github.com/migriv24/VoidHormiga/issues/new?template=feature.yml): describe the kind of organization and the task, never a real one |
| report a security problem | **privately**: see [SECURITY.md](SECURITY.md) |
| try it on a new platform | build it (below) and say what you saw. "It opened" is a real result: no one has watched a window open on macOS yet |
| improve the guide or the design documents | edit `AGENT-GUIDE.md` or `okf/`, and open a pull request |
| change the code | read on |

## Building

Hormiga is built beside the libraries it uses, as sibling folders:

    mkdir Void && cd Void
    git clone https://github.com/migriv24/void-core.git    VoidCore
    git clone https://github.com/migriv24/VoidMaiz.git     VoidMaiz
    git clone https://github.com/migriv24/VoidAllomone.git VoidAllomone
    git clone https://github.com/migriv24/VoidPalabra.git  VoidPalabra
    git clone https://github.com/<you>/VoidHormiga.git     VoidHormiga   # your fork

    cmake -S VoidCore/core -B VoidCore/core/build -DCMAKE_BUILD_TYPE=Release
    cmake --build VoidCore/core/build
    cmake -S VoidHormiga -B VoidHormiga/build -G Ninja
    cmake --build VoidHormiga/build
    ctest --test-dir VoidHormiga/build --output-on-failure -E reduce_conformance

You need a C++20 compiler, CMake and Ninja; on Linux, `xorg-dev` and
`libsodium-dev`; on macOS, `brew install libsodium`. Void Hammer, the
networking library, has no public repository yet. Without it, Hormiga builds
with networking left out and CMake says so; everything else works.
`AGENT-GUIDE.md` §10 has the failures that look like something they are not.

Then turn on the privacy hook once:

    cd VoidHormiga && git config core.hooksPath tools/hooks

It runs `tools/lint_public.py` on what you are about to commit and refuses
emails, phone numbers, home folders and anything shaped like a key.

## How the code is put together

Start at [`okf/index.md`](okf/index.md). The design lives there and the code
follows it. A pull request is reviewed against these, so it saves you time to
know them first:

1. **Every change is a command.** A contact edit, a block placed, a deploy:
   each goes through the dispatcher, is logged and can be replayed with no
   window. If a feature cannot be done from `voidhormiga-cli`, its design is not
   finished.
2. **Privacy is checked where data leaves.** Rendering, export, sync and
   publishing each have a seam that decides what may pass. Do not add a way out
   that goes around one.
3. **The libraries are not edited here.** Void Core, Void Maiz, Void Palabra,
   Void Allomone and Void Hammer are their own projects. If you need something
   from one, describe the gap in your pull request and a maintainer takes it to
   that project.
4. **Dependencies are vendored.** No package managers and no CDNs. A new
   library comes with its licence, under `vendor/`, listed in
   `THIRD-PARTY-NOTICES.md` and `void.json`. Ask before adding one.
5. **The design is written down.** A change to how something works updates its
   page in `okf/` and adds a dated entry to `okf/log.md`. Open decisions are in
   `okf/developer_questions.md`; if your change answers or opens one, say so.

## Pull requests

1. **For anything bigger than a fix, open an issue first**, so nobody builds
   something the design has already decided against.
2. Fork, branch, and keep each pull request to one thing.
3. Fill in the checklist in the pull request form.
4. CI builds on Windows, macOS and Linux, runs the tests and the privacy check.
   A maintainer reviews it and may ask for changes; that is normal.
5. **Only maintainers merge, and only maintainers publish releases.** Who the
   maintainers are is in [`.github/CODEOWNERS`](.github/CODEOWNERS).

**If you are an AI agent**, or a person using one: `CLAUDE.md` is the agent
instructions for this repository and applies to you, especially rule 2. An
agent that worked in an organization's folder keeps its notes there and never
copies them here.

## Licence

Void Hormiga is MIT licensed. By opening a pull request you agree your
contribution is licensed the same way. Vendored code keeps its own licence.

## What to expect

Issues are read and labelled (`needs-triage` until then). A bug that can be
reproduced on the Cat Colony is fixed far sooner than one that cannot. Requests
are weighed against the design, and the answer may be a different design than
the one proposed; you will be told why. The process is described in
[`okf/concepts/contributing/`](okf/concepts/contributing/index.md).
