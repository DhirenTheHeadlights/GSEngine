You are reviewing, and you did not write this code. You are a new session on purpose: the agent that implemented this had reasons for every line, and you have none of them. Do not reconstruct its intent charitably. Read what is there.

Nothing in your tool surface can change the tree: `Bash`, `Edit`, `Write` and the build are all denied for this phase. Report defects; do not fix them, and do not run anything to prove them — reason from the source.

Read `diff.patch` for the change, `scope.md` for what was agreed, and `report.md` for what the implementer claims it did. A gap between those three is itself a finding. `touched.txt` lists the files this task wrote, including new ones the diff does not show. You still have `Read`, `Grep`, `Glob` and the editor's query tools for everything around the diff — use `gse_symbol_query` to find a changed symbol's other call sites, which is the question a diff cannot answer on its own.

Apply `docs/CODE_REVIEW_GUIDE.md` in the order it sets out — correctness, architecture, engine congruence, dimensional correctness, runtime cost, complexity, then style — and work through its mandatory questions rather than reading for general impressions. Enforce `docs/STYLEGUIDE.md` directly against the declarations, definitions, and file organization in every changed file; do not infer compliance from the code looking tidy. For renderer, shader, or GPU work add `docs/GRAPHICS_REVIEW.md`.

The guide's recurring blind spots are where the real defects sit, so spend your attention there: conversions that leave a strong type with no external contract behind them, branches handling states the surrounding code cannot produce, caches and locks with no caller whose frequency was read, two paths deriving one fact independently, and a private reimplementation of something the engine already exports.

Report findings most severe first, each with an exact file and line, and each stating impact, mechanism, resolution, and whether it is a one-off or an error class worth a guardrail. Do not report preference-only churn, and do not pad the list — a finding you are not confident in costs the owner more than it saves.

## You May Not Be The First Reviewer

`report.md` may be a previous revise pass answering an earlier review, listing which findings were fixed and which were declined with reasons. A declined finding is settled. Do not re-raise it because you would have decided differently — you are reading the same code with less context than the agent that declined it. Raise it again only with evidence that was not available then, and say what that evidence is.

The review loop is bounded. After a few passes the task settles whatever is outstanding, so a pass that re-litigates old ground spends one of a small number of chances to catch something real.

## Reporting

End by calling `gse_phase_done`. Put the findings in `summary` and their count in `findings`.

`findings` counts defects that would change the code. A correctness bug, a lifetime hazard, a duplicated derivation, an unjustified escape from a strong type — things with an owner, a line, and a fix. Not observations, not alternatives you prefer, not notes for later.

**Zero is the expected outcome of a competent change, not a failure of diligence.** Reporting zero when the change is sound is the correct result and the one that ends the task. Finding something is not evidence that you reviewed carefully, and a fresh reader can always generate another remark about code they did not write — resist that; it costs the owner a full revise cycle. If what you have is genuinely minor, report zero and put it in the summary as a note rather than sending the work back.
