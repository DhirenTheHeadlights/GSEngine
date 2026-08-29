# Editor format + lint actions

Two deliverables:

1. More auto-format rules, each with its own settings toggle.
2. `Format` and `Lint` buttons right-anchored in the code panel status bar, opposite the analysis text. Both report what they did to the log.

Chosen behaviour (agreed up front): new rules *plus* a toggle each; the lint button re-analyzes when stale and reports cached results when current; all four candidate rules are in scope.

---

## Current state

| Piece | Location |
|---|---|
| Formatter | `Editor/Editor/Source/Format/Formatter.cppm` — `gse.ide.format:formatter`, 516 lines, pure |
| `compute` / `apply` | Formatter.cppm:289 / :502 |
| Linter | `Editor/Editor/Source/Lint/Lint.cppm` — 58 lines, 2 rules |
| Status bar | `Editor/Editor/Source/App/CodePanel.cppm:2082-2189` |
| Button idiom | `document_prompt_button`, CodePanel.cppm:1555 |
| Config | `Editor/Editor/Source/Config/Config.cppm:10` |
| Document state | `Editor/Editor/Source/Workspace/Documents.cppm:17` |

The formatter has eight rules today, all indentation-shaped: statement indent, the eligibility gate, anchor selection (closers / access specifier / switch / brace cross-line-pop), blank-line trim, `#`-directives to column 0, template-angle suppression, `#if` branch reconciliation, and whole-file bail-outs.

---

## Three structural blockers

### 1. `line_edit` can only rewrite leading whitespace

```cpp
struct line_edit { std::uint32_t line; std::string expected; std::string replacement; };
```

`apply` does `s.replace(0, e.expected.size(), e.replacement)` (Formatter.cppm:512). Every one of the four requested rules is a suffix, interior, or structural edit. None fit.

### 2. `fix_engine` is close but not reusable

`gse.ide.diagnostic`'s `fix_engine::apply` (Diagnostic.cppm:97) already does span edits bottom-up with an overlap guard and multi-line joins. It is still the wrong vehicle:

- It interprets `start_col`/`end_col` as **tab-expanded display columns** (`display_to_byte`, `gcc_tab_width = 8`), because it was built for GCC SARIF diagnostics. The formatter works in **byte** columns and this codebase indents with tabs by default — feeding byte columns in would silently mis-target every indented line.
- It never *inserts* lines. It joins (`end_line > line` erases the intervening rows) but a `\n` in `replacement` does not split. Designated-init-per-line cannot ride it.

Adding a column-space flag and an insertion path to `fix_engine` would change behaviour under the shipped quickfix path (`apply_quickfix`, CodePanel.cppm:2663 / :2667) for no benefit. The risk is asymmetric — leave `fix_engine` alone.

### 3. `compute` returning `{}` is ambiguous

Whole-file bail-outs (mismatched brackets :475, unbalanced conditionals :353/:361/:366, leftover open state :495) return an empty vector, identical to "already formatted". The log requirement makes this distinction mandatory.

---

## Design

### P0 — widen the edit type (pure refactor, no behaviour change)

New type in the format module, byte columns, rule-tagged:

```cpp
enum class rule { indent, blank_line, directive, trailing_space, final_newline,
                  alignment, separator_spacing, operator_spacing, designated_init };

struct edit {
    rule source = rule::indent;
    std::uint32_t line = 0;
    std::uint32_t end_line = 0;
    std::uint32_t start_col = 0;
    std::uint32_t end_col = 0;
    std::string expected;
    std::string replacement;
};

enum class outcome { ok, unbalanced_brackets, unbalanced_conditionals, unterminated };

struct result { std::vector<edit> edits; outcome status = outcome::ok; };
```

`format::apply(lines, edits) -> apply_report` applies bottom-up (descending line, then descending `start_col`), verifies `expected`, and supports joins *and* splits (`\n` in `replacement` inserts rows). Roughly 50 lines, self-contained, matching the module's existing purity.

The eight existing rules re-emit unchanged as `start_col = 0`, `end_col = leading-run length`. **This must be a byte-identical refactor**, verified before any new rule lands — the indent eligibility and anchor rules were each earned against a false positive and must not shift.

`adjust_after_format` (CodePanel.cppm:1131) has to be rewritten. Today it assumes a prefix edit on the caret's own line and early-outs on first match. New version walks edits bottom-up and, for each edit before the caret, shifts `line` by (rows added − rows removed) and shifts `column` by (replacement length − span length) when the edit is on the caret line and left of it. Applies to caret and anchor. Undo needs no change — `apply_format` already snapshots the whole buffer (CodePanel.cppm:1148).

### P1 — config plumbing

New fields on `config_system::data` (Config.cppm:10). Settings UI rows come free from reflection.

| Field | Default | Rule |
|---|---|---|
| `format_trailing_whitespace` | `true` | R1 |
| `format_final_newline` | `true` | R1 |
| `format_collapse_alignment` | `true` | R2 |
| `format_separator_spacing` | `true` | R3 tier A |
| `format_operator_spacing` | `false` | R3 tier B |
| `format_designated_init` | `false` | R4 |

`format::options` grows 2 → 8 fields. Both construction sites (CodePanel.cppm:1417 and :2729) grow with it.

### P2 — the four rules

**R1 — trailing whitespace + final newline.** Per line, span from last non-whitespace to EOL, empty replacement; ensure exactly one trailing newline at EOF. Must gate on the lexer's end-of-line mode so raw string literals and block comments are skipped, the same way the indent rule gates on `line_start_modes`. *Low risk.*

**R2 — collapse alignment padding.** Any run of 2+ spaces that is not leading indentation and not inside a string, comment, or raw literal collapses to one space. This is the direct encoding of the no-vertical-alignment rule (STYLEGUIDE.md:3). The 2026-07 sweep already found this damage across the codebase, so the rule has real work to do on day one. Open sub-decision: whether to collapse alignment inside trailing `//` comments — it is still alignment, but excluding comments is the safer start. *Low-medium risk.*

**R3 — separator and operator spacing.** The formatter is purely lexical (imports only `gse.ide.highlight`; `compute` takes `span<const std::string>` with no token or semantic input). Several cases are lexically undecidable — `a * b` vs `T* x`, `a & b` vs `T& x`, `a < b` vs `foo<T>`, unary vs binary `-`/`+`. So this ships in tiers:

- **Tier A** (`format_separator_spacing`, default on): exactly one space after `,` and `;` when not at EOL; no space before them; no padding directly inside `(`/`[`. No declarator ambiguity in any of these.
- **Tier B** (`format_operator_spacing`, default off): single space around unambiguously-binary operators — `=`, `==`, `!=`, `<=`, `>=`, `&&`, `||`, and the compound assignments. Excludes `*`, `&`, `<`, `>`, and unary `-`/`+`.
- **Tier C** — not a format rule. `*`/`&`/`<`/`>` need semantics; see *Why the formatter stays lexical* below. Declarator spacing belongs in lint.

*Medium risk at tier A, higher at tier B.*

**R4 — designated initializers one per line.** Explicitly mandated (STYLEGUIDE.md:304). Detect a brace-init body carrying 2+ top-level `.ident =` at the same brace depth; emit one edit whose replacement embeds `\n` per member plus the closing `}` / `};` handling.

This is the rule that forces the split support in `apply`, and it forces `compute` to become **two-phase**: split first, re-lex, then run the indent pass over the new line set. Bolting it into the single pass will corrupt the indentation of every inserted line. *Highest risk; largest lift of the four.*

### P3 — the buttons

Right-anchored strip inside `status_rect`: `Lint` at `status_rect.right() - pad`, `Format` to its left with a `pad * 0.5f` gap. Width per button = `text_view->width(label, font_sz) + pad * 2`; height = `status_rect.height()` less a small inset, vertically centered.

Reuse `document_prompt_button` (CodePanel.cppm:1555) — already `press_in_rect` + sprite + centered label, returns `activated`, takes `key`/`rect`/`label`/`enabled`/`danger`, and `ui` is already in scope in the status block. The name becomes a misnomer; optional rename to `small_button`.

Three changes to the existing status block are required or the bar visibly breaks:

1. Compute the strip rect **before** the text, and clip the status `queue_text` (:2174) to a `text_area` that stops at the strip's left edge. Otherwise long status text draws under the buttons.
2. Narrow the failure-tooltip hover test (:2178) from `status_rect` to `text_area`. Otherwise the tooltip fires while hovering the buttons.
3. Skip the strip entirely when the bar is too narrow for both buttons plus text, and when `status_h` has collapsed.

Enablement: `Format` on `doc.highlightable`; `Lint` on `doc.highlightable && ws.diagnostics_pending != active_document_id`.

### P4 — logging

House pattern is `audit_semantic_coverage` (CodePanel.cppm:1252): bracket prefix, `category::general`, `level::info`.

**Format.** `apply_format` returns a summary instead of `void`; both call sites are in the same file (:1179, :2749), so Shift+Alt+F starts logging too.

```
[format] {path}: no changes
[format] {path}: {n} edits on {m} lines - indent {a}, trailing {b}, alignment {c}, spacing {d}, designated-init {e}
[format] {path}: skipped - unbalanced brackets
```

That third line is the one that matters — today a bail-out is silently indistinguishable from a clean file.

**Lint.** Stale is `doc.diag_dirty || doc.persistence == document_persistence::dirty || doc.analysis_status == analysis::diagnostics_status::not_analyzed`.

- Current → log `doc.lint` immediately, one line per finding plus a summary.
- Stale → set `doc.diag_dirty = true`, log `re-analyzing...`, and set a new `bool lint_report_requested` on `document` (Documents.cppm:17). `apply_diagnostics` (CodePanel.cppm:1191) consumes the flag right after `doc.lint = std::move(check->lint)` (:1217) and emits the same report.
- The failure path (:1208-1214) **must** clear the flag and log `analysis failed - {status}`, or a stale request silently never reports.

```
[lint] {path}:{line}:{col}: {rule}: {message}
[lint] {path}: {n} findings across {k} rules
[lint] {path}: no findings
```

Note lint findings are currently never logged at all — the loop at :1238-1249 covers compiler diagnostics only.

---

## Why the formatter stays lexical

The plugin already emits everything needed to resolve the ambiguous cases: `GSETOK` carries a `type` kind, at **byte** columns (gse_tokens_plugin.cpp:1543) — the formatter's own column space — and `GSETARG` emits template `<`/`>` spans (:3240). "Is the identifier left of `*` a type?" is decidable.

It arrives at the wrong time. Semantic data is a position-keyed map stamped with the revision it was computed for (`syntax_producer::semantic_data`, SyntaxProducer.cppm:12); positions are meaningful only at that revision and there is no edit journal to remap them. `update_diagnostics` formats and saves the dirty buffer and *then* queues the analysis (CodePanel.cppm:1414-1438), so on the dominant path the map always describes an older revision than the text being formatted.

| Trigger | Buffer | Semantic validity |
|---|---|---|
| Format button / Shift+Alt+F on a clean analyzed doc | revision matches | exact |
| Ctrl+S format-on-save | dirty by definition | stale |
| Pre-analysis autosave (:1414) | dirty by definition | stale |

Consuming it anyway would mean the same file formats differently depending on whether analysis had landed — trading lexical false positives for nondeterminism. A formatter that runs on every save has to be a pure function of the text.

**The established pattern is the answer.** `GSEQUAL` (redundant namespace qualifier) is a style rule that needs semantics, and it was built as plugin record → lint diagnostic → fix-it, not as a format rule. That path runs when semantic data is exact and revision-matched, and fix-all is already wired via `fix_engine::rule_edits` (CodePanel.cppm:2667). So: **semantics-dependent style rules go to lint; the formatter stays lexical.** Declarator spacing becomes a lint rule with a fix-it rather than R3 tier C.

**Where compiler info does improve the formatter — offline, at zero runtime cost.** Have the plugin emit ground truth for template-argument-list spans and declarators across the codebase, then sweep it against the lexer's `angle_depth` / `has_matching_angle` heuristic (which currently bails on `&& || == != <= >= ?`) to find every place the heuristic is wrong, and harden the lexical rule from real data. This attacks the false *negatives* — the ~5% of lines the formatter currently refuses to touch — instead of adding a runtime failure mode. Pairs naturally with rebuilding the `format_sim.py` twin (risk 3).

**Ceiling, accepted deliberately.** Purely lexical can never resolve `T* x`, `T& x`, `foo<a,b>(c)` vs chained comparisons, or types in dependent and macro-expanded contexts. The existing design handles these by refusing to act rather than acting wrongly — false negatives, not false positives. That is the correct trade here and it stays.

---

## Open decision

**Bail-out scope.** Today one unbalanced bracket discards the entire edit set. With five more rules that means a broken brace stack also cancels trailing-whitespace trim and alignment collapse, which do not depend on the bracket stack at all. Options: keep whole-file bail-out (simple, conservative), or scope the bail-out to the structural rules and let R1/R2 through. Recommend the latter, but it changes behaviour on malformed files and should be a deliberate call.

---

## Risks, ordered by likelihood of biting

1. **Byte vs display columns.** `format::edit` and `ide::text_edit` look nearly identical and mean different things. They must never be routed into each other's `apply`.
2. **R4's two-phase requirement.** Single-pass integration corrupts inserted-line indentation.
3. **Indent regression during P0.** The eligibility and anchor rules were hard-won against false positives. The original work validated them with a Python twin (`format_sim.py`) sweeping all 394 engine + editor files at 0 idempotence failures. Rebuild that twin from `plans/editor-auto-format.md` and re-sweep after P0 and after each rule, or regressions land unnoticed.
4. **Caret/anchor remap.** Wrong math means the caret jumps on every save — silent and constant. Multi-line and split cases both need covering.
5. **Idempotence across composed rules.** `compute(apply(compute(x)))` must be empty for every combination of toggles, not just all-on.

---

## Sequencing

P0 and P1 are prerequisites for everything. P3 and P4 are independent of P2 and can land first — the buttons and logging are useful against the eight rules that already exist, and they give R1–R4 an observable surface to be validated through.

Suggested order: **P0 → P3 + P4 → P1 → R1 → R2 → R3 tier A → R3 tier B → R4.**
