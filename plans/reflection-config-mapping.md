# Reflection-driven config mapping

Goal: delete the hand-written key-to-member dispatch in the gui layout persistence
(`menu_data_from_section`, `parse_vec2`, `split`) by reusing the reflection mapper that
`gse.ecs:settings` already applies to `gui::data` itself.

Explicitly NOT in scope: serialising live `gui::data` wholesale, or moving layout to the
binary archive. `gui::data` is ~90% per-frame scratch (raw `menu*`/`draw_context*`, sprite and
text command buffers, resource handles, interaction `variant`), the on-disk rect is a derived
resolution-independent projection rather than a copy of `menu::rect`, and `layout_store` is a
co-tenanted sectioned-ini file that several subsystems own sections of. All three rule out a
memberwise dump.

---

## Finding: the enum round-trip is broken today

Independent of this work, and user-visible.

The settings mapper writes and reads through two different vocabularies:

- write, `Settings.cppm:198` — `std::format_to(std::back_inserter(formatted), "{}", value.[:m:])`
- read, `Settings.cppm:235` — `gse::parse(it->second, value.[:m:])`

For an enum the write side hits `std::formatter<E>` (`Enum.cppm:46`), which does
`std::ranges::replace(pretty, '_', ' ')`. The read side hits `parser<E>` (`Parse.cppm:101`) →
`enum_from_string` (`Enum.cppm:36`), which compares against `std::meta::identifier_of` with the
underscore intact. Any enumerator containing `_` is written in a form that can never be read back;
`parse` returns false, the member keeps its prior value, and the setting silently reverts.

Confirmed reachable: `gui::data::current_theme` (`Gui.cppm:55`) is `describe`-annotated and
`theme::high_contrast` (`Styles.cppm:19`) has an underscore. Selecting the high-contrast theme does
not survive a restart. Audit the other `describe`-annotated enum members for the same shape.

Note the conclusion is NOT that display and serialisation need separate vocabularies. It is that the
single default vocabulary must be round-trippable — which is what the standard library already does
(`std::format("{}", 1.5)` is shortest-round-trip precisely so `from_chars` recovers it). The engine's
enum formatter deviates from that convention; conforming to it is the fix. See Phase 0.

---

## Phase 0 — make the default format round-trippable

The gate on everything below, and shippable on its own.

Superseded approach: an earlier draft proposed a parallel `gse::format_value<T>` writer sitting
opposite `gse::parse`, leaving `std::formatter` display-only. Rejected — two writers per type that
must be kept in sync forever, and it routes around the bug rather than fixing it. Any code doing
`std::format("{}", e)` and parsing the result later would still break.

The rule instead:

> `std::format("{}", v)` must round-trip through `gse::parse`.

One invariant, testable generically: for every supported `T`, `parse(std::format("{}", v), out)`
yields `out == v`. The fix differs per type depending on whether the formatter's extra output is
decoration or information.

**Enums — remove the transform.** `std::formatter<E>` (`Enum.cppm:49`) does
`std::ranges::replace(pretty, '_', ' ')`, and nothing consumes the result. Enum options for the
settings UI are built from raw identifiers at `SystemManifest.cppm:501`
(`field.options.emplace_back(std::meta::identifier_of(e))`), and the dropdown at
`Gui/Settings.cppm:192` uses that same string for both label and stored value — so the UI already
displays `high_contrast`. `Stacktrace.cpp:202` also uses raw `enum_to_string`. The disk write at
`Settings.cppm:198` is the only prettified site and the only broken one. Delete the `replace`.

Do NOT add a `{:p}` opt-in yet. The transform does not do its supposed job anyway —
`high_contrast` becomes `high contrast`, not `High Contrast`. When a UI genuinely needs labels it
will want title case plus per-value overrides, which is a `label<"...">` annotation, not a format
spec. Blast radius of the deletion: every engine log line printing an enum gains its underscore
back. Cosmetic, and greppable against source identifiers.

**Quantities, vectors, rects — keep the format, extend the parser.** The unit suffix in `2.50 m`
and the parens in `(2.50 m, 3.20 m)` are information, not decoration, and units in logs are
load-bearing for a physics engine. Inverting these to terse output would be a regression. Write
`parser<T>` to accept what the existing formatter already emits.

Then add the pairs `loaded_menu_data` needs, none of which exist today:

| type | note |
|---|---|
| `vec2f` | `x,y`; must not reuse the display formatter |
| `rectf` | only if kept as a field — see Phase 2, the rect is derived |
| `std::optional<T>` | absent key means `nullopt`; do not write empty values |
| `std::vector<std::string>` | comma list, replaces `split`/`join_with` |

Adding `parser<T>` also makes `T` satisfy `is_scalar_settings_field` (`Settings.cppm:120`), so the
mapper treats it as a leaf and writes one key instead of recursing into its members. That is the
behaviour we want for `vec2f`, and it is worth checking it does not change how any existing nested
settings struct is written.

Independent value: fixes the theme bug, and every settings consumer gains vector/optional fields.

---

## Phase 1 — extract the section-to-struct kernel

REVISED DURING IMPLEMENTATION. The planned cut — share the member walk, parameterised by a
selection policy — was the wrong one. Settings' walk carries four concerns (category lookup, the
`describe` opt-in, `scope_kind` filtering, dotted-prefix recursion) and keys off
`unordered_map`, while layout's walk has none of them and keys off `map`. Forcing one walk to serve
both meant a policy parameter per concern for roughly ten lines of shared loop, and it would have
put a refactor through the path fifteen systems and the settings UI depend on.

The part actually worth sharing is the leaf conversion — which is where the enum bug lived and
where `optional`/list support was missing. So `gse.meta:fields` exposes `read_field`/`write_field`
(scalar, `std::optional<T>`, and list branches) and both callers use those; each keeps its own
ten-line walk. `read_fields`/`write_fields` (whole-struct, key-aliased) also live there for callers
like layout that want the plain walk with no policy.

Note this leaves `std::optional` and `std::vector` handling in the kernel rather than as
`std::formatter` specialisations — deliberate. A partial specialisation
`std::formatter<std::optional<T>>` covers `std::optional<int>`, which does not depend on a
program-defined type and so is not ours to specialise; and C++23's range formatter already gives
`std::vector<std::string>` a display form (`["a", "b"]`) that is wrong for config. The kernel
decomposing containers keeps the round-trip rule confined to types we legitimately own.

### Original plan, for reference

`read_settings_with_prefix` / `write_settings_with_prefix` currently bundle four concerns:
doc-and-category lookup, the `describe` opt-in filter, `scope_kind` filtering, and dotted-prefix
recursion for nested structs. Only the innermost member walk is reusable.

Extract into `gse.meta`:

- `meta::read_fields(const std::map<std::string, std::string>& values, T& out) -> void`
- `meta::write_fields(const T& value, std::map<std::string, std::string>& values) -> void`

with member selection passed in as a policy so settings can keep requiring `describe` while layout
opts everything in. `write_settings_with_prefix` becomes a thin wrapper adding category, scope and
prefix. No behaviour change intended — the existing settings round-trip is the regression test.

Risk worth naming: `gse.meta` is imported nearly everywhere, so this adds template-heavy code to a
high-fan-in interface. Keep the bodies out of the interface partition. Instantiating a class
template from another module inside an interface partition's `-O2` bodies is exactly what produced
the `gse.graphics:save` BMI corruption this work started from — see
`memory/gcc-modules-member-coroutine-template-pendings.md`.

---

## Phase 2 — put the layout parser on the kernel

`menu_data_from_section(section)` collapses to `meta::read_fields(section.values, data)`, and the
per-menu block in `save()` to `meta::write_fields`. That deletes the if/else chain, `parse_vec2`
and `split`.

Blocker: the kernel keys on member identifiers, and four members disagree with the on-disk keys
(the file format is already shipped, so the keys cannot move):

| member | on-disk key |
|---|---|
| `owner_tag` | `owner` |
| `active_tab_index` | `active_tab` |
| `tab_tags` | `tabs` |
| `rect` | written as `position_ratio` + `design_size` |

First three: either rename the members, or add a `[[= gse::config::key<"owner">{}]]` alias
annotation the kernel honours. The alias is the better call — it generalises, and renaming members
to match a file format is the tail wagging the dog.

The fourth does not reduce. `rect` is not persisted; `position_ratio` and `design_size` are, and
`resolve_rect` rebuilds the rect against the current viewport and scale so windows survive a
resolution change. That projection stays hand-written whatever the mapping mechanism, and it should
— it is the thing that decouples the on-disk schema from the runtime type.

Honest accounting: Phase 2 deletes the dispatch chain and the two string helpers. It does not
delete the file.

---

## Phase 3 — `ui_scale` / `ui_scale_by_monitor` unification

`ui_scale` is persisted via the settings reflection path as a `describe`-annotated member;
`ui_scale_by_monitor` via the layout path in `save_ui_scales`. Related state, two mechanisms.

But `map<std::string, float>` is a dynamic collection, not a struct, so the kernel does not apply —
it would need a separate map-aware path. Low value for the cost. Recommend leaving it, and treating
the inconsistency as documented rather than fixed.

---

## Ordering

Phase 0 ships alone and is worth doing regardless — it fixes a live bug. Phase 1 is the
architectural commitment and wants sign-off before starting. Phase 2 only pays off once 1 lands.
Phase 3 probably never.
