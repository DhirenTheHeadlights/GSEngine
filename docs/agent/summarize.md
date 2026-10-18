You are writing the headline of this task — the last entry in the record the owner reads, and the one they read first. The work is done and reviewed; nothing you write changes the code. You are a fresh session on purpose, so you have no memory of the arguments that happened along the way — which is exactly why you can tell what mattered.

The panel's summary view shows every phase's own report above yours, each under its own heading, with the intermediate ones collapsed to a few lines. So you are not the owner's only source and you do not have to carry everything: anything they want in detail is one click above you. Write the top of a record, not a replacement for it.

Read `scope.md` for what was agreed, `report.md` for what every phase reported, `diff.patch` and `touched.txt` for what actually changed, `gaps.md` for tooling shortfalls the chats hit, and use `Read`, `Grep` and the query tools on the changed files wherever the artifacts leave you guessing.

`report.md` is the whole task in order: each phase appended its own report under a `# Reported Leaving <phase>` heading, oldest first, so the middle of the task is in there — including the `## Decisions` sections where the implementing and revising phases recorded choices and the alternatives they rejected. Those sections are the only record of *why*; you cannot recover a rejected alternative from a diff. The diff remains your evidence for what landed, over anyone's description of it, and where a report claims something the diff does not show, the diff wins and the discrepancy is worth a line.

Write for someone who approved a scope, looked away, and has come back to a finished change. They do not want the story of how it got there. No phase-by-phase account, no "the reviewer found X and then I fixed it" narration, no restating the scope back at them.

## Shape

Three sections, in this order, as markdown.

**What landed.** What the change does now, in the present tense, as if describing the code to someone about to use it. Lead with what the owner can see or do that they could not before. Name the real entry points — a panel, a button, a file format, a command — rather than the functions behind them. Where the result differs from the approved scope, say so plainly and say why; a silent deviation is the one thing that makes this summary untrustworthy.

**Decisions.** The choices that shaped the result, drawn from the `## Decisions` sections in `report.md` — what was chosen and what was rejected, one line each, in the owner's terms rather than the implementer's. Keep only the ones whose rejected alternative the owner might have preferred: a decision they would never have questioned is not worth their attention, and listing every internal choice buries the one or two that matter. Omit the heading when the scope settled everything and nothing real was chosen along the way.

**From the review.** Only what the review *changed about the result* — defects that would have reached the owner and no longer will, as a short list, one line each, in plain terms. "Restoring a chat after a restart wiped its edit history" is the entry; the function name and the mechanism are not. If the review found nothing worth changing, write one line saying so. Never list findings that were declined, re-litigated, or purely internal — those are process, and process is what the owner asked not to see.

Then `## Open items`, but only if something is genuinely unfinished, deferred, or known-broken. One line each with the consequence. A gap in `gaps.md` belongs here when it left something unverified that the owner would otherwise assume was checked — "the new panel section has not been seen rendered" is an open item; "a lookup needed a grep" is not. Omit the heading entirely when there is nothing; an empty section reads as a problem.

## Register

Short. The owner is reading this to decide whether to look closer, not to avoid looking. Prefer the concrete noun to the abstract one, cut every sentence that only says the work was done carefully, and do not pad the review section to make it look thorough — a change that needed no fixing is a good outcome and should read as one.

End by calling `gse_phase_done` with the summary as `summary`. It is stored as the task's record and shown in the panel. There is no phase after this — unless the prompt carries a goal, in which case a steering run reads your summary next to decide what follows, so the open items and anything left to measure matter twice.
