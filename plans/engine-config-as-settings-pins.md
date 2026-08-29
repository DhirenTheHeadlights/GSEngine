# Engine config as settings pins

> **Status 2026-08-12: PROPOSED, not started.** Written from the question "should `engine_config`
> exist at all, or should there be a startup API that sets the value of anything annotated as a
> setting?" Answer below: the instinct is right about a real defect, but the fix is not "make
> everything a setting" — most of what `engine_config` pokes is deliberately not a setting and should
> not become one. `engine_config` shrinks; it does not die. Depends on
> `plans/settings-override-unification.md` phases 2–3, which are shipped.

Goal: delete the hand-written state-poke block in `engine::initialize` and route app-shape
declarations through the same layered override mechanism the ini and CLI already use, so that
"who set this value" has exactly one answer with provenance instead of a silent fourth tier.

## Current state (verified against source)

`engine_config` (`Runtime/Engine.cppm:45-63`) holds 18 fields that do three unrelated jobs:

- **Structural** — `render`, `simulate_world`, `create_window`. These drive `register_systems<>`
  calls (`Engine.cpp:110-114`), the `disabled` set fed to `resolve_activation` (`Engine.cpp:116-127`),
  and the run loop's window branch (`Bootstrap.cppm:146`). **These can never be settings**: a setting
  lives on a system's state, and these decide whether that state is registered at all.
- **Bootstrap / process wiring** — `persist_settings`, `project_settings_path`, `setting`, `bench`,
  `ipc_pipe_name`, `parent_pid` (`Bootstrap.cppm:115`), `dump_system_graph_path`
  (`Bootstrap.cppm:204`). **These can never be settings either**: they configure the save registry
  and the process, and must be known *before* any setting can be read.
- **State pokes** — `title`, `dark_background`, `video_encode`, `custom_chrome`, `attached`,
  `scale_ui_with_resolution`, `gui_layout_path`, `use_gpu_solver`. This is the middleman, and the
  only group in scope here.

The pokes run at `Engine.cpp:129-150` (window / gpu / gui) and `Engine.cpp:187-191`
(`use_gpu_solver`, in deferred boot). Both run **after** `register_systems` at `Engine.cpp:98-114`,
which is what triggers `registry::add()` and the user → project → override layering
(`SaveSystem.cppm:309-314`). So the poke block is an undeclared fourth precedence tier that outranks
every documented layer, has no provenance, and never appears in the settings screen.

### Annotation census of the poke targets

| target | annotated? |
| --- | --- |
| `window::data::mouse_visible` (`Window.cppm:132`) | **yes** — `describe` + `shared` |
| `physics::data::use_gpu_solver` (`System.cppm:121`) | **yes** — `describe` + `restart_required` + `shared` |
| `window::data::title` (`Window.cppm:157`) | no |
| `window::data::cursor_suppressed` (`Window.cppm:165`) | no |
| `window::data::attached` (`Window.cppm:166`) | no |
| `window::data::native_frame` (`Window.cppm:188`) | no |
| `gui::data::scale_with_resolution` (`Gui.cppm:80`) | no |
| `gui::data::reserve_top_bar` (`Gui.cppm:84`) | no |
| `gui::data::file_path` (`Gui.cppm:96`) | no |
| `gpu::context::data::swapchain_clear` (`Context.cppm:54`) | no |
| `gpu::device_settings::video_encode` (`DeviceSettings.cppm`) | no |

Nine of eleven are bare. That ratio is the whole argument: a boot API that reaches "anything
annotated as a setting" would reach two of these targets today. The other nine are **app-shape
declarations** — facts the app author fixes at build time, not user preferences.
`reserve_top_bar` as a user preference is meaningless; it means "this app has custom chrome".

### Live defect this produces

`custom_chrome` forces `win->mouse_visible = true` (`Engine.cpp:133`) after `registry::add()` has
already read the user's persisted value into that field. `mouse_visible` **is** an annotated,
auto-saved setting. So in the Editor the user's choice is overwritten every launch, and because
auto-save is on, the forced `true` is then written back to `Editor.ini` — the preference is not just
ignored, it is destroyed. `use_gpu_solver` escapes the same fate only because its poke happens to be
one-way (`if (config) state = true`), which nothing enforces.

### What already exists

`save::registry` has the boot API this plan needs, from settings-override-unification phases 2–3:
`set_override(category, key, value)`, `clear_override`, `release_override`, `override_of`, and
`provenance_of` → `code_default | project | user | session` (`SaveSystem.cppm:42-70`). `add()`
applies `m_overrides` on top of both scopes (`SaveSystem.cppm:312-313`). Missing: a way to reach
un-annotated app-shape fields, and a checked front door instead of `"Section.key=value"` strings.

## Decisions

1. **`engine_config` stays.** Structural and bootstrap fields have no other home. Only the eight
   state-poke fields move.
2. **App-shape is a third scope, not a preference.** Introduce `settings::scope_kind::app` rather
   than annotating plumbing with `describe` and hoping nobody surfaces it. An `app`-scoped field is
   settable at boot, never persisted to an ini, and never rendered in the settings screen.
3. **Named `engine_config` fields survive as expanders.** `custom_chrome` stays a named field that
   expands to three pins across two systems. Replacing it with a bag of unrelated keys at every call
   site would lose the concept.

## Phases

### 1. `scope_kind::app`

> **IMPLEMENTED 2026-08-12.** Pure mechanism — no field carries `app` scope yet, so runtime behaviour
> is unchanged. Edits: `SettingsAnno.cppm` (enumerator + `app_scope` alias); `SaveSystem.cppm`
> (`add()` gains the app write into `m_defaults` and the app override read; `apply_one_key` gains the
> app read); `SystemManifest.cppm` (`collect_annotated_fields_into` gains a `scope_kind Inherited`
> template parameter mirroring the read/write walkers, filters `app` at the leaf, and
> `reset_annotated_defaults` skips `app` fields).
>
> Four findings from doing it:
> 1. The `m_defaults` write is **required**, not tidiness. `clear_override` falls back user ini →
>    project ini → `m_defaults` (`SaveSystem.cppm:221`), and an app field is in neither ini, so
>    without the snapshot a cleared app pin would have nothing to fall back to.
> 2. `apply_one_key` needed the app read too, or `set_override`/`clear_override` *after* boot would
>    update the doc without touching the struct. Only `add()` was called out in the plan.
> 3. `reset_annotated_defaults` was not in the plan's list but had to change: it walks every
>    `describe`d member and assigns from `State{}`, so a GUI "reset to defaults" would silently undo
>    every app pin once phase 3 lands.
> 4. `save_to_file` already erases overridden keys from the emitted doc
>    (`SaveSystem.cppm:529-539`), so app pins have ini protection from both the scope filter and that
>    loop. `save_all` only ever writes with user/project filters — confirmed, no change needed.
>
> `collect_settings_keys` is deliberately left unfiltered, so app keys stay reachable by
> `--engine-setting` and by phase 4's consteval check. The `add()` assert is fields ⊆ keys
> (`SaveSystem.cppm:289-296`), so shrinking fields keeps it valid.

`Ecs/Settings.cppm` already filters per field with `effective == filter` (`Settings.cppm:193`) and
gates on `meta::find_describe(m)` (`Settings.cppm:184`), so scope is already the right lever. Add the
enumerator, then:

- `registry::add()` gains `entry.read(m_overrides, entry.category, entry.settings_ptr, scope_kind::app)`
  alongside the existing user/project override reads, and the matching `entry.write` into `m_defaults`.
- The ini write path keeps calling write with user/project filters only, so `app` fields never reach
  a file. **Confirm this in `save_now` before relying on it** — `add()` is not the only writer.
- `collect_annotated_fields` skips `app`-scoped fields so they do not render. The `add()` assert at
  `SaveSystem.cppm:289-296` still holds: it requires fields ⊆ keys, and this only shrinks fields.

Acceptance: an `app`-scoped field can be set through `set_override` at boot, is absent from the
written ini, and is absent from the settings screen.

### 2. Annotate the app-shape targets

> **IMPLEMENTED 2026-08-12.** All nine annotated. Keys now live: `Window.title`,
> `Window.cursor_suppressed`, `Window.attached`, `Window.native_frame`, `UI.scale_with_resolution`,
> `UI.reserve_top_bar`, `UI.file_path`, `Graphics.dark_background`,
> `Graphics.device_settings.video_encode`. `--engine-setting` reaches all of them as of this phase;
> the poke block still outranks them until phase 3.
>
> **`file_path` — parser only, no formatter.** `std::formatter<std::filesystem::path>` **already
> exists** in this toolchain (`msys64/mingw64/include/c++/17.0.0/bits/fs_path.h:1505`, gated on
> `__glibcxx_format_path`, C++26 — the same standard revision that gave the codebase
> `generic_display_string()`). Defining one would be a redefinition error. Its default presentation
> is the unquoted native string (quoting needs an explicit `?`), so `write_field`'s
> `std::format("{}", value)` round-trips through the new `gse::parser<std::filesystem::path>` in
> `Meta/Parse.cppm`. **Do not add a `std::formatter` for path.**
>
> **`swapchain_clear` — took option (b), not the plan's preferred (a).** New information the plan did
> not have: `color_clear` lives in `gse.gpu_backend:pipeline`, which imports only `:core`, `:enums`,
> `:image`, and `gse.math` — *not* `gse.meta`. A `parser<color_clear>` + `std::formatter<color_clear>`
> would put a `gse.meta` dependency on a partition imported by nearly every GPU TU, and this build is
> import-bound. Against that, "pin the RGBA directly" buys little: what `engine_config` actually
> declares is a bool.
>
> So `context::data::swapchain_clear` is **deleted** and replaced by an app-scoped
> `bool dark_background`, with the derivation moved into `context::init`. Deleted rather than kept
> because a public field that `init` silently overwrites is the same clobber trap this plan exists to
> remove — `dark_background` is now the single source of truth. This required changing
> `Engine.cpp:141` to poke `dark_background` instead of `swapchain_clear`, one line of phase 3 pulled
> forward to keep the tree working.
>
> `mouse_visible` and `use_gpu_solver` deliberately stay **user** scope. They are genuine preferences;
> phase 3 pins them through the override layer so a GUI edit can release the pin.

Give the nine bare targets `describe` + `scope_kind::app`. `describe` text is still worth writing —
it is what a future `--engine-setting` user or a system-graph reader sees.

Two need a shape decision first:

- `swapchain_clear` is a `color_clear`, and `dark_background` is a bool that *derives* it
  (`Engine.cpp:141`). Either add a `parser<color_clear>` specialization and let apps pin the colour
  directly (`.swapchain_clear = {0.05f, 0.05f, 0.06f, 1.f}`), or keep `dark_background` as an expander
  that emits the colour. Prefer the former — the derivation is arbitrary and only two call sites use it.
- `gui_layout_path` / `file_path` is a `std::filesystem::path`; check it satisfies
  `is_scalar_settings_field` or add the specialization.

### 3. Replace the poke block with pins

> **IMPLEMENTED 2026-08-12.** `engine::initialize` is poke-free: the window/gpu/gui block and both
> copies of the `use_gpu_solver` poke (render and headless branches) are gone. Eleven pins are pushed
> through a local `pin` helper that formats via `meta::write_field`, so a pin string is produced by
> the same function the ini writer uses and is guaranteed to parse back.
>
> **Precedence changed, deliberately.** Pins are pushed *before* `set_overrides(m_config.setting)`,
> and `set_overrides` assigns into the same `m_overrides` doc, so the order is now
> code default → project ini → user ini → **app pin → scenario pin → CLI**. Previously `engine_config`
> ran after `registry::add()` and beat everything including `--engine-setting`. A CLI flag on a
> migrated key now wins over the `engine_config` field, which is the whole point — but it is a real
> behaviour change, not a refactor.
>
> **The `mouse_visible` defect is fixed.** The pin lands in `m_overrides` before registration, so
> `add()` reads the user ini into `m_loaded` and then applies the pin on top. The struct reads `true`
> in the Editor, but `m_loaded` still holds the user's choice, and `save_to_file`'s override-restore
> loop (`SaveSystem.cppm:529-539`) writes that value back rather than the pinned one. The preference
> survives, `provenance_of` reports `session`, and a GUI edit releases the pin.
>
> Conditionality was preserved exactly: `native_frame`/`mouse_visible` pin only under `custom_chrome`,
> `cursor_suppressed`/`attached` only under `attached`, `UI.file_path` only when `gui_layout_path` is
> non-empty, `Physics.use_gpu_solver` only when set — so a user ini that enables the GPU solver is
> still honoured. `Window.title`, `Graphics.dark_background`, `Graphics.device_settings.video_encode`,
> `UI.scale_with_resolution` and `UI.reserve_top_bar` pin unconditionally, matching the unconditional
> assignments they replace.
>
> `Physics.use_gpu_solver` now applies during `register_deferred()` (when the deferred `physics::data`
> calls `add()`) instead of immediately after it. Earlier, and independent of boot branch — the poke
> was duplicated across the render and headless paths and one pin covers both.
>
> Two non-regressions worth stating: `UI.*` pins are silently dropped when `render == false` because
> the gui systems never register, which is exactly what `try_state_of<gui::data>()` returning null did
> before; and `set_override` calls `apply_one_key` under an already-held `m_entries_mutex`, which is
> safe because `apply_one_key` takes no lock of its own.

`engine::initialize` builds its pins **before** `begin_staging()` and pushes them through
`set_override`, so `registry::add()` applies them as part of normal layering. The block at
`Engine.cpp:129-150` and the `use_gpu_solver` poke at `Engine.cpp:187-191` both delete.

This is where the `mouse_visible` defect resolves: the pin lands in `m_overrides`, `provenance_of`
reports `session`, the settings screen shows a session badge, and per the
settings-override-unification decision a user edit *releases* the pin instead of fighting it.

`video_encode` can then carry a real `describe` at user scope rather than `app` if you want it
user-visible — the clobber problem that forced it to stay bare disappears once the value arrives
through the override layer instead of after it.

### 4. Checked front door

> **IMPLEMENTED 2026-08-12.** Shape is `registry::pin<State, "key">(value)`, not the sketched
> `set<"Category.key">`. Validation has to know the state type — both `collect_settings_keys<T>()` and
> `category_of<T>()` are keyed on `T`, and there is no compile-time registry mapping a category string
> back to its state type. Naming the state also documents the owner at the call site:
> `m_save.pin<gpu::context::data, "device_settings.video_encode">(m_config.video_encode)`.
>
> **`collect_settings_keys<T>()` could not be the checker** — it is a runtime function (`std::vector`
> plus `std::format`, neither usable here). Added `settings::settings_key_exists<T>(key)`, a consteval
> sibling that walks the same member set but matches by prefix instead of building dotted strings, so
> it allocates nothing.
>
> Two precedents de-risked it: `gse::has_describe_fields<T>()` (`SystemNode.cppm:123`) is the identical
> shape — consteval template, `template for` over `define_static_array(nonstatic_data_members_of(...))`,
> bool accumulator — and `collect_settings_keys_with_prefix<T>` already recurses into `<F>`, so
> instantiation recursion through this walker is proven. It also stays clear of
> the recursive-consteval BMI corruption class, which needs a **non-template** consteval self-call
> returning `std::vector`; this returns `bool`, recurses across distinct instantiations, and builds no
> containers.
>
> `pin` carries two `static_assert`s: the state has a `settings::category`, and the key names a real
> annotated setting. A typo is now a compile error instead of a key that silently never applies.
>
> **Enum round-trip verified at the source, as the plan asked.** `enum_to_string` returns
> `std::meta::identifier_of(v)` verbatim (`Enum.cppm:26`), `enum_from_string` compares against
> `identifier_of(v)` verbatim (`Enum.cppm:36`), and `std::formatter<E>` routes through
> `enum_to_string` — so `write_field` and `gse::parse` agree exactly. The stale underscore-substitution
> finding in `settings-override-unification.md` is confirmed dead. Caveat: none of the eleven pins is
> an enum today, so this is verified from the formatter/parser pair, not from an executed round-trip.

Today pins are `"Section.key=value"` strings parsed at `SaveSystem.cppm:186-206`, with typos
surfacing as a runtime warning. Replace with a consteval-validated key: `set<"Gui.reserve_top_bar">(true)`
checked against `collect_settings_keys<T>()` at compile time.

Deliberately **not** a typed `set<^^member>` API: recovering `Graphics.device_settings.video_encode`
from a member reflection needs the enclosing path, and `device_settings` is nested inside
`gpu::context::data`. A path-splice signature is worse to read than the string it replaces.

Value side: enums pass through `gse::parse` as strings. The settings-override-unification status note
says the round-trip is already conformed (the formatter emits identifiers verbatim), so this should
be clear — **re-verify with an enum pin before building on it.**

### 5. Acceptance

- Editor launches with custom chrome; `Editor.ini`'s `mouse_visible` is whatever the user last chose,
  and the settings screen shows the field with a session badge rather than a silently rewritten value.
- `engine_config` is down to structural + bootstrap fields plus named expanders.
- No `try_state_of` poke remains in `engine::initialize`.
- `--engine-setting` on any migrated key behaves identically to the `engine_config` field, and the
  two no longer race — last writer into `m_overrides` wins, visibly.
- Bench and scenario runs (`persist_settings = false`) still never write an ini.

## Gotchas for the implementer

- Pins must be pushed **before** `begin_staging()`. Pushing after `register_systems` reproduces
  exactly the ordering bug this plan removes.
- `set_override` re-reads the live struct the way `add()` does. At boot no struct exists yet, so
  that path must tolerate an empty `m_entries` — it does today, but the pin ordering above makes it
  the normal case rather than the rare one.
- `registry::add()` re-reads `m_overrides` on re-registration; pins must stay idempotent across
  system re-adds, same constraint phase 2 of the override plan already carries.
- Structural fields must not be migrated opportunistically. `render` and `simulate_world` gate
  `register_systems` and `resolve_activation`; a pin targeting a system that was never registered is
  silently dropped by `add()`, which would turn a loud misconfiguration into a quiet one.
- `title` feeds `identifiable(config.title)` in the `engine` constructor as well as `win->title`
  (`Engine.cpp:130`). The identity use is not a setting; only the window mirror is.
- Do not add a `describe` to a field and leave it at user scope "for now" — that is the
  `mouse_visible` bug. Scope and annotation land together.
