# Editor Auto-Format — Infrastructure Scope

Goal: format-on-save for the built-in editor, starting with indentation, scaling to full
house-style enforcement (STYLEGUIDE.md) over time. The formatter must be idempotent,
undoable in one step, and must never destroy author-chosen line breaks (the style guide
has **no line-length wrapping** — layout of a statement across lines is author-driven).

## Ground truth from the codebase

| Concern | What exists today |
|---|---|
| Buffer | `gse::gui::text_buffer` = `std::vector<std::string> lines` (Engine/Engine/Source/Graphics/2D/Gui/TextBuffer.cppm) |
| Programmatic edit precedent | `apply_quickfix` (Editor/Editor/Source/App/EditorApp.cppm ~L1474): undo snapshot → `fix_engine::apply` → re-clamp caret → set `dirty/highlight_dirty/diag_dirty`, `last_edit = now` |
| Batch edit primitive | `text_edit` + `fix_engine::apply` (Editor/Editor/Source/Diagnostic/Diagnostic.cppm): bottom-up application, expected-text verification |
| Save path | `workspace::save_document` (Workspace.cppm ~L449); two callers in EditorApp: Ctrl+S (~L2322) and debounced autosave-before-analyze (~L2305) |
| Undo | Full-snapshot stack in `text_area_state` (TextArea.cppm); `begin_edit(kind)` coalesces by edit kind |
| Sync lexer | `syntax::tokenize` in SyntaxProducer.cppm (~L266): line-carry `lex_mode { normal, block_comment, raw_string }`, all string prefixes incl. raw-string delimiters, preprocessor lines, comments. Pure C++, no plugin dependency |
| Per-line metadata | Spans/diagnostics are **regenerated** after edits (debounced), never shifted — a format pass needs no metadata remapping |
| Config | `config` (Editor/Editor/Source/Config/Config.cppm): `indent_width = 4`, `indent_with_spaces = false` already exist |
| Style source of truth | docs/STYLEGUIDE.md only — no .clang-format / .editorconfig in the repo. Tabs; no vertical alignment; no rewrapping; designated init one-per-line; bodies on own line |

## Architecture

### 1. Lift the lexer into a shared partition (prerequisite)

The formatter needs "what token/mode am I in" synchronously. That lexer already exists but
is a non-exported namespace inside `gse.ide.highlight:syntax_producer`.

- New partition `gse.ide.highlight:lexer` (Editor/Editor/Source/Highlight/Lexer.cppm):
  move `token`, `token_type`, `lex_mode`, `tokenize`, `split_lines`, char classifiers,
  `match_literal_start` there, exported. `syntax_producer` imports it and keeps only
  coloring. (Plain `import` from the format partition — `export import` stays confined to
  Import/*.cppm headers per house rules.)
- Additive change to `tokenize`'s surface: also expose the **line-start mode table**
  (`std::vector<lex_mode>`, one per line) so consumers know which lines begin inside a
  block comment / raw string without re-deriving it. Cheap: the loop already carries it.
- Module-identity gotcha: this is a file **move + new file**, not an in-place identity
  edit ([[cmake-module-rename-phantom-cycle]] — deliver as file-move to avoid the phantom
  cycle).

### 2. New module: `gse.ide.format`

`Editor/Editor/Source/Format/Formatter.cppm` (partition `gse.ide.format:formatter`,
re-exported from Editor/Editor/Import/Ide.cppm like the other subsystems).

Core API is a **pure function** — no I/O, no document coupling, trivially unit-testable:

```
namespace gse::ide::format {
    struct options {
        int indent_width;        // from config
        bool indent_with_spaces; // from config
    };

    struct line_edit {
        std::uint32_t line;
        std::string expected;    // current leading whitespace (verification)
        std::string replacement; // new leading whitespace
    };

    auto compute(std::span<const std::string> lines, const options& opts)
        -> std::vector<line_edit>;
}
```

Returning *edits* rather than a rewritten buffer keeps the pass minimal (only touched
lines change), makes caret adjustment exact, makes idempotence checkable (`compute` on
formatted output ⇒ empty), and reuses the `fix_engine` verification idea.

### 3. Applying edits: `apply_format` beside `apply_quickfix`

In EditorApp (or a small helper in the format partition taking `document&`):

1. `compute` on `doc.buffer.lines`; if empty → done (no undo entry, no dirty churn).
2. Push undo snapshot with a dedicated `format_edit_kind` (one Ctrl+Z reverts the whole
   format, matching quickfix behavior).
3. Apply per-line leading-whitespace replacements (top-down is fine — line indices are
   stable since phase 1 never inserts/deletes lines).
4. Caret/anchor: if their line's indent changed, shift `column` by
   `replacement.size() - expected.size()` (clamped ≥ 0); then `buffer.clamp`.
5. Set `dirty/highlight_dirty/diag_dirty = true`, `last_edit = now` — the existing
   debounce machinery re-highlights and re-analyzes for free.

Synchronous is correct here: a per-line lexer over even a large file is well under a
frame. No async infra needed for indentation; revisit only if a future phase becomes
compiler-plugin-driven (then reuse the `diagnostics_check` atomic-done pattern).

### 4. Format-on-save hook

Both save sites funnel through one helper so behavior can't diverge:

```
auto format_and_save(workspace::data& ws, std::uint32_t id) -> void; // format (if enabled) → save_document
```

- Replace the direct `save_document` calls in the Ctrl+S handler (~L2322) and the
  autosave-before-analyze path (~L2308).
- Formatting happens **before** the write, so disk always holds formatted text and the
  file-watcher's dirty-skip logic is untouched.
- New config fields: `bool format_on_save = true`, plus an explicit
  "Format Document" command/keybind (Ctrl+Shift+F?) that calls `apply_format` without
  saving — useful for testing and for `format_on_save = false` users.

### 5. Phase 1 algorithm — indentation

Single pass over `tokenize` output + line-start modes:

- **Skip entirely**: lines starting inside a block comment or raw string (mode table),
  and blank lines (strip to empty — no trailing-whitespace-only indent).
- **Depth tracking**: net `{`/`}`, `(`/`)`, `[`/`]` from *punctuation tokens only*
  (string/comment/preprocessor content can't fool it).
- **Expected indent** for a line = depth at line start, minus 1 if the first token is a
  closer (`}`, `)`, `]`). Emit `indent_width`-spaces or tabs per level
  (`indent_with_spaces`); house style is tabs.
- **Special cases** (phase 1 must handle, they're common in this codebase):
  - Preprocessor lines (`#...`): column 0.
  - `case X:` / `default:` / access specifiers / labels: one level out from the
    switch/class body — start with the simple rule (labels at parent depth) and iterate.
  - Continuation lines (statement spans lines without a bracket, e.g. `<<` chains,
    long conditions): **leave untouched** in phase 1 — author-driven per the style
    guide; only lines whose indent is bracket-derivable get normalized. Detection:
    previous code line doesn't end a statement (`;`, `{`, `}`, `:`, label, preprocessor).
    Conservative "don't touch what you can't prove" keeps the formatter trustworthy.
  - Lambdas / designated init blocks fall out of plain brace depth naturally.

Idempotence invariant: `compute(apply(compute(x))) == {}` — assert in tests.

### 6. Testing

Follow the existing pattern of pure-function subsystems: a small test harness (or the
editor's existing test setup if/when one lands) driving `format::compute` on fixture
strings — no GUI, no document needed. Golden cases: nested braces, switch/case, raw
strings containing `{`, block comments containing `}`, preprocessor lines, designated
init, lambda-in-argument, already-formatted files (empty edit list).

## Scale-up roadmap (later phases)

Each phase stays in `format::compute`, adding token-stream rewrites; `line_edit`
generalizes to intra-line ranges (`start_col/end_col`, reuse `text_edit` shape).

- **Phase 2 — whitespace normalization within lines**: collapse alignment padding
  (style guide *prohibits* vertical alignment — `feedback-no-alignment`), single space
  around `=`, single space inside `{ ... }` braces for inline init, trailing-whitespace
  strip, space-before-brace on control statements.
- **Phase 3 — structural rules**: designated initializers one-per-line with trailing
  comma; bodies-on-own-line for `if/for/while` and non-empty lambdas; empty bodies
  collapse to `{}`. These insert/delete lines → edits become full `text_edit`s applied
  bottom-up via `fix_engine::apply`, and caret adjustment needs line mapping.
- **Phase 4 — style-guide long tail**: one-param-per-line wrapped signatures with `)` on
  its own line, always-braces on control statements, all-or-nothing argument lists.
  Some of these need real parsing beyond the lexer — candidate for a check-only lint
  first (surface via the existing lint/diagnostic hint path) before auto-fixing.
- **Format selection**: run `compute` but filter edits to the selected line range —
  falls out of the edit-list design.

## Build/land order

1. Lexer lift (`gse.ide.highlight:lexer`) — pure refactor, no behavior change.
2. `gse.ide.format:formatter` + `compute` (indentation) — pure, testable in isolation.
3. `apply_format` + caret adjustment + undo kind in EditorApp.
4. `format_and_save` funnel + `format_on_save` config + Format Document command.

Steps 1–2 build and verify independently of the editor UI; 3–4 are small wiring diffs.
