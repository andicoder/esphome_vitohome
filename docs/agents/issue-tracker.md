# Issue tracker: claude-wiki vault

Specs and tickets for this repo live as notes in the private claude-wiki Obsidian vault, not in the repo, so they stay private even when the repo is public. The vault syncs itself through obsidian-git: write the files, never commit or push the vault by hand.

## Conventions

- Root: `~/vaults/claude-wiki/wiki/projects/tickets/esphome_vitohome/`, the same from every worktree of this repo
- One feature per directory: `<root>/<feature-slug>/`
- The spec is `<root>/<feature-slug>/<feature-slug> – spec.md`
- Tickets are one file per ticket at `<root>/<feature-slug>/issues/<feature-slug>-<NN>-<slug>.md`, numbered from `01`, never a single combined file. The feature prefix keeps filenames unique across the vault, which its wikilinks rely on.
- Every file starts with the vault's flat YAML frontmatter: `type` (`spec` or `ticket`), `title`, `created`, `updated`, `tags` (`ticket` plus the repo name), `status`
- Triage state is the frontmatter `status` (see `triage-labels.md` for the role strings, otherwise `ready-for-agent` / `claimed` / `done`); bump `updated` whenever you change a file
- Blocking edges are a `**Blocked by:**` line linking the blocking tickets as `[[wikilinks]]`
- Comments and conversation history append to the bottom of the file under a `## Comments` heading
- Never create files outside `~/vaults/claude-wiki/wiki/`

## When a skill says "publish to the issue tracker"

Create a new file under `<root>/<feature-slug>/` (creating the directory if needed).

## When a skill says "fetch the relevant ticket"

Read the file at the referenced path. The user will normally pass the path, the ticket number, or a wikilink.

## Wayfinding operations

Used by `/wayfinder`. The **map** is a note with one **child** note per ticket.

- **Map**: `<root>/<effort>/<effort> – map.md` (the Notes / Decisions-so-far / Fog body).
- **Child ticket**: `<root>/<effort>/issues/<effort>-<NN>-<slug>.md`, with the question in the body. A `Type:` line records the ticket type (`research`/`prototype`/`grilling`/`task`); the frontmatter `status` records `claimed`/`resolved`.
- **Blocking**: a `**Blocked by:**` line of wikilinks. A ticket is unblocked when every note it links is `resolved`.
- **Frontier**: scan `<root>/<effort>/issues/` for notes that are open, unblocked, and unclaimed; first by number wins.
- **Claim**: set `status: claimed` and save before any work.
- **Resolve**: append the answer under an `## Answer` heading, set `status: resolved`, then append a context pointer (gist + wikilink) to the map's Decisions-so-far.
