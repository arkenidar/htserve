# Curate Claude plans into docs/plans/

## Context
Claude's plan mode writes to `~/.claude/plans/<auto-named>.md` — per-session scratch outside the repo with noisy filenames. These plans capture the *why* behind a change, context that doesn't survive in commits or code. We want every plan tied to this project preserved in-repo under `docs/plans/`, in markdown (authoring source) and HTML (so it renders in the existing styled docs site servable via `htserve --public docs`). Coverage is full project history: pre-plan-mode commits get retroactive stubs, plan-mode features get curated copies, and a helper script makes adding future plans mechanical.

## Layout
- `docs/plans/<slug>.md` — curated markdown source.
- `docs/plans/<slug>.html` — generated HTML mirror, same styling as the rest of `docs/`.
- `docs/plans/index.html` — landing page listing all plans newest-first.
- `docs/plans/template.html` — HTML scaffold the script wraps each plan in.
- `scripts/curate-plan.sh` — copies a Claude plan into `docs/plans/`, renders it to HTML, and regenerates the index.

## The script
`curate-plan.sh <source.md> <slug> [--title "..."]` does three things:
1. Copies `<source.md>` to `docs/plans/<slug>.md` (skipped if source already lives there).
2. Renders markdown → HTML using a built-in awk-based renderer (no pandoc needed). Handles headings, fenced code, inline code, links, bold, bullet lists, simple pipe tables, blockquotes.
3. Wraps the rendered HTML in `template.html` and writes `docs/plans/<slug>.html`.
4. Regenerates `docs/plans/index.html` by listing every `*.md` in `docs/plans/` newest-first by mtime.

Idempotent: re-running on the same slug overwrites without duplicate index entries. `--rebuild` re-renders every existing plan after, e.g., editing the template.

## Bootstrap content
Six plans seeded:

- `initial-server` — retro stub for commit `915d58b`.
- `public-path-flag` — retro stub for commit `3ce09af`.
- `loopback-default-and-lan` — retro stub for commit `51d190e`.
- `windows-cross-compile` — curated from a real plan-mode file (commit `a7efee4`).
- `port-auto-fallback` — reconstructed from commit `93d8204` plus README/docs (the original plan file got reused for this task).
- `docs-and-plans-curation` — this very plan.

Retro stubs are clearly marked as reconstructed post-hoc.

## Wiring
- `docs/index.html` gets a "Plans" link in the nav and in the Sections list.
- `README.md` gets a short "Plans" section explaining the workflow.

## Future workflow
After Claude approves and implements a plan:
```
./scripts/curate-plan.sh ~/.claude/plans/<auto>.md <clean-slug>
git add docs/plans/<clean-slug>.{md,html} docs/plans/index.html
git commit -m "Add plan: <clean-slug>"
```
`~/.claude/plans/` stays Claude's scratch; only finalized plans get curated.

## Verification
- `./scripts/curate-plan.sh --help` prints usage.
- `./build/htserve --public docs` and browse `http://localhost:8880/plans/`:
  - Index lists all six plans newest-first.
  - Each plan renders with consistent styling.
  - Code blocks, links, lists all readable.
- Round-trip nav: docs home → plans index → individual plan → docs home.
- Re-run the script on an existing slug — confirm idempotency.

## Source
Curated from `~/.claude/plans/how-to-handle-server-deep-jellyfish.md`.
