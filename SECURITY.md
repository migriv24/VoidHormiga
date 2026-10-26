# Security

Void Hormiga holds organizations' people: names, phone numbers, addresses,
and notes that must never be published. A security problem here can put a real
person at risk, so please report one **privately**.

## How to report

Use GitHub's private advisory form:
**[Report a vulnerability](https://github.com/migriv24/VoidHormiga/security/advisories/new)**.

Only the maintainers can see it. It does not depend on any one person's inbox
or computer, so it reaches whoever is maintaining the project at the time.

Please include the version (`voidhormiga-cli update` prints it), the platform,
and the steps. Show it on the **Cat Colony**, the made-up database Hormiga opens
on its first run, or on data you invent. **Never send a real organization's
database, even privately.**

## What counts

Anything that lets data go where Hormiga promises it does not:

- a field marked private (`notes`, a rune tagged `private`) reaching a website,
  newsletter, map, calendar feed or export;
- a contact published without `clearance:public`, or their email or phone
  without `clearance:contact`;
- a person outside a shared database seeing or changing it, or one shared
  database reaching another;
- a key or token leaving the vault, reaching a command line or a `.miga`;
- an update or download that is not what the feed says it is;
- anything the Antfarm publishes that it did not show first.

A crash or a wrong render is an ordinary bug and goes in the public tracker.

## What happens next

A maintainer confirms they have it, works on it in the private advisory, and
publishes the fix and the advisory together, crediting you unless you ask
otherwise. Nothing is signed yet (see `okf/concepts/platform/distribution.md`),
so a fix arrives as an ordinary release with its checksum in the feed.

How the project handles reports is described in
`okf/concepts/contributing/intake.md`.
