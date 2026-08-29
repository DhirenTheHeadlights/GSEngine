# Settings override unification

> **Status 2026-08-11:** phases 0–2 shipped and confirmed working (Dev Spawn renders, 16k pyramid
> reachable from the in-game Settings menu). Phase 0 was already conformed — the enum formatter emits
> identifiers verbatim; the underscore-replace finding below is stale. **Phase 3 is next**, with two
> carried obligations: (1) `set_override`/`clear_override` currently re-read the live struct the way
> `add()` does — boot-safe, but GUI wiring MUST route the struct mutation through `change_request`
> (as `push_annotated_field_change` does) or it races the owning system; the registry keeps only the
> doc bookkeeping. (2) Add the drift guardrail as an assert that every supported-widget key in
> `register_settings_type::keys` has exactly one panel field — plain key-count equality is wrong
> because keys deliberately include unsupported-widget scalars.

Goal: one settings system. CLI `--engine-setting` flags and scenario pins stop being an invisible
boot-time bake and become a first-class, GUI-visible **session override layer**; every registered
setting — including nested ones — renders in the settings screen. Outcome that motivated this: change
`Dev Spawn.pyramid.base_count` and `Physics.solver_iterations` from the in-game menu and get the 16k
pyramid without a dedicated scene or launch line.

Decision, made 2026-08-11: **a GUI edit of an overridden field clears the session override for that
key and becomes the live value through the normal scope.** What you touch is what runs; pins are a
starting state, not a lock. No session-scope editor.

## Current state (verified against source)

- `save::registry` already holds three layered docs: `m_loaded` (user ini), `m_loaded_project`
  (project ini), `m_overrides` (session, fed by `engine_config::setting` — both CLI flags and
  scenario `info::settings` pins land there via `apply_scenario` prepending, CLI last so CLI wins).
- Layering is applied ONCE, at `registry::add()` (system registration): user → project → overrides
  read into the live struct. After that the override layer is invisible and dead — no provenance, no
  re-application, no GUI presence.
- Per-field persistence scope (`scope_kind::user`/`project`) comes from annotations via
  `field_scope_of`, inherited from the type default. Unrelated to the session layer; unchanged here.
- The reflection walkers in `Ecs/Settings.cppm` (`read/write/collect_settings_keys_with_prefix`)
  all RECURSE into nested non-scalar class members with dotted keys (`pyramid.base_count`).
- **Bug, user-visible today**: `collect_annotated_fields` (`SystemManifest.cppm:598`) does NOT
  recurse — `if constexpr (!(std::is_class_v<F> && !is_scalar_settings_field<F>) && ...)` drops
  nested members entirely, so the Dev Spawn panel renders empty even though its key exists and is
  CLI-settable. The GUI enumerates a different universe than the ini/CLI does — a paired-derivation
  defect: two consumers of "what settings exist" with independent derivations.

## Phases

### 0. Enum round-trip fix (prerequisite, shared with plans/reflection-config-mapping.md Phase 0)

Override strings go through the same `gse::parse` path that today cannot read back what the enum
formatter writes (underscores become spaces on write, compare with underscores on read; documented
with repro in reflection-config-mapping.md). Fix the formatter vocabulary to be round-trippable
before making overrides more prominent, or overridden enums fail silently.

### 1. Panel fields recurse (fixes Dev Spawn empty)

Make `collect_annotated_fields` mirror the keys walker: recurse into nested annotated class members,
producing fields keyed `outer.inner` whose format/push_change thunks compose the nested access path.
Acceptance: Dev Spawn shows `pyramid.base_count` with its range slider (1–200); every key in
`register_settings_type::keys` has exactly one corresponding panel field (assert this — it is the
guardrail that keeps the two derivations from drifting again).

### 2. Live session-override layer

`save::registry` gains `set_override(category, key, value)`, `clear_override(category, key)`,
`override_of(category, key) -> optional<string_view>`, and `provenance_of(category, key)` →
`code_default | project | user | session`. Setting or clearing an override re-reads the affected
entry immediately (same read chain as `add()`), so changes apply live without restart where the
consuming system reads per tick; `restart_required` annotations keep their existing meaning.
`set_overrides` (plural, boot path) becomes a loop over `set_override` — CLI and scenario pins are
unchanged as producers.

### 3. GUI provenance + edit semantics

> **Phase 3 IMPLEMENTED 2026-08-12.** One deviation from the edit set below: the non-const chain
> (item 2) was abandoned after tracing it — `const save::registry&` is a system parameter through
> `gui::run`, `editor_app::run`, and the main-menu screen, so the flip cascades through many system
> signatures. The registry already models "const = thread-safe shared service" (mutable mutex;
> `trigger_restart()` called through a const pointer), so `release_override` is const with
> `m_overrides` mutable — mutex-guarded doc-only erase, consistent with the class's existing
> concurrency design. The drift guardrail (item 5) lives in `registry::add()` (asserts every panel
> field key has a serialized key) rather than SystemManifest, which has no runtime assert import.
> Known gap: the hot-reload popout (`PopoutSystem.cppm:153`) and the crosshair custom page pass no
> registry to `draw_fields_for_entry`, so edits there don't release session pins and show no badge —
> wire when it matters. Original edit set for reference:
> 1. `SaveSystem.cppm`: add `release_override(category, key)` — mutex-guarded erase from `m_overrides`
>    ONLY, no struct re-read (the pushed value is already the live value; re-applying lower layers
>    would be a stale racy write). Distinct from boot-path `clear_override`, which re-applies.
> 2. Non-const chain: `settings::panel` takes `save::registry&` (not const), `settings_screen` stores
>    `save::registry*` non-const, ctor takes `save::registry&`; update the screen construction site.
>    The panel is a mutator of settings state by design — no `mutable`, no side channel.
> 3. `pending_field` (panel_state, Gui/Settings.cppm ~line 40 region) gains `std::string category`
>    and `std::string key`, filled at the same site that stores `push_change`
>    (`draw_fields_for_entry` line ~320). Both apply sites — the inline hot-reload push
>    (`draw_fields_for_entry` ~324) and the deferred Apply-button drain (the loop over
>    `pending_by_type` values that invokes `pending.push_change`) — call
>    `save_reg.release_override(pending.category, pending.key)` after a successful push.
> 4. Badge: in `draw_fields_for_entry`, when `save_reg.provenance_of(entry.category, field.key) ==
>    value_provenance::session`, append a session marker to `display_label` (built at ~line 314 via
>    `pretty_label`) — e.g. `std::format("{} [session]", pretty_label(field.key))`. Tooltip already
>    exists per field; richer badge styling can come later.
> 5. `build_annotated_settings_record` (SystemManifest.cppm): assert every `fields[i].key` appears in
>    `keys` — the supported-key⇔field drift guardrail (not raw count equality; keys legitimately
>    include unsupported-widget scalars).
> 6. `draw_fields_for_entry` signature gains `save::registry&` (passed from `panel`); check the other
>    caller at Gui/Settings.cppm ~line 343 region (`has_pending`/apply drain) for the same plumb.

Settings screen shows the effective value always. Fields whose key has a session override get a
"session" badge (tooltip: the override string and where it came from if cheaply attributable).
Editing such a field clears the override (decision above) and pushes the change through the existing
apply path. Runs with `persist_settings = false` (all scenario runs) must keep never writing inis —
GUI edits there are session-local, which falls out naturally once the override layer is live state.

### 4. Acceptance

- Launch plain, open Settings → Dev Spawn → base_count 180, Physics → solver iterations 40, enter
  Pyramid from the menu: the 16k pyramid stands and sleeps. No launch flags, no dedicated scene.
- Launch `--engine-bench-scenario pyramid16k_cpu` windowed-equivalent or with the same pins via CLI:
  both pinned keys show session badges in the settings screen; editing base_count mid-session clears
  its badge and the next Pyramid entry uses the edited size.
- `parity_*` and `pyramid*` bench numbers unchanged (this work must not touch the solver path).

## Gotchas for the implementer

- `std::define_static_array(std::meta::annotations_of(...))` is banned (silent cross-entity
  aliasing); the existing walkers use `define_static_array(nonstatic_data_members_of(...))` which is
  the allowed shape — keep it that way. Read annotations only through `gse.meta` helpers.
- The field thunks are POD function pointers today; nested access composition must not introduce
  stored capturing closures in module interfaces (BMI corruption class) — compose via template
  parameters / splices like the read/write walkers do.
- Scenario pins set `persist_settings = false` in `apply_scenario`; nothing in phase 2/3 may write
  an ini in that mode.
- `registry::add` also re-reads `m_overrides` on re-registration (hot path for system re-adds);
  keep set_override idempotent with that.
- The settings screen is channel-driven (`panel_writer` → owning systems apply); provenance querying
  must go through the registry reference the screen already holds, not a new side channel.
