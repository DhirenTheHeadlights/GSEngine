# Graphics Module Review — 2026-08-07

Scope: every file under `Engine/Engine/Source/Graphics/` — 127 files, ~24k lines. Reviewed against `docs/CODE_REVIEW_GUIDE.md` and `docs/STYLEGUIDE.md` across the guide's ordered dimensions (correctness, architecture, engine congruence, dimensional correctness, runtime cost, complexity, style) and its mandatory questions.

Method: nine independent read-only passes, one per subsystem, followed by a verification pass over every critical and high finding against the source. **Nothing was built, configured, compiled, or tested.** All findings are uncompiled static inspection of the working tree as of this date.

Confidence is stated per finding. `high` means the excerpt, line number, and rule application were all confirmed. Findings marked **[verified]** were additionally re-checked by hand against the source after the originating pass reported them.

---

## Contents

- [Cross-cutting root causes](#cross-cutting-root-causes)
- [1. GUI core](#1-gui-core)
- [2. GUI layout, screens, persistence](#2-gui-layout-screens-persistence)
- [3. Text and scroll widgets](#3-text-and-scroll-widgets)
- [4. Widget set](#4-widget-set)
- [5. Core render pipeline](#5-core-render-pipeline)
- [6. Post-process and cull passes](#6-post-process-and-cull-passes)
- [7. Effect and auxiliary renderers](#7-effect-and-auxiliary-renderers)
- [8. 3D scene and animation](#8-3d-scene-and-animation)
- [9. 2D pipeline and capture](#9-2d-pipeline-and-capture)
- [Corrections and rejected findings](#corrections-and-rejected-findings)
- [Coverage](#coverage)

---

## Cross-cutting root causes

Four causes account for most of the severe findings. Each is one change at a low layer that makes a whole class of defect unwritable. Fixing these first collapses a large fraction of the individual entries below.

### C1. GPU resources have no owner

`gpu::image` (`GpuBackend/Image.cppm:147-150`) and `gpu::buffer` (`GpuBackend/Buffer.cppm:45-48`) are both `final : public non_copyable` with a defaulted destructor, holding bare handles (`gpu::handle` is a `std::uint64_t`). Move-assignment overwrites the handle and nulls the source without retiring the old one.

`retire` appears **zero times** across the entire `Graphics/` tree. `vulkan::device::retire(handle<buffer>)` and `retire(handle<image>)` exist (`Vulkan/Device.cpp:1484,1489`) but have no callers and no backend vtable forwarder, so renderer code has no reachable way to free either resource type.

Consequence: every `x = {}` and every `x = device->create_buffer(...)` abandons the device allocation and its bindless slot permanently. Confirmed leaking at:

- `BloomRenderer.cpp:146-148` — up to 14 half-screen HDR images per swapchain recreate
- `GeometryCollector.cpp:546-559` — instance buffers, all frames-in-flight, with no growth factor
- `LightCullingRenderer.cpp:102-122` — tile buffers
- `SceneSnapshotRenderer.cpp:37-47` — two swapchain-format full-screen images
- `AtmosphereRenderer.cpp:205` — 3D aerial-perspective volume
- `CloudRenderer.cpp:197` — half-res HDR cloud target
- `WorldTextRenderer.cpp:137-152`, `PhysicsDebugRenderer.cpp:287-299,306-321`, `RtShadowRenderer.cpp:150,218`, `SkinRenderer.cpp:231,250,292`, `PhysicsTransformRenderer.cpp:104` — growth paths, bounded only by the doubling schedule
- `Gpu/Graph/RenderGraph.cpp:62` — engine code, not renderer code

A window-resize drag emits one recreate event per mouse move; at 1080p that is roughly 20 MB leaked per event.

**Resolution.** Give `gpu::image`/`gpu::buffer` real ownership: retire the outgoing handle in the destructor and in move-assignment, through a device-held deferred-delete queue keyed on `m_resource_frame + max_frames_in_flight` (the `retiring_pool` mechanism already exists in `GpuBackend/Arena.cppm` and already handles the in-flight delay). Wire `retire_buffer`/`retire_image` into the backend vtable so the existing `collect_garbage()` reclaims them. This simultaneously fixes a second latent bug at every growth site listed above: those sites currently overwrite buffers that in-flight frames still reference, which only appears benign because nothing is destroyed.

**Prevention.** The ownership belongs in the type, not in ~30 call sites that must each remember. Once the wrapper owns the handle, the mistake becomes unwritable; until then every new renderer reproduces it.

*Confidence: high — leak mechanism and absent API confirmed by reading the type definitions and by exhaustive caller search.* **[verified]**

The same shape applies one level down to bindless descriptor slots: `SceneSnapshotRenderer.cpp:31-35` assigns `{}` to `bindless_handle` without calling `release`, permanently consuming two descriptor slots per resize until the pool is exhausted. The `if (d.slots[i].valid())` guard there is dead — assigning `{}` to an invalid handle is a no-op — which is a tell that the loop was written to *look* like a release.

### C2. `draw_context` publishes raw device input

`mouse_pressed`, `mouse_released`, `mouse_held`, `mouse_position`, `scroll_delta`, `key_pressed`, `key_held`, and `text_entered` all sit on the **exported** `draw_context` (`Types.cppm:310-337`), reachable through `builder::draw`, which forwards `draw_context&` to every widget.

`WidgetContext.cppm` — the module-internal partition the guide designates for exactly this capability — adds nothing at all. It exists solely to expose the protected `draw_context` constructor.

The leak has already happened. `Editor/Editor/Source/Profile/Profile.cpp` calls `ctx.scroll_delta()` (640), `ctx.mouse_held()` (661, 1139), `ctx.mouse_position()` (1132) and `ctx.mouse_pressed()` (1138) from outside the GUI module, bypassing the clip stack, render-layer arbitration, and press consumption.

**Resolution.** Move the unscoped accessors from `draw_context` to `widget_context`, leaving `draw_context` with only the rect-scoped, arbitrated queries (`hovers`, `mouse_pressed_for`, `mouse_released_for`, `scroll_delta_for`, `key_pressed_for`) plus consumption. `Profile.cpp` then declares `input::data` as a system dependency and passes a value snapshot into its deferred draw callback.

**Prevention.** Once the raw accessors exist only on the non-exported `widget_context`, out-of-module use is a compile error rather than a review catch. This is also why `widget_context` should not stay an empty shell — an empty internal partition reads as an accident and invites the next accessor onto the exported type.

*Confidence: high.* **[verified]**

### C3. Only one widget uses the shared press behavior

`interaction::press_in_rect` (`Interaction.cppm:171-173`) is the single sanctioned press-behavior entry point, and `interaction::press::color(press_palette)` the single hot/active/disabled color ladder. `Button.cppm:83-90` uses both. Nothing else does:

| Site | What it re-derives | Drift |
|---|---|---|
| `Selectable.cppm:87-99` | `mark_hot` + `activate_on_click`, hand-written 4-branch color ladder | no `enabled`, no `press::color` |
| `NavItem.cppm:60-73` | same two calls, same hand-written ladder | `active` and `hot` map to the same color |
| `Dropdown.cppm:329-351` | `mark_hot` + `activate_on_click`, hand-written ladder | two branches produce an identical value; no `animated_color` |
| `GraphCanvas.cppm:116-120` | `mark_hot` + `activate_on_click` | colors by additive offset, not style |
| `Toggle.cppm:61-69` | nothing — does not import `:interaction` | assigns `hot` by hand, never touches `active`, activates on **press** |
| `Section.cppm:96-118` | nothing | no ids at all; activates on **press** |
| `TabStrip.cppm:370-381` | nothing | takes no `hot`/`active` parameters |
| `ColumnHeader.cppm:98` | private `int resizing = -1` ownership | invisible to `active_widget_id` |
| `TextArea.cppm:111` | `(void)active;` + private `state.selecting` | drag has no capture ownership |
| `TextInput.cppm:396` | `hovered && ctx.mouse_held()` | drag freezes at the rect edge |

The observable consequence: button, selectable, nav item, dropdown, and graph node commit on **release** (press-then-drag-off cancels), while toggle, section actions, and tab strip commit on **press** (no cancel). Nothing fails to compile; the UI simply disagrees with itself about what a click is.

**Resolution.** Route every rectangle-scoped control through `press_in_rect` and present through `press::color`. Extend `press_palette` with a `selected` slot (Selectable and NavItem both need it). Add a `drag_in_rect` behavior returning `{ grabbed, dragging, released }` that owns the grab/unscoped-release pair, for ColumnHeader, TabStrip, TextArea, TextInput, and the `LayoutOps` divider.

**Prevention.** Narrow the API rather than adding one: move `mark_hot`/`grab_active`/`activate_on_click` behind `press_in_rect` and `drag_in_rect`, leaving those two as the only reachable entry points from widget partitions. That makes the divergent spellings unwritable instead of merely discouraged.

*Confidence: high.*

### C4. Deferred callbacks retain raw references to system state

`gpu::context::on_swap_chain_recreate` takes a `std::function<void()>` stored on the swapchain (`Gpu/Context.cpp:43-45`) with no lifetime tie to the registering system. Five renderers capture `data& d` by reference plus a `shared_view` by value, then **mutate** that state — creating GPU resources, reseating `[[= gse::shared]]` fields — from the swapchain-recreate context, entirely outside the scheduler that is the engine's only synchronization mechanism:

- `ForwardRenderer.cpp:230` (also captures `rt_state`)
- `AtmosphereRenderer.cpp:302-310`
- `CloudRenderer.cpp:277-282`
- `GiProbeRenderer.cpp:130-135`
- `TonemapRenderer.cpp:109-114`, `TaaRenderer.cpp:145-151`, `LightCullingRenderer.cpp:165-170`, `OitRenderer.cpp:182-187`, `BloomRenderer.cpp:205-211`

`shared_view<S>` is a reflection-generated aggregate whose members are raw pointers into the target system's live state (`Ecs/SharedView.cppm:96-110`), so capturing it by value into long-lived storage retains those pointers past the scheduler's knowledge. Nothing unregisters, so a second `init` appends another closure permanently.

Framebuffer recreation ordering is also correct only by accident: `render_graph`'s constructor registers `recreate_framebuffer_images()` first (`Gpu/Graph/RenderGraph.cpp:37-39`) and `gpu::context::init` runs before any renderer's `init`, so every `rebind_views` happens to see the new image. Nothing but registration order guarantees it and there is no diagnostic if it changes.

**Resolution.** Have the swapchain publish a recreate generation counter (or push a `swapchain_recreated` channel message) that each system compares against its own cached generation inside its scheduled `frame()`. `SceneSnapshotRenderer.cpp:80-82` already does exactly this and needs no callback at all. That keeps every mutation inside the scheduled run where the scheduler can order it, removes nine retained references, and makes the "extent changed" fact observable — which the LightCulling extent-divergence and Bloom quality-latch findings both need anyway.

**Prevention.** Remove the capability. `on_swap_chain_recreate` taking an arbitrary callable invites precisely this and has no safe use from a system.

*Confidence: high on the pattern; medium on whether a re-init path is currently reachable.*

---

## 1. GUI core

Files: `Gui.cpp`, `Gui.cppm`, `Interaction.cppm`, `WidgetContext.cppm`, `InputLayers.cppm`, `Types.cppm`, `Types.cpp`, `DrawStruct.cppm`, `IDs.cppm`.

### HIGH | Gui.cpp:1327 | Tab bar context is built with `current_z_order = 0`, so every tab interaction fails arbitration

```cpp
		.current_layer = layer,
		.current_z_order = 0,
		.input_layer = d.input_layer_render,
```

**Impact.** Clicking a tab to switch tabs, the tab close button, tab right-click, and tab-strip wheel scrolling are all dead from the second frame a menu exists onward. `tab_strip`'s returned `activated` / `close_requested` (`Gui.cpp:1346-1351`) can never fire.

**Mechanism.** `process_menu` registers the menu's whole `display_rect` — title bar included — at `menu_z` (`Gui.cpp:800`), and `menu_z` is `d.next_z_order++` seeded at 1 (`Gui.cppm:85`, `Gui.cpp:763`), so it is always ≥ 1. `TabStrip.cppm` gates every interaction on `ctx.hovers(...)` / `ctx.mouse_pressed_for(...)`, which reach `draw_context::input_available_at` (`Types.cpp:111-122`) → `input_layer::input_available_at` (`InputLayers.cppm:179-185`), whose final test is `return widget_z >= top_z;`. With `widget_z == 0` and `top_z >= 1` this is always false. Because `topmost_at` reads `m_previous_regions`, tabs work for exactly the first frame a menu is registered and then go dead — which is why the symptom reads as flaky rather than absent.

**Resolution.** Pass `.current_z_order = current_menu.z_order`. The underlying cause is that this is the third hand-written `widget_context` aggregate: fold the three sites into one factory taking the menu and layer so a field cannot be silently omitted or defaulted at one site.

**Prevention.** Recurring class — the three initializers already disagree on `current_z_order` in three different ways (`menu_z`, `0`, omitted). The guardrail is the shared factory plus making `current_z_order` a required field of `draw_context_init` rather than a defaulted member.

*Confidence: high.* **[verified — full chain traced through `next_z_order` seed, `register_hit_region`, and `input_available_at`]**

### HIGH | Gui.cpp:1160 vs Gui.cpp:1742 | Title bar drawn from `display_rect`, hit-tested from `menu.rect`

```cpp
	const float top_inset = current_menu.bare ? 2.f : tab_chrome_height(d, current_menu, display_rect.width());
	const rectf title_bar_rect = rectf::from_position_size(display_rect.top_left(), { display_rect.width(), top_inset });
```
```cpp
			const rectf title_bar_rect = rectf::from_position_size(
				{ current_menu.rect.left(), current_menu.rect.top() },
				{ current_menu.rect.width(), current_menu.bare ? 2.f : tab_chrome_height(d, current_menu, current_menu.rect.width()) }
			);
```

**Impact.** For any menu with docked children (`display_rect != menu.rect`), the draggable title bar is a different rectangle from the drawn one, at a different height. Dragging starts from empty space beside the bar, and part of the visible bar is not draggable. The same divergence mis-places the popout close-button exclusion at `Gui.cpp:1747-1753`.

**Mechanism.** `draw_menu_chrome` derives the bar from the group bounding box; `handle_idle_state` derives it from the raw menu rect. Both also feed a different width into `tab_chrome_height`, which drives wrapped-row count (`Gui.cpp:103-105`), so even the heights disagree. This is the Paired Derivations failure exactly.

**Resolution.** Give `menu` (or a free function beside `calculate_display_rect`) a single `chrome_rects(data&, const menu&)` returning `{ display_rect, title_bar_rect, body_rect, close_rect }`, consumed by `process_menu`, `draw_menu_chrome`, and `handle_idle_state`. `process_menu:808-813` and `draw_menu_chrome:1158-1170` are already a third and fourth copy of the same `top_inset`/`body_rect` computation.

**Prevention.** Four copies of one geometry derivation exist in one file. The single geometry accessor makes the invalid pairing unspellable.

*Confidence: high.*

### HIGH | Types.cppm:310-337 | Exported `draw_context` publishes raw device input; `widget_context` is empty

See [C2](#c2-draw_context-publishes-raw-device-input). *Confidence: high.* **[verified]**

### HIGH | Gui.cppm:159 / Gui.cpp:743 | `gui::shutdown` carries no system hook annotation and has no call site

```cpp
	auto shutdown(
		data& d
	) -> void;
```

**Impact.** Window layout, dock arrangement, tab grouping, and per-monitor UI scale changed in the last ≤30 s before exit are silently discarded on every run. `save_ui_scales` never runs at exit either.

**Mechanism.** System hooks are discovered purely by annotation — `gse::meta::find_system_hook_anno` (`SystemAnno.cppm:103-114`) scans `annotations_of(fn)` for `system_init`/`system_frame`/`system_shutdown`/`system_run`. `init` has `[[= gse::system_init{}]]` (`Gui.cppm:140`) and `run` has `[[= gse::system_run<>{}]]` (`Gui.cppm:148`); `shutdown` has none, and a repo-wide search finds no caller. Six other systems (`Audio`, `AssetState`, `WorldSystem`, `Window`, `CaptureRenderer`, `gpu::context`) carry the annotation correctly. The only surviving save is the 30-second timer at `Gui.cpp:451-454`.

**Resolution.** Add `[[= gse::system_shutdown{}]]` to the declaration. GUI shutdown only touches the filesystem, so it has no GPU ordering dependency against the recorded `gpu::context::shutdown` constraint.

**Prevention.** The error class — an unannotated hook that compiles, links, and silently never runs — is general. A consteval check in the system registration pass could flag a function named `shutdown`/`init`/`run` in a namespace owning a `system_state` that carries no hook annotation.

*Confidence: high.* **[verified — annotation absence and zero call sites both confirmed]**

### HIGH | Gui.cpp:1250-1258 | Popout close button reconstructs hover/press/release from raw input

```cpp
		const bool hovered = close_rect.contains(mouse_pos);
		const bool pressed = input_state.mouse_button_pressed(mouse_button::button_1);
		const bool released = input_state.mouse_button_released(mouse_button::button_1);
```

**Impact.** The close button ignores the clip stack, ignores render-layer arbitration, and never consumes the press. The same press is therefore also seen by the raise-to-front test (`Gui.cpp:802-806`) and by whatever the underlying menu's widgets claim, so one click closes the popout *and* activates whatever is beneath it.

**Mechanism.** `rect.contains(mouse) && raw transition` is the pattern the GUI Interaction Authority section names explicitly. A `widget_context` for exactly this title bar is constructed ~20 lines later in `draw_tab_bar` (`Gui.cpp:1314`), so the authority is available and unused. `handle_idle_state:1747-1753` carries a second copy of the same `close_rect.contains(mouse)` test purely to stop a drag starting on the button — computed from a different `title_bar_rect`, so the two copies disagree.

**Resolution.** Construct the chrome's `widget_context` once in `draw_menu_chrome`, express the close button as `interaction::press_in_rect(...)`, and delete the duplicate exclusion in `handle_idle_state` — with the press consumed, the drag path stops competing.

**Prevention.** Once `draw_menu_chrome` receives a `widget_context` instead of `const input::state&`, the raw spelling is unavailable.

*Confidence: high.*

### HIGH | DrawStruct.cppm:30,66,74 | Raw `std::meta` at use sites, including a hand-rolled `template for` over `enumerators_of`

```cpp
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
```
```cpp
			template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^F))) {
				v.emplace_back(std::meta::identifier_of(e));
```

**Impact.** Rejected on sight per the Reflection Helpers section. The enum branch (lines 61-98) reimplements `enum_to_string` — it builds a `std::vector<std::string>` of identifiers and a parallel `std::vector<F>` of values in two function-local statics, then linear-scans for the current index every frame. `Gui.cpp:1131` carries the same raw enumeration for `style` members.

**Mechanism.** The file already imports `gse.meta` and uses its helpers (`meta::find_describe`, `meta::member_name`, `has_annotation`, `annotation_of`), so the raw calls are a second way to express what is already available. The two `static const std::vector` locals also put a thread-safe-init guard and two heap-allocating vectors on a per-frame draw path, and pair a name list with a value list positionally.

**Note.** This is the "raw `std::meta` at a use site" defect, **not** the outright-banned `std::define_static_array(std::meta::annotations_of(...))` form — the wrapped calls are `nonstatic_data_members_of` and `enumerators_of`. The silent-collision hazard does not apply here.

**Resolution.** Route member enumeration through the `gse.meta` member helper, and replace the enum branch entirely with `enum_to_string`/`enum_from_string` — the dropdown then needs no `options`/`values` vectors and no linear scan. Same for `Gui.cpp:1131`.

**Prevention.** A `gse.meta` member-walk helper covering this shape, plus a lint for `std::meta::` and `std::define_static_array(` outside `gse.meta` — the module boundary is the natural enforcement point.

*Confidence: high.* **[verified — confirmed which reflection primitives are wrapped]**

### HIGH | Gui.cpp:1522-1601 | Eight `std::function` allocations plus eight full `style` copies per frame

```cpp
	struct resize_rule {
		std::function<bool(const rectf&, const vec2f&)> condition;
```
```cpp
		{ [style](
		const rectf& r,
```

**Impact.** `handle_idle_state` runs every frame from the state machine (`Gui.cpp:425-434`). Each entry constructs a `std::function` over a lambda capturing the whole `style` aggregate **by value** — eight heap allocations and eight struct copies per frame, before any hit testing. The array is then scanned per visible menu, and `calculate_group_bounds` (which itself allocates a `std::function` for recursion, line 1611) is called inside that loop, making the hover probe O(menus²) with an allocation per menu.

**Mechanism.** The parameter is `const style& style`; `[style]` captures by copy. The table is otherwise fixed metadata: each row maps a `resize_handle` to a predicate and a `cursor::style`.

**Resolution.** Make the rows `constexpr` — the only frame-varying input is `style.resize_border_thickness`, which can be a predicate parameter rather than a capture, letting `condition` become a plain function pointer or a `switch`. Hoist `calculate_group_bounds` out of the per-rule loop and replace its `std::function` recursion with an explicit worklist.

**Prevention.** `std::function` recursion helpers appear five times in this file (225, 1611, 1903, 1988, 2002), all on per-frame paths. A small non-allocating recursive-descent helper over `d.menus` (parent → children) removes all five and also removes the O(n) child scan each level performs.

*Confidence: high.*

### MEDIUM | Types.cpp:42,67 | Clip stack applied only to layers ≤ `popup` on draw, unconditionally on hit test

```cpp
	if (!clip_stack.empty() && static_cast<std::uint8_t>(cmd.layer) <= static_cast<std::uint8_t>(render_layer::popup)) {
```

**Impact.** Content drawn at `modal`, `cursor`, or `debug` from inside a scroll region escapes the container visually, but `hovers`/`mouse_pressed_for` still reject it because they test the clip with no layer exemption (`Types.cpp:129-131,143-145,166-168`). Visible-but-dead UI — the inverse of the canonical Paired Derivations failure, and equally invisible until someone raises an element's layer.

**Resolution.** One predicate — `clip_for(render_layer)` returning `std::optional<rectf>` — called from `queue_sprite`, `queue_text`, `hovers`, `mouse_pressed_for`, `mouse_released_for`, and `scroll_delta_for`.

*Confidence: high.*

### MEDIUM | Types.cpp:229-233 | `scroll_delta_for` is the only rect-scoped query that skips the clip test

```cpp
auto gse::gui::draw_context::scroll_delta_for(const rectf& rect) const -> vec2f {
	const input::state& input = m_input;
	if (!rect.contains(input.mouse_position())) {
		return {};
	}
```

**Impact.** A scrollable element scrolled out of sight inside a parent scroll region still consumes the wheel, stealing it from the visible container.

**Mechanism.** `hovers`, `mouse_pressed_for`, and `mouse_released_for` each add `if (const std::optional<rectf> clip = current_clip(); clip && !clip->contains(...))`; this one does not.

**Resolution.** The shared `clip_for` predicate above.

*Confidence: high.*

### MEDIUM | Gui.cpp:1439-1444 | Context menu registers no hit region and consumes neither press nor release

```cpp
		const bool hovered = row.contains(mouse) && it.enabled;
		const bool activated = interaction::activate_on_click(d.active_widget_id, row_id, hovered, hovered && left_pressed, left_released);
```

**Impact.** The panel is drawn at `popup`/z 4000 but never registered, so nothing beneath it knows it is there. Because the press is never consumed, activating a row also activates any `popup`-layer widget underneath, and the outside-press dismissal (`Gui.cpp:1496-1502`) both closes the menu and fires whatever is under the cursor. Content-layer widgets are spared only by the coarse `d.input_layer_render = popup` gate at `Gui.cpp:403` — a second, unrelated mechanism doing the arbitration the hit-region system exists for.

**Mechanism.** `process_context_menu` draws straight into `d.sprite_commands`/`d.text_commands` without ever constructing a `draw_context`.

**Resolution.** Build a `widget_context` for the panel as `draw_tab_bar` does (with the correct `current_z_order`), register `panel` as a hit region, drive rows through `press_in_rect`, and consume the dismissal press.

**Prevention.** Third site in the file that draws without a context. Once the shared `widget_context` factory exists, "draw without a context" stops being the path of least resistance.

*Confidence: high.*

### MEDIUM | Gui.cpp:791-800 | Chrome drawn before the active-tab check, hit region registered after it

```cpp
	if (!is_active_tab) {
		stamp_z();
		end_menu(d);
		return;
	}
```

**Impact.** If a menu's active tab produces no `menu_content` this frame — a producer system that skipped a frame, was gated by `occluded`, or whose tab was just removed — the chrome is fully drawn (`draw_menu_chrome` already ran at line 782) but `register_hit_region` at line 800 is never reached. The menu is visible and completely click-through.

**Resolution.** Register the hit region alongside the chrome draw, keyed on `chrome_drawn_this_frame`, so presentation and interactivity are established together.

*Confidence: high.*

### MEDIUM | Gui.cpp:1088-1098 | `calculate_display_rect` is order-dependent within a frame

```cpp
	for (const menu& child : d.menus.items()) {
		if (child.owner_id() == m.id() && !child.was_begun_this_frame && child.was_visible_last_frame) {
```

**Impact.** The same menu yields different rects at different points in one frame, because `was_begun_this_frame` flips as `menu_content` messages are processed. Chrome is drawn once at one display rect, while `process_menu` recomputes the rect at line 798 for every subsequent tab — so body, hit region, and title bar can be laid out against a rect the chrome was not drawn to. Visible geometry depends on channel message ordering.

**Resolution.** Compute all display rects once, at the top of `update_body`, from the previous frame's visibility, and read the snapshot for the rest of the frame.

*Confidence: medium.*

### MEDIUM | Gui.cpp:406-414 | `name_to_menu_id` is a hand-rolled name→id cache rebuilt per frame while `d.menus` mutates elsewhere

```cpp
	d.name_to_menu_id.clear();
	for (menu& m : d.menus.items()) {
		...
		for (const std::string& tab : m.tab_contents) {
			d.name_to_menu_id.emplace(stable_id(tab), m.id());
```

**Impact.** Two effects. (1) Per-frame rebuild of stable data: `stable_id` over every tab name of every menu, every frame. (2) The state machine runs *after* the rebuild (line 425) and can add menus — `handle_pending_drag_state:2344` creates a torn-off menu — so that menu is absent from the map when the `menu_content` loop reaches it at line 518. `begin_menu` then resolves the tab name to the old host, whose `tab_contents` no longer contains it, and `process_menu:791` early-returns: the torn-off panel draws nothing for one frame while being dragged. `emplace` also silently keeps the first entry, so a menu tag colliding with another menu's tab name is dropped without diagnostic.

**Resolution.** Drop the map. `id_mapped_collection` is already keyed by `id`, and `ids::make(name)` derives a stable id from the name — resolve directly and look up once. If a lookup index is genuinely needed, refresh it inside `begin_menu`/`remove` rather than once per frame.

*Confidence: medium (rebuild cost: high; the tear-off frame drop: medium).*

### MEDIUM | Types.cppm:251 | `mutable render_layer current_layer` lets a `const draw_context&` change the layer for everyone

```cpp
		mutable render_layer current_layer = render_layer::content;
```

**Impact.** `const draw_context&` — the type every widget receives — carries no guarantee. `scoped_layer() const` (`Types.cpp:391-393`) mutates shared draw state through it, while the sibling mutation (`clip_stack` in `scroll_handle`) correctly requires a non-`const draw_context&` (`Types.cppm:461-464`). Two contradictory conventions for one object.

**Resolution.** Drop `mutable` and make `scoped_layer` non-`const`, matching `scroll_region`. Widgets needing a layer scope then take `draw_context&`, which `builder::draw` already forwards.

*Confidence: high.*

### MEDIUM | Types.cpp:319-320 | `.as<seconds>()` to do arithmetic, and an untyped rate

```cpp
	const float dt = system_clock::dt<time>().as<seconds>();
	const float t = std::clamp(speed * dt, 0.f, 1.f);
```

**Impact.** The interpolation rate leaves the type system with no external contract behind it. `speed` (declared `float speed = 10.f`, `Types.cppm:380`) is a physical quantity — an inverse time — carried as a raw float, so `speed * dt` is unchecked.

**Resolution.** Express the parameter as a time constant instead of a rate: `const time smoothing = milliseconds(100.f); const float t = std::clamp(static_cast<float>(system_clock::dt<time>() / smoothing), 0.f, 1.f);` — same-dimension division is already dimensionless, so nothing converts.

**Prevention.** `animated_color`'s parameter type is the API: once it takes a `time`, no caller can pass a bare rate.

*Confidence: high.*

### MEDIUM | Gui.cppm:104 / Gui.cpp:855 | `data::context` stores a raw pointer to a stack object in persistent system state, never read

```cpp
		draw_context* context = nullptr;
```

**Impact.** Dead state that is also a lifetime hazard: it points at a `widget_context` living on `process_menu`'s / `process_screen`'s stack. Writes at `Gui.cpp:855, 865, 1049, 1065, 1075`; no reads anywhere. It sits inside a `[[= gse::system_state<"Gui">]]` aggregate that reflection walks.

**Resolution.** Delete the member and the five assignments.

**Prevention.** Reflected `system_state` aggregates should not contain raw non-owning pointers at all; a concept check on `system_state` members is proportionate if this recurs.

*Confidence: high.*

### MEDIUM | Gui.cpp:96-102 | `tab_chrome_height` allocates a vector of string-copying descriptors per call

```cpp
	std::vector<tab_desc> descs;
	descs.reserve(m.tab_contents.size());
	for (const std::string& tag : m.tab_contents) {
		descs.push_back({ .caption = tag });
	}
```

**Impact.** Called from `process_menu:808`, `draw_menu_chrome:1158`, and `handle_idle_state:1744` (the last once per visible menu inside the hover probe). Each call heap-allocates the vector and copies every tab caption. `draw_tab_bar:1304-1311` and `handle_idle_state:1758-1765` build two more such vectors per frame.

**Resolution.** Make `tab_desc::caption` a `std::string_view` (captions are owned by `menu::tab_contents` for the whole frame), give `tab_strip_measure`/`tab_strip_layout` a range-based overload so no vector is materialised, and cache the computed chrome height on `menu` per frame — it is recomputed at least three times for identical inputs.

*Confidence: high.*

### MEDIUM | Gui.cpp:1757-1773 | `handle_idle_state` re-runs `tab_strip_layout` to duplicate the hit test `tab_strip` already performs

```cpp
					const std::vector<tab_strip_placement> placements = tab_strip_layout(d.fonts.text, d.fstate.sty, title_bar_rect, descs, current_menu.tab_bar, tab_overflow::wrap, 60.f, 200.f);
					for (const tab_strip_placement& p : placements) {
						if (p.rect.intersection(title_bar_rect).contains(mouse_position)) {
```

**Impact.** Two independent answers to "which tab is under the cursor", from two different `title_bar_rect`s. They diverge for any menu with children. The duplicate exists because the authoritative one is broken — fixing the z-order finding makes this copy redundant rather than merely wrong. Also allocates two vectors per frame inside the hover probe.

**Resolution.** After fixing the z-order, take the tab index from `tab_strip`'s result and route the tear-off intent through the same path (e.g. a `tab_drag_started` field on `tab_strip_result`), deleting this block.

*Confidence: high.*

### MEDIUM | Gui.cpp:1955-1976 vs 1528-1601 | `resize_handle` → cursor mapping exists twice

```cpp
	auto handle_to_cursor = [](const resize_handle h) -> cursor::style {
		switch (h) {
			case resize_handle::top_left:
				return cursor::style::resize_nw;
```

**Impact.** Two sources of truth for fixed per-enumerator metadata. `Gui.cpp:643-663` is a third enum→enum table (`cursor::style` → `cursor_shape`), and `Gui.cpp:2195-2208` a fourth (`dock::location` → `cursor::style`).

**Resolution.** Annotate `resize_handle` enumerators with their cursor style (one concrete aggregate with fixed-size `char` arrays or an enum field) and derive with `annotation_from_enum`. Same for the `cursor::style` → `cursor_shape` table.

**Prevention.** Four tables in one file. Annotation-derived lookup makes an unmapped enumerator a compile-time gap rather than a `default:` fallthrough.

*Confidence: high.*

### MEDIUM | Gui.cpp:108-124 | `remove_tab_from_host` does the same lookup twice and can `erase(end())`

```cpp
	const auto tab_it = std::ranges::find(host->tab_contents, menu_name);
	const auto removed_idx = static_cast<std::uint32_t>(std::distance(host->tab_contents.begin(), tab_it));
	host->tab_contents.erase(tab_it);
```

**Impact.** The erase is unguarded. Safe today only because the scan at lines 110-115 established the name is present in the menu `try_get(host_id)` returns — a non-local invariant with nothing enforcing it.

**Resolution.** Capture the index in the first pass (or return `std::optional<std::pair<id, std::size_t>>` from a small finder) and erase by index.

*Confidence: high.*

### MEDIUM | Gui.cpp:1308,1347 | Tab identity is an array index smuggled through an `id` with a `+1` sentinel

```cpp
			.tab_id = generate_temp_id(i + 1),
```
```cpp
		current_menu.active_tab_index = static_cast<std::uint32_t>(tabs.activated.number() - 1);
```

**Impact.** Tab identity is positional, and `+1` exists only to keep index 0 distinguishable from a null id. Any reorder or removal between layout and result decode retargets the action to a different tab. `close_requested` (1350) decodes the same way and is applied a frame later via `d.pending_tab_close`, across which `tab_contents` can change.

**Resolution.** Derive the tab id from the tab's own name (`ids::make(tab_name)` or `generate_temp_id(stable_id(tab_name))`) and resolve back by name, so identity survives reordering. The same encode/decode appears three times (1308, 1339, 1347/1350).

*Confidence: high.*

### MEDIUM | Gui.cppm:124-125 | `widget_scrolls` and `widget_anim_colors` grow without bound

```cpp
		std::unordered_map<std::uint64_t, scroll_state> widget_scrolls;
		std::unordered_map<std::uint64_t, vec4f> widget_anim_colors;
```

**Impact.** `draw_context::animated_color` (`Types.cpp:311-323`) inserts an entry for every widget id it sees and nothing ever removes them. Dynamic content — file lists, log rows, search results, tree nodes — mints new ids continuously, so both maps grow for the life of the process. Both are keyed by raw `std::uint64_t` rather than `id`.

**Resolution.** Store a last-touched frame index alongside each value and sweep entries untouched for N frames at `begin_frame`; key both maps on `id`.

**Prevention.** A small `widget_cache<T>` owning insert + sweep + `id` keying makes both maps one concept.

*Confidence: medium (growth certain; practical severity depends on session length).*

### MEDIUM | InputLayers.cppm:89-90 | `k_layer_count` / `k_button_count` hardcode enum cardinality; out-of-range registrations are silently dropped

```cpp
		static constexpr std::size_t k_layer_count = 7;
		static constexpr std::size_t k_button_count = 8;
```

**Impact.** Both match today. Adding a `render_layer` enumerator makes `register_hit_region` (`InputLayers.cppm:126-128`) silently skip every region on the new layer — drawn but never hit-tested, no diagnostic. The `if (index < size())` guard converts a bounds error into invisible input loss.

**Resolution.** Derive both from the enum (a `gse.meta` count helper, or an explicit `count` enumerator) and `assert` rather than skip on an out-of-range layer. Also drop the `k_` prefix — the style guide specifies plain snake_case for compile-time constants, and a repo-wide search finds only four `k_`-prefixed constants total.

*Confidence: high.*

### MEDIUM | Types.cpp:326-335 | `scroll_handle` mutates the menu's persistent rect as a layout side channel

```cpp
	if (ctx.current_menu) {
		const rectf shrunk = rectf({ .min = ctx.current_menu->rect.min(), .max = { ctx.current_menu->rect.max().x() - config.scrollbar_width, ctx.current_menu->rect.max().y() } });
		ctx.current_menu->rect = shrunk;
	}
```

**Impact.** Inside a scroll region, `menu::rect` — persistent state that also drives `calculate_display_rect`, chrome geometry, saved layout, and the idle-state hover probe — holds a temporary value. Any code reached from inside the region that reads `current_menu->rect` gets the lie. RAII bounds the window, but the representation permits a state the rest of the system treats as impossible.

**Resolution.** Carry the content width on the `draw_context` (already per-frame and per-region) instead of mutating the menu. Note the save/restore body is duplicated verbatim between `operator=` (347-354) and `~scroll_handle` (368-377) — factor into one `release()`. Same duplication in `layer_scope` (409-411 vs 420-422).

*Confidence: medium.*

### MEDIUM | Gui.cpp:63-71 vs Types.cpp:49-56 | Two implementations of text interning against the same pool and counter

```cpp
auto gse::gui::intern_text(data& d, const std::string_view text) -> std::string_view {
	std::deque<std::string>& pool = d.text_pools[d.text_pool_slot];
	if (d.text_pool_used == pool.size()) {
```

**Impact.** Duplicated knowledge with a shared mutable counter. Both are correct today only because `std::deque` never invalidates references on `emplace_back` — a switch to `std::vector` would dangle every `string_view` already handed to a `text_command`, in both copies.

**Resolution.** Once the chrome/tooltip/context-menu paths use a `widget_context`, delete `intern_text` and use `draw_context::intern`.

*Confidence: high.*

### MEDIUM | Gui.cpp:915,964-971 | `caption_button` takes `const std::string&` for a key, allocating three strings per frame

```cpp
auto gse::gui::caption_button(builder& b, const rectf& rect, const std::string& key, const std::span<const symbol::stroke> glyph, const vec4f hover_color, const bool enabled) -> bool {
```

**Impact.** Every caller passes a literal — `"##screen_caption_close"` (22 chars), `"##screen_caption_max"`, `"##screen_caption_min"` — all longer than the 15-char SSO buffer, so each call heap-allocates a `std::string` that is immediately hashed and discarded. Three allocations per frame while a screen with chrome is up. `ids::make` already takes `std::string_view`.

**Resolution.** Change `key` to `std::string_view` and propagate to `process_menu`/`begin_menu`.

*Confidence: high.*

### MEDIUM | Gui.cppm:171-312 | Implementation internals are exported, inflating the BMI for every importer

```cpp
	auto handle_idle_state(
		data& d,
```

**Impact.** `init_body`, `update_body`, all five `handle_*_state`, `draw_menu_chrome`, `draw_tab_bar`, `process_context_menu`, `begin_menu`, `end_menu`, `calculate_display_rect`, `usable_screen_rect`, `sync_monitor_scale`, `reload_font`, and `caption_button` have no consumer outside `Gui.cpp`. Only `apply_scale`, `clear_menu_interaction`, `init`, `run`, `save`, and `process_menu`/`process_screen` are genuine API. Every exported declaration is serialised into the BMI that every `import gse.graphics` consumer deserialises — the dominant compile cost in this build.

**Resolution.** Move the internal declarations into the existing non-exported `namespace gse::gui { ... }` block (`Gui.cppm:35-48`). No definition needs to move.

*Confidence: high.*

### MEDIUM | InputLayers.cppm:162-185 | Hit arbitration is O(regions) per query, and every widget queries it

**Impact.** `topmost_at` linearly scans every region in every layer for each call, reached from `hovers`, `mouse_pressed_for`, `mouse_released_for`, and `scroll_delta_for` — once or more per widget per frame. Total cost O(widgets × regions), quadratic in UI density, on the input path.

**Resolution.** The query position is `m_input.mouse_position()` for essentially every call in a frame, and regions come from the previous frame, so `topmost_at` can be computed once in `begin_frame` and the per-widget path becomes a comparison.

*Confidence: high.*

### LOW findings — GUI core

- **InputLayers.cppm:112-123,163** — Hit-test regions lag drawing by one frame (`topmost_at` reads `m_previous_regions` while the current frame registers into `m_current_regions`). Inherent to the immediate-mode design and the reason the z-order tab bug is intermittent rather than absolute. Named explicitly so it is not rediscovered as a defect. *Confidence: high.*
- **InputLayers.cppm:113,117** — Two double-buffering mechanisms in one class: `m_current_regions`/`m_previous_regions` swapped by hand with `std::swap` while `m_resize_blocks` uses `double_buffer<T>`. Use `double_buffer` for both. *high*
- **InputLayers.cppm:97,221-227** — `std::unordered_set<int> m_consumed_keys` discards the typed `key` enum and allocates a node per consumed key on a per-frame path. A `std::bitset` sized from the enum removes both. *high*
- **InputLayers.cppm:144-160** — `right_edge_block_span` filters on `rect.height() <= rect.width()`, an undocumented "not a vertical divider" heuristic. Encode the intent at registration instead. *high*
- **InputLayers.cppm:18-19** — Empty `namespace gse::gui { }` block. Delete. *high*
- **IDs.cppm:24-36** — `ids::scope` has a dead `active` flag (inheriting `non_copyable` plus a user-declared destructor deletes the move ops, so it can never become `false`) and a public non-`m_` member. Either add the defaulted move ops the style guide requires, or delete `active`. Both constructors also assign `active = true` in the body rather than the initializer. *high*
- **IDs.cppm:40 + Gui.cppm:127** — `inline thread_local std::vector<std::uint64_t> id_stack` is popped by an `ids::scope` heap-allocated into `data::current_scope` (`Gui.cpp:877,899`) — one allocation per menu per frame — whose construction and destruction happen in different functions (`begin_menu`/`end_menu`). If the GUI system is ever scheduled onto a different worker between them, the pop lands on the wrong thread's stack. `process_menu`/`process_screen` already use a correct stack-scoped `ids::scope` (`Gui.cpp:830,1025`), making `d.current_scope` redundant. *medium*
- **Types.cppm:126,132** — Defaulted destructors on `dock::area`/`dock::space` suppress implicit moves on plain aggregates; `space` is held in `std::optional` and copied at `Gui.cpp:465`. Delete both. *high*
- **Gui.cpp:465** — `const auto [areas] = d.active_dock_space.value();` copies a `std::array<area, 5>` every frame a drag is active. Use `const auto& [areas] = *d.active_dock_space;`. *high*
- **Gui.cpp:386,396** — `d.fstate = {};` at 386 is overwritten at 396 with no intervening read. More broadly, `frame_state` is transient per-frame data stored in persistent system state and zeroed at both entry (386) and exit (738), so any out-of-frame reader silently sees a default style. *high*
- **Gui.cpp:401,854,1048** — `hot_widget_id` is reset three times per frame, so it only reflects the last menu; `draw_menu_chrome` marks the popout close button hot at 1258 before the reset at 854 wipes it. Reset once per frame. *high*
- **Gui.cpp:389** — `d.text_pool_slot ^= 1;` silently breaks for any `per_frame_resource<T, N>` with `N != 2`. Rotate with `(slot + 1) % frames_in_flight`, which the type already exposes. *high*
- **Gui.cpp:1817-1861** — Raw `menu*` (`m` at 1818, `visible_menus` at 1824-1830) held across `d.menus.remove(current.menu_id)` at 1855, which swap-removes and can reallocate. `m` is dereferenced again at 1860 in the sibling branch — safe only because `layout::dock` happens not to add or remove. *medium*
- **Types.cppm:190** — `context_menu_target = std::variant<std::monostate, std::uint64_t, id>` mixes a raw integer identity with `id` and forces the hand-written `std::visit` hash at `Gui.cpp:1413-1424`. Collapse to `std::optional<id>`. *medium*
- **Types.cpp:135-156** — `mouse_pressed_for` is a `[[nodiscard]] const` query with a global consumption side effect; calling it twice for the same rect returns `true` then `false`. Consider `try_claim_press(rect)`. Relatedly `press_in_rect` (`Interaction.cppm:172`) pairs the *consumed* press with the *unconsumed* `ctx.mouse_released()`, so a widget using `mouse_released_for` elsewhere and a `press_in_rect` widget both respond to one release. *medium*
- **Gui.cppm:137 / Types.cppm:47** — `static constexpr time` members in exported structs. The style guide bans unit-typed `constexpr` at module namespace scope because it serialises a reflection-derived NTTP into the BMI; class scope in an exported struct is serialised the same way. Flagged for awareness. *low*
- **Gui.cpp:1511** — Parameter named `style` shadows the type `style`; `apply_scale` uses `sty` for the same thing. *high*

### Style findings — GUI core

**Gui.cpp**
- 1528-1601 — `resize_rules` initializer wraps each lambda's parameter list declaration-style inside an expression and indents bodies with alignment padding.
- 1664-1666, 1671-1673, 1678-1680, 1685-1686 — positional aggregate initialization with hanging alignment, in the same function that uses designated initializers at 1647-1654 and 1730-1737. Two same-typed `id` fields in `resizing_divider` make the positional form swappable.
- 1377 — non-empty lambda body collapsed onto one line.
- 2247-2248, 2285-2286, 2332-2333 — alignment padding on ternary continuations.
- 26 — stray double blank line between import groups.
- 178-179 vs 148/1142 — `asset::get<gse::font>` vs `asset::get<font>` in one file; `gse::` is redundant inside `gse::gui::` definitions. Same for `gse::input::state` throughout (301, 311, 422-423, 461, 752, 1151, 1290, 1354, 1511, 1817).

**Types.cppm** — 508-509 identifier split across lines (`using value_type = std::` / `variant<...>;`); 513-523 template member definitions written inline inside the namespace; 515-516 empty constructor body not collapsed to `{}`.

**Types.cpp** — 12-13 stray double blank line between import groups.

**DrawStruct.cppm** — 53-54 expression split mid-member-access; 105-106, 113-114 designated initializers indented with alignment padding; 101 `gse::internal::is_quantity<F>` reaches into another module's `internal` namespace from the GUI layer and carries a redundant `gse::` qualifier.

**InputLayers.cppm** — 89-90 `k_` constant prefix.

**IDs.cppm** — 24-36 `scope::active` public and unprefixed.

### Clean — GUI core

- **Interaction.cppm** — declarations/definitions correctly split, module-private helper block follows the sanctioned pattern, the unit-typed threshold is a function-local `constexpr`, `press::color` derives presentation from one state, and `press_from` orders `mark_hot`/`activate_on_click` so `held` cannot be true on the release frame. Its two design notes (unscoped `mouse_released` at 172, `enabled` gating hover before hot-marking) are the deliberate behaviour the guide sanctions for stateful controls.
- **WidgetContext.cppm** — clean as written; its defect is that it is *empty*, which is an architecture finding against `Types.cppm`.

---

## 2. GUI layout, screens, persistence

Files: `Layout.cppm`, `LayoutOps.cppm`, `Builder.cppm`, `Styles.cppm`, `Cursor.cppm`, `MenuStack.cppm`, `Settings.cppm`, `SettingsScreen.cppm`, `Save.cpp`, `Save.cppm`, `ContextActions.cppm`, `PopoutSystem.cppm`, `Loading.cppm`, `LoadingScreen.cppm`, `DevOverlays/Profiler.cppm`.

### CRITICAL | Layout.cppm:52 | `dock()` permits a parent/child cycle; `update()` then recurses without bound

```cpp
auto gse::gui::layout::dock(id_mapped_collection<menu>& menus, const id child_id, const id parent_id, const dock::location location) -> void {
	menu* parent = menus.try_get(parent_id);
	menu* child = menus.try_get(child_id);
```

**Impact.** Stack overflow / process crash from an ordinary drag gesture — dragging a menu onto a menu that is already docked *into* it.

**Mechanism.** `dock()` validates only that both menus exist and that the location is a real split (`!parent || !child || location == none || location == center`); it never checks whether `parent_id` is a descendant of `child_id`. `child->swap_parent(*parent)` reparents the child but leaves the old parent's owner pointing back at it, producing `A→B→A`. `layout::update` (`Layout.cppm:150-177`) then walks `owner_id()` links recursively with no visited set and no depth bound (`update(menus, child->id())`, line 176), so both nodes satisfy `docked_to != none` and `was_visible_last_frame` and it recurses forever. Reachability: `Gui.cpp:1921-1932` selects `potential_dock_parent_id` by hit-testing visible menus and excludes only `other_menu.id() == current.menu_id` — descendants are not excluded, and a docked child sits directly adjacent to its parent, so it is under the cursor during a normal drag.

**Resolution.** Make the cycle unrepresentable rather than guarding the traversal. `dock()` should reject a `parent_id` reachable from `child_id` via `owner_id()`, and the ancestry walk belongs on the collection/menu type (`is_ancestor_of`) so both `dock()` and the `Gui.cpp` drop-target filter consult one authority. Bounding `update()`'s recursion alone would hide the invalid graph rather than prevent it.

**Prevention.** The same unguarded owner-walk appears three more times (`Layout.cppm:103` `expand`, `Layout.cppm:127` `scale_group`, `Gui.cpp:1903` `move_group`), each of which also loops forever on a cycle. Put parent/child traversal on the menu collection once — a children view plus an ancestry predicate — so no call site owns the walk.

*Confidence: high.* **[verified — absence of the ancestry check and the unbounded recursion both confirmed against source]**

### CRITICAL | PopoutSystem.cppm:67 | Raw `register_settings_type*` escapes the registry mutex and is retained across frames

```cpp
	save_reg.for_each_entry([&](const gse::settings::register_settings_type& entry) {
		if (entry.category == category && entry.settings_ptr && std::ranges::any_of(entry.fields, &gse::settings::settings_field::hot_reloadable)) {
			found = &entry;
```

**Impact.** Use-after-free inside the GUI draw path — reading freed `settings_field` vectors and calling a freed `push_change` thunk — the first time any settings type registers after a popout is opened.

**Mechanism.** `save::registry` stores entries as `std::vector<settings::register_settings_type> m_entries` and appends with `push_back` (`Save/*.cppm:93,177`), so every element pointer is invalidated on growth. `find_hot_entry` takes the address of a loop element and returns it after `for_each_entry` releases `m_entries_mutex`. That pointer is stored in `popout_entry::entry` (line 23) for the lifetime of the popout, re-read every frame (139-143), and captured by value into a deferred `menu_content.build` callback (149) that another system executes (`Gui.cpp:517`). Any later `registry::add` silently dangles all of them.

**Resolution.** Store the identity, not the address: `popout_entry` already has room for `entry.type_id` (a `gse::id`), and the draw path should resolve through the registry at use time, inside the lock. If the resolve cost matters, publish an immutable `shared_ptr<const>` generation rather than handing out interior pointers.

**Prevention.** `for_each_entry` invites address capture because it hands out references under a lock it then drops. Return a copy or an owning snapshot, or make the callback parameter a handle/id rather than a `const&`, so a caller cannot take an address that outlives the lock.

*Confidence: high.* **[verified — pointer escape past the lock confirmed against source]**

### HIGH | PopoutSystem.cppm:149 | Deferred draw callback binds a reference to `ctx.channels` and mutates another system's state

```cpp
			.build = [entry = popout.entry, ps_ptr = &d.panel_state, &channels_ref = ctx.channels](builder& b) {
				gse::settings::draw_fields_for_entry(b, *ps_ptr, channels_ref, *entry, true);
			},
```

**Impact.** Two defects. (1) A reference to the per-run `gse::context`'s channel writer outlives the run that produced it. (2) `popout_system::data::panel_state` is written from inside the GUI system's `run()` — pending-field values, dropdown open/close state, text-input state — which the scheduler cannot see and therefore cannot serialize against this system's own next run.

**Mechanism.** The callback is pushed on the `menu_content` channel and invoked later by `Gui.cpp:517`. Every other producer captures the writer **by value** — `EditorApp.cppm:474`, `:508`, `Terminal.cpp:354` all spell `channels = ctx.channels`. `draw_fields_for_entry` writes `ps.dropdowns[...]`, `ps.input_states[...]` and `pending.value` through `ps_ptr`.

**Resolution.** Capture the writer by value like every sibling. For the mutable UI state, move popout `panel_state` into the GUI's own widget state (it already owns `widget_scrolls`, `widget_anim_colors`, tooltip and context-menu state keyed per widget), or route field changes back through a channel so the popout system applies them in its own `run()`.

**Prevention.** Having `context::channels` expose only a copyable writer value makes binding a reference to it inexpressible.

*Confidence: high.*

### HIGH | DevOverlays/Profiler.cppm:303 | Time quantities converted to raw `double` for display, ratios and scaling

```cpp
				const double dur_ns = has_cpu_timing ? static_cast<double>(static_cast<std::uint64_t>(n.stop - n.start)) : 0.0;
				const double pct_frame = (frame_ns > 0.0 && has_cpu_timing) ? (dur_ns / frame_ns) * 100.0 : 0.0;
				draw_col(to_fixed(dur_ns / 1000.0, buf, 32, 1), draw_x_dur, w_dur);
```

**Impact.** Every arithmetic step after the cast is dimensionally unchecked; the displayed unit exists nowhere in the code (the heading is `"Duration"`, line 258) so a future edit to `/1000.0` produces plausible wrong numbers with no diagnostic. `profile_tree::frame_ns` (line 47, `double`) and the write at 123 restate the same defect at rest.

**Mechanism.** The inputs are already strongly typed — `trace::node::start/stop/self` are `time_t<std::uint64_t>` (`Diag/Trace.cppm:165-167`) and `profile_row` preserves that (39-41). Line 303-304 casts them out; 305 divides two same-dimension values *after* stripping them (the style guide's exact "wrong" example, and `percentage<T>` exists); 336-337 hand-convert ns→µs by `/1000.0`; 350-351 exit via `agg->ema.as<microseconds>()` / `agg->peak.as<microseconds>()` purely to print, under headings `"Avg"`/`"Peak"` that name no unit at all. None of these exits has an external contract. The correct idiom is in this same subsystem: `ProfileSummary.cppm:178` uses `{:.3f:ms}` and 200 pairs `{:>13}` headings with unit-bearing specs.

**Resolution.** Keep `profile_row::start/stop/self` and `profile_tree::frame` as `time`, compute `dur = n.stop - n.start` and `pct = dur / frame` (already dimensionless), and format through the quantity spec — `std::format("{:.1f:us}", dur)` — replacing `to_fixed` entirely for the time columns. Column headings then need widening for the appended unit, exactly as `ProfileSummary.cppm` does.

**Prevention.** Delete the local `to_fixed` helper: a private hand-rolled numeric formatter next to a value that already has a formatter is what makes the conversion feel necessary. If a fixed-precision no-allocation path is genuinely needed, it belongs on the quantity formatter so every caller inherits it.

*Confidence: high.*

### HIGH | DevOverlays/Profiler.cppm:206 | Column resize hand-rolled; `gui::column_header` already owns this, including the cursor

```cpp
	auto handle_resize = [&](float& width, const float right_anchor_x, const float split_x, const int idx) {
		const bool hovered = ctx.hovers(hit_rect);
		if (resizing_col_idx == idx) { width = std::max(20.f, right_anchor_x - mouse_pos.x()); set_style(cursor::style::resize_ew); }
```

**Impact.** Three failures. (1) A press that `hovers()` would reject still starts a resize, because activation is `ctx.mouse_held()` — an unscoped raw device read consulting neither the clip stack, nor `input_available()`, nor press consumption. Clicking a popup drawn over the profiler at that x can grab a divider. (2) `set_style` from inside a widget is the behaviour the guide forbids — it works only for callers inside the GUI module and silently does nothing elsewhere. (3) Column geometry is duplicated instead of shared.

**Mechanism.** `gui::column_state`, `gui::column_header(...)` and `gui::column_cell(state, row, index)` already exist (`Widgets/ColumnHeader.cppm:22-56`) and `column_header_result::resize_cursor` (38) already lets the *caller* apply the cursor. The Editor's profile panel is the reference consumer: `Profile.cpp:861` calls `column_header`, 871 accumulates `resize_cursor`, 1186-1187 pushes `set_cursor_shape_request`. The engine's own overlay does the opposite of its own widget.

**Resolution.** Replace the five width statics, `resizing_col_idx`, `handle_resize`, `draw_header_item` and the `draw_x_*` arithmetic with `column_state` + `column_header` + `column_cell`, and report the cursor out of `profiler::draw` instead of calling `set_style` — which requires `profiler::result` to stop being `void`.

**Prevention.** Make `cursor::set_style` uncallable from widget code (see the Cursor finding), at which point hand-rolling a resize grip stops compiling in this shape.

*Confidence: high.*

### HIGH | DevOverlays/Profiler.cppm:188 | Hit-tested and drawn column edges computed from different snapshots of the same widths

```cpp
	const float x_frame_right = menu_content.right();
	const float x_frame_left = x_frame_right - w_frame;
	...
	const float draw_x_frame = menu_content.right() - w_frame;
```

**Impact.** While dragging, the divider's hit rect sits at the previous frame's edge and, for every column left of the one being dragged, at a stale edge within the same frame. The grip visibly detaches from the line being dragged, and adjacent dividers become grabbable where nothing is drawn.

**Mechanism.** Lines 188-201 compute the hit anchors, 226-230 call `handle_resize` which **mutates** `w_frame`/`w_peak`/`w_avg`/`w_self`/`w_dur`, and 232-236 recompute the same formulas for drawing from the mutated values. Because the anchors cascade right-to-left, mutating `w_frame` at 226 invalidates `x_peak_right`/`x_peak_left` used at 227, and so on.

**Resolution.** `column_cell(state, row, index)` is the single predicate both the header hit test and the row cells go through in `ColumnHeader.cppm:105` and `Profile.cpp:894,904`. Adopting it removes both copies rather than resynchronising them.

*Confidence: high.*

### HIGH | DevOverlays/Profiler.cppm:148 | Widget state held in function-local `static`s

```cpp
	static profile_tree tree;
	static interval_timer refresh(milliseconds(100.f));
	static bool interacting = false;
```

**Impact.** Two profiler panels (two menus, or a popout plus the docked panel) silently share one tree, one selection, one set of column widths and one resize latch, so interacting with either drives both. The state is owned by no system, is never released, survives scene teardown, and is invisible to the scheduler; `static profile_tree tree` also retains a full nested `std::vector` graph for the process lifetime.

**Mechanism.** Lines 148-150, 167-173 (`w_dur`…`resizing_col_idx`) and 269-270 (`selection`, `options`) are all function-local statics inside `profiler::draw`. The engine's convention is the opposite: `draw_context` carries `widget_scrolls`/`widget_anim_colors` keyed per widget, and `settings::panel_state` threads dropdown/input state explicitly.

**Resolution.** Give the overlay a `profiler_state` aggregate held by the owning system (or keyed into the draw context's widget-state maps) and pass it in, as `column_state`, `tree_selection` and `panel_state` already are.

**Prevention.** A widget whose `draw` takes no state parameter cannot be stateful — the same rule already applied to `dropdown`, `tree`, `text_input` and `column_header`.

*Confidence: high.*

### HIGH | ContextActions.cppm:32 | Hand-rolled `std::meta` annotation scan duplicating `gse::meta::find_class_template_annotation`

```cpp
	consteval auto find_context_action_anno(std::meta::info fn) -> std::meta::info {
		for (const auto ann : std::meta::annotations_of(fn)) {
			const auto t = std::meta::dealias(std::meta::type_of(ann));
			if (std::meta::has_template_arguments(t) && std::meta::template_of(t) == ^^context_action) {
```

**Impact.** A second, divergent implementation of the engine's annotation read. Rejected on sight regardless of correctness, and it will not inherit fixes made to the helper.

**Mechanism.** `gse::meta::find_class_template_annotation(m, template_reflection)` (`Meta/SettingsAnno.cppm:170`) is a line-for-line match — same `dealias`, same `annotations_of`, same `has_template_arguments`/`template_of` test — with six existing call sites (`Ecs/SystemAnno.cppm:117,125`, `Meta/Args.cppm:66,75,84`, `Meta/Fields.cppm:62`). The only difference is that the helper returns the annotation *type* while this returns the annotation, which line 46 (`using anno_t = [:std::meta::type_of(ann):]`) immediately converts back. The file already imports `gse.meta` and uses the sanctioned helper one line later at 47 — the search stopped at the first mechanism that worked.

**Resolution.** Delete `find_context_action_anno`; use `constexpr auto anno_t_info = meta::find_class_template_annotation(Fn, ^^context_action);`. The `context_action<Label, Group, Icon>` payload shape is fine — class-template annotations read through this helper are the established idiom (`system_state<>`, `field_key<>`, `range<>`), so no annotation redesign is needed.

**Prevention.** `gse.meta` should be the only module permitted to name `std::meta::annotations_of` — already true of every other engine file and directly greppable.

*Confidence: high.*

### HIGH | Styles.cppm:47 | `style` default member initializers disagree with `midnight()` on six fields

```cpp
		vec4f color_separator = { 0.18f, 0.26f, 0.32f, 1.0f };      // line 47
		...
			.color_separator = { 0.16f, 0.26f, 0.32f, 1.0f },        // line 163, midnight()
```

**Impact.** A default-constructed `style` and `style::from_theme(theme::midnight)` render differently, so "the default theme" means two different things depending on which path produced the value. Drifted fields: `color_separator` (47 vs 163), `color_widget_background` (63 vs 170), `color_widget_hovered` (64 vs 171), `color_button_background` (69 vs 174), `color_button_hovered` (70 vs 175), `color_shadow` alpha (97 vs 191). Additionally `color_folder`/`color_file` (58-59) are overridden by only two of six themes, so `frost` and `high_contrast` get tuned icon colours while `eclipse`/`ember`/`forest` silently inherit midnight-tuned ones.

**Resolution.** One source of truth. Make the NSDMIs the neutral base and have each theme factory apply only its deltas, or drop the NSDMIs and make `midnight()` the sole default. The second also collapses six near-identical 50-line blocks, since most fields in every factory are either the shared base value or a mechanical function of `accent`/`accent_dim` (`color_widget_active`, `color_slider_fill`, `color_toggle_on`, `color_dock_preview`, `color_selection` are `accent`-derived in all six).

**Prevention.** An `apply_accent(style&, vec4f accent)` plus per-theme delta functions makes it impossible to restate a field that is not actually theme-specific.

*Confidence: high.*

### HIGH | LoadingScreen.cppm:54 | `const_cast` to mutate `loading::state` through a const pointer

```cpp
	const_cast<loading::state*>(m_state)->mark_rendered();
```

**Impact.** The type system's statement that this screen only observes the loading state is false, and the write happens on the render thread against a state written by the loader thread. The style guide bans `mutable` for exactly this; `const_cast` is the stronger version of the same hole.

**Mechanism.** The constructor takes `const loading::state&` (21-23) and stores `const loading::state* m_state` (39), but `build()` needs to record that the screen has rendered at least once. `mark_rendered()` is a plain `store` on `std::atomic<bool> m_rendered_once` (`Loading.cppm:66-68`), so it has no reason to be non-const.

**Resolution.** Make `mark_finished()` and `mark_rendered()` `const` — they touch only atomics and are logically observations-of-completion — or take the state by non-const reference if the screen genuinely owns the write.

*Confidence: high.*

### HIGH | Cursor.cppm:45 | Non-`inline` mutable namespace-scope variable, and a second cursor-delivery mechanism

```cpp
namespace gse::cursor {
	auto current_style = style::arrow;
```

**Impact.** Directly violates the style guide's variable-linkage rule — a non-`inline` namespace-scope variable in a module interface is emitted by every importing TU. Beyond linkage, this is process-global mutable UI state with no owner: last-writer-wins within a frame, ordering-dependent, and reset only by scattered `set_style(cursor::style::arrow)` calls in `Gui.cpp` (1513, 1811, 1820, 1886, 1945, 1951, 2184, 2189). A widget that sets a style and does not run again leaves the cursor stuck until unrelated code resets it.

**Mechanism.** `set_style`/`current` are exported (31-35) and write/read this global; `Gui.cpp:642-665` reads `cursor::current()` and translates it into a `set_cursor_shape_request` channel push. Two delivery mechanisms for one fact — precisely what makes a widget's behaviour depend on which layer instantiated it. `ids::id_stack` (`IDs.cppm:40`) shows the correct spelling: `inline thread_local`.

**Resolution.** Minimum: `inline auto current_style = style::arrow;`. The real fix is to stop exporting `set_style` — widgets return their desired `cursor::style` in their result (as `column_header_result::resize_cursor` already does) and the GUI frame aggregates one winner per frame, which also removes the reset scatter in `Gui.cpp`.

*Confidence: high.*

### HIGH | Settings.cppm:410 | Per-frame allocation and repeated locked registry scans in the settings draw path

```cpp
	std::vector<std::string> category_order;
	std::unordered_set<std::string> seen;
	save_reg.for_each_entry([&](const register_settings_type& entry) {
```

**Impact.** Every frame the settings panel is open this rebuilds stable data from scratch: a `vector<string>` plus an `unordered_set<string>` of categories (411-427), then `1 + 2N` calls to `save_reg.for_each_entry` for N categories (413, 431, 453, 470), **each acquiring `m_entries_mutex`** (`Save/*.cppm:105`) — a mutex taken repeatedly in a per-frame path. Per category it constructs two `std::function<void()>` (445-450, 452-468). Per field it allocates `pretty_label(field.key)` (312), a `field.format(...)` result string (306), and for sliders a `std::format("{}", value)` (206, 228). `builder::draw` then copies the whole `params` aggregate — including both `std::function`s — by value (`Builder.cppm:48`).

**Resolution.** Build the category list and its closures once (invalidated by a registry generation counter) and store them in `panel_state`; iterate the registry once per frame into a category-bucketed view; make `builder::draw` take `const typename W::params&`; hold `pretty_label` results in `panel_state` keyed by the field key.

**Prevention.** Expose a cached, generation-stamped snapshot from `save::registry` so consumers cannot repeatedly lock it in a draw path.

*Confidence: high.*

### HIGH | SettingsScreen.cppm:217 | The category list is derived twice with different eligibility rules

```cpp
auto gse::gui::settings_screen::refresh_categories() -> void {
	if (!m_categories.empty()) {
		return;
	}
```

**Impact.** The sidebar can list a category whose content pane renders nothing, and a category registered after the screen first built never appears.

**Mechanism.** `refresh_categories` (217-234) accepts any entry with a non-empty category. `settings::panel` (`Settings.cppm:413-426`) applies two more filters the sidebar does not: it skips entries with `fields.empty() && !draw_page` (417) and applies `category_filter`. So a category contributed only by a field-less, page-less entry is selectable but blank. Separately, `if (!m_categories.empty()) return;` freezes the list permanently after the first non-empty build while `panel()` re-derives it every frame — the two views can never re-converge.

**Resolution.** One `categories()` query (on the registry or `panel_state`) applying the eligibility rule, consumed by both the sidebar and `panel()`. Invalidate on a registry generation change rather than caching on emptiness.

*Confidence: high.*

### MEDIUM findings — GUI layout, screens, persistence

- **Loading.cppm:38** — `mutable std::mutex m_mutex` with `phase()`/`done()`/`total()` each taking the lock separately: three lock acquisitions and one `std::string` heap allocation per frame in the loading draw path (`LoadingScreen.cppm:69-71`), plus a torn read — `done()` and `total()` are separate locked reads, so the bar can briefly show a ratio greater than 1. `m_finished`/`m_rendered_once` use atomics, so the type has two synchronization mechanisms. Publish progress as one value (a packed `std::atomic<std::uint64_t>` or a `shared_ptr<const progress>` generation) and expose the phase as a stable `id`/enum so the getter does not allocate. *high*
- **SettingsScreen.cppm:138** — `ctx.sprites.push_back` bypasses `queue_sprite`'s layer clamping, z-order assignment and clip-stack intersection. The same function uses `ctx.queue_sprite` everywhere else (165, 210, 272, 364, 413, 444). Make `sprites`/`texts` non-public on `draw_context` so the emission paths are the only ones. *high*
- **SettingsScreen.cppm:159** — `draw_close_button` (159-180) and `draw_scope_entry` (302-336, where 333 launches `shell::reveal` on press) hand-roll press behaviour and fire on press, while `draw_footer_button` (398-433) 240 lines later correctly uses `interaction::press_in_rect` and `btn.color(...)`. *high*
- **SettingsScreen.cppm:398** — `draw_footer_button(ui, rect, label, bool enabled, bool primary, id key)` — adjacent same-typed booleans, called as `(..., true, true, ...)` (501), `(..., can_apply, true, ...)` (512), `(..., can_apply, false, ...)` (522). Same shape in `layout::inset_per_side(parent, top, right, bottom, left)` (`LayoutOps.cppm:114-121`, called at 197 and 352-358) and `fit_card(viewport, min, max, margin)`. Use named aggregates with designated initialization. *high*
- **Settings.cppm:51** — Four parallel maps (`dropdowns`, `input_buffers`, `input_states`, plus `pending_settings::fields`) keyed by raw `std::uint64_t` describe one thing. `discard_all` (380-388) clears three of them and leaves `dropdowns` populated, so a dropdown's open/selected state survives a discard. The key is built by hand (`field_widget_key`, 132-134) even though line 290 already converts it via `gui::ids::make_from_key`. Collapse to one `field_ui_state` aggregate in one `id`-keyed collection. *high*
- **Settings.cppm:33** — `dimensioned_input_state`, `panel_state::dimensioned_states` and `panel_state::input_buffers` are never read or meaningfully written anywhere in the repository — only cleared, in two places written separately (`discard_all` 380-388 and the section reset lambda 462-467, which diverge: one erases per type, the other clears wholesale). Delete the dead members and make the reset a single `panel_state` member. *high*
- **Settings.cppm:137** — Unit-typed settings have no control. A setting declared with a `gse` quantity classifies as `settings_field_widget::text`, so it renders as a free-text box; the value round-trips through the quantity formatter and back through `gse::parse` every frame, and `pending.modified = pending.value != live_value` (316) compares those strings — any formatting asymmetry latches the field as permanently unsaved and inflates the footer counter. `gui::quantity_slider<T, Unit>` exists (`Widgets/Slider.cppm:98-110`) and `dimensioned_input_state` is the vestige of the intended handling; `draw_field_control` has no case for it and `default: break;` (249) silently draws nothing. *medium*
- **Layout.cppm:103** — `std::function` recursive lambdas with O(n²) sibling scans; heap allocation per call plus an indirect call per recursion step, each level rescanning every menu. Repeats at 127 (`scale_group`) and `Gui.cpp:1903` (`move_group`). A `children_of(id)` accessor collapses all four sites into plain recursive named functions. *high*
- **Layout.cppm:121** — `constexpr vec2f min_menu_size = { 200.f, 200.f }` duplicates `style::min_menu_size` `{ 200.f, 120.f }` (`Styles.cppm:106`), which is `[[= gse::scaled]]`. This copy disagrees on height and does not track DPI, and the `scale_x`/`scale_y` computation (124-125) forces the group to 300×300 rather than enforcing a minimum. *high*
- **Layout.cppm:156** — `update()` lays out only the first docked child (`break` at 161) though `dock()` places no limit on how many may dock to one parent, and gates on `was_visible_last_frame`, so a docked child hidden last frame is skipped and the parent occupies only its split remainder. *medium*
- **Layout.cppm:195** — `areas[0..4]` and `dock::location` are two representations of one enumeration kept in sync by hand and by comment (194, 201, 208, 215, 222). Iterate the enumerators and derive the widget offset from an annotation; that deletes the index constants, the five blocks and the comments together. *high*
- **Cursor.cppm:109** — Eight switch cases differing only by a direction vector, with `inv_sqrt2` redeclared four times (146, 156, 166, 176), plus a second parallel table mapping the same enum to `cursor_shape` in `Gui.cpp:642-661`. Put the arrow directions and the corresponding `cursor_shape` on the `cursor::style` enumerators as annotations. *high*
- **MenuStack.cppm:343** — `apply`'s factory branch calls `f()` then `s->on_push()` with no null checks, while `push_factory` (268-278) checks both `!factory` and `!s`. `apply` should call `push_factory(f)`; the transition belongs in one function. *high*
- **MenuStack.cppm:122** — Two deducing-`this` overloads of `top` with identical bodies (294-306) where the style guide asks for one `auto top(this auto& self) -> decltype(auto)`. *high*
- **LayoutOps.cppm:311** — `update_split` reimplements press/active tracking outside `interaction`: hover as `rect.contains(mouse)`, activation as a raw pressed flag, ownership as a `bool& dragging` out-parameter. Five call sites depend on it (`EditorApp.cppm:293,305`, `Profile.cpp:1134`, `SystemGraph.cpp:594`, `Terminal.cpp:790`). The `blocked` field is a caller-supplied substitute for arbitration the context would have done. *medium — the `blocked` mechanism is a deliberate prior fix, so any change must migrate the resize-block list rather than drop it.*
- **LayoutOps.cppm:413** — `within_scope` mutates the live menu rect (which is what layout persistence saves, `Save.cpp:18-24`, and what next frame's drag/resize reads) and establishes no clip, so content overflowing the sub-rect draws outside it *and* stays hit-testable. Visible at `SettingsScreen.cppm:278-291`: the sidebar's `nav_item` list is unbounded, so with enough categories rows draw over the content pane and footer and remain clickable. Push the sub-rect onto `clip_stack` and carry it on the draw context; wrap the sidebar in `gui::scroll_region` bound via `scroll_region_info::size`. *high*
- **Save.cpp:90** — `load` discards default menus absent from the file (a panel added in a later build never appears until the layout file is deleted), returns a default-constructed `rectf` from `resolve_rect` when `position_ratio` or `design_size` is missing (46-49) yielding an invisible zero-size menu, and `loaded_map.at(tag)` (143) throws on an inexact tag round-trip in a boot path with no handler. Merge onto `default_menus` rather than replacing. Also `save` takes `id_mapped_collection<menu>&` non-const without mutating, forcing the full-collection copy at line 93. *high*
- **Builder.cppm:48** — `draw` takes widget params by value, copying `std::function`s every frame; for `gui::section` that is two heap allocations per category per frame on top of the caller's two. Take `const typename W::params&`. *high*
- **PopoutSystem.cppm:92** — `popout_entry::menu_id` is computed and never read; `menu_name`, `menu_id` and the map's `category` key are three representations of one identity. `menu_content::menu` being a `std::string` (`Builder.cppm:68`) is why a name is needed at all. *high*
- **Settings.cppm:444** — Raw UTF-8 escape bytes as icons (`"\xE2\x86\x97 Live"`, `"\xE2\x86\xBA Reset"`) where the stroke-based `symbol` system exists and is used at `SettingsScreen.cppm:172`. *high*
- **Settings.cppm:362** — `needs_restart` latches permanently: `restart_pending_applied` is set in `apply_all` (373) and never cleared, not by `discard_all` and not by reverting the field, so the footer shows "Restart required" for the rest of the session. Derive it rather than storing it. *high*

### LOW findings — GUI layout, screens, persistence

- **Comments (banned outright)** — `Layout.cppm:194, 201, 208, 215, 222`; `Styles.cppm:41, 49, 55, 62, 68, 72, 76, 83, 88, 93, 99, 102, 119, 129, 135`. Where a comment carries real information (`Styles.cppm:99` "set by apply_scale, not a styled dimension", `:102` "auto-scaled via `[[= gse::scaled]]`"), the annotation already states it — `scale_factor` is the one field without `[[= gse::scaled]]`, which is the distinction the comment restates. *high*
- **Redundant namespace qualifiers** — `SettingsScreen.cppm` pervasively (37-38, 51-52, 56, 64, 70, 76, 81, 86-88, 92-97, 107-111, 120-121, 128-129, 132-135, 138-155, 159-177, 182-213, 222, 236-299, 302-334, 338-395, 398-424, 435-524), including `gse::rect_t<gse::vec2f>` where the module spells it `rectf`; `PopoutSystem.cppm:23, 29, 36-38, 49-50, 55, 67-70, 77, 97, 108, 150`; `Builder.cppm:76`; `Settings.cppm:262-263, 284-287, 290`. *high*
- **ContextActions.cppm:59** — Second `export namespace gse::gui` block reopened to interleave definitions; `append_context_action` (42-56) and both templates are defined inside their namespace blocks. *high*
- **Definitions inside the namespace/type** — `MenuStack.cppm:55-95` (`on_push`, `on_pop`, `captures_input`, `occludes`, `wants_chrome`, `caption_exclusion_range`, `dismissable`, `should_dismiss`, `title` in-class, while `draw_caption`/`body_rect`/`draw_backdrop` in the same struct are correctly split at 192-234); `Builder.cppm:48-55`; `ContextActions.cppm:42-56, 61-68, 71-84`. *high*
- **Empty bodies not collapsed** — `MenuStack.cppm:55-57, 58-60`; `Loading.cppm:11` (`state() {}` should be `= default`); `LoadingScreen.cppm:45-46` and `SettingsScreen.cppm:120-122` (constructors with an initializer list and empty body). *high*
- **LayoutOps.cppm:155-169** — Hand-written deleted copy/move where `non_copyable`/`non_movable` exist; `loading::state` (`Loading.cppm:9`) and `draw_context` (`Types.cppm:235`) show the intended form. *high*
- **Vertical alignment and mangled continuations** — `Builder.cppm:26, 38`; `Settings.cppm:68-75` (the `custom_draw_fn` typedef's `void (\n\t\t\t*\n\t)(` split is garbled) and `:450`; `SettingsScreen.cppm:488-492`; `LoadingScreen.cppm:109-111`; `Profiler.cppm:293-301`; `PopoutSystem.cppm:98-153` (whole `run` body indented one level too deep relative to `return {};` at 155). *high*
- **Settings.cppm:58-65** — Six adjacent function declarations packed with no separating blank lines, mixing one-line and wrapped forms. *high*
- **Settings.cppm:81** — `page_drawer` template is exported and unreferenced anywhere in the repository. (`draw_with` immediately above it *is* used correctly via `has_annotation`/`annotation_of` in `DrawStruct.cppm:37-38`.) Delete. *high*
- **Cursor.cppm:59** — `arrow_head_params` embeds mutable output (`std::vector<renderer::sprite_command>& commands`) in a parameter aggregate while its sibling `draw_line` takes it explicitly; and `render_to` computes `constexpr vec4f color` (81) for `draw_line` while `draw_arrow_head` hardcodes its own `constexpr vec4f white` (255), so arrow-head cursors ignore the caller's colour. *high*
- **Cursor.cppm:80** — `find("blank")` hashes a string literal on every cursor render; `draw_context` already caches this as `blank_texture` (`Types.cppm:241`). *medium*
- **LoadingScreen.cppm:40** — Diagnostic logging in a per-frame draw path: lines 49-52 and 73-81 log from `build()` four times a second for the whole boot, and `m_logged_first_build` is a member existing solely for a one-shot log line. `Loading.cppm:58` logs on every progress change *while holding the mutex*. Boot-path logging has previously cost real time in this engine. *high*
- **LoadingScreen.cppm:148** — `captures_input()` override is identical to `screen::captures_input`'s default (`MenuStack.cppm:61-63`). Delete. *high*
- **MenuStack.cppm:221** — Component decomposition where `{ vec3f(color), alpha }` is the module idiom (`Styles.cppm:184`, `SettingsScreen.cppm:148`). *high*
- **ContextActions.cppm:74** — `std::uint32_t i` compared against `std::size_t`, manual indexing where `std::views::enumerate` applies (used correctly at `Save.cpp:67`), and `.action_id = i` uses a positional index as identity, so reordering the annotated function pack silently remaps every menu entry. *high*
- **MenuStack.cppm:15** — `caption_exclusion { int y0; int y1; }` uses raw `int` where the module's vocabulary is `float`/`rectf`; if the Win32 caption hit-test is the forcing contract, the conversion belongs at that boundary. `nav::m_depth` (44) duplicates `m_stack.size()` and is resynchronised by hand at 338 and 359. *medium*
- **Settings.cppm:410** — `panel(..., std::string_view category_filter = "")` with no filter emits one `scroll_region` per category (437), each of which, with `scroll_region_info::size == {0,0}`, sizes itself from the current cursor to the menu bottom (`Widgets/Scroll.cppm:198-203`), stacking multiple full-height regions. The only caller always passes a filter, so this is latent. *medium*
- **Save.cpp:71** — `out.append(std::format("[menu {}]\n", index))` names sections positionally though the record's identity is `data.tag` (load keys on it at 101), so reordering menus rewrites every section header. *high*
- **Builder.cppm:71 / MenuStack.cppm:41** — `menu_content::build` and `nav::factory` place `std::function` in types other partitions load — the recorded engine hazard around type-erasure in cross-loaded BMIs. It evidently builds today; noted in case either partition later fails to load. *low*

### Clean — GUI layout, screens, persistence

- **Save.cppm** — clean apart from being the interface half of the `Save.cpp` merge findings. The `[[= gse::field_key<"...">]]` annotations are the correct idiom (read by `meta::find_key` via `find_class_template_annotation`, `Meta/Fields.cppm:62`) and the reflection-walked `read_fields`/`write_fields` plus `layout_store` usage is exactly what the guide asks for — no hand-rolled binary or ini IO anywhere in this file pair.
- **Loading.cppm** — only the synchronization finding above and the `state() {}` nit.

---

## 3. Text and scroll widgets

Files: `Widgets/TextArea.cppm`, `Widgets/TextInput.cppm`, `TextBuffer.cppm`, `Widgets/Slider.cppm`, `Widgets/Scroll.cppm`, `Widgets/Tree.cppm`.

### CRITICAL | Widgets/TextInput.cppm:344 | Text editing is byte-indexed while rendering is UTF-8-aware

```cpp
else if (state.caret > 0) {
    buffer.erase(state.caret - 1, 1);
    --state.caret;
```

Companion sites: `TextArea.cppm:689-706` (`pos_left`/`pos_right` step ±1 byte), `TextArea.cppm:909`, `:925`, `TextInput.cppm:325-329`, `TextArea.cppm:171-179` (`text_area_position_at` picks any byte index, including a continuation byte).

**Impact.** Backspace, Delete, Left/Right arrow, and mouse click over any multi-byte character split the UTF-8 sequence. The buffer then holds an invalid sequence, `font::width`/`caret_offsets` mis-decode it, and — because the Editor writes these buffers back to disk (`text_buffer::from_file` is the loader) — the corruption is persisted to the user's file. This is silent data loss, not a display glitch. It requires non-ASCII content to trigger, so it is latent in pure-ASCII source; a code editor opens arbitrary files.

**Mechanism.** `Font.cpp:196/233` decodes with `decode_utf8`, and `caret_offsets` deliberately assigns *every continuation byte* the trailing edge offset (`Font.cpp:248-250`) — the font layer knows about code points. The widgets index the same string by raw `char`: `buffer_position::column` (`TextBuffer.cppm:10`) and `text_input_state::caret` are byte counts advanced by literal `±1`, and `text_buffer::insert`/`erase` (`TextBuffer.cppm:87,119`) operate on byte columns with no boundary validation. `text_area_position_at`'s nearest-offset loop iterates over every byte, so a click can return a mid-code-point column directly.

**Resolution.** Move code-point stepping into `text_buffer`, which already owns positions: add `next_position`/`previous_position` and a `clamp` that snaps a column to a code-point boundary, and make `insert`/`erase`/`clamp` the only way column arithmetic happens. Both widgets then call those instead of `column ± 1`. `text_area_position_at` must snap its picked byte index to a boundary before returning. `text_input_state` should carry a `buffer_position`-equivalent rather than a bare `int`.

**Prevention.** It recurs in two widgets today and will recur in the next one. The guardrail is representational: make the caret a type that cannot name a non-boundary offset (a position produced only by `text_buffer`), so `caret - 1` stops compiling. Snapping inside `text_buffer::clamp` alone is insufficient, because the widgets construct positions arithmetically and only clamp afterwards.

*Confidence: high.* **[verified — byte-granular erase confirmed against source]**

### HIGH | Widgets/TextArea.cppm:274 | Hit-testing silently uses a different font than drawing

```cpp
	auto pick_position = [&](const vec2f mouse) -> buffer_position {
		return text_area_position_at(ctx, buffer, state, rect, show_line_numbers, indent_width, mouse);
	};
```

**Impact.** Whenever a caller supplies `params.font`, every mouse click, drag-selection, and right-click position resolves against `ctx.fonts.code` instead of the font the text was drawn with. The caret lands on the wrong column, selections cover the wrong range, and the error grows with column index. The symptom reads as a rendering offset, not an input bug.

**Mechanism.** `text_area_position_at`'s trailing `font` parameter defaults to `{}` (declaration `TextArea.cppm:106-107`), and `pick_position` — five lines below `const auto fnt = font.valid() ? font : ctx.fonts.code;` at line 201 — does not forward it. `text_area_position_at` then re-derives `line_h`, `gutter_width`, `text_x`, and `top_y` from the *default* font while the draw path derives them from `fnt`. The Editor call sites (`CodePanel.cppm:266`, `Terminal.cpp:506`) also omit it.

**Resolution.** Delete the defaulted parameter and make the font mandatory, so omission is a compile error rather than a wrong answer. Better: remove the second derivation entirely — see the next finding.

*Confidence: high.* **[verified — seven-argument call confirmed against the eight-parameter declaration]**

### HIGH | Widgets/TextArea.cppm:136 and :248 | Column→x mapping implemented twice, from the same inputs

```cpp
auto gse::gui::draw::text_area_position_at(...) -> buffer_position {
    ...
    std::vector<std::size_t> col_to_expanded(line.size() + 1);
```
versus
```cpp
auto line_column_x = [&](const std::string_view line) -> std::vector<float> {
    std::string expanded;
    std::vector<std::size_t> col_to_expanded(line.size() + 1);
```

**Impact.** Tab expansion, `caret_offsets`, gutter width, `left_inset`, `text_x`, and `top_y` are each computed by two independent bodies (136-184 and 218-272). Only one will be updated when tab or gutter rules change; the previous finding is the first instance of that already happening. The duplication is also why the whole layout is recomputed per hit-test.

**Resolution.** One `text_area_layout` value (font, `line_h`, `left_inset`, `text_x`, `top_y`, tab width) built once from `params`, with `column_x(line, col)` and `position_at(mouse)` as its two members. `text_area_position_at` becomes a thin call on it; `text_area_in_rect` builds it once and reuses it for selection, underlines, caret, and caret-follow scrolling.

**Prevention.** Once both directions are members of the layout object, there is no second place to encode the tab rule.

*Confidence: high.*

### HIGH | Widgets/TextArea.cppm:111 | The text area never claims `active`, so a selection drag has no capture ownership

```cpp
auto gse::gui::text_area::draw(const draw_context& ctx, const params& p, id& hot, id& active, id& focus) -> void {
    (void)active;
```

with the drag loop at 392-415:
```cpp
if (state.selecting) {
    if (ctx.mouse_held()) {
```

**Impact.** While the user drags a text selection, no widget owns `active_widget_id`. Any other control the pointer passes over is free to become hot/active, and the GUI's own drag arbitration (menu move/resize in `Gui.cpp`) sees the button as unowned. Conversely nothing else can tell the text area is mid-drag.

**Mechanism.** `text_area_in_rect` takes only `hot_widget_id` and `focus_widget_id` (declaration 90-96); `active` is discarded at the wrapper. Drag continuation is re-derived from raw `ctx.mouse_held()` plus a private `state.selecting` bool — the "stateful control observing an unscoped release outside the established behavior" case. The sanctioned path exists and is used by `Button.cppm:83`: `interaction::press_in_rect`, backed by `grab_active`/`release_active`, which clear ownership on release regardless of pointer position.

**Resolution.** Thread `active_widget_id` through `text_area_in_rect` and drive the drag from `press_in_rect` (press to begin, `active == widget` to continue, `release_active` to end). `state.selecting` then becomes derived rather than stored.

**Prevention.** Treat any widget that takes `hot`/`focus` but not `active` as a defect — a widget with drag state cannot be correct without it.

*Confidence: high.*

### HIGH | Widgets/TextInput.cppm:396 | Drag-selection is gated on hover, so it freezes when the pointer leaves the box

```cpp
if (hovered && ctx.mouse_held()) {
    const float x_local = ctx.mouse_position().x() - box_rect.left();
    const int current = std::clamp(pick_index_from_x(x_local), 0, static_cast<int>(buffer.size()));
```

**Impact.** Press inside the field and drag past its left or right edge — the normal way to select to the start/end of a value — and the selection stops extending at the boundary. Re-entering resumes it. There is also no press-ownership flag, so the branch is entered by hover alone whenever the field is focused and the button is down.

**Mechanism.** `hovered` is `ctx.hovers(box_rect)` (125), a per-frame containment test standing in for capture. `text_input_state` has `select_origin` and `select_granularity` but no "I own this drag" state. `TextArea.cppm:392` solves the same problem differently, which is why the two widgets behave differently for one gesture.

**Resolution.** Take `active_widget_id` and use `press_in_rect`; extend the selection while `active == widget`, independent of hover. This also fixes the unhandled `select_granularity == 2` case (triple-click then drag currently does nothing, 399-413).

*Confidence: high.*

### HIGH | Widgets/Tree.cppm:94 | Tree expansion lives in a mutable module-global; caller-supplied `open_keys` can never be collapsed

```cpp
expand_state global_expand_state;
```
```cpp
std::unordered_set<std::uint64_t>& open_set = global_expand_state.open[tree_scope];
for (const std::uint64_t key : opt.open_keys) {
    open_set.insert(key);
}
```

**Impact.** Three problems from one representation. (1) Expansion state is owned by the widget, not the caller, so it cannot be saved, restored, or reasoned about by the owning system, and two trees sharing an `ids::current_seed()` share expansion. (2) `opt.open_keys` is re-inserted every frame, so any node the caller lists is force-reopened immediately after the user collapses it — the chevron appears to do nothing. (3) The map is never pruned; a tree over dynamic content grows a per-key entry forever.

**Mechanism.** `open_keys` (caller-owned) and `global_expand_state.open[scope]` (widget-owned) are two sources of truth for one fact, merged one-way every frame. Also violates the style guide's inline-variable rule: a non-`constexpr`, non-`inline` namespace-scope variable in a module interface unit.

**Resolution.** Give `tree` an explicit caller-owned state parameter (a `tree_state` holding the open-key set, alongside `tree_selection`), the way `text_area` takes `text_area_state`. `open_keys` then disappears. That removes the global, the leak, the force-reopen, and the `inline` question together.

**Prevention.** The module already establishes "caller owns the state struct" (`text_area_state`, `text_input_state`, `tab_strip_state`); a widget file declaring a namespace-scope mutable variable is the greppable tell that the pattern was skipped.

*Confidence: high.*

### HIGH | Widgets/Slider.cppm:216 | Quantity sliders write through a hand-rolled `reinterpret_cast` and label a value they never convert

```cpp
using underlying = internal::vec_storage_type_t<T>;
auto& value_u = *reinterpret_cast<underlying*>(&value);
```
with, at 165-171 and 271-276:
```cpp
constexpr std::string_view unit_name = Unit.unit_name;
...
name_with_unit += " (";
name_with_unit += unit_name;
...
std::format_to(std::back_inserter(value_str), "{:.2f}", value_u);
```

**Impact.** Two defects. (a) The strong type is bypassed: a writable `reinterpret_cast` alias is created over a `quantity` object and written through — a strict-aliasing violation and, more importantly, the point where all dimensional checking stops; everything from 224 to 254 is unchecked `float`/integer arithmetic. (b) `to_storage` yields the value in the quantity's *canonical* storage unit, but the label is built from the `Unit` template parameter, so any instantiation where `Unit != default_unit` prints a number in one unit under a heading naming another. Every call site currently passes `typename F::default_unit{}` (`DrawStruct.cppm:102`), so the bug is latent — the machinery exists solely to be wrong when used.

**Mechanism.** `internal::to_storage` is the sanctioned *reader* (`Vector.cppm:31`); there is no sanctioned writer, and rather than adding one the widget spelled its own non-const cast. The formatter rule is then broken exactly as the style guide names it: the value is printed as a bare number and the unit appended to a label string.

**Resolution.** Do the interaction math in the quantity domain — `value = min + (max - min) * ratio` is well-defined on quantities, and `(value - min) / (max - min)` is dimensionless by construction, so `fill_ratio` needs no conversion. Format the quantity itself so the spec owns the conversion and the suffix, and drop `name_with_unit` and the `Unit` parameter. If a unit-agnostic scalar is still needed for the integral case, add a sanctioned `from_storage` next to `to_storage`.

**Prevention.** `gse.math` should expose the writable counterpart to `to_storage` (or none at all), so `reinterpret_cast` over a quantity in feature code has no excuse and is greppable.

*Confidence: high.*

### HIGH | Widgets/Tree.cppm:208 | A `std::format` heap allocation per node per frame to build a row id

```cpp
const id row_widget_id = ids::make(std::format("tree_row##{}", key));
```

**Impact.** Formats and allocates one string per visited node every frame, then hashes it. `tree_node` recurses through the entire *open* subtree regardless of visibility (339-343), so a file tree or scene hierarchy with a few hundred expanded nodes pays several hundred allocations plus formatting per frame — for a value that is a pure function of a `std::uint64_t` already in hand.

**Mechanism.** `ids::make(string_view)` hashes the text (`IDs.cppm:58`); `ids::make_from_key(std::uint64_t)` (62) exists precisely to skip the string, and `Slider.cppm:330` already uses it.

**Resolution.** `ids::make_from_key(hash_combine(stable_id("tree_row"), key))`. Separately, cull the recursion so an unopened viewport does not cost per-node work.

**Prevention.** `ids::make(std::format` is always wrong — the inputs to the format are always available to `hash_combine`. Greppable.

*Confidence: high.*

### HIGH | Widgets/TextArea.cppm:112 | `value_or` on a side-effecting `next_row` consumes a layout row even when a rect was supplied

```cpp
const rectf rect = p.rect.value_or(ctx.next_row(p.font.valid() ? p.font : ctx.fonts.code, 8.f));
```

**Impact.** `std::optional::value_or` evaluates its argument unconditionally. `draw_context::next_row` mutates `layout_cursor` (`Types.cpp:307`: `layout_cursor.y() -= row_height + style.padding + style.item_spacing;`). So drawing a `text_area` with an explicit `rect` still advances the enclosing layout by eight text rows, leaving a large gap before the next widget and mis-measuring content extent for any enclosing `scroll_region`.

**Resolution.** `const rectf rect = p.rect ? *p.rect : ctx.next_row(...);`

**Prevention.** `value_or(` with a function-call argument is a cheap, high-signal grep; the same shape at `Tree.cppm:201` is harmless only because `context_rect` is a pure value.

*Confidence: high.* **[verified]**

### HIGH | TextArea + TextInput | Two independent implementations of one editing behavior set, already divergent

| Behavior | TextArea | TextInput |
|---|---|---|
| Shift+arrow extends selection | no (`move_to`, 760-765, always collapses) | yes (`move_caret`, 241-251) |
| Word classification | 3-class `classify_char` incl. punctuation (278) | `std::isalnum` only, and a *different* rule again in `word_left`/`word_right` (253-273) |
| Key repeat | none (`rpt_active`/`rpt_next` declared at 45-46, never read) | hand-rolled 400 ms/33 ms timer (369-394) |
| Undo/redo | yes | none |
| Context menu | yes | none |
| Blink interval | `params.blink_interval` | hard-coded `milliseconds(500)` (430) |
| Drag capture | `state.selecting` + unscoped `mouse_held` | `hovered && mouse_held` |

**Impact.** The same key does different things in two fields in one application, and every future fix must be made twice. Shift+arrow selection being absent from the code editor is a user-visible gap, not just a maintenance cost.

**Mechanism.** `text_buffer` is the natural owner — a single-line input is a one-line buffer — but `TextInput` operates on a bare `std::string` with `int` offsets while `TextArea` operates on `text_buffer` with `buffer_position`, so nothing is shared.

**Resolution.** Move the caret/selection/word/clipboard/undo transitions onto `text_buffer` (or a `text_edit_state` beside it) operating on `buffer_position`, and let `text_input_state` hold that state over a one-line buffer. Each widget keeps its own presentation and layout; neither keeps its own editing rules. That single change also carries the UTF-8 fix, the shift+arrow gap, and the capture fix to both widgets at once.

*Confidence: high.*

### MEDIUM findings — text and scroll widgets

- **TextArea.cppm:318** — The width signature folds only line *lengths* and rescans the whole buffer every frame (O(total lines) for a 20k-line file, before anything is drawn). Because only lengths are folded, any same-length edit (overtype, tab↔space swap, `i`→`W`) leaves `state.widest_line_px` stale, so `content_width` (353) and the horizontal scroll extent are wrong. Give `text_buffer` a revision counter bumped by `insert`/`erase` and key the cached width off `{revision, tab_width, scale}`. *high*
- **TextArea.cppm:987, :1103, :1122** — `line_column_x` allocates three heap containers per call (`expanded`, `col_to_expanded`, plus `caret_offsets`' returned vector, then a fourth for the result), called once per selected visible line, once per underlined line, once for the caret, and again in the caret-follow branch (1149). Adjacent: `std::string(line_digits, '0')` at 143 and 219, `std::to_string` at 142/218 and per visible line at 1007, `fnt_view->width(" ", scale)` per selected line at 990. Hold the scratch buffers in the layout object and add a `caret_offsets` overload filling a caller-provided vector. *high*
- **TextArea.cppm:1047** — Highlighted runs are positioned by summing per-run widths (`run_x += fnt_view->width(seg, scale)`) while the caret, selection and underlines advance by `caret_offsets(whole_expanded_line)`. `font::width` applies kerning only *within* its argument (`Font.cpp:207-213`), so every syntax-highlight boundary drops one kerning pair and the drawn text separates from the selection rectangle behind it. `disp += seg.size()` also advances the *display column* by a **byte** count, so tab stops after any non-ASCII glyph are placed wrong. Only monospace fonts with no kerning hide it. *high*
- **TextArea.cppm:216** — The scrollbar gutter is excluded from the hit rect whenever content *is* scrollable, but `scroll_config::auto_hide_scrollbar` defaults to `true` (`Types.cppm:72`) so `scroll_area` only shows the bar on hover/held/in-region (`Scroll.cppm:305-306`). The result is an 8-pixel strip down the right edge of every scrollable text area that looks like text, is text, and cannot be clicked. Have `scroll_area` report the rect it actually reserved. *high*
- **Scroll.cppm:257 and :132** — `.mouse_pressed = ctx.mouse_pressed() && ctx.input_available()` is the banned spelling and is not equivalent to `mouse_pressed_for`: it skips the clip-stack test and, critically, `is_press_consumed`. `axis.hovered = thumb_rect.contains(input.mouse)` and `in_region = visible_rect.contains(ctx.mouse_position())` (280) likewise. `scroll_axis_advance` consumes *after* acting (261-263) but never asks whether the press was already taken. Collision is avoided today by layout convention — `scroll_handle` shrinks `current_menu->rect` and `TextArea` shrinks its own hit rect — not by arbitration. Compute the flags with `ctx.mouse_pressed_for(track_rect)` / `ctx.hovers(...)` inside `scroll_axis_advance`. *high*
- **Scroll.cppm:301** — `set_style(cursor::style::omni_move)` — the widget performs delivery as well as decision, and `scroll_area` returns only `vec2f` so the caller cannot override or compose it. Return the desired shape alongside the offsets. *high*
- **Scroll.cppm:219** — `(config.smooth_scrolling ? axis.target : axis.offset) -= wheel_amount * config.scroll_speed;` — with smoothing off, a wheel event moves `offset` and leaves `target` behind, and nothing reconciles them; the 0.5px snap at 229 then silently reverts sub-threshold wheel deltas, and enabling smoothing later snaps the view back to a long-stale target. Make `target` authoritative and `offset` derived. `scroll_axis::velocity` is dead across the whole codebase. *medium — the desync is certain from the code; whether any caller runs with `smooth_scrolling == false` was not verified.*
- **Scroll.cppm:210** — `ctx.widget_scrolls[key]` keyed by a raw `uint64` (`hash_combine(ids::current_seed(), stable_id(info.id))`) with no eviction; a region whose `info.id` varies with content leaves a permanent entry per key. Key by `ids::make(info.id)` in an `id_mapped_collection` with a per-frame touch flag. *high*
- **Tree.cppm:202** — Three different predicates for "is this row on screen": drawing uses `row_visible` (a ±`row_height` slop test against `effective_clip`), hover uses `mouse_in_clip && ctx.hovers(row_rect)`, press uses `ctx.mouse_pressed_for(row_rect)` with *no* `mouse_in_clip` term. When `clip_stack` is empty, `effective_clip` falls back to `context_rect`, which `hovers`/`mouse_pressed_for` know nothing about — so a row can be pressed without ever being hovered, taking `active` while `released_by_me` stays false. Delete `mouse_in_clip`; derive `row_visible` from `ctx.current_clip()`. *high*
- **Tree.cppm:41** — `tree_ops` is eight `std::function` members in an exported partition type, instantiated across the Editor — the recorded engine hazard around `std::function` in a partition another loads, plus eight indirect calls per node per frame. Replace with POD function pointers taking an explicit context parameter, or make `tree` a template over an ops concept. *medium — the linkage hazard is a known engine constraint rather than something verified for this partition; the cost and idiom mismatch are certain.*
- **TextInput.cppm:463** — The selection highlight is queued with no `clip_rect` while the text it highlights is clipped (476). `ax`/`bx` (454-455) are full un-truncated widths minus `scroll_x`, so selecting a string longer than the field paints a bare coloured bar over adjacent widgets. The caret sprite (486-490) has the same omission. *high*
- **TextInput.cppm:416, :454, :480** — `buffer.substr(0, state.caret)` + `width()` recomputed three to four times per frame, each allocating a prefix copy and re-walking with kerning lookups; `pick_index_from_x` (142) additionally allocates a full `caret_offsets` vector per call including once per drag frame. Compute `caret_offsets` once per frame into a reused buffer and index it. *high*
- **TextInput.cppm:134** — `else if (ctx.mouse_pressed() && focus_widget_id == widget_id && ctx.input_available())` — the banned spelling verbatim, bypassing press consumption; and because `TextArea` has no equivalent, focus semantics differ between the two text widgets. Let the frame that owns `focus_widget_id` resolve it. *high*
- **TextArea.cppm:465** — `state.undo_stack.push_back({ buffer.lines, state.caret, state.anchor })` (also 799, 812) copies the entire `std::vector<std::string>` per undo group — 20k string allocations per edit group on a large file — and neither stack is ever capped. Since `text_area_state` is held per open document (`Workspace/Documents.cppm:22`), this scales with open files. Record the edit (range + replaced text + resulting caret) rather than the document. *high*
- **TextBuffer.cppm:37** — `lines` is public and mutated directly at `TextArea.cppm:580, 615, 632, 642, 802, 815`, bypassing the `insert`/`erase`/`clamp` invariants; `if (buffer.lines.empty()) buffer.lines.emplace_back();` is re-implemented at 576 and 600. Any invariant added later — revision counter, dirty flag, UTF-8 validation, line-ending normalisation — is defeated by six existing call sites. Make `lines` private with a `std::span<const std::string>` accessor and add the operations the widget needs. *high*
- **TextArea.cppm:440** — `.action_id = static_cast<std::uint32_t>(text_edit_action::copy)` is blind-cast back at `EditorApp.cppm:545`, so any other context menu sharing the same `tag` writes an arbitrary integer into a `text_edit_action`. `pending_action` on `text_area_state` is also a channel implemented as a mutable field. Make `menu_item::action_id` an `id`. *high*
- **Slider.cppm:237** — Drawing a slider rewrites the caller's value every frame: a value legitimately outside the presentation range is silently destroyed on the first frame the panel is visible, with no interaction. Clamp only inside the active-drag branch; clamp `fill_ratio` for display. *high*
- **Slider.cppm:228** — `const float raw = static_cast<float>(min_u) + ratio * static_cast<float>(max_u - min_u);` quantises `double`-backed quantities to `float` precision and loses the low bits of 64-bit integral sliders (`max_u - min_u` can also overflow before the cast). *high*
- **TextArea.cppm:760, :39** — `int last_edit_kind` takes 0/1/2/3 at 656, 826, 857, 871, 880, 904 with no name for any of them; `int select_granularity` takes 0/1/2 compared numerically at 315 and `TextInput.cppm:399,411`. Every value outside those sets is representable and silently means "word". Two small enums. *high*
- **TextArea.cppm:112 / Tree.cppm:83** — Widget-dispatch paths drop identity and results: every `text_area` drawn through the widget path shares the constant id `ids::make_from_key(stable_id("##TextArea"))`, so two in one scope fight over `hot`/`focus`; `text_area::result` is `void` so the `modified` flag is discarded; `tree::draw` discards `hot` entirely, so a tree row never becomes hot. *high*
- **TextInput.cppm:447, :418-422** — `constexpr float text_padding = 5.f` and the caret-follow `- 5.f` margins do not scale, while `style` marks every dimension `[[= gse::scaled]]` and carries a runtime `scale_factor`. `milliseconds(500)` (430) and `milliseconds(400)`/`milliseconds(33)` (372/389) are unconfigurable where `text_area::params` exposes `blink_interval`. *high*
- **TextArea.cppm:201** — No font-validity guard: `resource::handle::resolve()` returns a `shared_ptr<const T>` that is null for an unresolved handle, and `fnt_view->line_height(...)` at 213 dereferences it. `Button.cppm:59` and `Tree.cppm:126` both guard. Separately, a font that resolves with no glyphs gives `line_h == 0`, and `state.scroll.y.offset / line_h` (974) and `(top_y - mouse.y()) / line_h` (150) then produce NaN/inf, which is UB when cast to `int`. *medium — the null-on-unresolved behaviour is inferred from the return type and peer guarding rather than read from `resolve()`'s body.*

### LOW / style findings — text and scroll widgets

**TextInput.cppm** — 23, 38, 61: three `export namespace` blocks, reopening `gse::gui` at 61 after `gse::gui::draw` at 38. 70-72: `text_input::draw` defined inline in-class. 71: `std::string(p.name)` allocates per frame for a `const std::string&` parameter. 157, 255, 258, 266, 269: `std::isalnum` is locale-dependent and gives `word_left`/`word_right` a different word rule than `classify_char`/`word_bounds` in the same file. 422: `if (constexpr float inner_l = 5.f; ...)` — a `constexpr` init-statement as a named constant. *high*

**Slider.cppm** — 54-59, 72-77: template argument lists broken mid-argument. 93-95, 107-109, 121-123: three `draw` bodies defined inline in-class. 94, 108, 122: `std::string(p.name)` per frame ×3. 269: `thread_local std::string value_str;` inside a template function body — a hidden mutable global per instantiation, where `ctx.intern` (`Types.cpp:49`) already owns text pooling. 220: `hot_widget_id = widget_id;` written directly instead of via `interaction::mark_hot`, which the file already uses at 353. *high*

**Tree.cppm** — 83-85: `tree::draw` inline in-class. 94: namespace-scope mutable variable (see HIGH). 32-33: `std::uint64_t reveal_key = 0` sentinel and `float* reveal_offset = nullptr` — mutable output inside an options aggregate. 36-38: `tree_selection` stores `std::unordered_set<std::uint64_t>` plus `std::uint64_t activated = 0` sentinel; both should be `gse::id`. 136-137: `std::numeric_limits<float>::lowest()` as a "no row" sentinel where `std::optional<float>` says it. 301: `if (released_by_me && hovered)` — `released_by_me` already includes `hovered` (`Interaction.cppm:155`). 256: the chevron is drawn from the pre-toggle `is_open` while the toggle runs at 307-315, so on the click frame the arrow direction disagrees with the expanded children. *high*

**TextArea.cppm** — 203: `(void)spans;` is stale (`spans` is used at 1018-1091). 45-46: `bool rpt_active` / `time rpt_next` never read in this file — dead state copied from `text_input_state`. 465, 799, 812: positional aggregate init of `text_edit_snapshot`. 318: hand-rolled FNV constants (`14695981039346656037ull` / `1099511628211ull`) where the module already has `hash_combine`/`stable_id`. 111: `(void)active;` on a named parameter — here the right fix is to *use* it. *high*

**TextBuffer.cppm** — 43-49: `line_count()` and `line()` defined inline in-class. 47-49: `line()` silently returns an empty view for an out-of-range index, masking index bugs at every call site, and takes `std::size_t` where `buffer_position::line` is `std::uint32_t`, forcing a narrowing cast at every caller. 87: `insert(buffer_position, std::string_view)` will invalidate `text` if a caller ever passes a view into `lines`; not currently reachable, but nothing in the signature prevents it. *high*

**Scroll.cppm** — 75: `system_clock::dt<time>().as<seconds>()` is a `.as<>()` exit with no external contract; `speed * (dt / seconds(1.f))` keeps the ratio inside the type system. 213: five-argument positional `scroll_handle{...}` of same-shaped values. 220: assignment through a conditional lvalue. 106: `update_scroll_bar` writes `axis.content = input.content_extent` even though `scroll_area:268` and `scroll_axis_advance:217` already treat `axis.content` as authoritative. *high*

**Out-of-scope observations** — `Styles.cppm:99` contains a comment (banned outright); `scroll_axis::velocity` (`Types.cppm:53`) is dead across the entire repository.

### Cross-cutting themes — text and scroll widgets

Four root causes account for most of the above, and fixing them collapses the list:

1. **No shared text-editing authority** — produces the UTF-8 corruption (twice), the shift+arrow gap, the divergent word rules, the two drag-capture bugs, and the duplicated clipboard/selection code.
2. **Layout computed ad hoc instead of once** — the wrong-font bug, the kerning drift, the per-frame allocations, and the stale `widest_line_px` all trace here.
3. **Interaction policy re-derived below `draw_context`** — `mouse_pressed() && input_available()`, `rect.contains(mouse)`, and hover-gated drag in `Scroll`, `Tree`, and `TextInput`, with `press_in_rect` bypassed in four widgets.
4. **Widget-owned state that should be caller-owned** — `global_expand_state`, the never-evicted `widget_scrolls` map, and `pending_action` as a mutable mailbox.

---

## 4. Widget set

Files: `Widgets/TabStrip.cppm`, `Dropdown.cppm`, `Value.cppm`, `GraphCanvas.cppm`, `Selectable.cppm`, `ColumnHeader.cppm`, `Section.cppm`, `Button.cppm`, `Toggle.cppm`, `NavItem.cppm`, `PanelBackdrop.cppm`, `Text.cppm`, `Marquee.cppm`, `Separator.cppm`.

### HIGH | Widgets/Section.cppm:26 | Section callbacks allocate on the heap every frame and run mid-draw

```cpp
			std::function<void()> on_action = {};
			std::string_view secondary_action_icon = {};
			std::function<void()> on_secondary_action = {};
```
```cpp
		if (ctx.mouse_pressed_for(action_rect)) {
			on_click();
		}
```

**Impact.** Two `std::function` objects constructed per section per frame. The live call site (`Settings.cppm:445-452`) builds closures capturing `&channels, &save_reg, &ps, cat` — well past the small-object buffer — so this is a guaranteed heap allocation/free pair per settings category per frame in the GUI draw path. Separately, `on_click()` executes arbitrary caller code (here a `channels.push`) *while the widget tree is being built*, which is the re-entrancy shape the producer/consumer model exists to prevent.

**Mechanism.** `section` is the only widget in the set whose `result` is `void`; because it cannot report an action, the params aggregate carries the behavior. `builder::draw(typename W::params p)` takes params by value, so the aggregate is materialized fresh at every call.

**Resolution.** Give `section` a real result — `struct result { bool action = false; bool secondary_action = false; };` — drop both `std::function` members, and let the caller push its channel message after the draw returns, as `nav_item`, `selectable`, and `toggle` already do.

**Prevention.** State the rule at the `parameterized_widget` concept layer: widget `params` are plain data; a widget that needs to report something declares a `result`. `std::function` in params also collides with the engine's "no `std::function` in a partition another loads" hazard.

*Confidence: high.*

### HIGH | Widgets/Selectable.cppm:87 | Press/hot/active ladder re-derived in six widgets; only Button uses the shared behavior

See [C3](#c3-only-one-widget-uses-the-shared-press-behavior) for the full table and resolution. *Confidence: high.*

### HIGH | Widgets/Dropdown.cppm:490 | Popup dismissal reads raw mouse state and a stale open flag; capture outlives the popup by one frame

```cpp
		const bool still_open = state.open_dropdown_id == dropdown_id;
		const bool raw_press = ctx.mouse_pressed();
		if (still_open && !header_rect.contains(mouse_pos) && !list_rect.contains(mouse_pos) && !state.scroll.y.held && raw_press) {
```

**Impact.** Three defects in one construct. (1) `ctx.mouse_pressed()` bypasses `input_available()`, the clip stack, layer arbitration and press consumption — a press consumed by an unrelated modal above the dropdown still dismisses it. (2) The whole dismissal path sits inside `if (is_open && count > 0)` (382), so with `count == 0` there is **no outside-click dismissal at all**; the only way out is re-clicking the header. (3) `is_open` is captured at 305, *before* the header toggle at 331-340 mutates `state.open_dropdown_id`. On the frame the dropdown closes, 382 still sees `is_open == true`, so the list is drawn one extra frame *and* `ctx.register_hit_region(render_layer::modal, list_rect_early)` (322) already fired. Because `topmost_at` reads `m_previous_regions` (`InputLayers.cppm:163`), the region below the dismissed list is input-dead for the frame *after* it closes — the first click after closing a dropdown is swallowed, and it reads as an input bug in whatever was underneath.

**Resolution.** Read `state.open_dropdown_id == dropdown_id` at each point of use (or restructure so the toggle happens first), hoist the dismissal check out of the `count > 0` guard, and make the dismissal owned by the popup layer so it consumes the press it acts on. Add an `Escape` path via `ctx.key_pressed_for(key::escape)`.

**Prevention.** Consolidate open/dismiss/capture into one `popup_scope` helper in the interaction partition owning *register hit region → draw → dismiss → release capture* as a single lifetime, so the popup's live rect and its hit region cannot come from different frames. The same triple will appear for context menus and any future popover.

*Confidence: high.*

### HIGH | Widgets/ColumnHeader.cppm:98 | Column resize ownership hand-rolled with an int sentinel and raw `mouse_held()`

```cpp
	if (!held) {
		state.resizing = -1;
	}
```

**Impact.** `column_state::resizing` is a parallel, private notion of "who owns the mouse" that `hot_widget_id`/`active_widget_id` never learn about. While a column is being dragged, every other widget still believes nothing is active — so a drag that leaves the header and passes over a button leaves that button hot and, on release, activated. The unscoped release is observed as `!ctx.mouse_held()` rather than through `interaction::release_active`, the one sanctioned way to see a release outside the originating rect.

**Mechanism.** `interaction::grab_active`/`release_active` (`Interaction.cppm:126-138`) exist for exactly this and are exported, but `column_header` takes no `id&` parameters at all, so it invented a local ownership. The `-1` sentinel is the second half of the same shortcut: an index that must be checked before every use (116, 120, 141) rather than an `id` that answers `exists()`.

**Resolution.** Give `column_header` the `id& hot, id& active` parameters every other widget takes, replace `int resizing` with `id resizing_column` (or drop the field and read `active`), grab via `grab_active(active, grip_id, ctx.mouse_pressed_for(grip))` and release via `release_active(active, grip_id, ctx.mouse_released())`.

**Prevention.** `TabStrip.cppm:498-517` has the identical hand-rolled drag. Add a `drag_in_rect` behavior alongside `press_in_rect` returning `{ grabbed, dragging, released }` that owns the grab/unscoped-release pair; then neither widget needs a private ownership field.

*Confidence: high.*

### HIGH | Widgets/GraphCanvas.cppm:163 | Background click fires on any release over the canvas, including releases that began elsewhere

```cpp
	if (released && ctx.hovers(p.area) && !out.clicked) {
		out.background_clicked = true;
	}
```

**Impact.** `released` is `ctx.mouse_released()` (109) — the raw, unscoped device transition. Any drag that starts on another widget (a scrollbar thumb, a slider, a menu title bar) and ends with the cursor over the graph canvas reports `background_clicked`. In the live caller (`SystemGraph.cpp:822`) that clears the node selection, so releasing a drag over the canvas silently deselects.

**Mechanism.** `draw_context` ships `mouse_released_for(rect)` (`Types.cppm:304-308`, implemented at `Types.cpp:158-179`) which checks containment, the clip stack, `input_available()` and release consumption, and consumes on success. The widget bypasses it. The `released` local also has to exist for the `activate_on_click` calls at 120 — which is the *sanctioned* use of a raw release — which is what made reusing it look natural.

**Resolution.** `if (ctx.mouse_released_for(p.area) && !out.clicked)`. That restores clipping, layering and consumption and removes the need for the `ctx.hovers` term.

**Prevention.** State the rule: a raw `mouse_released()` may only be handed to `interaction::release_active`, never used as a hit test.

*Confidence: high.* **[verified — raw release at line 109, dual use at 120 and 163 confirmed]**

### HIGH | Widgets/TabStrip.cppm:434 | Tab strip reimplements `scroll_area`

```cpp
		else if (std::abs(wheel.x()) > 0.001f || (shift && std::abs(wheel.y()) > 0.001f)) {
			state.scroll.offset -= (wheel.x() + wheel.y()) * 80.f * sty.scale_factor;
			state.scroll.target = state.scroll.offset;
			ctx.consume_scroll();
```

**Impact.** Lines 325-331 (vertical) and 422-439 (horizontal) read `ctx.scroll_delta()` raw, test `ctx.hovers(area)` and `ctx.is_scroll_consumed()` by hand, and call `ctx.consume_scroll()` — reproducing `draw_context::scroll_delta_for(rect)` (`Types.cpp:229-248`), which does containment + availability + consumed-check + consume in one owned derivation. 422-439 additionally duplicates the shift-to-horizontal redirect that `scroll_area` owns (`Scroll.cppm:277-278`), and 530-558 hand-builds a track rect, calls `update_scroll_bar`, forwards `used_press`, and re-draws track and thumb with locally invented colors — all of which `scroll_axis_advance` + `draw_scroll_bar` (`Scroll.cppm:216-265`) already do, **including the `register_resize_block` call the tab strip omits**. So a window-edge drag steals the hand-rolled bar — exactly the class of bug the resize-vs-scrollbar work fixed — and its thumb colors ignore `axis.held`.

**Mechanism.** `tab_strip_state` stores a bare `scroll_axis` rather than a `scroll_state`, which put `scroll_area` (whose signature takes `scroll_state&`) out of reach.

**Resolution.** Change `tab_strip_state::scroll` to `scroll_state`, then replace 325-331, 422-439 and 530-558 with one `scroll_area(ctx, state.scroll, area, { total, area.height() }, cfg)`. The wrap-mode row-count wheel handling (425-433) is genuinely feature-specific and stays, gated on `ctx.scroll_delta_for(area)`.

**Prevention.** Stop exporting the raw `scroll_delta()` from `draw_context`, leaving only `scroll_delta_for(rect)`, so a widget cannot read an unscoped wheel at all.

*Confidence: high.*

### HIGH | Widgets/Value.cppm:146 | Quantity display converts by hand and puts the unit in the format literal

```cpp
		{ std::format(
			"{:.2f} {}",
			gse::internal::value_in<decltype(Unit)>(value),
			std::string_view(Unit.unit_name)
		) }
```

**Impact.** The exact shape the guide declares wrong on sight. `std::formatter<gse::internal::quantity<...>>` (`Math/Units/Quantity.cppm:682-781`) already converts *and* appends the unit name from a `{[value-spec]:[unit]}` spec, and its default path (726-743) produces byte-identical output to this hand-rolled version for the default unit. Line 158 repeats the conversion for the vector overload without even the unit label, so a `vec3<length>` renders three bare numbers with no unit anywhere.

Compounding it: `gse::internal::value_in` is an implementation detail. The GUI reaches past the public `gse::` surface (`Engine/Import/Math.cppm:30-36` re-exports `is_arithmetic`/`is_quantity` but deliberately not `value_in`) to get a raw scalar, with no external contract behind the exit — the destination is a `std::format` call.

**Mechanism.** The unit is a template NTTP (`auto Unit = typename T::default_unit{}`), so the author could not splice it into a compile-time format spec and reached for the escape hatch instead of extending the spec's reach.

**Resolution.** Carry the unit as a `fixed_string` NTTP and build the spec at compile time, or accept the quantity and format with `"{:.2f}"` so the formatter's default path owns the conversion — never `value_in` at a display site.

**Note.** The originating pass recommended deleting `Value.cppm` as dead. **That is wrong** — see [Corrections](#corrections-and-rejected-findings). The file has live callers; only the *quantity* specializations appear unexercised.

**Prevention.** A lint on `gse::internal::value_in` being named outside `Math/`: it is the single symbol behind essentially every "converted by hand to print it" defect, and there is no legitimate GUI-layer use of it.

*Confidence: high.*

### MEDIUM findings — widget set

- **Toggle.cppm:87** — `const float track_width = 40.f * (ctx.style.font_size / 16.f);` divides a *scaled* value (`font_size` is `[[= gse::scaled]]`) by an *unscaled* literal that re-encodes the default `font_size`. Under `theme::high_contrast` (`Styles.cppm:475`, `font_size = 18.f`) the toggle is 12.5% larger than its neighbours at scale 1.0. Use `ctx.style.scale_factor`, or promote the three dimensions into `style` with `[[= gse::scaled]]`. *high*
- **Toggle.cppm:66** — Commits on press, takes `active` and never writes it, and hand-inlines `mark_hot` at 61-63; the only interactive widget in the set that does not import `:interaction`. *high*
- **Separator.cppm:36, :44** — Hardcodes `line_height = 1.f` and uses `color_border`, ignoring `style::separator_thickness` (`[[= gse::scaled]]`, `Styles.cppm:127`) and `color_separator` (`Styles.cppm:47`) — the two tokens that exist for it. `SettingsScreen.cppm:208-212, 270-274, 362-366, 442-446` draws its separators with exactly those two tokens, so the dedicated widget is the one place that ignores them: at DPI 2.0 every hand-drawn separator doubles and the widget's stays one physical pixel. The divergence survived because `separator` has zero call sites; fixing it should include converting the four `SettingsScreen` sites. *high*
- **Button.cppm:56** — `draw::button` (56-76) and `button::draw` (115-132) are duplicate bodies computing the same `widget_height`, `content_rect` and `button_rect` and advancing `layout_cursor` identically. They already differ: 63 computes a `widget_id` that is never used. `button::draw` should forward to `draw::button`, as `selectable::draw` (`Selectable.cppm:63-65`) correctly does. *high*
- **Button.cppm:32** — `ids::make(key)` hashes the key string every frame for every button, and because the parameter is a `string_view` needing per-instance uniqueness, callers concatenate: `Chrome.cppm:765` builds `"##git_init_" + rootless.generic_display_string()` per frame per button. `ids::make_from_key` exists and `Dropdown` already ships a `dropdown_in_rect_keyed` overload for this reason. Make every `_in_rect` entry point take an `id`, with string-keyed forms as thin wrappers. Same class: `selectable_info::key`, `nav_item::params::text`, `toggle::params::name`. *high*
- **Marquee.cppm:59** — `const double elapsed = system_clock::now<time_t<double>>().as<seconds>();` is an exit with no external contract — the destination is `operator*` and `std::fmod`, both supported on the type. Write `now<time_t<double>>() / seconds(1.0)`. Secondarily, phasing off `now()` rather than an owned elapsed time means a marquee that begins scrolling (`Selectable.cppm:162` enables it on hover) starts at an arbitrary offset, so hovering a truncated row snaps the text mid-scroll. *high*
- **TabStrip.cppm:266** — `state.spinner_phase += 0.16f; const angle spin = radians(state.spinner_phase);` — three problems: a raw `float` holding an angle (the next line proves it); a per-*frame* rather than per-*second* increment, so the spinner runs ~2.7× faster at 165 Hz than at 60 Hz and stutters with frame time; and no wrap into `[0, 2π)`, so after a few hours `float` precision at ~10⁵ rad visibly quantizes `cos`/`sin`. It also advances for every menu's tab bar every frame whether or not any tab is `busy`. Make the field a `gse::angle`, advance as `angular_velocity * dt`, wrap, and gate on `busy`. *high*
- **TabStrip.cppm:142** — `draw_tab_spinner` receives `close_rect` and passes `.clip_rect = rect` — a rect clipping itself. A busy tab partially scrolled out of the strip draws its spinner in full, outside `area`. The `else` branch three lines apart (311-315) correctly clips the close glyph to `visible`. Add a container parameter. *high*
- **TabStrip.cppm:192** — Close-button geometry derived twice: `tab_strip_layout::make_close` (192-197, horizontal path) and the vertical path at 362 compute the identical rect, but `make_close` returns `{}` when `!tab.closeable && !tab.busy` while the vertical path always produces a real rect. `tab_strip_layout` is *exported* and called externally by `Gui.cpp:1766` and `CodePanel.cppm`. *high*
- **TabStrip.cppm:306, :371, :480** — Close-button hover uses `ctx.hovers` for the highlight and bare `rect_t::contains` for the action. Masked today because the action nests inside a successful `mouse_pressed_for(visible)`, but the moment the strip is drawn inside a `scroll_region` whose clip excludes the button, it stops highlighting while still closing the tab. *high*
- **TabStrip.cppm:475, :521** — Hit-test loop and draw loop cull with different predicates (`!ctx.hovers(visible)` vs `visible.height() <= 0.f`) over the same `placements`, and the draw loop recomputes `visible`, `tab`, `is_active` and `hovered` the hit loop already had. Fuse into one loop with a `consumed` flag replacing the `break`. *high*
- **Dropdown.cppm:312** — `list_rect_early` (312-319, for `register_hit_region`) and `list_rect` (385-396, for drawing) are two independent derivations of where the popup is — one governing input capture, the other pixels. Textually identical today; the `_early` suffix is the tell the author knew they were the same thing. *high*
- **Dropdown.cppm:429** — The option loop culls three separate times (index range 429-430, y-bounds 435-437, `clipped_height <= 0` 449-451) and hand-computes a clipped rect (445-456) that `rectf::intersection(content_area)` produces in one call — and that `queue_sprite` would apply automatically if `content_area` were on the clip stack. `row_height` also comes from `header_rect.height()`, so a zero-height header divides by zero at 429 and casts an infinity to `std::size_t`. *high*
- **Dropdown.cppm:126** — Five exported entry points (42, 53, 65, 77, 89) differing only in option container type and whether a rect is supplied, unified through two `std::function` temporaries constructed per call per frame (an indirect call per visible option) — a job a template parameter does at zero cost. Only one of the five accepts a `font`, so `dropdown_in_rect` can never render in anything but `ctx.fonts.text`. *high*
- **Dropdown.cppm:342** — Header color ladder hand-rolled with two branches assigning the same value (`is_open` and `active_widget_id == dropdown_id` both → `color_widget_active`), and unlike button/selectable/nav-item/toggle it does not pass through `ctx.animated_color`, so it is the one control whose hover snaps instead of easing. *high*
- **GraphCanvas.cppm:23, :43** — Hardcodes its entire palette (`{ 0.20f, 0.22f, 0.26f, 1.f }` node, `{ 0.09f, 0.10f, 0.13f, 1.f }` background, plus edge and label colors at 30 and 150-152) as `params` defaults, bypassing the theme system. Under `frost` (a light theme, `Styles.cppm:374-429`) it renders as a black rectangle in the middle of a white panel. The label color is chosen by a luminance test against the hardcoded background (145-152) — a second, private theming mechanism existing only because the first was bypassed. Default from `ctx.style` inside `draw`. *high*
- **GraphCanvas.cppm:126** — Node highlight by unclamped additive offset (`fill.x() + 0.14f`), so saturation depends on the base color rather than the theme's hover token, and skips `animated_color`. *high*
- **GraphCanvas.cppm:19** — `node::key`, `result::clicked` and `result::hovered` traffic in raw `std::uint64_t`, immediately converted via `ids::make_from_key(n.key)` at 113. `key = 0` is both "unset" and a valid key. Same class: `dropdown_in_rect_keyed` (89) takes the same raw integer. *high*
- **GraphCanvas.cppm:141** — Label clip rebuilt by hand where `n.rect.intersection(clip)` exists. The hand-written form omits the disjoint case that `intersection` handles (`Rectangle.cppm:198-201` returns an empty rect); here disjoint inputs produce `min > max`, and `rect_t`'s `min_max_params` constructor silently swaps them (`Rectangle.cppm:76-78`), yielding an inside-out rect that clips to the wrong region rather than to nothing. `TabStrip.cppm:283, 474` use `intersection` correctly for the same job. *high*
- **Selectable.cppm:129** — With `align == center` and a non-empty `detail`, `split` defaults to the row's horizontal midpoint (127) — exactly where centered text begins — so the clip rect passed at 139-142 hides the right half of every centered label. When `info.accent` is set, `text_left` advances past the swatch (122) but the centered branch ignores it, so swatch and label overlap. Derive position and clip from one text-column rect. *high*
- **Selectable.cppm:162** — `.scrolling = hot_widget_id == widget_id` reads back a frame-global variable to recover `hovered`, which it already holds from line 84. Any future change to `mark_hot` silently changes marquee behavior. *high*
- **Dropdown.cppm:30** — `dropdown_result { bool changed; std::size_t new_index = 0; }` makes `new_index = 0` indistinguishable from "option 0 selected"; `column_header_result` (`ColumnHeader.cppm:35-39`) has the same shape. `graph_canvas::result` (`GraphCanvas.cppm:33-37`) already uses `std::optional` and gets it right, so the set contains both spellings. *high*
- **ColumnHeader.cppm:65** — `seed_columns` re-seeds only when the column *count* changes, so changed content, longer labels, or a font-size change never re-seed. Adding `ctx.style.padding` inside the seed (69-71) also embeds the padding at seed time so it never tracks a style change. *high*
- **ColumnHeader.cppm:88** — Silently draws nothing when widths and captions disagree, so a caller that forgets `seed_columns` gets an invisible header *and* a table that stops responding to clicks, with no diagnostic. `tab_strip` asserts its own precondition (`TabStrip.cppm:245-250`). *high*
- **PanelBackdrop.cppm:49** — `ctx.clip_stack.empty() ? rectf{} : ctx.clip_stack.back()` open-codes `draw_context::current_clip()` (`Types.cppm:383`), tying the widget to `clip_stack` being a `std::vector` member and duplicating the empty-case decision. `clip_stack` should not be a public member at all. *high*
- **PanelBackdrop.cppm:67** — `std::min(accent.width, std::min(body.width(), body.height()))` clamps against both dimensions, so a wide short panel (a header strip — the common case for a left accent bar) silently narrows its accent to the panel's height. Move the clamp into the `switch`. *medium*

### LOW / style findings — widget set

- **Declaration layout and in-class definitions** — Unwrapped parameter declarations: `Button.cppm:52`; `Value.cppm:66, 78, 90, 102`; `Text.cppm:34`; `Dropdown.cppm:113`. Definitions inside the `export namespace` block: `Dropdown.cppm:113-121`; `Text.cppm:34-37`; `Value.cppm:66-68, 78-80, 90-92, 102-104`. Bin-packed argument list at `Dropdown.cppm:114-116`. *high*
- **Value.cppm:42** — Template parameter mangled across three lines (`const gse::` / `vec<T,` / `N>& v`) by a bad reformat; same damage at 51-54 and `Types.cppm:508-509`. It obscures a real problem: the function is named `vec` inside `gse::gui::draw`, shadowing `gse::vec` and forcing the qualification that got mangled. Rename to `vec_row`. *high*
- **Value.cppm:26, 71, 95** — `is_quantity` (the public alias, `Engine/Import/Math.cppm:36`) and `internal::is_quantity` used interchangeably in one file. *high*
- **Dead widget surfaces** — `gui::button` (`Button.cppm:45-54`) and `draw::button` (56), `gui::text` (`Text.cppm:28-38`), `gui::separator` (`Separator.cppm:15-23`) reported as having zero call sites. **Treat as unverified** — the originating pass searched only `Engine/` and `Editor/`, which is how it also mis-reported `Value.cppm`. Re-check against `Sandbox/` and `Engine/Server/` before deleting anything. *low confidence*
- **Marquee.cppm:25** — `vec4f color` is the only field in `marquee_info` with no default, so omitting it under designated initialization value-initializes to transparent black and the text draws invisibly. Make it `std::optional<vec4f>` defaulted from `ctx.style.color_text`. *high*
- **Value.cppm:228** — `ctx.style.padding * std::max(0.0f, static_cast<float>(N - 1))` — `N` is `std::size_t`, so at `N == 0` the subtraction wraps to `SIZE_MAX` and the cast yields ~1.8e19, which `std::max` passes through, producing a large negative `value_box_width` and tripping the `size.x() >= 0` assertion in `rect_t::from_position_size`. The `std::max` catches the wrong sign. *high*
- **GraphCanvas.cppm:61** — Canvas background clipped to itself (`.rect = p.area` with `clip == p.area`). A no-op today, but the same spelling propagates: `SystemGraph.cpp:830-834` passes `.rect = reset_rect, .clip = reset_rect`. *high*
- **PanelBackdrop.cppm:75, 79, 83, 87** — `rectf(rectf::min_max_params{ ... })` with the redundant type name and designated initializers packed onto one line, where `GraphCanvas.cppm:78-81, 141-144` writes the same construction as `rectf({ .min = ..., .max = ... })`. *high*
- **Separator.cppm:3** — A 49-line file using only `rectf`, `draw_context` and `style` imports six engine modules; the same block is pasted into `Text.cppm`, `Marquee.cppm`, `Selectable.cppm`, `NavItem.cppm`, `Button.cppm`, `Toggle.cppm`, `ColumnHeader.cppm` and `Section.cppm`, several of which additionally pull `gse.os`, `gse.gpu` and `gse.assets` without touching them. Flagged as maintenance noise rather than a build-cost claim, since BMI loading is use-driven. *medium*
- **Value.cppm:36 vs :48** — Two `draw::vec` overloads (`const gse::vec<T,N>&` and by-value) form identical conversion sequences with neither more specialized under partial ordering, so a plain `draw::vec(ctx, name, some_length_vec)` is ambiguous. It compiles today only because both live call sites disambiguate. Overload #1 is also unconstrained (`typename T`) where the style guide requires the concept in the parameter slot; constraining it to `is_quantity T` fixes both. *medium — partial-ordering conclusion is from the rules, not a compiler run.*
- **GraphCanvas.cppm:48** — Declaration names a `focus` parameter the definition (58) drops; every other widget leaves the unused slot unnamed in both places. *high*
- **NavItem.cppm:67-72** — `hot` and `active` branches resolve to the same color — the same duplicate-branch shape reported for Dropdown. *high*

---

## 5. Core render pipeline

Files: `Renderers/Renderer.cppm`, `Renderer.cpp`, `GeometryCollector.cppm`, `GeometryCollector.cpp`, `ForwardRenderer.cppm`, `ForwardRenderer.cpp`.

### CRITICAL | Renderers/GeometryCollector.cpp:148 | Batch overflow throws `std::bad_alloc` from inside the frame

```cpp
		batches.push_back({
			.key = key,
			.first_instance = global_instance_offset,
```

**Impact.** A scene with more than 256 distinct `(model, mesh_index)` batches terminates the frame with a thrown `std::bad_alloc` from the middle of `geometry_collector::run`. 256 distinct meshes is a modest scene, reached long before the 4096-instance limit the same system advertises.

**Mechanism.** `batches` is `std::inplace_vector<normal_instance_batch, render_data::max_batches>` (`GeometryCollector.cppm:161,164`, `max_batches = 256` at 158). Per `[inplace.vector]`, `push_back` on a full `inplace_vector` throws `bad_alloc`; it does not assert and it does not saturate. `build_batches` loops over the whole queue with no capacity test, while the sibling limit `max_materials` *is* guarded (`GeometryCollector.cpp:614`) — the asymmetry shows the bound was considered for materials and missed for batches.

**Resolution.** Make the limit unrepresentable rather than fatal: use `try_push_back` and stop merging once full, or use a `linear_vector` reserved to `max_batches` at init. Whichever is chosen, the same count must gate the indirect-command buffer (sized `max_batches * sizeof(draw_mesh_tasks_indirect_command)` at 408-409) and `cull_compute`'s `batch_info_buffer` (`CullComputeRenderer.cpp:75`, sized `max_batches * 2`). Those three are one fact expressed three times.

**Prevention.** `inplace_vector::push_back` reads exactly like `vector::push_back` at the call site but has a hard ceiling, and this codebase uses it in several renderers. A small engine helper pairing a fixed-capacity container with its GPU buffer would declare the capacity once and choose the overflow policy once.

*Confidence: high.*

### CRITICAL | Renderers/GeometryCollector.cpp:546 | Instance-buffer growth leaks device memory and bindless slots every frame a scene grows

```cpp
		if (data.instance_staging.size() > d.instance_capacity) {
			d.instance_capacity = data.instance_staging.size();
			for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
				d.instance_buffer[i] = gpu_s.device->create_buffer(
```

**Impact.** Unbounded VRAM growth and bindless-descriptor exhaustion under any scene whose instance count rises over time. Because the new capacity is set to *exactly* the observed size, a scene that adds one renderable per frame recreates every frame-in-flight buffer *every frame*, each recreation abandoning the previous allocation and its bindless slot permanently.

**Mechanism.** Two defects compound. First, no geometric growth — `d.instance_capacity = data.instance_staging.size()` makes the next frame's `>` test true again as soon as one instance is added. Second, `gpu::buffer` does not own anything (see [C1](#c1-gpu-resources-have-no-owner)). The engine's own growth idiom is `world_text::ensure_vertex_capacity` (`WorldTextRenderer.cpp:131-150`): it grows only the *current* frame's buffer, doubles, and explicitly clears first. This code does none of the three.

**Resolution.** Adopt the `ensure_vertex_capacity` shape: geometric growth, current frame's buffer only — recreating all frames-in-flight is both unnecessary and what multiplies the leak. Then fix the leak at its source per C1 rather than at this call site.

**Prevention.** `PhysicsTransformRenderer.cpp:102-115` contains the identical recreate-all-frames-with-no-growth-factor pattern, so this has already been copied once. Making `gpu::buffer` own its handle removes the leak for every call site; an `ensure_capacity` member on `per_frame_resource<gpu::buffer>` removes the hand-rolled growth loop.

*Confidence: high.* **[verified — exact-size assignment and all-frames loop confirmed against source]**

### HIGH | Renderers/GeometryCollector.cpp:450 | `render.empty()` early-return also skips all skinned geometry

```cpp
	if (render.empty()) {
		co_return;
	}
```

**Impact.** A scene containing skinned characters but no `render_component` entities renders nothing at all — no `render_data` is published, so `depth_prepass::frame`, `forward::frame`, `oit::frame`, `cull_compute::frame`, `physics_transform::frame` and `rt_shadow` all see an empty channel and skip. The symptom is a black viewport that points at the renderers rather than at this guard.

**Mechanism.** The guard tests only the `render_component` chunk, but `collect_skinned` (called at 475) draws from `skeleton_instance_component` and appends to the same `data.render_queue`. The two sources are independent; the guard treats one as a proxy for both. `read_body_index_map(ctx)` at line 448 also builds and discards a full hash map *before* the return.

**Resolution.** Delete the guard — the function is already correct with empty inputs. If the intent was to avoid publishing an empty payload, test the assembled queues after collection.

*Confidence: high.* **[verified — guard precedes `collect_skinned` by 25 lines]**

### HIGH | Renderers/ForwardRenderer.cpp:230 | `std::function` retains `&d` and a `shared_view` for the program lifetime, never unregistered

See [C4](#c4-deferred-callbacks-retain-raw-references-to-system-state). *Confidence: high on the mechanism; medium on whether a re-init path is currently reachable.*

### HIGH | Renderers/ForwardRenderer.cpp:171 | Unchecked dereference of a raw pointer published through a shared view

```cpp
		const auto tlas_address = (*rt_state.tlas_ptrs[fi]).device_address();
```

**Impact.** Null dereference at startup or on swapchain recreate if `rt_shadow::init` has not populated `tlas_ptrs`, or if the RT path is ever made conditional. Because this also runs from the swapchain callback, it can fire long after init when no null check is plausible from the call site.

**Mechanism.** `rt_shadow::data::tlas_ptrs` is `[[= gse::shared]] per_frame_resource<const gpu::tlas*>` (`RtShadowRenderer.cppm:29`), default-initialized to null and filled only in `rt_shadow::init` (`RtShadowRenderer.cpp:60`). Publishing raw pointers into another system's address space through a shared view is the deeper issue — the `publish_kind::stable_pointer` path exists for `[[= stable_shared]] unique_ptr`, and a hand-rolled `T*` bypasses the `static_assert`s in `publish_kind_of` (`SharedView.cppm:44-65`) that enforce the ownership rules.

**Resolution.** Guard the dereference, and raise `tlas_ptrs` to the sanctioned representation so the guard becomes structural. `GiProbeRenderer.cpp:108` is a verbatim copy including the `(*ptr).member` spelling. Note the redundant index: `fi` (167) exists only to index `tlas_ptrs` while `i` indexes everything else in the same loop.

**Prevention.** Reject a bare pointer member carrying `[[= gse::shared]]` at the shared-view layer, the way `unique_ptr` without `stable_shared` is already rejected.

*Confidence: high on the missing check; medium on current reachability.*

### HIGH | Renderers/GeometryCollector.cppm:195 | `prev_model_matrices` grows without bound and is never pruned

```cpp
		std::unordered_map<id, std::vector<std::optional<spatial_matrix>>> prev_model_matrices;
```
```cpp
		auto& entity_prev = prev_model_matrices[eid];
		if (entity_prev.size() < component.model_count) {
			entity_prev.resize(component.model_count);
		}
```

**Impact.** A permanent leak proportional to the number of entities that have *ever* carried a `render_component`. Every despawned entity's motion-vector history stays resident for the life of the process; each retained entry is a heap-allocated vector of 64-byte matrices. The map is also probed with a hash lookup once per entity per frame to reach what is conceptually per-render-component state.

**Resolution.** Move the previous model matrix onto the `render_component` itself, where the ECS already handles its lifetime — the component already stores `models`, `sizes`, and `tints` in parallel arrays, so this is a fourth parallel array living in the wrong place. That removes the map, the per-entity hash lookup, the `resize` dance, and the leak in one change.

*Confidence: high.*

### HIGH | Renderers/ForwardRenderer.cpp:420 | Batch draw-list derivation duplicated four times; the collector emits commands for batches every drawer then skips

```cpp
	for (std::size_t i = 0; i < normal_batches.size(); ++i) {
		const auto& batch = normal_batches[i];
		const auto& mesh = batch.key.resolve_mesh();
```

**Impact.** Four independent copies of "which batches are drawable and where is batch *i*'s indirect command" — `GeometryCollector.cpp:566-602`, `DepthPrepassRenderer.cpp:140-185`, `ForwardRenderer.cpp:420-480`, `OitRenderer.cpp:270-309`. They agree today only because all four iterate the same vector in the same order and multiply by the same stride. Any change to one — an added filter, a compacted command list, a per-batch sort — silently desynchronizes `i` from the buffer offset and draws the wrong mesh's meshlets with another batch's instance range. Nothing fails; the picture is just wrong.

**Mechanism.** The collector writes one command per batch unconditionally (569-578), including for batches whose mesh has no meshlets or whose upload has not completed. Each drawer then re-resolves the mesh and re-evaluates the readiness predicate independently — `ForwardRenderer.cpp:424-430` and `DepthPrepassRenderer.cpp:143-149` split it into two `if`s, `OitRenderer.cpp:273` collapses it into one. The index-to-offset relationship (`i * sizeof(gpu::draw_mesh_tasks_indirect_command)`) is re-derived at three call sites, `mesh.meshlet_count()` is read by the collector and again by each drawer, and the group size 32 appears at `GeometryCollector.cpp:571`, `:590`, and twice in `meshlet_geometry.slang`.

**Resolution.** Give `normal_instance_batch` the drawable predicate and the command index as data produced once by the collector — it already resolves the mesh to compute the command. Drawers then iterate and dispatch without re-deriving anything, and the wasted indirect-command slots for non-drawable batches disappear. The group size belongs on the `meshlet_entry` type where the shader's `numthreads` is declared.

*Confidence: high.*

### HIGH | Renderers/GeometryCollector.cppm:213 | `write<render_component>` is declared but never written; three shared views declared but never read

```cpp
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		data& d,
		shared_view<camera::data> cam_state,
		shared_view<primitive_resolver::data> resolver_state,
```

**Impact.** The scheduler serializes `geometry_collector::run` against every other reader and writer of `render_component`, and adds four false edges to the system graph. Since the ECS scheduler uses declared access to serialize — there are no runtime locks by design — over-declaration directly costs parallelism on the hottest per-frame system.

**Mechanism.** `collect_static` only reads through the token (`GeometryCollector.cpp:181` onward: `component.render`, `model_count`, `models[j].valid()`, `models[j].resolve()` (const), `sizes[j]`, `tints[j]` — no mutation). `gpu_s`, `assets_s` and `resolver_state` are accepted by `run` (531) and never forwarded to `tick`, whose signature omits them (444). `ctx` is unused in `initialize` (392). The same defect appears in `ForwardRenderer.cppm:91-101`: `init` declares `assets_s`, `lc_r`, `atm_state`, `gi_state`, `gc_state`, and `ForwardRenderer.cpp:177-238` uses none of them.

**Resolution.** Narrow to `read<render_component>` and delete the five unused parameters across the two files.

**Prevention.** Unused parameters are ordinarily noise, but here they are load-bearing scheduling declarations. Enabling an unused-parameter diagnostic for annotated system entry points catches the whole class cheaply.

*Confidence: high.*

### HIGH | Renderers/GeometryCollector.cpp:481 | The two largest per-frame vectors are the only ones not reserved, and the queue is re-sorted every frame

```cpp
	const auto total_entries = data.render_queue.size() + data.transparent_queue.size();
	data.instance_staging.reserve(total_entries);
	data.physics_mappings.reserve(total_entries);
```

**Impact.** `owned_render_queue_entry` is large — three `spatial_matrix` (192 bytes), two `shared_ptr`, an `aabb`, a handle, an `id` and an index, well over 250 bytes — and `render_queue`/`transparent_queue` grow to one entry *per mesh per model per entity* with **no reservation at all**. Growing to N entries costs log₂(N) reallocations, each memcpy-ing the whole array and touching every `shared_ptr`. `reserve` is then called on the two *other* vectors after the expensive growth has already happened. `render_data data;` is a fresh local (454) moved into the channel (503), so every frame allocates and frees the full set from scratch. `sort_queues` (275-304) additionally runs a four-key comparison sort over the entire queue every frame even when nothing moved — the key is `(model id, skinned id, owner, mesh index)`, all stable across frames for a static scene.

**Resolution.** Hoist the queue storage into `geometry_collector::data` and reuse it across frames (`clear()` retains capacity), publishing a `shared_ptr<const render_data>` snapshot through the channel. Since the sort key is stable, maintain sorted order incrementally — the queue only needs resorting when an entity's model set changes, which is a structural event.

*Confidence: high on the missing reservations; medium on the incremental-sort resolution.*

### HIGH | Renderers/GeometryCollector.cpp:546 | `instance_capacity` can exceed `max_instances`, which other systems treat as a hard cap

**Impact.** Once the instance count passes 4096, ray-traced shadows, ambient occlusion and reflections silently stop covering the excess geometry. Objects render in the forward pass but cast no shadows and appear in no reflection — a visual defect that reads as a lighting bug.

**Mechanism.** `data::max_instances = 4096` (`GeometryCollector.cppm:185`) is not a soft hint: `rt_shadow::init` sizes its TLAS with it (`RtShadowRenderer.cpp:58`), reserves its instance array to it (61), and hard-breaks its build loop at it (204). The collector's growth path raises `instance_capacity` without bound and without diagnostic.

**Resolution.** Decide which it is. If 4096 is a real ceiling, clamp in the collector and log once. If it is meant to grow, publish the current capacity as shared state and have `rt_shadow` rebuild when it changes.

**Prevention.** A `static constexpr` in one system's `data` read as a hard limit by three others is a cross-system invariant with no enforcement; the `constexpr` is what makes the divergence invisible.

*Confidence: high.*

### MEDIUM findings — core render pipeline

- **GeometryCollector.cpp:135** — `key_of` returns `normal_batch_key` by value holding two `shared_ptr`s, so the batching inner loop (`while (batch_end < items.size() && key_of(items[batch_end]) == key)`) performs four atomic RMWs per queue entry per frame — and `operator==` (109-118) compares the snapshots by *pointer identity*, so the ownership the copy paid for is never used. A `same_batch(const owned_render_queue_entry&, const owned_render_queue_entry&)` predicate reads the same six fields with zero refcount traffic. *high on mechanism; medium on magnitude.*
- **Renderer.cpp:46** — `gpu_s.render_graph->set_gpu_timestamps_enabled(...)` mutates another system's state through a read-only shared view, three lines after the same function correctly pushes a channel request (40) and eleven before another (67). The type system does not catch it: `view_member_specs` publishes stable pointers as `add_pointer(dealias(pointee))` with no `add_const` (`SharedView.cppm:88-95`), so the view hands out a non-const `gpu::render_graph*`. These two lines are also unconditional while the three settings around them are change-gated by `last_*` mirrors. Adding `add_const` to the stable-pointer publish path closes the hole engine-wide, but the number of call sites relying on non-const should be measured first. *high*
- **Renderer.cppm:67** — `vec2f last_viewport{ 1920.f, 1080.f }` is the third independent declaration of that default (also `camera::viewport_update::size`, `CameraSystem.cppm:22`, and `camera::data::viewport`, `:48`). On a 1920×1080 window the viewport update is correctly suppressed *only because* all three agree; change any one and the camera silently keeps a stale aspect ratio at exactly the most common resolution. `last_viewport` is a shadow copy of another system's state used for change detection, but `renderer::run` does not declare `shared_view<camera::data>` so it cannot compare against the real value. `last_hot_reload_enabled`/`last_profile_aggregator_enabled`/`last_profile_frame_recording` are the same shape. Change detection belongs to the settings system. *high*
- **Renderer.cppm:71** — `bool render_world = true;` is written by `Engine.cpp:176` and read by nobody. `Editor/Main.cpp:29` and `Project.cpp:315` both pass `.render_world = false`, and `docs/power_efficiency_plan.md:31` describes it as dropping the 3D world. Three call sites believe they are disabling world rendering; the flag does nothing. The only surviving effect of `m_config.render_world` is the swapchain clear colour at `Engine.cpp:173`. *high*
- **GeometryCollector.cpp:604** — Material-palette overflow: `material_palette_index` (110-117) hands out `map.size()` and never rejects, so indices grow past `max_materials`; the upload loop then skips exactly those (614-616), leaving the buffer short. `meshlet_geometry.slang:355` reads `material_palette[inst.material_index]` with no bound check, so those surfaces render with whatever is in that memory — garbage base colour, roughness, metallic, and a garbage `diffuse_index` fed into `textures[NonUniformResourceIndex(...)]`. The host-side write is safe, so nothing crashes; only the GPU reads out of range. Clamp in `material_palette_index` and log once. *high on mechanism; medium on reachability.*
- **GeometryCollector.cppm:169** — `physics_mapping_count` duplicates `physics_mappings.size()`, and `PhysicsTransformRenderer.cpp:98` sizes a GPU upload from the count while 109/117 pass the vector's data pointer. They agree today only because the count is assigned once at `GeometryCollector.cpp:501` after the vector is final; nothing enforces that ordering. Delete the field. *high*
- **GeometryCollector.cpp:421** — The transparent indirect buffer omits `.stride` (and `byte_address`/`.writable`) that its twin sets 13 lines above (410-419). The writability asymmetry is explainable — the normal buffer is written by `cull_compute`'s `rw_byte_address_buffer` binding — but the missing `.stride` is not: it describes the buffer's element layout independent of who writes it, so its bindless structured-buffer view is described with stride 0. *medium on impact (depends on backend handling of stride 0); high that the asymmetry is unintended.*
- **GeometryCollector.cpp:566** — The normal and transparent indirect-command blocks are byte-identical across 37 lines (566-583 and 585-602) except for two identifiers — same `(meshlet_count() + 31) / 32`, same designated initializer, same emptiness guard. One function taking the batch span and target buffer, called twice, also gives the magic 32 a single home. *high*
- **GeometryCollector.cppm:171** — `std::flat_map<const material*, std::uint32_t> material_palette_map` travels through a channel and is consumed by `RtShadowRenderer.cpp:188` (`find(&mesh_ptr->material())`), so this is a cross-system contract on raw pointer identity. Validity depends on an unexpressed relationship: the materials live inside `model` objects kept alive only by the `model_snapshot`/`skinned_snapshot` `shared_ptr`s stored in a *different* field of the same aggregate — which are only used for identity comparison and are an obvious candidate for removal. The map's iteration order is also address order, making the palette contents non-reproducible across runs, which matters given the determinism baseline. Key by a stable identity. *high on the fragility; medium on current reachability.*
- **GeometryCollector.cppm:44** — Four dimensional exits in the AABB path, none with an external contract: (1) `meters(1.0f)` as the homogeneous `w` component (44-51, eight occurrences) — a projective `w` is dimensionless by definition, and `model_matrix * corner` then multiplies the translation column by one metre; (2) `transform_aabb` takes `vec3<length>` but is called with `mesh::aabb()`, which returns `vec3<displacement>` (`Mesh.cppm:78`) — widening a local-space extent to the `length` root discards the relative/absolute distinction; (3) `vec3<length>(gse::min(mn, q.skinned_bounds.min))` (`GeometryCollector.cpp:336-337`) explicitly converts a `vec3<position>` bound into a `vec3<length>` accumulator — the accumulator's type is what is wrong; (4) `1.0e9f` as an "unbounded" sentinel, twice with different names (`unbounded_extent` at 248, `physics_cull_extent` at 345), fed through `meters()`. Transform the point through `spatial_matrix`'s point-transform rather than hand-building a `vec4<length>`; make `world_aabb_min/max` `vec3<position>`; give the local-space parameters `vec3<displacement>`; replace the sentinel with a `bounds` type carrying an unbounded state. *high on 1, 2 and 4; medium on 3.*
- **GeometryCollector.cppm:33** — Three functions fully defined inside `export namespace gse::renderer` with unwrapped parameter lists (`compute_render_transform` 33-40, `transform_aabb` 43-64, `extract_frustum_planes` 66-99), plus a second export block at 138, plus two member functions defined in-class (`normal_batch_key::operator==` 109-118, `resolve_mesh` 120-124). Beyond style: these bodies sit in a module *interface* partition imported by at least seven TUs, so they are serialized into the BMI every consumer deserialises — a measurable cost in an import-bound build. *high*
- **ForwardRenderer.cpp:309** — Three near-identical light-append loops (309-323, 325-348, 350-367) each repeating bound test, `break`, designated-initializer fill and `++light_count`, guarded at 309 by `if (light_count < max_lights)` where `light_count` was initialized to 0 two lines earlier and not modified — a branch that reads as a bound check and checks nothing. Related, same block: `staging.assign(...)` followed by `reinterpret_cast<shaders::forward::light*>(staging.data())` (288-292) reimplements a typed vector over a byte buffer, with manual `* sizeof(light)` arithmetic at 289 and 370; `d.light_staging.reserve(light_buffer_size)` at 184 reserves *bytes*, correct only because the element type is `std::byte`. A `linear_vector<shaders::forward::light>` removes all of it. *high*
- **ForwardRenderer.cppm:35** — `shadow_quality_level`, and the two sibling quality enums, have numeric values that are a wire contract with `meshlet_geometry.slang`, where `get_shadow_config(int quality)` (169-177), `get_ao_config` (192-200) and `get_reflection_config` (247-254) switch on the same integers. Renumbering an enumerator on the C++ side silently reassigns quality levels on the GPU; the shader's `clamp(pc.shadow_quality, 0, 4)` returns the `default:` config, which disables the feature. Put the configuration on the enumerators as annotations and feed the resolved config through the push constant. *high on the duplication; medium on the resolution's cost, since it changes the push-constant layout.*
- **GeometryCollector.cpp:48** — `build_batches` is a six-parameter callable template with four lambda arguments and exactly one caller (`build_queue_batches`, 306-390), which supplies all four inline across 85 lines. The AABB accumulation policy — three mutually exclusive strategies for skinned, physics-backed, and static batches — is buried inside an anonymous lambda argument where it cannot be named or tested. The indirection also produces the layout damage at 326-331 and 366-371. Inline it and lift the three strategies into named functions. *high*
- **ForwardRenderer.cpp:435** — All nineteen binding slots are re-pushed per batch though only five change; fourteen are hoisted out of the loop at 406-418 *because* they are invariant, then copied back into the memcpy'd binding-argument struct every iteration. Same in `DepthPrepassRenderer.cpp:159-177` and `OitRenderer.cpp:280-301`. This belongs in the recording layer — a way to bind the invariant set once per pass and push only the per-draw remainder — and should be sized before being attempted. *high on the redundancy; low on impact magnitude without measurement.*
- **GeometryCollector.cppm:177** — `filter_render_queue` is exported, has no caller anywhere in the repository, and returns `std::vector<render_queue_entry>` — a type holding a `resource::handle<model>` with no `model_snapshot` — so its implementation (510-521) copies `queue_entry.entry` out of `owned_render_queue_entry` and **discards the `shared_ptr` that keeps the model alive**. Any future caller inherits a lifetime bug invisible at the signature. It is also O(N·M) with a fresh vector allocation per call. Delete it. *high*
- **GeometryCollector.cpp:489** — Deformed-vertex slots are patched onto opaque batches only, but `collect_skinned` (234-273) appends unconditionally to `data.render_queue` with no tint or opacity test, unlike `collect_static` which routes on `component.tints[j].w()` (213-214). So a skinned character with a transparent material renders fully opaque, while `OitRenderer.cpp:293` reads `batch.deformed_vertices` on transparent batches where this loop guarantees it is always invalid — silently using undeformed vertices. Two coupled assumptions in different files with nothing reconciling them. *high on mechanism; medium on whether transparent skinned materials occur in practice.*
- **GeometryCollector.cpp:164** — `read_body_index_map` builds and discards a node-based `unordered_map` every frame, populated one node at a time and queried once per entity, *before* the `render.empty()` early return. Hold it in `data` and `clear()` per frame, or use `std::flat_map`. *high*

### LOW findings — core render pipeline

- **GeometryCollector.cpp:354** — Twelve lines of component-wise `std::min`/`std::max` where `gse::min(vec, vec)`/`gse::max(vec, vec)` are used correctly by the *same lambda* eighteen lines earlier (336-337) and by `transform_aabb` (`GeometryCollector.cppm:59-60`). Three spellings of one operation in one file. *high*
- **GeometryCollector.cpp:604, :614** — `geometry_collector::` is redundant inside `gse::renderer::geometry_collector::frame`. *high*
- **GeometryCollector.cpp:526, :531** — `init` forwards to `initialize` and `run` forwards to `tick`, each adding a coroutine frame and a second name for one operation. `init`/`run` mark their `shared_view` parameters `const` while `frame` (536) does not, and `initialize`/`run`/`tick` are coroutines while `frame` returns `{}`. *high*
- **GeometryCollector.cppm:109, :120** — `operator==` and `resolve_mesh` defined in-class; `resolve_mesh` also dereferences `model_snapshot` unchecked and is called from four systems every frame. *high*
- **GeometryCollector.cppm:146** — `invalid_body_index` sentinel where `std::optional<std::uint32_t>` says it. Related: `GeometryCollector.cpp:385` reaches `key.model_snapshot->center_of_mass()` guarded only by this sentinel — the guard happens to imply a non-null snapshot today because `collect_skinned` never sets a body index, but the two facts are unrelated by construction. *high on style; medium on the null-deref reachability.*
- **GeometryCollector.cpp:261** — Identity matrix constructed three times per skinned mesh per entity per frame (`.model_matrix`, `.normal_matrix`, `.prev_model_matrix` each `spatial_matrix(mat4f(1.f))`). *high*
- **ForwardRenderer.cpp:281** — Three non-const aliases of by-value parameters (`dir_chunk`, `spot_chunk`, `point_chunk`) that rename without adding meaning and drop `const`. *high*
- **ForwardRenderer.cpp:441** — `.screen_size = vec2u{ ext.x(), ext.y() }` decomposes and reassembles a `vec2u`. *high*
- **ForwardRenderer.cpp:376** — Four `_i`-suffixed locals (`num_lights_i`, `shadow_quality_i`, …, plus `gi_atlas_size_v` at 381) exist only to name a cast; the suffix encodes the type, which the codebase does not do elsewhere, and three of the four are used exactly once. *high*
- **ForwardRenderer.cpp:251, :391** — `gpu::color_clear{ 0.0f, 0.0f, 0.0f, 1.0f }` positional, while the adjacent `gpu::depth_clear{ .depth = 1.0f }` (255-257) uses designated form. Same at `OitRenderer.cpp:228,234` and `DepthPrepassRenderer.cpp:123,127`. *high*
- **Renderer.cpp:31** — `.default_combo = { .k = key::f11 }` packs a nested designated initializer onto one line. *high*
- **Renderer.cpp:66** — Vector comparison decomposed into components where `new_viewport != d.last_viewport` says it; line 64 also builds a `vec2f` by casting components individually. *high*
- **Vertical alignment** — `ForwardRenderer.cpp:285-286`, `Renderer.cppm:61-62`, `ForwardRenderer.cppm:60-61`, `ForwardRenderer.cppm:71-73` (the `describe<...>` string continuation aligned under the opening quote). *high*

### Verified correct — core render pipeline

Recorded so it is not re-investigated: `meshlet_push_constants` is layout-portable — no vector straddles a 16-byte boundary, `gpu::push_constant<T>` `static_assert`s this via `push_constant_layout_is_portable` (`PipelineBuilder.cppm:287-290`), and the struct totals 80 bytes against the 256-byte budget. Frame-system ordering is sound: `run_node_frame` awaits `frame_state_deps` (`Scheduler.cpp:283-285`), so the `shared_view<geometry_collector::data>` dependency guarantees the forward, depth-prepass, OIT and cull-compute frames run after the collector's.

---

## 6. Post-process and cull passes

Files: `Renderers/DepthPrepassRenderer.{cppm,cpp}`, `CullComputeRenderer.{cppm,cpp}`, `LightCullingRenderer.{cppm,cpp}`, `OitRenderer.{cppm,cpp}`, `TaaRenderer.{cppm,cpp}`, `TonemapRenderer.{cppm,cpp}`, `BloomRenderer.{cppm,cpp}`.

Two hypotheses were checked and **discarded**, recorded here so they are not re-raised: (a) the bloom/upsample intra-pass RAW between consecutive dispatches is *not* a missing barrier — `register_bindless_usage` calls `note_touched` + `emit_intra_pass_barrier` for every single-descriptor bindless binding, so the render graph derives it; (b) OIT omitting `.textures` from its binding args is *not* a bug — `ForwardRenderer.cpp:452-471`, `UiRenderer`, and `WorldTextRenderer` all do the same, because `descriptor_count_v > 1` arrays are deliberately skipped by the registration path.

### CRITICAL | Renderers/BloomRenderer.cpp:146-148 | Mip-chain images dropped without `retire`

```cpp
	for (std::uint32_t i = 0; i < max_mip_count; ++i) {
		d.mips_down[i] = {};
		d.mips_up[i] = {};
```

Instance of [C1](#c1-gpu-resources-have-no-owner). Up to 14 R16G16B16A16 images (7 down + 7 up, largest at half screen resolution) per swapchain recreate; a one-second resize drag leaks hundreds of megabytes. *Confidence: high on the mechanism; medium on "unbounded" only because device-internal reclamation could not be traced without building.* **[verified — `~image() = default` and zero `retire` calls in `Graphics/` both confirmed]**

### HIGH | Renderers/BloomRenderer.cpp:174-186 | `rewrite_descriptors` frees a bindless slot, then reallocates it, with an early return between

```cpp
auto gse::renderer::bloom::rewrite_descriptors(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	d.hdr_view = {};
```

**Impact.** Three defects. Line 175 performs a synchronous free — `bindless_handle::operator=(bindless_handle&&)` calls `m_pool->release(m_slot)` before taking the new value (`Bindless.cppm:139-150`), and `bindless_slot_pool::release` pushes straight back onto the free list with no frame deferral (`Bindless.cppm:114-117`). The early return at 178-180 (`!hdr.handle() || d.active_mip_count == 0`) then exits *after* the free, leaving bloom with no descriptor at all. And the `if (!d.hdr_view.valid())` at 182 is dead, because 175 guaranteed it.

**Note on severity.** The originating pass rated this critical on the grounds that the freed slot could be handed to an unrelated resource. The pool is **LIFO** — `allocate` pops from the back of the same `free_list` `release` pushes to — so in the straight-line path line 183 hands back the identical index line 175 freed. The cross-resource aliasing scenario requires an interleaving allocation that this synchronous function does not permit. Downgraded to high accordingly; the sequencing defects above are unaffected.

**Mechanism.** Every sibling renderer gets this right by never releasing: `TaaRenderer.cpp:115-118, 122-125`, `OitRenderer.cpp:129-132, 136-139`, `TonemapRenderer.cpp:80-83, 87-90`, and `LightCullingRenderer.cpp:88` all keep the slot and only rewrite its contents. Bloom is the sole divergence.

**Resolution.** Delete line 175. The function then matches the sibling shape: keep the slot, rewrite the descriptor. A slot is a stable identity for the lifetime of the system, not a per-generation resource.

**Prevention.** Make the slot pool defer releases by `max_frames_in_flight` the way `device::retire` already does (`Vulkan/Device.cpp:1486`), so a correct-looking free is also a safe one.

*Confidence: high.* **[verified — LIFO free list, synchronous release, and dead `if` all confirmed]**

### HIGH | Renderers/BloomRenderer.cpp:231-234 | Bloom rewrites a live bindless descriptor every frame

```cpp
	if (!d.hdr_view.valid()) {
		d.hdr_view = gpu_s.device->allocate_image_slot();
	}
	gpu_s.device->write_sampled_image(d.hdr_view.slot(), hdr);
```

**Impact.** `write_sampled_image` mutates descriptor heap memory that command buffers from the previous frame are still executing against. Without `UPDATE_AFTER_BIND` semantics this is undefined behavior on Vulkan and a torn-descriptor read on DX12.

**Mechanism.** Also a Paired Derivations violation: `rewrite_descriptors` is the declared authority for this descriptor (called from `init` at 203 and the recreate callback at 209), yet `frame` re-derives it unconditionally on the hot path. Two authorities for one descriptor means the swapchain callback is not actually load-bearing — and if `rewrite_descriptors`' early return leaves the handle invalid, `frame` silently papers over it. `TonemapRenderer::frame`, `TaaRenderer::frame`, and `OitRenderer::frame` all read `d.*_view.slot()` without rewriting.

**Resolution.** Delete 231-234 and let `rewrite_descriptors` be the single authority. If `post_taa_color` can change outside a swapchain recreate, that is the fact to fix — one owner for "the framebuffer image generation changed".

*Confidence: high.*

### HIGH | Renderers/BloomRenderer.cpp:260-262 | `mips_up[0]` is composited while never having been written

```cpp
	if (count < 2) {
		co_return;
	}
```

**Impact.** When `active_mip_count == 1`, bloom records the downsample pass, returns before the upsample pass, and never writes `mips_up[0]`. Tonemap then samples and additively composites that image's undefined contents into the final frame (`TonemapRenderer.cpp:131-133, 142, 155`). Visible garbage or a bright flash.

**Mechanism.** Tonemap's activity predicate and bloom's productivity predicate are different facts computed independently:

```cpp
	const bool bloom_active = bloom_state.bloom_quality != bloom::quality_level::off && bloom_state.active_mip_count > 0;
	const auto bloom_slot = bloom_active ? bloom_state.mips_up[0].sampled_slot() : d.hdr_view.slot();
```

`active_mip_count > 0` is satisfied at count == 1, but bloom only produces an upsample result at count ≥ 2. `compute_mip_chain` yields count == 1 whenever one axis of the render extent falls in [16, 31] px — reachable transiently on a window resize and routinely when an editor viewport panel is dragged small.

**Resolution.** Publish the fact tonemap actually needs: replace `active_mip_count` with one published value meaning "there is a valid composite result in `mips_up[0]`", set in `recreate_mip_chain` as `count >= 2 ? count : 0`, read by both bloom's guard and tonemap's `bloom_active`. The `count < 2` special case then disappears from `frame`.

*Confidence: high on the mechanism; medium on frequency.*

### HIGH | Renderers/BloomRenderer.cpp:141-142 | Changing `bloom_quality` at runtime has no effect

```cpp
auto gse::renderer::bloom::recreate_mip_chain(const shared_view<gpu::context::data> gpu_s, data& d) -> void {
	const auto [count, extents] = compute_mip_chain(gpu_s.render_graph->extent(), d.bloom_quality);
```

**Impact.** `bloom_quality` is a settings-exposed, `[[= gse::shared]]` control (`BloomRenderer.cppm:28-32`). `recreate_mip_chain` is its only consumer and is called from exactly two places: `init` (202) and the swapchain-recreate callback (208). Setting `high` while bloom is `off` produces nothing (`active_mip_count` stays 0, `frame` returns at 222). Setting `off` while bloom is `high` leaves bloom dispatching all 7 down-mips every frame while tonemap stops compositing them — full GPU cost, zero visible effect. Both states persist until the user resizes the window.

**Resolution.** Compare the quality the cache was built for against `d.bloom_quality` at the top of `frame` and rebuild on difference — or derive the invalidation from the settings system so both inputs are handled by one mechanism.

**Prevention.** Every `[[= gse::settings]]` field that feeds resource creation rather than a per-frame push constant has this hazard. `TaaRenderer`'s `blend_alpha` and `Tonemap`'s `exposure` are safe only because they are push-constant values. A settings-change notification makes "this setting requires a rebuild" expressible rather than remembered.

*Confidence: high — verified by grepping the whole repository for `bloom_quality`; the only reads are `recreate_mip_chain` and `TonemapRenderer.cpp:131`.*

### HIGH | Renderers/TaaRenderer.cppm:31-34 | History ping-pong indexed by the frame counter against a hardcoded 2-element array

```cpp
		std::array<gpu::bindless_handle, 2> history_views;
		std::uint32_t frames_since_history_invalid = 0;

		[[= gse::shared]] std::array<gpu::image, 2> history;
```

**Impact.** `TaaRenderer.cpp:167, 179, 192, 204` index these with `frame_index` and `1u - frame_index`, where `frame_index = render_graph->current_frame()` — bounded by `gpu::max_frames_in_flight`, currently `2` (`GpuBackend/Enums.cppm:138`). Correct today. Raise it to 3 and `d.history[2]` is out of bounds, and `1u - 2u` evaluates to `0xFFFFFFFF`. Silent memory corruption, in a file that gives no hint it depends on that constant.

**Mechanism.** The literal `2` is written three times (twice as an array bound, once as the `>= 2` warm-up threshold at 172) and is nowhere tied to `max_frames_in_flight`. Every other renderer in the set expresses per-frame storage as `per_frame_resource<T>`, which carries `frames_in_flight` as a static member and drives its own loops.

**Resolution.** Make both arrays `per_frame_resource<...>` and derive the read index from `frames_in_flight` rather than `1u - frame_index`.

*Confidence: high on the coupling; the out-of-bounds access is latent, not live.*

### HIGH | Renderers/LightCullingRenderer.cpp:83-85, 282, 302 | Tile grid and shader `screen_size` derived from two extents that can disagree

```cpp
auto tile_count(const data& d) -> vec2u {
	return { (d.current_width + tile_size - 1) / tile_size, (d.current_height + tile_size - 1) / tile_size };
}
```
```cpp
		.screen_size = vec2u{ extent.x(), extent.y() },
```

**Impact.** The dispatch group count (302, from `tile_count(d)` → `d.current_width/height`) and the shader's authoritative screen size (282, from `graph.extent()`) come from independent copies of the render resolution. When they disagree, the compute shader derives a tile index from the *new* extent and writes into `light_index_list`/`tile_light_table` buffers sized for the *old* one. Both are `.writable = true` storage buffers — out-of-bounds GPU writes, i.e. corruption of whatever the allocator placed next, not a clean fault.

**Mechanism.** `d.current_width`/`current_height` are assigned only in `rebuild_tile_buffers` (94-95), which runs from `init` and the recreate callback. `graph.extent()` is read fresh in `frame` (192). Any path changing the render extent without running the callback — or any frame between the two — leaves them inconsistent, with no assertion or reconciliation.

**Resolution.** Delete `d.current_width`/`current_height`. Have `tile_count` take the extent as a parameter and call it with `graph.extent()`, and store the buffer-sizing extent alongside the buffers so `frame` can detect and skip a mismatched frame.

*Confidence: high that the duplication exists and can diverge; medium on how often it is observable.*

### HIGH | Renderers/TonemapRenderer.cpp:144-149 vs :161 | Velocity read declared conditionally, bound unconditionally

```cpp
	if (d.show_velocity) {
		rec.sample_image(
			gpu_s.render_graph->framebuffer_image<targets::velocity>(),
			gpu::pipeline_stage_flag::fragment_shader
		);
	}
```
```cpp
			.velocity_color = d.velocity_view.slot(),
```

**Impact.** The declaration driving barrier and layout derivation is gated on `d.show_velocity`; the descriptor binding is not. If the shader ever reads `velocity_color` outside the `show_velocity` branch, it reads an image with no declared transition to `sampled` for this pass — undefined layout read on Vulkan.

**Mechanism.** The sibling case in the same function is handled correctly: `bloom_slot` (133) falls back to `d.hdr_view.slot()` when inactive and `hdr` is unconditionally declared at 140. The velocity path did not receive the same treatment. Note that `register_bindless_usage` will call `note_touched` for the velocity binding when `push_bindings` runs, since it is a single-descriptor `texture2d` — which likely masks the defect today and makes the explicit `sample_image` call redundant rather than protective.

**Resolution.** Apply the `bloom_slot` treatment: `const auto velocity_slot = d.show_velocity ? d.velocity_view.slot() : d.hdr_view.slot();` and drop the conditional `sample_image` block. One expression decides both halves.

*Confidence: medium — auto-derivation probably makes this benign at runtime; reported because the construction is the canonical silent-divergence shape and the correct pattern is three lines away.*

### MEDIUM findings — post-process and cull passes

- **LightCullingRenderer.cpp:200** — `std::array<shaders::forward::light, max_lights> lights{}` — ~96 bytes × 1024 ≈ 96 KB, value-initialized every frame. Because `frame` is a coroutine this lives in the coroutine frame, not the stack, so it is 96 KB of frame-arena allocation plus a 96 KB `memset` per frame, of which only `light_count * sizeof(light)` bytes are ever written (276). `ForwardRenderer.cpp:369-370` uses the identical shape. Move the staging array into `data`. *high on size and cost; medium on impact.*
- **CullComputeRenderer.cpp:117** — `std::vector<batch_info> batch_staging(normal_count)` heap-allocates per frame for data whose maximum is a compile-time constant (`normal_batches` is `std::inplace_vector<..., max_batches>` with `max_batches = 256`). The `if (!batch_staging.empty())` guard at 129 is also redundant since 108 already returned when `normal_count == 0`. *high*
- **DepthPrepassRenderer.cpp:89/115, CullComputeRenderer.cpp:89/114/130, LightCullingRenderer.cpp:175/276/285** — Three of the seven `frame` functions take `const data& d` and then write through it via `gpu::buffer::host_write` (a `const` member that memcpys into mapped memory and stores to a `mutable` dirty flag); the other four take `data&`. The `const` describes nothing, and the split means the signature carries no reliable information either way. Make all seven take `data&`. *high on the inconsistency; medium on impact.*
- **BloomRenderer.cpp:252-256, 282-286 and LightCullingRenderer.cppm:22** — Thread-group geometry duplicated between the shader entry and the dispatch math: `7u`/`8u` restate `gpu::threads<8, 8, 1>` from `downsample_entry`/`upsample_entry` (`BloomRenderer.cpp:83-85`), and `light_culling::tile_size = 16` restates `gpu::threads<16, 16, 1>` (`LightCullingRenderer.cpp:77`) — where the constant is *also* the shader's tile size, so it is duplicated a third time in Slang. Changing the thread group size silently leaves the top and right edges of every mip unwritten, which reads as a shader bug. Expose the thread-group extent from the entry type; a `dispatch_covering<Entry>(vec2u extent)` helper on `recording_context` would remove the duplication at every call site at once. *high*
- **BloomRenderer.cpp:107-119** — `mips_for_quality` is a switch mapping every enumerator to fixed metadata, stated three times: in the switch, in the settings description string `"Bloom mip count: low=4, medium=6, high=7"` (`BloomRenderer.cppm:29`), and implicitly in `max_mip_count = 7` which must equal the `high` case. The unreachable `return 0;` at 118 exists solely to silence the resulting warning. Put the mip count on the enumerators as a plain aggregate annotation read with `annotation_from_enum`. *high*
- **TaaRenderer.cppm:25** — `float blend_alpha = 0.1f` is the per-frame coefficient of an exponential moving average — an integration step with no `dt`. At 30 fps it retains history over ~0.3 s; at 144 fps over ~0.07 s, so the same setting produces materially different ghosting at different framerates. Not an escape-hatch violation — the quantity was never typed. Express the setting as a `time` half-life and compute the per-frame alpha from `system_clock::dt()` at the point of use; the push constant stays a bare `float`, which is the legitimate shader ABI contract. *high on the dimensional argument; medium on visible impact.*
- **TaaRenderer.cpp:166-173** — Any frame returning early at 158, 163, or 168 skips writing `d.history[frame_index]` while `frame_index` advances regardless, so two frames later the pass reads `d.history[1u - frame_index]` holding content from *two* frames ago and reprojects it with one frame of motion vectors. `frames_since_history_invalid` is reset only in `recreate_history` (85) and incremented only on the non-early-return path (173), so the `history_ready` predicate at 172 can report ready against stale history. Track validity per history slot rather than as a global counter; that also removes the bare `>= 2` literal restating the ping-pong depth. *medium.*
- **BloomRenderer.cppm:51** — `mip_extents` duplicates the extent the images already carry: dispatch group counts (252-256, 282-286) come from `mip_extents[i]` while the storage image bound in the same call is `mips_down[i]`/`mips_up[i]`. A failed `create_image` leaving a null image with a populated extent, or a partial `recreate_mip_chain`, makes the shader write past the bound image. *high on the duplication; medium on reachability.*
- **LightCullingRenderer.cpp:102-122** — Tile buffers recreated for all frames in flight with no retirement (instance of C1) and no in-flight guard. Both arrays are `[[= gse::shared]]` and read by `ForwardRenderer.cpp:409-410`, so replacing a published generation in place, from a callback outside the scheduler, is what the Published Ownership rule warns against. *high on the missing retirement; medium on the published-generation hazard.*
- **Interface imports** — `DepthPrepassRenderer.cppm:5-19` declares only `gpu::shader_program`, `per_frame_resource<gpu::buffer>`, `context`, `shared_view`, `async::task`, `asset::data`, `geometry_collector::data`, `camera::data` — yet imports `gse.os`, `gse.containers`, `gse.time`, `gse.diag`, `gse.math`, plus the partitions `:cull_compute_renderer` and `:physics_transform_renderer` needed only by the implementation's `.after<>()`. `TaaRenderer.cppm` imports `gse.containers` and `gse.math` with no apparent interface use. The same block is copy-pasted across all seven interfaces. An unused import in an *interface* propagates to every importer; in an implementation it costs one TU. Move implementation-only partition imports into the `.cpp`. *medium — usage inferred from declarations rather than a compile.*
- **Unused `shared_view` parameters** — `assets_s` unused in `depth_prepass::init`, `cull_compute::init`, `light_culling::init`; `gc_r` unused in `cull_compute::init`; `bloom_state` unused in `tonemap::init`; `context& ctx` unused in all seven `init` functions (each ends `return {};`, not `co_return`). `shared_view` parameters are one of the three edge sources the scheduler and the editor's system-graph panel derive ordering from, so an unused one declares a dependency that does not exist. *high that they are unused; medium on scheduling impact.*
- **LightCullingRenderer.cpp:82-126** — Three module-private functions defined inline inside a *second* `namespace gse::renderer::light_culling` block (the first is at 24), with no declarations, no parameter wrapping, and no indentation. Beyond style, the definitions are now order-dependent — `rebuild_tile_buffers` must appear after `tile_count` and `update_depth_descriptor` — which is exactly the coupling the declared form removes. `OitRenderer.cpp:120-123`, `TaaRenderer.cpp:72-80`, `TonemapRenderer.cpp:71-74`, and `BloomRenderer.cpp:87-104` all follow the guide correctly; LightCulling is the sole outlier. *high*
- **LightCullingRenderer.cpp:203-215** — `if (light_count < max_lights)` where `light_count` was initialized to 0 at 201 and not modified: a branch that reads as a bounds guard mirroring 218, 234, and 257 and checks nothing. Four copies of the same insertion transition; route all four through one bounded helper. *high*
- **LightCullingRenderer.cpp:194-196** — `auto& dir_chunk = dir_lights;` and two siblings: non-const references bound to parameters, used once each, adding a name without meaning. *high*
- **OitRenderer.cpp:61-70** — `float pad` is a manual alignment fix for the two `vec3f` members. The layout happens to work out (48 bytes, both `vec3f` on 16-byte boundaries) but by coincidence of the preceding field sizes — inserting or removing any 4-byte member ahead of them silently breaks the shader-side layout with no diagnostic. A shader constant layout is a real external contract, so padding is legitimate in principle; what is not is that it is unverified. *medium — the Slang side was not read.*
- **BloomRenderer.cpp:270-271** — `(i + 1 == count - 1)` is a first-iteration test disguised as index arithmetic, inside a loop whose `i-- > 0` idiom obscures the range, guarded by a `count < 2` early return twenty lines away that is what actually keeps `count - 1` from underflowing. Three separate pieces of reasoning are needed to confirm the indices are in range. Hoist the seed out of the loop. *high that the current code is correct; the finding is complexity.*
- **BloomRenderer.cpp:160-169** — `recreate_mip_chain` allocates `mips_up[i]` for every `i < count`, but the upsample loop only writes `count-2` down to `0`, so the coarsest up-mip is allocated, transitioned, and never used. *high*
- **Framebuffer recreation ordering** — correct today only by registration accident; folded into [C4](#c4-deferred-callbacks-retain-raw-references-to-system-state). *high on the current ordering; the finding is fragility.*

### LOW findings — post-process and cull passes

- **Double blank line between partition and engine imports** — `DepthPrepassRenderer.cpp:12-13`, `CullComputeRenderer.cpp:8-9`, `OitRenderer.cpp:17-18`, `TaaRenderer.cpp:14-15`, `TonemapRenderer.cpp:14-15`, `BloomRenderer.cpp:13-14`. `LightCullingRenderer.cpp:12` has one, which is the correct form. *high*
- **LightCullingRenderer.cpp:248-249** — `gse::cos(...)` inside `gse::renderer::light_culling::frame`. (The `cos(angle) -> float` conversion itself is dimensionally correct.) *high*
- **DepthPrepassRenderer.cpp:119, TaaRenderer.cpp:175, TonemapRenderer.cpp:135** — Fully-qualified pass tags (`^^gse::renderer::taa::frame`) inside definitions already in those namespaces, while `OitRenderer.cpp:224,315` and `BloomRenderer.cpp:236,264` use the unqualified tag-struct form. Reported as an inconsistency, not a definite violation — whether the unqualified spelling of a function's own reflection operand compiles inside its own body was not verified. *low*
- **BloomRenderer.cppm:17-22** — `enum class quality_level : int` with explicit `= 0, 1, 2, 3` restating the defaults, and an unusual underlying type where the codebase otherwise uses `std::uint8_t`. If the mip counts move onto annotations, the explicit values become actively misleading. *high*
- **CullComputeRenderer.cpp:65, :75** — `constexpr` locals declared inside a loop. `DepthPrepassRenderer.cpp:72` hoists the equivalent correctly, then re-spells the same `sizeof` expression on 78 for `.stride`. *high*
- **CullComputeRenderer.cpp:75** — `max_batches * 2 * sizeof(batch_info)` allocates twice what can be written (`normal_batches` holds at most `max_batches`). Either the `* 2` covers `transparent_batches` — in which case nothing writes them here — or it is dead over-allocation. Safe in the conservative direction; name or derive the constant. *high that it is unexplained; low on what was intended.*
- **LightCullingRenderer.cpp:99** — `total_tiles * max_lights_per_tile` evaluates in `std::uint32_t` before `sizeof` promotes, then narrows back into a `std::uint32_t` initializer with no diagnostic. Unreachable in practice (8K needs 33 MB) but the declared type is smaller than the expression's natural type and `create_buffer`'s `.size` is a `device_size`. *high*
- **LightCullingRenderer.cppm:41, :48** — Attribute on the same line as the declaration where the other six interfaces put it on its own line. *high*
- **TonemapRenderer.cpp:166** — Missing trailing newline; all thirteen other files in the set have one. *medium — inferred from the read.*
- **TonemapRenderer.cppm:29 / TonemapRenderer.cpp:78** — `hdr_view` is bound to `targets::post_taa_color`, not `hdr_color` (which `TaaRenderer.cpp:113` and `OitRenderer.cpp:317` both use). A reader tracing the post-process chain must check the binding to learn which target is meant. Rename to `post_taa_view`. *high*

### Adjacent observation

`shaders::forward::light` (`SharedShaders.cppm:60-75`) types `linear` as `inverse_length` but leaves `quadratic` a bare `float`, in an attenuation polynomial `constant + linear·d + quadratic·d²` where dimensional consistency requires inverse-area. `LightCullingRenderer.cpp:247,267` populate it, so the reviewed code consumes the gap without introducing it. Reported again under section 9 against the declaring file. *high on the dimensional analysis.*

### Clean — post-process and cull passes

`CullComputeRenderer.cppm`, `OitRenderer.cppm`, `TonemapRenderer.cppm` — no findings of their own: declarations correctly inside the single `export namespace` block, one parameter per line, `)` on its own line, empty bodies collapsed, no `inline`, no `detail` or anonymous namespaces, no `mutable`, no `get_` prefixes. `DepthPrepassRenderer.cppm` clean apart from the shared interface-import and unused-parameter findings. `OitRenderer.cpp` is the only file in the set where the module-private declaration/definition split, the tag-struct pass identifiers, and the non-`const` state parameter are all done the way the guide asks.

---

## 7. Effect and auxiliary renderers

Files: `Renderers/AtmosphereRenderer.{cppm,cpp}`, `CloudRenderer.{cppm,cpp}`, `GiProbeRenderer.{cppm,cpp}`, `RtShadowRenderer.{cppm,cpp}`, `SdfGridRenderer.{cppm,cpp}`, `SceneSnapshotRenderer.{cppm,cpp}`, `UiRenderer.{cppm,cpp}`, `WorldTextRenderer.{cppm,cpp}`, `SkinRenderer.{cppm,cpp}`, `PhysicsDebugRenderer.{cppm,cpp}`, `PhysicsTransformRenderer.{cppm,cpp}`.

### CRITICAL | Renderers/SceneSnapshotRenderer.cpp:37-47 | GPU images and buffers never released

Instance of [C1](#c1-gpu-resources-have-no-owner), with the fullest list of affected growth sites. *Confidence: high on the mechanism; medium on "unbounded".* **[verified]**

### HIGH | Renderers/UiRenderer.cpp:205-207 | Empty UI frame is never published, so the last UI generation redraws forever

```cpp
	if (sprite_commands.empty() && text_commands.empty()) {
		return {};
	}
```

**Impact.** On any frame where the GUI emits no commands — all menus closed, HUD toggled off, GUI system gated or skipped — the previously published batch stays on screen indefinitely. The symptom presents as "UI won't go away", pointing at the GUI rather than at the renderer.

**Mechanism.** `d.buffered_frames` is `n_buffer<gpu_frame_data, 3>`; `read()` returns `m_buffers[m_ready_index.load()]`, i.e. the last *published* generation (`Containers/NBuffer.cppm:88-90`). The early return skips `write()`, `vertices.clear()`, `indices.clear()`, `batches.clear()` and `publish()` (209-212), so `frame()` (365-369) sees the stale non-empty `batches` and draws it.

**Resolution.** Delete the early return. Always clear the write buffer and `publish()`; the `batches.empty()` check in `frame()` then correctly renders nothing. The empty case is not a fast path worth a branch — it is the branch that makes "no UI" unrepresentable.

**Prevention.** Recurring for any producer writing into an `n_buffer`. Have `write()` return a scope guard that publishes on destruction, so a producer cannot return early having mutated-but-not-published, and cannot publish a buffer it never reset.

*Confidence: high.* **[verified — `read()` returning the last published generation and the skipped clear/publish both confirmed]**

### HIGH | Renderers/WorldTextRenderer.cpp:117-119 | Static grid labels re-formatted, re-laid-out and re-tessellated every frame

```cpp
		const length offset = major_spacing * static_cast<float>(n);
		const auto text = std::format("{:.0f}", offset);
		const float text_width = f.width(text, world_scale);
```

**Impact.** With the defaults (`fade_distance = 200 m`, `major_spacing = 10 m`) `max_ticks` is 20, so `frame()` runs 80 labels per frame: 80 `std::string` allocations from `std::format`, 80 `std::vector<positioned_glyph>` allocations (`Font.cppm:84-88` returns by value), 80 `f.width` re-scans, plus a fresh `std::vector<world_text_vertex>` (206) and full re-tessellation into ~6 vertices per glyph. Roughly 170 heap allocations and a full geometry rebuild per frame for content that changes only when `major_spacing`, `fade_distance`, or `label_size` change.

**Resolution.** Build the label vertex buffer once into system state and rebuild only when those three inputs differ from the values the current buffer was built with — one stored tuple plus an equality test. That also removes the per-frame `std::vector` in favour of the already-sized `d.vertex_buffers`.

**Prevention.** `f.text_layout` returning a fresh `std::vector` makes *any* per-frame text path expensive; the same cost lands in `UiRenderer.cpp:129`. Give `text_layout` an overload appending into a caller-supplied `linear_vector`, so the allocation is opt-in rather than the default every caller inherits.

*Confidence: high.*

### HIGH | Renderers/RtShadowRenderer.cppm:23-26 | BLAS cache retains a raw `const mesh*` obtained from a per-frame channel snapshot

```cpp
	struct blas_entry {
		const mesh* source = nullptr;
		gpu::blas blas;
	};
```

**Impact.** The pointer is both the cache-validity key (`RtShadowRenderer.cpp:97, 182`) and dereferenced to fetch vertex/index buffers during rebuild (244-253). After a model is unloaded the pointer dangles; if a new mesh lands at the same address the stale BLAS is silently accepted as current, and ray-traced shadows/GI trace geometry no longer in the scene. Nothing fails — the wrong answer is delivered consistently.

**Mechanism.** `mesh_ptr` comes from `batch.key.model_snapshot->meshes()[...]` — a `shared_ptr` owned by that frame's `geometry_collector::render_data` channel payload. `d.blas_cache` is system state outliving every payload and holds only the raw address. It is also never erased, so entries for unloaded models persist forever, compounding the aliasing window and leaking one BLAS per stale entry.

**Resolution.** Key cache validity on something with an owned identity — store the owning `std::shared_ptr<const model>` in `blas_entry` alongside the mesh index, or a mesh generation/upload id. Then add eviction driven by the render queue, which fixes both the aliasing and the leak.

**Prevention.** The guide's "deferred payload retains a raw pointer obtained from a shared view" rule, with a cache instead of a callback. Making `blas_entry` hold the `shared_ptr` makes the invariant structural.

*Confidence: high.*

### HIGH | Renderers/PhysicsDebugRenderer.cpp:406-409 | Unchecked `reinterpret_cast` and index into a GPU snapshot readback

```cpp
			const auto* snapshot_states = reinterpret_cast<const vbd::body_state*>(bytes.data());
			const std::uint32_t snapshot_body_count = ps.gpu_solver.body_count();
			d.cpu_body_staging.resize(snapshot_body_count);
			for (std::uint32_t i = 0; i < snapshot_body_count; ++i) {
```

**Impact.** `bytes` is the snapshot buffer's mapped span; `snapshot_body_count` is the solver's *current* body count. The two are from different points in time — after a spawn the count grows before the older snapshot buffer does, and the loop reads past the end of mapped GPU memory. Nothing bounds-checks them against each other. The related defect is downstream: `build()` (456) takes `body_index` straight from `d.body_index_map` — populated from `ps.id_to_body_index`, which may name indices beyond `d.cpu_body_staging.size()` — and hands it to the shader, which does `body_state body = body_data[inst.body_index]` with no clamp.

**Mechanism.** `host_read()` returns a `std::span<const std::byte>` whose size is known and discarded; the loop bound comes from an unrelated accessor.

**Resolution.** Derive the count from the data: `const auto available = bytes.size() / sizeof(vbd::body_state);` and iterate `std::min(available, snapshot_body_count)`. Replace the `reinterpret_cast` with a typed accessor on `gpu::buffer` (`host_read<T>()` returning `std::span<const T>`) so the size relationship is computed once inside the type. Reject `body_index >= d.cpu_body_staging.size()` in `build()`.

**Prevention.** The typed `host_read<T>()` span removes both the cast and the ability to state a length independently of the buffer.

*Confidence: high.*

### HIGH | Renderers/PhysicsTransformRenderer.cpp:96-100, :121-123 | Stale `cached_mapping_count` dispatches against a mapping buffer that was not refreshed

```cpp
	if (!render_items.empty() && render_items[0].physics_mapping_count > 0) {
		const auto& data = render_items[0];
		const auto required = data.physics_mapping_count * sizeof(geometry_collector::physics_mapping_entry);
		d.cached_mapping_count = data.physics_mapping_count;
```
```cpp
	if (d.cached_mapping_count == 0) {
		co_return;
	}
```

**Impact.** On any frame where the geometry collector publishes no physics mappings, the update block is skipped but `d.cached_mapping_count` keeps its previous value, so the dispatch still runs. It writes transforms for a mapping set that no longer describes the current `gc_r.instance_buffer[frame_index]` — instances receive another instance's body transform, and objects visibly snap to the wrong place for a frame.

**Mechanism.** The count is cached in system state independently of the buffer it describes, and `d.mapping_buffers[frame_index]` is host-written only inside the guarded block, so a skipped frame leaves that slot holding an older generation while the count claims it is current.

**Resolution.** Do not cache the count. Derive both count and buffer from the same channel read every frame and `co_return` when the channel has no mappings. If retaining across frames is genuinely wanted, store the count *inside* the per-frame buffer record so they cannot be updated separately.

*Confidence: high.*

### HIGH | Renderers/PhysicsTransformRenderer.cpp:22-26 | The physics-mapping GPU ABI is declared twice, in two modules, with no link between them

```cpp
	struct [[= shaders::shader_struct]] physics_mapping {
		std::uint32_t body_index;
		std::uint32_t instance_index;
		vec3f center_of_mass;
	};
```

**Impact.** `geometry_collector::physics_mapping_entry` (`GeometryCollector.cppm:139-143`) declares the same record with `vec3<length> center_of_mass`. The producer writes and strides by `sizeof(geometry_collector::physics_mapping_entry)` (98, 107, 117) while the shader binding declares `element = physics_mapping`. They agree in size today by coincidence; any reorder, insertion, or type change on one side silently feeds garbage centres of mass to the transform compute shader, and the symptom is misplaced geometry with no diagnostic. The shader-facing copy also drops the unit type, so the producer's dimensional checking stops at the module boundary.

**Resolution.** Delete `physics_mapping` and annotate `geometry_collector::physics_mapping_entry` with `[[= shaders::shader_struct]]`, using it directly as the binding element. One declaration, one stride, unit type preserved — unit types are layout-compatible with their scalar, so the shader side is unaffected.

**Prevention.** A `shader_struct` is declared once, next to the producer that fills it, and imported by the consumer — never re-declared shader-side. Where a genuine second declaration is unavoidable, a `static_assert` on `sizeof` and each `offsetof` is the minimum.

*Confidence: high.*

### HIGH | Renderers/SkinRenderer.cpp:187-189 | Deformed-vertex ping-pong index advanced in `collect` but written in `frame`, which can bail out

```cpp
			if (const auto target = d.targets.find(std::pair(owners[i], m)); target != d.targets.end()) {
				target->second.write_index ^= 1u;
			}
```

**Impact.** `frame()` returns early on `!d.initialized || d.instances.empty()` (218) and `d.bone_bindings.empty()` (224). When `collect` has already flipped `write_index` and `frame` bails, `deformed_slots_for` reports the un-written buffer as `current` and the freshly-written one as `previous`. Consumers get last-generation vertices as current and inverted motion vectors — a one-frame geometry pop and wrong TAA/velocity.

**Mechanism.** One ping-pong state machine (`write_index`, `writes`, `vertices[2]`) driven from three places: `collect` flips it, `frame` resets it on reallocation (304) and increments `writes` (324). No single owner enforces "flip exactly once per completed write".

**Resolution.** Flip `write_index` in `frame()` immediately before `rec.dispatch`, so an aborted frame cannot advance it. Better, hoist the ping-pong into a small type owning `{buffers[2], write_index, writes}` with a `begin_write()` returning the slot and advancing atomically.

**Prevention.** `RtShadowRenderer` and `PhysicsDebugRenderer` grow the same ad-hoc ping-pong/capacity state and would collapse into the same helper.

*Confidence: medium-high — mechanism certain; whether `frame`'s early-outs fire in practice depends on scene content.*

### HIGH | Renderers/SkinRenderer.cppm:68 | `d.targets` is never pruned, so every entity that ever had a skinned mesh holds two GPU vertex buffers forever

```cpp
		[[= gse::shared]] deformed_target_map targets;
```

**Impact.** `collect` clears `instances`, `bone_bindings` and `bounds` each frame but never touches `targets`. `frame()` inserts via `d.targets[std::pair(...)]` (288) and nothing erases. In a scene that spawns and despawns skinned characters, VRAM grows without bound — and because of C1, even a reallocation of an existing entry leaks rather than replaces.

**Resolution.** Prune in `collect`, where the live instance set is already being rebuilt: after filling `d.instances`, erase `targets` entries whose key is absent. `std::flat_map` makes a bulk `erase_if` cheap.

**Prevention.** `RtShadowRenderer`'s `blas_cache` has the same shape. Any per-entity GPU cache is rebuilt or reconciled in the pass that rebuilds the entity list, never inserted-only.

*Confidence: high.*

### HIGH | Renderers/AtmosphereRenderer.cpp:302-310 | Long-lived swap-chain callbacks capture a `shared_view` and a reference to system state

See [C4](#c4-deferred-callbacks-retain-raw-references-to-system-state). Also `CloudRenderer.cpp:277-282` and `GiProbeRenderer.cpp:130-135` (which additionally captures `rt_state`). *Confidence: high.*

### HIGH | Renderers/PhysicsDebugRenderer.cpp:401-417 | Debug rendering does a full GPU→CPU→GPU round trip of the physics state every frame, on by default

```cpp
	const bool use_snapshot = ps.use_gpu_solver && ps.gpu_solver.buffers_created() && ps.gpu_solver.body_count() > 0;
	if (use_snapshot) {
		const auto safe_slot = 1u - ps.gpu_solver.latest_snapshot_slot();
		const auto bytes = ps.gpu_solver.snapshot_buffer(safe_slot).host_read();
```

**Impact.** `enabled` defaults to `true` (`PhysicsDebugRenderer.cppm:36`), so the shipped default reads the entire body-state snapshot out of host-visible device memory each frame, copies position+orientation into `cpu_body_staging`, rebuilds `body_index_map` by copying all of `ps.id_to_body_index` (413-415), then re-uploads the whole staging array to the GPU (551). On discrete GPUs the readback traverses uncached write-combined memory, roughly an order of magnitude slower than normal reads. The data is already resident on the GPU and could be bound directly.

**Mechanism.** `prepare()` copies GPU state to the CPU purely so the debug shader can index it from a *different* buffer the CPU then writes. Nothing between the readback and the upload transforms the data except discarding fields.

**Resolution.** Bind `ps.gpu_solver.snapshot_buffer(safe_slot)` to the `body_data` binding directly and delete `cpu_body_staging`, `cpu_body_buffers`, and `cpu_body_capacity`, keeping the CPU fallback (427-443) only for the non-GPU-solver path. Separately drop `d.body_index_map` and look up in `ps.id_to_body_index` at 452. Also worth revisiting whether `enabled` should default to `true`.

**Prevention.** A readback in a per-frame path needs a named consumer that is not the GPU.

*Confidence: high.*

### MEDIUM findings — effect and auxiliary renderers

- **SdfGridRenderer.cpp:87-97** — Every renderer maintains a private camera UBO and recomputes the same two matrix inverses per frame. Byte-identical blocks at `WorldTextRenderer.cpp:221-231` and `PhysicsDebugRenderer.cpp:519-529` among these files, and at `DepthPrepassRenderer.cpp:115`, `ForwardRenderer.cpp:279`, `OitRenderer.cpp:222` outside them — six renderers, each with its own `per_frame_resource<gpu::buffer> camera_ubo_buffers`. Twelve 4×4 inversions and twelve buffer uploads per frame for one fact, plus twelve GPU buffers holding identical bytes. Worse than the cost: any renderer reading `cam_state` at a different scheduling point produces a *different* camera block, so passes can disagree about the camera within one frame. Have `camera::data` own the per-frame `camera_data` and its buffer and publish the bindless slot as `[[= gse::shared]]`. *high*
- **AtmosphereRenderer.cpp:381-394, 415-437** — Atmosphere and cloud passes sample bindless images without declaring the read to the graph: `sky_view_lut` is written by the sky-view compute pass and read the same frame by `sky_raster_pass` (432) and `cloud_raymarch_pass` (`CloudRenderer.cpp:366`); `transmittance_lut`/`multiscatter_lut` at 387-388, 406-407 and `CloudRenderer.cpp:365`; `shape_noise`/`detail_noise` at `CloudRenderer.cpp:367-368`. None calls `rec.sample_image`, which is the *read declaration* (`GpuRecord/RecordingContext.cpp:193-201` calls `note_touched(..., shader_sampled_read)` and `transition_image_for_binding(..., resource_state::sampled, ...)`) — not a manual barrier. Without it the graph records no read and performs no layout transition or write→read barrier; only the execution ordering from `.after<>()` remains. Every peer renderer calls it for externally-created bindless images (`ForwardRenderer.cpp:400`, `BloomRenderer.cpp:239`, `TaaRenderer.cpp:187`, `OitRenderer.cpp:320`, and `CloudRenderer.cpp:386` for its own target). Deriving the read declaration from the binding arg at `push_bindings`/`dispatch` time would make the omission impossible. *medium — whether `.after<>()` alone happens to emit a sufficient barrier in the current graph implementation was not traced.*
- **SceneSnapshotRenderer.cpp:30** — `recreate_resources` defined inside the namespace block, making it order-dependent on its callers. The only such site across all 22 files in this section. *high*
- **SceneSnapshotRenderer.cpp:62-64** — `run` is `return {};` but declares `data& d` (write access) and `shared_view<gpu::context::data>`, so the scheduler must serialize a no-op against every other system touching that state. Delete it from both the `.cppm` (30-35) and the `.cpp`. *high*
- **SceneSnapshotRenderer.cpp:31-35** — Bindless texture slots dropped, not released, on every recreate; independent of and additional to C1. See C1's closing note. *high*
- **AtmosphereRenderer.cppm:189** — `bool luts_ready = false` is a one-shot latch, so `frame()` uploads the full `atmosphere_data` payload every frame (330) while computing the transmittance and multiscatter LUTs only once (332-363). Changing `bottom_radius`, `top_radius`, `rayleigh_scale_height`, `mie_*`, `ozone_*` or the scattering/absorption coefficients through settings updates the UBO the sky-view and AP passes read while leaving the LUTs derived from the *old* values — a physically inconsistent sky with no error. Store the `atmosphere_data` the LUTs were baked from and re-run on difference; the payload is already materialised each frame at 329. (`CloudRenderer.cppm:179` `noises_ready` has the same shape, benign because the noise bake takes no parameters.) *high*
- **GiProbeRenderer.cpp:184** — `.sky_color = vec3f{ 0.5f, 0.7f, 1.0f }` — a fixed daylight blue used by the shader as both miss radiance and ambient bounce (`gi_probe_update.slang:31,51`), in a function that already takes `shared_view<atmosphere::data>` and reads `sun_direction`, `sun_intensity` and `sun_color` from it (180-182). Indirect lighting does not respond to time of day; at night the GI is still lit by a bright blue sky. `sky_view_lut` is `[[= gse::shared]]` and already available. *high*
- **GiProbeRenderer.cpp:163** — The probe grid origin follows the camera unsnapped, so each probe's world position moves by a sub-spacing amount every frame and re-samples a different point. Combined with `fibonacci_sphere(..., pc.frame_counter)` re-randomising all 64 ray directions per frame and `irradiance_atlas_out[atlas_xy] = ...` overwriting rather than blending, the atlas is uncorrelated frame to frame and indirect diffuse will visibly boil. The temporal jitter exists but nothing accumulates it. Snap the origin to the probe lattice *and* blend the new estimate into the atlas — snapping alone is insufficient. The hardcoded `meters(1.0f)` grid Y should also be a setting. *medium-high.*
- **GiProbeRenderer.cppm:23-28** — `quality_level` exposes four values, three indistinguishable: `quality` is only ever compared against `off` (141, `ForwardRenderer.cpp:385`), and `low`/`medium`/`high` produce identical `rays_per_probe`, `grid_dim` and dispatch. The enum was modelled on `bloom::quality_level`, which does map each enumerator to behaviour, but the mapping was never written. Either implement the levels (with the metadata on the enumerators as annotations) or replace with `bool enabled`. *high*
- **GiProbeRenderer.cpp:158-159** — Camera world position recovered by inverting the view matrix, duplicating `camera::position` (`CameraSystem.cppm:65-67`). *high*
- **RtShadowRenderer.cpp:23, 261-286** — `constexpr bool use_gpu_tlas_transform_update = true` guards five `if constexpr` sites (163, 199, 213, 261) that are never anything but taken; the `else` branch at 282-286 is a complete second TLAS path never built and never tested, which must stay compilable. Line 55 logs the value of a compile-time constant at startup. Delete the constant, the `else`, and the guards. *high*
- **RtShadowRenderer.cpp:112-118** — On the frame a model becomes upload-ready, the render thread scans that mesh's entire CPU index array to compute a maximum, then formats and emits a log line per mesh — a visible hitch when a scene streams in many meshes, and debug residue with no gating. Emit the log only when `indices_out_of_range` is true (already computed at 117) and use `std::ranges::max`. If the check is a genuine safety net it belongs at mesh upload time. `GiProbeRenderer.cpp:119-128` has the same shape. *high*
- **UiRenderer.cpp:214-215** — `std::vector<unified_command> unified` heap-allocates per UI frame, sized by the full command count, of a struct (`UiRenderer.cppm:85-104`) roughly 180 bytes carrying every sprite field *and* every text field, half dead for any given element — and making invalid combinations (a text command with a `corner_radius`) fully representable. On top of that `add_text_quads` calls `font_view->text_layout(...)` (129) which returns a fresh `std::vector` per text command. Use `linear_vector` (the frame-arena container this file already uses for `gpu_frame_data`); better, sort indices into the two source channels by `(layer, z_order, type)` rather than materialising a merged copy at all. *high*
- **UiRenderer.cpp:382-389** — `orthographic(meters(0.0f), meters(static_cast<float>(width)), ...)` — `width`/`height` are framebuffer pixels, so wrapping them in `meters()` states a dimension that is false and carries it into every UI vertex transform. This is the escape-hatch-as-compliance shape one layer earlier: a *constructor* rather than an `.as<>()`. `WorldTextRenderer.cpp:89-92` converts glyph pixel coordinates to `meters` for the same reason, so a second caller already exists. Add a screen-space `orthographic` overload, or introduce a `pixels` unit if screen-space arithmetic is worth checking. *high*
- **UiRenderer.cpp:505-515** — Clip-rect scissor can produce an offset outside the framebuffer: `const float top = std::min(window_size.y(), clip_rect->top());` does not clamp a negative `top`, so `window_size.y() - top` exceeds the framebuffer height and `vkCmdSetScissor` violates `offset.y + extent.height <= height`. The four edges are each clamped against only one side, so a fully off-screen rect survives with an out-of-range Y origin. Intersect the clip rect with the viewport using `rect_t`'s own intersection and skip the batch when empty — a `scissor_from(rect, viewport)` returning `std::optional` makes the invalid case unrepresentable and gives the batch loop the "skip" answer it needs. *medium-high — arithmetic certain; whether the GUI ever emits a fully-off-screen clip rect was not traced.*
- **UiRenderer.cppm:19-155** — Three namespace blocks, two exporting, plus per-declaration `export` inside a non-export block (72-77). Determining what the module exports requires reading all three and scanning for `export` keywords. Line 77's `export constexpr std::size_t frames_in_flight = 2;` duplicates `per_frame_resource<T>::frames_in_flight` (`Concurrency/PerFrameResource.cppm:19`); `d.gpu_frames` is sized by the local constant but indexed by `render_graph->current_frame()` (371-372), so divergence goes out of bounds silently. *high*
- **UiRenderer.cppm:34-36** — `text_command::text` is a `std::string_view` pushed by the GUI (`Gui.cpp:735`) and consumed later by `ui::run`, which copies it into `unified_command` and dereferences it in `add_text_quads`. Validity depends entirely on every producer keeping the backing string alive until the UI system runs. String literals are safe; a label produced by `std::format` is a use-after-free that reads as glyph corruption. Make the payload own its text, or have the GUI copy label text into the frame arena. *medium-high — hazard structural; no currently-dangling producer identified.*
- **SkinRenderer.cpp:217-222 and PhysicsTransformRenderer.cpp:78** — Both call `render_graph->current_frame()` and open a pass without first checking `frame_in_progress()`. Every other renderer in these files checks it (`AtmosphereRenderer.cpp:316`, `CloudRenderer.cpp:288`, `GiProbeRenderer.cpp:145`, `RtShadowRenderer.cpp:77`, `SdfGridRenderer.cpp:79`, `SceneSnapshotRenderer.cpp:67`, `UiRenderer.cpp:361`, `WorldTextRenderer.cpp:190`, `PhysicsDebugRenderer.cpp:502`). The states where no frame is in progress — minimised window, swapchain out of date, headless/attached mode — are exactly the ones where a frame index is meaningless. A precondition repeated at every call site will be missed; fold it into `gpu::pass<T>` or have `current_frame()` return an optional. *high*
- **PhysicsDebugRenderer.cppm:68** — `std::unordered_map<id, std::uint32_t> body_index_map` is a parallel ID-to-index map alongside contiguous storage where `id_mapped_collection` owns the invariant. Same class as `std::flat_map<std::pair<id, std::uint32_t>, ...>` in `RtShadowRenderer.cppm:21` and `SkinRenderer.cppm:57`, where call sites read `it->second` and construct keys positionally (`std::pair(owners[i], m)`) with no field names to prevent a swap. Replace `std::pair` with a named aggregate carrying `operator<=>`. *high*
- **CloudRenderer.cppm:155-159** — `wind_offset` is typed `vec3<atmosphere_length>` but described as `"Cloud wind velocity in world space (km/s)"`, and the shader receives it as a static offset with no time integration anywhere on the CPU side. Whichever is wrong, a user setting this expecting wind gets a fixed displacement, and the settings UI labels a length as a velocity. The unit type is doing its job — it is the *description* that escaped checking. Deriving the unit shown in settings UI from the field's quantity type rather than hand-written text would make this contradiction unwritable. *high*
- **WorldTextRenderer.cpp:204** — `std::max(1, static_cast<int>(grid_d.fade_distance / grid_d.major_spacing))` — `major_spacing` (`SdfGridRenderer.cppm:29`) carries no `settings::range` annotation and settings.ini overrides code defaults, so `major_spacing = 0` makes the ratio infinite and the cast undefined, while a small non-zero value makes `max_ticks` enormous and line 207 reserves `max_ticks * 32` vertices. Either path takes the process down from a config file. Add `settings::range` and clamp regardless. *high*
- **WorldTextRenderer.cpp:186, :208** — `world_text` reads `enabled`, `show_labels`, `label_size`, `label_color`, `major_spacing` and `fade_distance` from `sdf_grid::data` and owns none of them, while `sdf_grid::data` carries four fields its own `frame()` never uses. Neither system's state describes what it does. Move `show_labels`, `label_size` and `label_color` into `world_text::data`; `major_spacing`/`fade_distance` are legitimately shared. *high*
- **RtShadowRenderer.cpp:53** — `assets_s` and `ctx` unused; same in `SkinRenderer.cpp:103`, `PhysicsTransformRenderer.cpp:70`, and `ctx` alone in `AtmosphereRenderer.cpp:244`, `CloudRenderer.cpp:229`, `GiProbeRenderer.cpp:114`, `SdfGridRenderer.cpp:54`, `WorldTextRenderer.cpp:155`. Each declared `shared_view` is a real scheduling dependency forcing an ordering edge nothing needs. Since these signatures *are* the dependency declaration, an unused-parameter warning on system entry points catches every instance mechanically. *high*
- **PhysicsDebugRenderer.cpp:177-186** — Unit-shape vertex lanes are not (x, y, z, w): the shader reads them as `x*radial`, `y*axial + z*radial`, `w*depth`, so lane 2 is a second Y contribution scaled by the radial extent and lane 3 is the Z coordinate. Nothing on the C++ side records this; with comments banned, the three generator functions are unreadable without opening the shader, and a plausible edit (writing z into lane 2) produces flat spheres. Declare a `[[= shaders::shader_struct]] unit_shape_vertex { float radial_x; float axial_y; float radial_y; float depth_z; }` and build the tables with designated initialisers. *high*
- **UiRenderer.cppm:79-83** — `max_vertices` is 131,072 and `vertex` is 52 bytes, so each `gpu_frame_data` holds ~6.8 MB of vertices plus ~0.8 MB of indices; `triple_buffer` instantiates three and `d.gpu_frames` adds two GPU buffers of the same size — roughly 38 MB reserved eagerly for a worst case typical UI never approaches. The overflow path already degrades gracefully (84, 130 both stop appending), so a smaller ceiling drops quads rather than corrupting anything. *high*

### LOW findings — effect and auxiliary renderers

- **Vertical alignment and bin-packed argument lists** — `AtmosphereRenderer.cpp:245-246`; `PhysicsDebugRenderer.cpp:141-152` (the `edges` initialiser, eleven aligned continuation lines), `:639-642` (`draw_shape(...)` aligned, between a fully-inline call at 638 and a fully-wrapped one at 643-648 — three formattings of one call in eleven lines), `:553-559`; `UiRenderer.cpp:391-392`; `WorldTextRenderer.cpp:122-123`. *high*
- **Positional aggregate initialisation** — `UiRenderer.cpp:110-113` and `:149-152` (six positional fields, three of them adjacent `vec2f`); `WorldTextRenderer.cpp:104-109`; `PhysicsDebugRenderer.cpp:244-245` (also a redundant type name). *high*
- **RtShadowRenderer.cpp:81, PhysicsTransformRenderer.cpp:97** — Local named `data` shadows the system-state type of the same name, both in scope. *high*
- **PhysicsDebugRenderer.cpp:403** — `1u - ps.gpu_solver.latest_snapshot_slot()` is silently wrong if the solver ever uses three slots and underflows to a huge value if the accessor ever exceeds 1. Ask the solver for the safe slot. *high*
- **RtShadowRenderer.cpp:237** — Bare `64` for the TLAS instance stride (the shared 64-byte ABI) while the same file uses `sizeof(shaders::common::instance_data)` for the neighbouring stride at 269. *high*
- **CloudRenderer.cpp:361** — `static_cast<float>(d.frame_counter)`: past 2²⁴ frames (~78 hours at 60 fps) consecutive counter values map to the same float and the shader's temporal jitter freezes. *high*
- **PhysicsTransformRenderer.cppm:20** — `bool initialized = false` set in `init` (73) and never read. `SkinRenderer.cppm:64` has the same field and does read it, but it is derivable from `d.palette_pipeline.valid()`. *high*
- **SceneSnapshotRenderer.cppm:18-21** — `ready` is exactly `current_extent != vec2u{0,0}` and is only ever set true (never reset on failure), so the two can disagree; `enabled` carries no settings annotation and is never assigned anywhere, so its two guards (71, 56) are permanently taken. *high*
- **AtmosphereRenderer.cppm:101** — `[[= gse::shared]] vec3f sun_direction` is stored published state recomputed from `sun_azimuth`/`sun_elevation` each frame (`AtmosphereRenderer.cpp:322`) and consumed by cloud and GI probe. Between a hot-reload of the angles and the next atmosphere frame the three disagree, and the initial value `{0,1,0}` matches neither default. *high*
- **UiRenderer.cpp:409-418 and WorldTextRenderer.cpp:253-257** — Text-shadow constants hardcoded twice with different values (`1.0/0.7/0.45` vs `1.5/0.6/0.55`): two MSDF text renderers with independently invented shadow parameters, neither a setting, neither referencing the other. *high*
- **UiRenderer.cpp:268, :280** — `static_cast<std::uint8_t>(a.layer) < static_cast<std::uint8_t>(b.layer)` — scoped enums support relational operators directly; and `a.texture.id().number() < b.texture.id().number()` exits the `id` type to compare. *high*
- **WorldTextRenderer.cpp:88, PhysicsDebugRenderer.cpp:254, 261, 276** — Unexplained magic quantities (`meters(0.001f)` lift; `meters(0.05f)` as both a penetration normalisation scale and a contact cross size; `meters(0.15f)`/`meters(0.5f)` normal-arrow bounds). Correctly unit-typed; the issue is that each is a tuning value with no name beyond its local `const`. Name them at file scope in the module-private namespace — not `constexpr` at namespace scope, per the BMI rule. *high*
- **WorldTextRenderer.cpp:70-77** — Trailing `bool along_x` on a six-argument helper; call sites read `..., max_ticks, true)` (208-209). An `axis` enum or a parameter aggregate. *high*
- **GiProbeRenderer.cpp:103-111** — `fi` exists only to index `rt_state.tlas_ptrs`, which accepts `std::size_t`. `SkinRenderer.cpp:122` and `PhysicsDebugRenderer.cpp:432` likewise index components and a parallel `owner_ids()` array by raw index where a zipped range would bind them. *high*
- **CloudRenderer.cppm:24-25** — `using atmosphere_length = atmosphere::atmosphere_length;` and its sibling create two exported names for one type in the same module. *high*
- **Double blank lines after the import block** — `AtmosphereRenderer.cpp:9-10`, `CloudRenderer.cpp:11-12`, `GiProbeRenderer.cpp:11-12`, `RtShadowRenderer.cpp:9-10`, `SdfGridRenderer.cpp:11-12`, `SceneSnapshotRenderer.cpp:12-13`, `UiRenderer.cpp:14-15`, `WorldTextRenderer.cpp:12-13`, `PhysicsTransformRenderer.cpp:7-8`, `PhysicsDebugRenderer.cpp:24-25`. *high*
- **WorldTextRenderer.cpp:137-139, PhysicsDebugRenderer.cpp:287-289, 306-308** — `if (buf.valid()) { buf = {}; }` immediately followed by `buf = device.create_buffer(...)`: the intermediate assignment does nothing the subsequent one does not, and is the shape of a release that never happens (C1). *high*

### Clean — effect and auxiliary renderers

`SdfGridRenderer.cppm`, `SdfGridRenderer.cpp`, `PhysicsDebugRenderer.cppm`, `SkinRenderer.cppm`, `RtShadowRenderer.cppm`, and `AtmosphereRenderer.cppm` (aside from the `luts_ready` and `sun_direction` findings) carry no findings of their own beyond the grouped style items and the engine-wide classes above.

### Verified correct — effect and auxiliary renderers

Recorded so they are not re-investigated. `PhysicsDebugRenderer`'s unit box/sphere/capsule meshes are generated once in `init` (343-384), **not** per frame — the suspected per-frame debug-mesh rebuild does not occur. `UiRenderer`'s batch-break logic is correct: `bindless_slot`'s default index is `UINT32_MAX`, not 0, so the first-command and invalid-slot comparisons behave. The `^^gse::renderer::X::frame` qualification is the established engine-wide convention for function-valued pass tags (14 sites) and is not a redundant-qualifier violation. All push-constant structs in these files satisfy 16-byte vector alignment and fit the 256-byte budget.

---

## 8. 3D scene and animation

Files: `3D/Camera/{CameraComponent,CameraData,CameraSystem}.{cppm,cpp}`, `3D/Lights/{DirectionalLight,PointLight,SpotLight}.cppm`, `3D/{Material,Mesh,Model,Primitives,PrimitiveResolver,PrimitiveSpecs}.*`, `3D/Animations/{AnimationComponents,BlendSpace,Clip,ClipPlayer,SkinnedModel,SourceReader,Ragdoll}.*`.

No critical findings. **No `.as<Unit>()` exits exist anywhere in this file set**, and the suspected sampling-logic duplication between `ClipPlayer`, `BlendSpace` and `SkinnedModel` does not exist — weights, sampling, and skinning each have one home.

### HIGH | 3D/Animations/ClipPlayer.cpp:198-200 | Unvalidated baked rig topology indexes a 32-element stack array

```cpp
			const auto& parent = bone.parent == skinned_model::no_parent
				? root
				: joints[bone.parent];
```

**Impact.** A malformed or badly exported `.gsmdl` produces an out-of-bounds read of a stack array in a per-frame loop — garbage poses at best, a crash at worst. Malformed `.gsmdl` files have already occurred in practice in this project.

**Mechanism.** `SkinnedModel.cppm:122` reads `bone.parent = reader->u16();` and neither `bake` nor `skinned_model::load` validates that every parent is `no_parent` (0xFFFF) or `< slot`. `joints` is `std::array<joint_transform, skeleton_instance_component::max_bones>` (32), so any parent in [32, 65534] reads past it. The loop bound `slot_count = std::min(bones.size(), joints.size())` bounds the *iteration* index, not the parent value, and the `slot >= skeleton->bone_count` guard at line 204 fires only *after* the OOB read. The compose loop additionally assumes topological order (parent slots already composed) — also unvalidated. Same trust class: `skinned_vertex::bone_slots` (uint8, `SkinnedModel.cppm:156`) is uploaded to a bindless storage buffer unchecked; a slot ≥ bone count becomes a GPU out-of-bounds palette read.

**Resolution.** Validate in `skinned_model::load` (once, at the authority): reject the asset unless every `bone.parent == no_parent || bone.parent < slot_index`, and clamp/reject `bone_slots >= bones.size()`. Loading is the correct layer — the per-frame loop stays branch-free.

**Prevention.** Asset trust is a recurring class. The proportionate guardrail is a validation pass owned by the asset layer per baked type (a `validate()` alongside `load_baked`), not per-consumer checks.

*Confidence: high.* **[verified — `joints` extent, loop bound, and unvalidated `bone.parent` all confirmed against source]**

### MEDIUM findings — 3D scene and animation

- **3D/Animations/Ragdoll.cpp:56** — `for (std::uint32_t i = 0; i < skeleton->bone_count && i < bones.size(); ++i)` bounds by the *model's* rig size (unbounded by 32), not by `skeleton->bones` (`std::array<id, 32>`), so `bone_count > max_bones` reads out of bounds. `bone_count` is `[[= networked]]` (`AnimationComponents.cppm:21`) and replication can deliver any value; the invariant lives only in a remote writer (`Sandbox/RuntimeSpawns.cppm:873-905`). `ClipPlayer.cpp:91` clamps the analogous `layer_count` correctly — the same derivation done two ways. A clamped span accessor on the component makes the invalid iteration unwritable for every consumer. *high*
- **3D/Animations/ClipPlayer.cpp:69-216** — ClipPlayer never consults `ragdolling` and relies on Ragdoll's `playing = false` side effect. For every ragdolling character the full per-bone pipeline still runs each frame — layer acquire, binding lookup, keyframe binary searches, quaternion blending, compose, kinematic-target writes — all discarded, because `physics::apply_kinematic_targets` (`Physics/System.cpp:603`) skips non-kinematic bodies. Also a latent divergence: gameplay setting `playing = true` on a ragdolling character makes the animation fight the flip logic. `if (skeleton->ragdolling) { continue; }` at the top of the per-player body; Ragdoll then no longer needs to touch `player->playing` — one field, one owner. *high*
- **3D/Camera/CameraComponent.cppm:16** — `bool use_entity_position = true` is a dead flag callers actively set. `camera::run` unconditionally uses `cam.position + cam.offset`; repo-wide search finds two writers (`Sandbox/Shared/OrbitCamera.cppm:99`, `Examples/Object/FreeCamera.cppm:112`, both `= false`) and zero readers. Anyone relying on the default `true` gets a camera frozen at the origin with no diagnostic. Both current callers happen to match the actual behaviour, so nothing fails today. *high*
- **3D/Animations/Clip.cppm:103-104** — `out.tracks.resize(reader->u32())` and the per-track `track.keys.storage.resize(reader->u32())` (108) allocate from unvalidated counts, so a corrupt or truncated `.gclip` triggers a multi-gigabyte allocation — `bad_alloc`/OOM instead of the `false` the function is designed to return. Paired-derivation divergence with the sibling reader: `SkinnedModel.cppm:161-163` guards `if (index_count > reader->remaining() / sizeof(std::uint32_t)) { return false; }` and grows with `emplace_back` bounded by `!reader->overran()`. Only one copy of the defensive pattern was updated. The guard belongs in `source_reader` itself — a `count(std::size_t element_size)` accessor returning 0 and setting `m_overran` when the claimed count exceeds `remaining()` — so every format reader inherits it. *high*
- **3D/Model.cppm:121-130** — Model center-of-mass computed on the GPU thread: only `mesh.initialize` needs the GPU context, but the COM loop (which iterates every triangle with double-precision tetrahedron math) was placed *after* `co_await gpu::on_gpu`, so a large model stalls GPU submission for the whole scan — invisible hitching attributed to "loading". `SkinnedModel::load` gets this right (conversion loops before the await) and is the pattern to mirror. *high*
- **3D/Mesh.cppm:273 + 3D/Model.cppm:130** — Center-of-mass math is wrong for meshes not containing the origin, and the model average ignores mesh volume. `const auto volume = abs(dot(a, cross(b, c))) / 6.0;` — the tetrahedron decomposition against the origin is only valid with *signed* volumes; the negative tetrahedra are what cancel geometry outside the solid. `abs` destroys the cancellation, so any mesh whose interior does not contain the origin (and any concave mesh) gets a biased COM. The assert at 280 ("closed and correctly oriented") documents the signed-volume precondition the code then discards — and `abs` also makes that assert unreachable, since a non-degenerate mesh can no longer sum to zero. `model::load` then averages per-mesh COMs unweighted, so a pebble counts as much as a building. Drop `abs` (take it only of the final `total_volume`), expose the total volume from `mesh::center_of_mass`, and produce the volume-weighted centroid. A focused test with an off-origin cube pins it permanently. *high on the mechanism; medium on observable severity, since bind-pose meshes usually straddle the origin.* **[verified]**
- **3D/Animations/ClipPlayer.cpp:138-139** — Non-looping clips loop: `player.phase -= std::floor(player.phase)` wraps unconditionally, and `clip_asset::loops()` (`Clip.cppm:149`) has zero callers though the format carries the flag (101, 135). A clip baked with `loops = false` — a death or flinch one-shot — restarts every period. Either honour it (needs a policy for mixed-layer blends) or delete `loops` from `baked`, the accessor, and the bake read; keeping a stored-but-ignored behaviour flag is the worst of both. *high that it is unconsumed; the intended semantics need an owner decision.*
- **3D/Camera/CameraSystem.cpp:149** — `const float t = std::clamp(d.blend_elapsed / d.blend_duration, 0.f, 1.f);` — `blend_in_duration = time{}` (a natural way to spell "hard cut") combined with a `dt == 0` frame yields `0/0 = NaN`, and `clamp` passes NaN through, so the camera target, view matrix, and jitter go NaN for the frame. With `dt > 0` the division saturates to infinity, clamps to 1, and self-heals — so the bug hides until the dt-freeze coincidence, which attached mode demonstrably produces. Per this engine's history, NaN reaching the GPU can hang the rasterizer. Treat non-positive `blend_duration` as an immediate cut. *high*
- **3D/Camera/CameraSystem.cpp:28-32** — `direction_relative_to_origin` hand-expands the quaternion sandwich product that `rotate_vector` owns — and line 158 of the same file uses `rotate_vector(d.current.orientation, ...)` for the identical operation. Two derivations of one fact in one file. *high* **[verified]**
- **3D/Camera/CameraData.cppm:20-26** — `camera::request` is an exported dead type: repo-wide search finds no reference to `requester_id` or the type outside its definition, and `camera::run` reads only `ui_focus_request`, `viewport_update`, `camera_yaw_request`, and `follow_component`. Future authors will push into a void. Deleting it also removes the `target target{}` member-shadows-type oddity. *high*
- **3D/Animations/Clip.cppm:67 + ClipPlayer.cpp:47** — `m_track_by_joint` is rebuilt on every clip load (`Clip.cppm:138-141`) and `track_for` (157-163) has zero callers; the one consumer that resolves joint names, `binding_for`, does its own `std::ranges::find` linear scan instead. Two joint-name resolution paths, only the unused one indexed. Since `binding_for` is cached per (model, version, clip, version) and runs once, the linear scan is fine and the map is pure overhead. *high*
- **3D/PrimitiveSpecs.cppm:12-17** — Most of `material_spec` is dead and it replicates: `roughness`, `metallic`, and all three optional texture-name strings have zero readers — `attach_box`/`attach_sphere` read only `base_color` and `opacity` (`PrimitiveResolver.cpp:37,48`). Because `material` is `[[= networked]]` in both specs, the dead strings ride replication for every primitive entity, and authors setting roughness or textures get silent no-ops. The primitives' shared meshes make per-entity roughness impossible without a per-entity material path, so shrinking is the honest option today. *high*
- **3D/Animations/SkinnedModel.cppm:216-223 vs 3D/Model.cppm:102-109** — The `material_baked` → `material` conversion (three scalars plus three `try_get` name resolutions) exists twice with two shapes: Model does it in one designated init, SkinnedModel splits aggregate plus assignments. A change to texture resolution updates one and silently not the other. One module-private `resolve_material(asset::data&, const model::material_baked&) -> material`. *high*
- **3D/Lights/SpotLight.cppm:8** — `import :gui;` in a 24-line light component that references nothing from the GUI partition: a build-graph edge from a leaf data component to the entire 2D GUI, so every GUI edit rebuilds `:spot_light` and its importers. (Package imports like the unused `gse.log` in `Model.cppm` are excluded per the import-economics policy; a *partition* edge is a rebuild-cascade fact, not a lazy-use fact.) *high*
- **3D/Camera/CameraSystem.cpp:118-120** — `best_target.fov = degrees(45.0f); best_target.near_plane = meters(0.1f); best_target.far_plane = meters(10000.0f);` restate `target`'s member initializers (`CameraData.cppm:15-17`), and `best_target{}` (101) already carries those exact values — so the assignments are behaviorally dead today and become divergence the first time either copy changes. Same pattern: `best_blend_duration = milliseconds(300)` (102) restates `follow_component::blend_in_duration`'s default, and `{ 1920.f, 1080.f }` appears in both `viewport_update` (`CameraSystem.cppm:22`) and `data::viewport` (48). It also exposes a design gap: `follow_component` cannot express FOV/near/far at all, which is why the literals crept in. *high*

### LOW findings — 3D scene and animation

- **3D/Lights/PointLight.cppm:16 + SpotLight.cppm:18** — In `constant + linear·d + quadratic·d²`, `linear` is `inverse_length` but `quadratic` is a raw `float` where it is dimensionally per-square-meter — the one term of the polynomial easiest to get wrong is the one with no checking. If `linear` merits typing, so does `quadratic`; the unit system has `area`. *high on the inconsistency; medium on whether a shader contract motivated it — the struct is not itself a shader block.*
- **3D/PrimitiveResolver.cpp:67-99** — `populate` scans every spec every frame with `write<>` access, claiming hazards on both spec types and `render_component` and serializing against other writers even in the steady state where everything is resolved. The `resolved` flag is per-element progress state standing in for a pending queue. Keep a pending-id list fed by the same drains `ensure_renders` uses. *high on mechanism; medium on cost materiality at current entity counts.*
- **3D/PrimitiveSpecs.cppm:29, :36** — `resolved` is local while everything it gates is networked. On a client, a replicated spec arrives with `resolved = false` while the server-populated `render_component` (all `[[= networked]]`, `RenderComponent.cppm:16-34`) also replicates; the client resolver then appends a second copy (`model_count` 1 → 2, duplicate draw) until the next snapshot overwrites it. Continuous snapshot sync self-heals; event/delta sync would not. *medium — depends on whether resolver systems are scheduled on clients, which could not be confirmed statically.*
- **3D/Mesh.cppm:166, :222 vs :498-500** — Triangle-buffer padding is enforced in one place and assumed in another: the upload reads `tri_size = (triangles.size() + 3) & ~3` bytes from a vector whose 4-alignment is guaranteed only because `build_runtime_meshlets` pads. If padding is an invariant the round-up is dead; if not, it is a 3-byte OOB read. One of the two is wrong. *high on the duplication; low on current exploitability — the runtime builder is today the only producer.*
- **3D/Animations/ClipPlayer.cppm:48 + ClipPlayer.cpp:57** — The `bindings` cache keys embed `model_version`/`clip_version`, so every hot-reload mints new keys and old entries are never erased — unbounded growth over long editor sessions. Same class: `Ragdoll.cpp:44` `anchors` are inserted and never erased, including for despawned characters. *high*
- **3D/Animations/ClipPlayer.cpp:36-58** — `binding_for` returns a reference into a `flat_map` that `insert_or_assign` reallocates, so `const auto& a = binding_for(...); const auto& b = binding_for(...);` silently dangles `a`. The sole current caller survives only because it copies immediately. Return `clip_binding` by value (68 bytes). *high*
- **3D/Animations/Ragdoll.cpp:41, :85 vs ClipPlayer.cpp:52, :190** — Two derivations of "the root bone": Ragdoll assumes slot 0; ClipPlayer and `binding_for` derive it as `parent == no_parent` — and with multiple parentless bones, `binding_for`'s ground-speed picks the last while Ragdoll picks the first. Consistent only because bakes happen to emit a single root first, an invariant no code states or checks. One `root_slot()` accessor on `skinned_model`, validated at load alongside the topology check above. *high*
- **3D/PrimitiveResolver.cpp:31-51** — `attach_box`/`attach_sphere` duplicate the capacity check, slot fill and count increment, and overflow at `max_models` silently discards — the merely-consistent failure rather than the conspicuous one. *high*
- **3D/Animations/ClipPlayer.cpp:159, :187** — `quat rotation(0.f, 0.f, 0.f, 0.f)` accumulated then `normalize(rotation)`: two near-opposite rotations at equal weight can sum to ~zero, and `normalize(0)` yields NaN that propagates into kinematic targets. Sign alignment against the first sample makes this rare, not impossible (>2 layers). *medium*
- **3D/PrimitiveResolver.cpp:68 + ClipPlayer.cpp:60** — Unused `context&` parameters: `populate` takes `ctx` only to `(void)` it; `animation::run` takes `ctx` and never touches it. `camera::init(data&)` proves context is omittable from system signatures. (`clip_asset::load`'s `(void)ctx;` is different — that signature is the fixed asset interface.) *high for `populate`; medium for `run`.*
- **3D/Animations/BlendSpace.cppm:69, :83 + ClipPlayer.cpp:94** — The negligible-weight epsilon `0.0001f` exists three times. One module-level `constexpr float min_layer_weight` in `gse::animation` (plain float — no reflection-NTTP BMI concern). *high*
- **3D/Mesh.cppm:77** — `indices()` returns `const std::vector<std::uint32_t>&` where the style guide requires `std::span<const T>` and every sibling (`model::meshes`, `skinned_model::bones`, `clip_asset::tracks`) already complies. *high*
- **3D/Mesh.cppm:202** — `.usage = storage_dst | gpu::buffer_flag::byte_address` spells set-union with an operator where every other `usage` in the file uses a braced flag set. *medium — the operator may be an intentional usage-plus-flag affordance.*
- **3D/Mesh.cppm:94-96** — `meshlet_count()` dereferences the optional unchecked; the precondition pair is split across two accessors. *high*
- **3D/Camera/CameraSystem.cppm:36** — `active_priority` is write-only state: set at `CameraSystem.cpp:136`, initialized to -1, read nowhere. *high*
- **3D/Mesh.cppm:317-380** — `generate_bounding_box_mesh` declares `const std::vector vertices`/`indices`, so they cannot move into `mesh_data{ .vertices = vertices, ... }` — full copies on a path that could be zero-copy. *high*
- **3D/Animations/SourceReader.cppm:172-185** — Unknown shape `kind` values (including corruption) fall through to `box_shape` — the consistent failure instead of the conspicuous one, in a pipeline whose malformed files have already cost boot-time debugging. Set `m_overran` so `bake`'s existing check rejects it. *medium*
- **3D/Mesh.cppm:62** — `mesh : non_copyable` declares no destructor where the style guide's `non_copyable` rule asks for one plus re-defaulted moves. It works today (implicit moves keep `std::vector<mesh>` valid); note the rule's trap — adding `~mesh();` alone would silently delete the moves `model` depends on. *medium*
- **3D/Animations/SkinnedModel.cppm:42 + 3D/Camera/CameraData.cppm:22** — `mass mass;` and `target target{};` shadow their own type names, making them unusable for the rest of the scope. *high*

### Style findings — 3D scene and animation

- **3D/Mesh.cppm:343, 349, 355, 361, 367, 373** — Six face-label comments (`0, // Front face`) in `generate_bounding_box_mesh`. Comments are banned outright. If the labels carry value, restructure so they are unnecessary (build the index list from the `f * 4` loop pattern already used, or use per-face named locals). *high*
- **3D/Camera/CameraSystem.cppm:20, :26** — Two `export namespace gse::camera` blocks in one file; merge `viewport_update` into the single block. *high*
- **3D/Camera/CameraSystem.cpp:58-76** — `halton` and `apply_jitter` defined inline inside `namespace gse::camera { ... }`. `Primitives.cpp:15-22` and `PrimitiveResolver.cpp:17-29` do this correctly and are the in-tree pattern to copy. *high*
- **Alignment padding** — `Mesh.cppm:162-163` (second `buffer_upload` entry column-aligned under the first), `:259-267` (`length_d(...)` continuation columns), `PrimitiveResolver.cpp:94-96` (ternary branches aligned with tab runs), plus the stray `uploads\n\t.push_back` break at `Mesh.cppm:223-224`. *high*
- **Positional aggregate initialization** — `Mesh.cppm:162-163` and `219-224` (`{ &m_vertex_buffer, m_vertices.data(), vertex_buffer_size }` — three positional fields including a pointer and two sizes, exactly the swap-prone shape); `Primitives.cpp:38-41`, `:143` (`vertex` pushed as `{ a, n, { 0.0f, 0.0f } }`). *high*
- **Blank-line noise** — `Primitives.cpp:9-10` and `PrimitiveResolver.cpp:10-11` (double blank line inside the import list); `Mesh.cppm:86-87`, `96-97` (no blank line between adjacent member function definitions). *high*
- **3D/Mesh.cppm:84-99** — Mixed const-accessor idioms in one class: `material()`/`indices()`/`aabb()` use trailing `const` while `vertex_gpu_buffer`/`index_gpu_buffer`/`meshlet_gpu` use `this const mesh& self` for the identical single-const-overload case. Deducing `this` buys nothing without a non-const pair. *high*

### Clean — 3D scene and animation

`3D/Lights/DirectionalLight.cppm` (fully unit-typed: `irradiance`, `length`), `3D/Material.cppm`, `3D/Primitives.cppm`, `3D/Animations/AnimationComponents.cppm` (`phase` as a normalized dimensionless scalar is legitimate; `desired_speed` correctly `velocity`), `3D/Animations/BlendSpace.cppm` (apart from the shared epsilon — weight math correctly dimensionless throughout), and `3D/Animations/Clip.cppm`'s sampling path (`sample`, `ground_speed_of`) — exemplary unit discipline: `time` keyframes, binary search, `time/time` alpha, `displacement/time` velocity, no exits from the type system.

---

## 9. 2D pipeline and capture

Files: `2D/{Font,FontCompiler,Texture,TextureCompiler,Symbols}.*`, `Capture/{Mp4Muxer,Ring}.*`, `Renderers/CaptureRenderer.*`, `{SharedShaders,RenderTargets,RenderComponent,RenderLayer,AssetTypes}.cppm`.

### CRITICAL | Renderers/CaptureRenderer.cpp:182, :373 | Deferred IO jobs capture a raw pointer into system-state-owned atomics that shutdown never drains

```cpp
 write_flag = d.write_in_progress.get()] mutable {
...
 flag = d.clip_save_in_progress.get()] mutable -> void {
```

**Impact.** Quitting (or tearing down the capture system) while a screenshot PNG write or a clip mux is still queued or running writes `false` through a freed `std::atomic<bool>` — heap use-after-free at process exit, non-deterministic and hard to attribute.

**Mechanism.** `data::write_in_progress`/`clip_save_in_progress` are `std::unique_ptr<std::atomic<bool>>` (`CaptureRenderer.cppm:64-65`) owned solely by the system state. `.get()` hands a non-owning pointer to a `task::post_io` job whose lifetime is unbounded. `shutdown` (463-480) only joins the recording thread, returns early when `!d.recording->active`, and never observes either flag. A ~30 MB atlas-sized PNG encode or a 100 MB mux easily outlives shutdown.

**Resolution.** Make both flags `std::shared_ptr<std::atomic<bool>>` and capture the `shared_ptr` by value — the guide's "retain an immutable owning snapshot" rule applied verbatim. It also removes the `unique_ptr` indirection that existed only to give the atomic a stable address, and needs no shutdown drain.

**Prevention.** The same `.get()`-into-a-task shape appears at line 307 (`state = d.recording.get()`), safe only because `shutdown` happens to join. Any handle escaping into `task::post_*` should be a `shared_ptr` or a value, with `[[= gse::stable_shared]]` the only sanctioned raw-pointer escape from system state.

*Confidence: high.*

### CRITICAL | Renderers/CaptureRenderer.cpp:413-418 | Screenshot request survives an early return and rebinds to a different frame slot

```cpp
		d.screenshot_requested = true;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}
```

**Impact.** `capture_swapchain` targets a staging buffer that was never allocated (or was sized for a different extent), and the later readback `memcpy`s `width*height*4` bytes out of a span that may be empty or shorter. Invalid GPU buffer use plus a CPU out-of-bounds read; corrupt or crashing screenshots.

**Mechanism.** The request block (399-414) sizes `staging` and writes `width`/`height` into **the current frame slot's** `pending_screenshot`, then sets the *global* `d.screenshot_requested`. If `frame_in_progress()` is false that frame — out-of-date swapchain, minimised window, failed acquire — the function returns with the flag still set. The next invocation has a different `frame_index`, so `staging`/`width`/`height` (bound at line 161) refer to a **different slot**, and line 437 records the capture into that slot's buffer and sets its `pending = true`. Line 171 later does `gse::memcpy(pixels.data(), staging.host_read().data(), byte_count)` with `byte_count` derived from the mismatched extents and no check against `host_read().size()`.

**Resolution.** Delete `d.screenshot_requested`. The request *is* the per-slot `pending_screenshot`; give it a tri-state (`idle`/`armed`/`captured`) and arm the slot only inside the `frame_in_progress()` guard, so a slot's buffer, extent, and armed-ness are one fact that cannot be split across frames. `buffer::host_read()` returns a sized span, so the `memcpy` should also be span-to-span with a size check.

**Prevention.** A global "requested" boolean paired with per-slot storage is the "state duplicated such that two fields can disagree" case. Keep request state inside `per_frame_resource` so slot identity and request identity cannot diverge.

*Confidence: high.* **[verified — flag set before the early return, slot rebind on the next invocation]**

### HIGH | Capture/Mp4Muxer.cpp:846 | Edit list `media_time` is set to the absolute encoder pts, pointing past the end of the media

```cpp
	const auto first_pts_us = pts_to_timescale(units.front().pts);
...
			emit_elst(moov_bytes, media_duration, static_cast<std::int64_t>(first_pts_us));
```

**Impact.** Every clip saved after the first ~30 seconds of a session presents as blank or empty in any player that honours `elst` — QuickTime, most browser/MSE pipelines. The feature's primary output is silently unplayable in exactly the players that follow the spec.

**Mechanism.** `pts` comes from the encoder's own clock started at encoder creation (`Vulkan/VideoEncoder.cppm:820`), so it is *session*-absolute. The samples' media timeline starts at 0 — `sample_durations` (823-830) are deltas and `stts` encodes them from zero. Setting `media_time = first_pts_us` therefore tells the player "this edit starts at media time 300 s" for a 30 s track. The fragmented path does not have this problem: `tfdt` carries `m_decode_time`, which also starts at 0, and `live_muxer` emits no `elst` at all — the two writers disagree about what the media timeline means.

**Resolution.** `media_time` must be `0` here. If the intent is to preserve the clip's wall-clock offset, that belongs in metadata, not the edit list. Better: normalise `pts` to the first sample once, where the ring snapshot enters the muxer, so both `mux` and `live_muxer` consume a zero-based timeline.

**Prevention.** Nothing in the muxer states whether incoming `pts` is absolute or clip-relative. Normalise at the single entry point and the question stops being answerable two ways.

*Confidence: high.*

### HIGH | Capture/Mp4Muxer.cpp:785-786 | `rewrite_co64_offset` locates the chunk-offset field by scanning for the literal bytes `co64`

```cpp
	for (std::size_t i = 0; i + 4 <= moov.size(); ++i) {
		if (moov[i] == marker[0] && moov[i + 1] == marker[1] && moov[i + 2] == marker[2] && moov[i + 3] == marker[3]) {
```

**Impact.** A `63 6F 36 34` byte sequence occurring anywhere earlier in `moov` — inside the `av1C`/`hvcC` codec config, inside an `stsz` sample size, inside an `stts` duration — wins the scan, and the real chunk offset is left at 0 while eight bytes of unrelated table data are overwritten. The result is a structurally valid but undecodable MP4, with no diagnostic. The `stsz` table grows one `u32` per sample, so collision probability scales linearly with clip length.

**Mechanism.** `emit_co64(moov_bytes, 0)` (862) writes a placeholder because `mdat_payload_offset` is not known until `moov` is complete. The patch-up then re-derives the field position by content search instead of by construction — even though this file already implements the correct idiom three times (`emit_mvhd_unknown`, `emit_tkhd_unknown`, `emit_mdhd_unknown` all return a `std::size_t& duration_offset_out`).

**Resolution.** Give `emit_co64` a `std::size_t& offset_field_out` parameter matching the `*_unknown` emitters, thread it out through `emit_stbl`/`moov` assembly, and patch by offset. Then delete `rewrite_co64_offset`.

**Prevention.** "Find a binary field by searching for its tag" will be reached for again the next time a forward reference appears. Make the offset-returning emitter the only way to write a patchable field: a tiny `patch_slot` type returned by such emitters and consumed by a `write_u64_at` makes the byte-scan version unnatural to write.

*Confidence: high.* **[verified — full-buffer scan and first-match patch confirmed against source]**

### HIGH | Capture/Mp4Muxer.cpp:1274 | H.265 samples written as raw Annex-B under an `hvc1`/`hvcC` entry declaring 4-byte length prefixes

```cpp
	const std::uint32_t per_sample_prefix = m_track.codec == gpu::video_codec::av1 ? 2u : 0u;
```

**Impact.** Every HEVC recording and clip is non-conformant and will fail to decode in spec-following demuxers. `mux()` has the same defect (it writes `u.bytes` verbatim at 907-909 and sizes `stsz` from the unmodified length).

**Mechanism.** `build_hvcc` writes `0x0F` as the final configuration byte (522), whose low two bits set `lengthSizeMinusOne = 3` — samples must be a sequence of 4-byte-big-endian-length-prefixed NAL units. But the encoder emits Annex-B: `split_h265_nalus` (457-489) only works because it scans for `00 00 01`/`00 00 00 01` start codes, and those bytes pass through to `mdat` untouched. Additionally `hvc1` forbids in-band parameter sets, yet the VPS/SPS/PPS NALUs remain inline in each keyframe.

**Resolution.** Convert Annex-B to length-prefixed form when writing samples: for each NALU emit `u32be(payload_size)` then the payload, dropping VPS/SPS/PPS (they live in `hvcC`). Compute the converted size once and use it for both `stsz`/`trun` sizes and the `mdat` total, so the declared size and the emitted bytes come from one derivation rather than `per_sample_prefix_bytes` arithmetic. Alternatively switch the sample entry to `hev1`, which permits in-band parameter sets — the length-prefix requirement stands either way.

**Prevention.** `per_sample_prefix_bytes` is a scalar fudge letting the declared size and the written bytes be computed independently. Replace it with a function that *produces* the sample payload (`std::vector<std::byte> sample_payload(codec, unit)`); then `stsz`/`trun` size it and `mdat` writes it, and they cannot disagree.

*Confidence: high on the code; medium on blast radius, since `video_codec::av1` is the default and the HEVC path may be untested.*

### HIGH | 2D/Texture.cpp:81-84 | Channel-count-to-format table has no case for 2 channels, producing a 3-byte format for 2-byte data

```cpp
	const auto gpu_format = channels == 4
		? (use_linear ? gpu::image_format::r8g8b8a8_unorm : gpu::image_format::r8g8b8a8_srgb)
		: channels == 1 ? gpu::image_format::r8_unorm
						: (use_linear ? gpu::image_format::r8g8b8_unorm : gpu::image_format::r8g8b8_srgb);
```

**Impact.** A grey+alpha PNG (`channels == 2`) is uploaded as `r8g8b8`, so `upload_image_2d` reads 50% past the end of `m_image_data.pixels` — heap over-read, garbage texels, potential crash. Silent and asset-data-dependent.

**Mechanism.** `image::load` does **not** force RGBA (`force_rgba` is only set on the `load_rgba` path); `png_get_channels` after `png_read_update_info` returns 2 for `PNG_COLOR_TYPE_GRAY_ALPHA` (`Os/STB/ImageLoader.cpp:141`). The nested ternary's final `else` is a catch-all, so 2 (and 5+) land on the 3-channel branch. The assert at 74 only checks `data_size > 0 && !pixels.empty()` — it never relates the buffer size to the format's stride.

**Resolution.** Make the mapping total and explicit: a `switch (channels)` with a `default` that fails loudly, or a `format_for(channels, use_linear)` returning `std::optional`. Then assert `pixels.size() >= width * height * bytes_per_pixel(gpu_format)` before `upload_image_2d`.

**Prevention.** Nested ternaries over an unbounded integer domain silently absorb unhandled values. Carrying the channel count as an enum on `texture::baked` makes the mapping exhaustive by construction and reflection-derivable.

*Confidence: high.*

### HIGH | Capture/Ring.cppm:103-106 | The keyframe-alignment eviction loop drains most of the ring each time the budget expires a keyframe

```cpp
	while (!m_units.empty() && !m_units.front().keyframe) {
		m_bytes -= m_units.front().bytes.size();
		m_units.pop_front();
	}
```

**Impact.** The rolling buffer sawtooths. Whenever budget eviction removes the oldest keyframe, this loop discards the entire following GOP — so "save the last 30 seconds" can yield a clip of a fraction of a second, depending purely on when the key is pressed relative to the GOP boundary. If the encoder never emits a keyframe, the ring stays permanently empty and `save_clip` silently reports "no keyframe yet".

**Mechanism.** Both loops run unconditionally on every `push`. The first evicts by time; the second then throws away everything up to the *next* keyframe. This is destructive and redundant: `snapshot_from_earliest_keyframe` (48-72) already skips a non-keyframe prefix at read time, so the retained P-frames cost nothing but the bytes they occupy and could still be needed if a keyframe precedes them within budget.

**Resolution.** Delete the second loop. Budget eviction is the ring's policy; keyframe alignment is the snapshot's policy and already lives there. If bounding memory is the real concern, add an explicit byte budget — `m_bytes` is already maintained, and `bytes_stored()`/`frame_count()` have no callers anywhere in the repo.

**Prevention.** Paired-derivation defect: "where does the clip start" is decided by both the evictor and the snapshotter. Keeping the decision solely in `snapshot_from_earliest_keyframe` makes the invalid pairing unwritable.

*Confidence: high.*

### HIGH | 2D/Font.cpp:123, :187, :222 | Three independent implementations of glyph advance that disagree on newlines and zero-index glyphs

```cpp
auto gse::font::text_layout(...) const -> std::vector<positioned_glyph> {   // advances for index-0 glyphs, resets on '\n'
auto gse::font::width(...) const -> float {                                 // skips index-0 glyphs, ignores '\n'
auto gse::font::caret_offsets(...) const -> std::vector<float> {            // skips index-0 glyphs, ignores '\n'
```

**Impact.** Measured text width does not match drawn text width, and caret positions do not match drawn glyph positions. For multi-line text `width()` returns the *sum* of all lines rather than the longest, so any layout that sizes a box from `width()` and fills it via `text_layout()` is wrong. For any codepoint present in the atlas with `ft_glyph_index() == 0`, `text_layout` advances the cursor by `x_advance` (176, unconditional) while `width` and `caret_offsets` `continue` past it (203-205, 235). Text-input carets land between the wrong characters; the defect reads as a rendering fault.

**Mechanism.** Three loops each re-derive "decode a codepoint, look it up, apply kerning, advance" from `m_glyphs`/`m_kerning`. Nothing forces them to agree, and only one will be updated when the rule changes. The kerning key construction `(prev << 32) | index` is likewise spelled out three times (154, 208-209, 238-239).

**Resolution.** One authoritative advance walk — a generator or callback over `(codepoint, byte_range, cursor, glyph*)` owning decoding, newline handling, kerning, and advance. `text_layout` emits quads from it, `width` takes the max line extent, `caret_offsets` records the cursor per byte. The newline handling then exists once.

*Confidence: high.*

### MEDIUM findings — 2D pipeline and capture

- **Capture/Mp4Muxer.cpp:268-273** — `box_scope` silently truncates box sizes to 32 bits (`static_cast<std::uint32_t>(size)` with no check), so any box exceeding 4 GiB writes a wrapped size and produces an unparseable file. The author knew about the limit — `mux` hand-writes a 16-byte 64-bit `largesize` `mdat` header (892-905) rather than using `box_scope` — but the guard exists only at that one site. Promote to `largesize` in the destructor when the payload exceeds `0xFFFFFFFF`; then `mux` can use `box_scope` for `mdat` too and the special case disappears. *high*
- **Capture/Mp4Muxer.cpp:821-830** — The sample-duration loop is duplicated verbatim from `compute_sample_durations` (921-933), which is declared and defined in the same file and parameterised. The trailing-duration fallback also differs in spelling (`timescale / 60` inline vs the `trailing_default` parameter), so the two writers can drift on the last sample. *high*
- **Capture/Mp4Muxer.cpp:1266-1267, 1320-1322** — Fragment decode time drifts because the last sample's duration is guessed from data already in hand: `flush_fragment` is invoked from `append` (1166-1168) *at the moment the next keyframe arrives* — that keyframe's `pts` is the exact end of the pending fragment — but it is not passed in, so `compute_sample_durations` repeats the previous delta. `baseMediaDecodeTime` accumulates one guessed inter-fragment gap per GOP. *high*
- **Capture/Mp4Muxer.cpp:1153-1172** — `m_pending` is unbounded and nothing reaches disk between keyframes: with a long or infinite GOP an entire recording accumulates in RAM and only lands at `close()`, so a crash loses everything. Separately, none of the `m_file.write` calls in `flush_fragment` (1294, 1305, 1310-1315) is checked, so a full disk silently drops frames; `append` merely starts returning early at the next `valid()` check with no log. Flush on `keyframe || m_pending.size() >= cap` (a non-keyframe-started fragment is legal in fMP4; `trun` sample flags already encode sync-ness) and log once on transition to failed. *high*
- **Capture/Mp4Muxer.cpp:907-909 vs :1307-1316** — The two writers disagree on AV1 sample framing: `live_muxer::flush_fragment` prefixes every AV1 sample with a temporal-delimiter OBU; `mux` does not. Whatever the correct answer is, only one path embodies it. Same resolution as the H.265 finding — one `sample_payload(codec, unit)` consumed by both writers. *high*
- **Capture/Mp4Muxer.cpp:882-911** — A failed mux leaves a truncated `.mp4` at the destination path while the caller logs "Failed to mux clip" (`CaptureRenderer.cpp:384-389`) — a partial, unplayable file sits in the user's clips directory looking like a successful capture. Write to a temporary sibling and `std::filesystem::rename` on success, the discipline `layout_store` already uses. *high*
- **Capture/Mp4Muxer.cpp:510-522** — `hvcC` profile, tier, level, and constraint bytes are hard-coded rather than taken from the SPS, so the container declares Main profile / Level 3.1 regardless of what the encoder produced; a 4K or high-bitrate stream is mislabelled and conservative players reject it. The SPS is already parsed out and available — its `profile_tier_level` is exactly these fields. (`build_av1c` correctly parses its sequence header for the equivalent.) *medium — the byte layout is right, but which HEVC profile the Vulkan encoder is configured for was not confirmed.*
- **Capture/Mp4Muxer.cpp:312-315** — A float-backed `time` used as a 1-microsecond media timeline. The muxer declares `timescale = 1'000'000` but `time` is `time_t<float>` stored in nanoseconds, so at engine uptime *T* seconds the representable step is roughly `T × 60 ns`: ~3.6 µs at one minute, ~36 µs at ten, ~214 µs at an hour. Frame durations computed as `pts[i+1] - pts[i]` carry that error directly (catastrophic cancellation between two large near-equal floats), so `stts`/`trun` durations jitter by a growing fraction of a frame on long sessions. The `.as<microseconds>()` exit here *is* justified — the MP4 timescale is a genuine external contract — but the type feeding it cannot represent the contract's resolution. Carry the encoder timestamp as `time_t<std::int64_t>`, or normalise pts to clip-relative at the muxer boundary. *high on the arithmetic; practical severity depends on session length.*
- **Capture/Mp4Muxer.cppm:10-19** — `track_info`'s hand-written constructors make it a non-aggregate, so designated initialization is unavailable and both call sites pass positionally (`CaptureRenderer.cpp:288`, `:376`). The default constructor exists only to value-initialise `extent`, which a default member initialiser does better. *high*
- **Renderers/CaptureRenderer.cppm:31-40 + CaptureRenderer.cpp:246-271** — "Am I recording" is represented four ways — `active` (atomic), `running` (mutex-guarded), `thread.joinable()`, and a non-empty `path` — kept in step by hand across five branches. Any future branch that forgets one leaves `active == true` with a dead thread, so every subsequent frame enqueues into a queue nobody drains. The 300 ms debounce adds a fifth field, `last_toggle`, in `std::chrono::steady_clock` where the engine has `time` and a `system_clock` (already used at 174), and the log then bakes the unit into the literal: `"...{}ms since last toggle"`. Collapse to one `enum class recording_phase { idle, running, stopping }` behind a helper owning start/stop, and print via `{:.0f:ms}`. *high*
- **Renderers/CaptureRenderer.cpp:213-226** — The encoded frame is deep-copied every frame while recording: a heap allocation plus a full memcpy of the compressed frame on the render thread, purely because the ring and the muxer queue each want ownership. `gpu::encoded_unit::bytes` is a `std::vector<std::byte>` (`GpuBackend/Video.cppm:23`), which also makes `ring::snapshot_from_earliest_keyframe` (`Ring.cppm:60-71`) copy the *entire* 30-second buffer — potentially 100 MB+ — synchronously inside `frame()`, a visible hitch on every clip save. Change `bytes` to `std::shared_ptr<const std::vector<std::byte>>` so the ring, the recorder queue, and the mux snapshot share one immutable buffer. *high*
- **Renderers/CaptureRenderer.cpp:237-241 vs :442-459** — The encode is submitted before, and outside, the render-graph pass that writes the planes it reads: the submit at 237 reads `y_planes[frame_index]`/`uv_planes[frame_index]` which the `rec.dispatch<entry>` at 451 writes *later in the same invocation* — a read/write hazard the graph's barrier derivation cannot see, because `encode_frame` is a direct queue submit (`Vulkan/VideoEncoder.cppm:823-826`), not a graph pass. Additionally, for the first `frames_in_flight` frames the planes have only been transitioned (116, 127), never written, so the encoder consumes undefined image contents — garbage at the head of every ring and recording, and `append` drops pre-keyframe units but not a garbage keyframe. Move the submit after the dispatch with an explicit cross-queue wait, and gate the first submit per slot. *medium — the Vulkan encoder's submit path was not read closely enough to rule out an internal wait.*
- **2D/TextureCompiler.cppm:65-74** — Hand-rolled string-to-enum chain where `gse::enum_from_string<E>(std::string_view, E&) -> bool` exists in `gse.meta` (`Meta/Enum.cppm:15-18`) and is reflection-derived. Adding a `texture::profile` enumerator silently does nothing until someone extends the chain, and unknown strings fall through to `generic_repeat` with no diagnostic, so a typo in a `.meta` file produces a wrong-but-plausible texture. Two of the three literals already equal the enumerator spelling; only `"clamp_to_edge"` → `generic_clamp_to_edge` differs — which is itself the argument for renaming the enumerator or carrying the alias as an annotation. *high*
- **2D/TextureCompiler.cppm:55-59** — The `.meta` sidecar parser reads only the first line and silently ignores everything else: a blank first line, a different key first, or CRLF-plus-BOM yields `generic_repeat`, so the texture bakes with the wrong sampler policy and nothing reports it. `line.substr(8)` also depends on the untracked length of `"profile:"`. The codebase already owns a sectioned key/value store and declares the sidecar through the `asset_format::meta_sidecar<>` annotation on `texture::baked` (`Texture.cppm:30`) — parse it with the existing reader, keyed by the annotation. *high*
- **2D/FontCompiler.cppm:111-113** — `FT_Load_Glyph`'s result is discarded, so on failure `ft_face->glyph` still holds the *previous* glyph's slot and the codepoint is baked with a wrong `x_advance`. Text spacing is subtly wrong for that character forever, with no warning — and the geometry warning path (137-147) does not fire because `msdfgen::loadGlyph` is a separate call that may still succeed. Every FreeType call in this function ignores its result (`FT_Get_Kerning` at 264 likewise); a thin checked wrapper in `gse.freetype` would make the unchecked spelling unavailable. *high*
- **2D/FontCompiler.cppm:45-46** — `msdfgen::initializeFreetype()`'s result is passed straight into `loadFont` with no null check, and `ft_lib`, `ft_face`, `ft_handle`, `font_handle` are released only on the linear happy path (285-288) — any exception from `generateMTSDF`, from allocating the ~30 MB atlas, or from the logging calls leaks all four. Three early-return cleanup blocks each repeat a different subset of the teardown (41-42, 49-52). Wrap each handle in a scope guard declared once at the top. *high*
- **2D/FontCompiler.cppm:273-283** — A ~30 MB debug atlas PNG is written into the shipping baked-asset root on every font bake — a full PNG compression pass over a 3072×2496×4 buffer, dropped next to the real `.gfont` output. Since `font` is `[[= asset::boot_critical{}]]`, this sits directly on the first-run boot path and the artifact ships in the baked directory. *high*
- **2D/Font.cpp:104-110** — The ~30 MB font atlas is copied rather than moved into the texture: `texture`'s third constructor takes `const std::vector<std::byte>& data` (`Texture.cppm:51`) and copies into `m_image_data.pixels` (`Texture.cpp:33`), while `baked` is a local `expected` about to go out of scope. On the boot-critical path. *high*
- **2D/Font.cpp:124-135, :223** — `text_layout` and `caret_offsets` heap-allocate per call on the per-frame GUI path; they are called from 31 files across the immediate-mode GUI (every `Widgets/*`, `Gui.cpp`, `UiRenderer.cpp`, `WorldTextRenderer.cpp`), so a UI-heavy frame performs one heap allocation per text element per frame plus one per caret query. Have the shared advance walk emit into a caller-provided vector or a frame-arena-backed span. *high*
- **2D/Texture.cpp:102-135** — Sampler policy is a `switch` over `profile` assigning fields on a default-constructed aggregate (`gpu::sampler_desc desc; desc.max_lod = 1.0f;`) — two style-guide breaches and one structural one: adding a `profile` enumerator compiles fine and produces a silently default-sampled texture. Four cases differ only in filter and address mode, three identical apart from `mag`/`min`. Put a `sampler_policy` annotation on each enumerator and read it with `annotation_from_enum`; the switch and the mutable `desc` both disappear and a new enumerator without an annotation fails at compile time. *high*
- **SharedShaders.cppm:68-71** — `float constant; inverse_length linear; float quadratic;` — in `1 / (constant + linear·d + quadratic·d²)` the quadratic coefficient has dimension 1/length², so the one term whose unit is easiest to get wrong has no checking. The struct otherwise demonstrates the right pattern (`irradiance intensity`, `length source_radius`, `vec3<position>`, `vec3<displacement>`), and quantities pass through push constants directly, so the shader-layout contract does not force the exception. `constant` is genuinely dimensionless; `cut_off`/`outer_cut_off` are cosines and fine. *high*
- **SharedShaders.cppm:20** — `constexpr std::uint32_t invalid_index = std::numeric_limits<std::uint32_t>::max();` shadows `gpu::bindless_slot::invalid_index`. They agree today; nothing keeps them agreeing. The duplicate also drives redundant code at `CaptureRenderer.cpp:431` — `bindless_slot::index` already *defaults* to `invalid_index` (`GpuBackend/Core.cppm:49-50`), so both the ternary and the constant are unnecessary. *high* **[verified]**
- **2D/Symbols.cppm:255-281** — The two `draw` overloads duplicate their entire body — including the `min_half_thickness` clamp and the pixel-centre snap the icon-homogenisation work depends on — differing only in the sink (`ctx.queue_sprite` vs `out.push_back`) and the blank-texture source. A change to icon weighting applied to one silently leaves the other (used by `Gui.cpp:1276`) rendering differently. *high*
- **RenderComponent.cppm:16-33** — `model_count` can disagree with three parallel fixed arrays, all replicated in full: nothing ties the count to the arrays, so a writer filling `models[3]` without bumping the count leaves a silently invisible model, and a count larger than the filled range reads default-constructed handles. `[[= networked]]` on all three arrays also replicates the full 16-slot payload regardless of `model_count`, on every entity. At minimum gate all mutation through `add_model`/`remove_model`; better, one `std::array<model_slot, max_models>` of `{handle, tint, size}` plus the count. *medium — the replication encoder's handling of `model_count` is outside this file set.*
- **2D/Texture.cppm:68** — `create_vulkan_resources` names a specific backend inside a backend-agnostic type that talks only to `gpu::device`/`gpu::image`/`gpu::sampler_desc`. The only remaining `vulkan` mention outside `Source/Vulkan` in the graphics module; single call site at `Texture.cpp:52`. *high*

### LOW findings — 2D pipeline and capture

- **2D/Font.cppm:44-47** — `positioned_glyph` has `const` data members while being stored in a `std::vector`, so move assignment is deleted, vector growth falls back to copying, and `erase`/`assign`/`resize` are unavailable. The `const` buys nothing the API does not already give. *high*
- **2D/Font.cppm:14-42** — `glyph` is `glyph::info` plus four trivial accessors and a constructor, storing exactly the four fields of its own nested aggregate and enforcing no invariant. It also forces `font::texture()` to be written as `-> const gse::texture*` because the accessor name collides with the type. Replace with the aggregate; rename the accessor to `atlas()`. *high*
- **Capture/Ring.cppm:42, :98** — `m_newest_pts = unit.pts;` overwrites rather than maxes, so any pts regression (encoder reset, reordering) makes the budget difference negative, disables eviction, and lets the ring grow without bound. `std::max` costs nothing and the quantities compare directly. `bytes_stored()`/`frame_count()` have no callers in the repository. *high*
- **Renderers/CaptureRenderer.cpp:307-329** — `std::thread` with an unbounded queue: the stop signal, the join, and the thread handle are three manual steps in three places, and if disk writes fall behind the encoder `state->queue` grows without limit. A `jthread` collapses `running` into its stop token; a bounded queue that drops-and-logs is the standard back-pressure answer. *high*
- **Renderers/CaptureRenderer.cpp:183-188** — `needs_swizzle` is loop-invariant but tested inside the per-pixel loop. On the IO thread, so cost is low. *high*
- **Renderers/CaptureRenderer.cpp:399** — A screenshot request arriving during an in-flight write is dropped with no diagnostic, while the clip path logs "Clip save already in progress, ignoring request" (341). *high*
- **Renderers/CaptureRenderer.cppm:58, :69** — `std::array<gpu::bindless_handle, per_frame_resource<gpu::image>::frames_in_flight> rgba_slots;` reaches into another type's constant to size a parallel array — the tell that it should be `per_frame_resource<gpu::bindless_handle>`. `bool first_ring_push_logged` is one-shot debug scaffolding in shipped system state. *high*
- **Capture/Mp4Muxer.cppm:71** — `std::uint32_t m_last_default_duration = 16'667;` is the 1/60 s-at-1 MHz value, unnamed and unrelated to `timescale`, which is declared in the `.cpp`. Derive it (`timescale / 60`) from the same constant `mux` uses at 829. *high*
- **Capture/Mp4Muxer.cpp:408-421** — The AV1 sequence-header bit reader returns partial values on overrun instead of failing, so a truncated header silently yields a plausible-looking `av1C` with wrong profile/level. When `timing_info_present_flag` is 1 the parser also skips the rest and leaves `seq_level_idx_0 == 0`. Return `std::optional` and let `extract_codec_config` fail the mux, matching how the H.265 path already rejects missing SPS/PPS. *high*
- **Capture/Mp4Muxer.cpp:461** — `for (std::size_t i = from; i + 3 < bitstream.size(); ++i)` requires four readable bytes even to test the three-byte start-code form, so a start code in the final three bytes is missed. Harmless in practice (a NALU with a zero-byte payload). *high*
- **AssetTypes.cppm:25-32** — `static_assert(format_of<clip_asset::baked>().source_dir == "Clips")` and its siblings restate the annotations they read: `source_dir`, `baked_dir`, `baked_ext`, and `magic` are declared by the annotation on the type, so asserting the literal back can only fail when someone deliberately renames. The two `has_compile_path` asserts above are meaningful — keep those. `import gse.gpu;` also appears unused. *medium*

### Style findings — 2D pipeline and capture

- **Empty constructor bodies not collapsed** — `Texture.cpp:18-22, 24-26, 28-36`; `Mp4Muxer.cppm:10-12, 14-19`. All five write the init list on its own line and the body as `{\n}`. *high*
- **CaptureRenderer.cpp:57** — `.default_combo = { .k = default_key, .mods = default_modifiers },` packs designated initializers onto one line. *high*
- **2D/Symbols.cppm:23-36** — Fourteen adjacent function declarations with no blank lines between them (lines 38-51 in the same block are correctly separated), and all fourteen lack `[[nodiscard]]`, which every other span-returning accessor in these files carries. *high*
- **Aggregate fields assigned after default construction** — `Texture.cpp:102-103`; `FontCompiler.cppm:88-90`. Also `FontCompiler.cppm:31, 37`: `FT_Library ft_lib;` / `FT_Face ft_face;` left uninitialized before their out-param writes. *high*
- **2D/Font.cpp:168-173** — `positioned_glyphs.emplace_back(positioned_glyph{ ... })` constructs a temporary then move-constructs, defeating emplacement and carrying a redundant type name. *high*
- **CaptureRenderer.cppm:44-45** — Vertical alignment padding in a `describe<...>` string continuation. *high*
- **`[[nodiscard]]` placement inconsistent within one declaration block** — `Font.cppm:83-84` (own line) vs 90, 94, 99, 104, 108, 112, 116, 120 (inline); `Mp4Muxer.cppm:25-26, 47-48` vs 60, 62. `Texture.cppm:61-62` omit it entirely while 65 has it. *high*
- **Double blank line after the import block** — `Font.cpp:7-8`, `Texture.cpp:6-7`, `Mp4Muxer.cpp:7-8`, `CaptureRenderer.cpp:11-12`. *high*
- **RenderComponent.cppm:16-35** — `[[= networked]]` inline on 16, 17, 34, 35; expanded across three lines on 18-20 and 26-28, for the same attribute in one struct. *high*
- **2D/Texture.cppm:35, SharedShaders.cppm:61** — `profile profile = profile::generic_repeat;` and `light_type light_type;` shadow their own type names. *high*
- **2D/Texture.cppm:39-41** — `texture(const std::filesystem::path&)` is not `explicit`, permitting implicit `path` → `texture` conversion; `font`'s equivalent (`Font.cppm:71`) is correctly `explicit`. *high*
- **Capture/Mp4Muxer.cppm:33-43** — `live_muxer` hand-writes its move operations instead of deriving from `non_copyable`, which the same file's `box_scope` (`Mp4Muxer.cpp:44`) uses correctly. *high*
- **Imports with no corresponding use** — `FontCompiler.cppm:9,13,14,15,16`; `TextureCompiler.cppm:7,8,11,12,13,14,15`; `Ring.cppm:6,7`; `AssetTypes.cppm:7`. Given the build is BMI-deserialization-bound these are real compile cost — but verify before removing: some may be transitively load-bearing (`vec4f` at `FontCompiler.cppm:217` has no direct `gse.math` import and must arrive through a re-export). *medium*

### Clean — 2D pipeline and capture

`RenderTargets.cppm` — fully compliant; `static constexpr` members correctly omit `inline`, bodies expanded, designated init throughout, no unused imports. `RenderLayer.cppm` — clean.

### Cross-cutting note — the muxer

The muxer's three highest-severity defects (edit-list media time, the `co64` byte scan, H.265 framing) share one root cause: `mux` and `live_muxer` independently derive sample framing, timeline origin, and duration policy from the same inputs. Consolidating those three rules into a shared sample/timeline layer closes all of them and prevents the class, which is a better outcome than three separate patches.

---

## Corrections and rejected findings

Recorded so they are not re-raised, and so the reliability of the remaining findings can be judged.

### `Value.cppm` is not dead code — do not delete it

The widget-set pass recommended deleting the file outright on the grounds of zero call sites. It searched only `Engine/` and `Editor/` and missed two live consumers:

- `Engine/Server/Application.cppm:99, 103, 122` — `ui.draw<gui::value<std::uint32_t>>`
- `Sandbox/Sandbox/Source/GameUI.cppm:155, 159` — `ui.draw<gse::gui::value<float>>` and `<int>`

The narrower point survives: all five live call sites instantiate the *arithmetic* specialization, so the quantity path carrying the hand-rolled conversion at line 146 does appear unexercised. That is an argument for fixing the formatter usage, not for removing the file.

**The same caveat applies to that pass's other dead-surface claims** — `gui::button`, `draw::button`, `gui::text`, `gui::separator` — which were determined with the same too-narrow scope. Re-check against `Sandbox/` and `Engine/Server/` before deleting any of them.

### Bloom bindless-slot churn downgraded from critical to high

The post-process pass rated `BloomRenderer.cpp:175` critical on the grounds that the synchronously freed slot could be handed to an unrelated resource while in-flight frames still sampled it. Verification showed `bindless_slot_pool` is **LIFO** — `allocate` pops from the back of the same `free_list` `release` pushes to (`Bindless.cppm:104-117`) — so in the straight-line path line 183 hands back the identical index line 175 freed. The cross-resource aliasing scenario requires an interleaving allocation that this synchronous function does not permit. The sequencing defects (early return after the free, dead `if`) are real and unaffected; severity reduced accordingly.

### `DrawStruct.cppm` is not the banned reflection form

The GUI-core pass flagged it under the Reflection Helpers section. Verification confirmed it wraps `nonstatic_data_members_of` and `enumerators_of`, **not** `annotations_of`. So it is the "raw `std::meta` at a use site" defect the guide rejects on sight, but *not* the outright-banned `std::define_static_array(std::meta::annotations_of(...))` form, and the silent-collision hazard that ban exists to prevent does not apply here.

### Hypotheses checked and discarded

- The bloom/upsample intra-pass RAW between consecutive dispatches is **not** a missing barrier — `register_bindless_usage` calls `note_touched` + `emit_intra_pass_barrier` for every single-descriptor bindless binding.
- OIT omitting `.textures` from its binding args is **not** a bug — `ForwardRenderer.cpp:452-471`, `UiRenderer`, and `WorldTextRenderer` all do the same, because `descriptor_count_v > 1` arrays are deliberately skipped by the registration path.
- `PhysicsDebugRenderer`'s unit box/sphere/capsule meshes are generated once in `init` (343-384), **not** per frame.
- `meshlet_push_constants` is layout-portable and `static_assert`ed as such; 80 bytes against a 256-byte budget.
- Frame-system ordering is sound — `run_node_frame` awaits `frame_state_deps` (`Scheduler.cpp:283-285`), so the `shared_view<geometry_collector::data>` dependency guarantees the forward, depth-prepass, OIT and cull-compute frames run after the collector's.
- `UiRenderer`'s batch-break logic is correct: `bindless_slot`'s default index is `UINT32_MAX`, not 0.
- The `^^gse::renderer::X::frame` qualification is the established engine-wide convention for function-valued pass tags (14 sites), not a redundant-qualifier violation.
- No `.as<Unit>()` exits exist anywhere in `Graphics/3D/`, and the suspected sampling duplication between `ClipPlayer`, `BlendSpace` and `SkinnedModel` does not exist.

### Findings whose reachability was not established

Stated plainly rather than assumed: whether the render graph's `.after<>()` edge alone emits a sufficient barrier absent a `sample_image` read declaration; whether any live `text_command` producer passes a view to a temporary; whether `major_spacing` is set out of range by any shipped `settings.ini`; whether the swap-chain-recreate callbacks can fire concurrently with a `frame()` coroutine in practice; whether transparent skinned materials occur in any current scene; whether resolver systems are scheduled on clients.

---

## Coverage

All **127** files under `Engine/Engine/Source/Graphics/` were read in full across nine passes. No file was left partially reviewed.

Files with no findings of their own beyond the grouped style items and the four cross-cutting classes:

`RenderTargets.cppm`, `RenderLayer.cppm`, `Interaction.cppm`, `WidgetContext.cppm` (clean as written — its defect is that it is empty), `Save.cppm`, `3D/Lights/DirectionalLight.cppm`, `3D/Material.cppm`, `3D/Primitives.cppm`, `3D/Animations/AnimationComponents.cppm`, `3D/Animations/BlendSpace.cppm`, `SdfGridRenderer.{cppm,cpp}`, `PhysicsDebugRenderer.cppm`, `SkinRenderer.cppm`, `RtShadowRenderer.cppm`, `CullComputeRenderer.cppm`, `OitRenderer.cppm`, `TonemapRenderer.cppm`, `DepthPrepassRenderer.cppm`.

### Style baseline

Directly audited rather than inferred from a formatter. Across all 127 files: **zero** `static` file-scope functions, **zero** anonymous namespaces, **zero** `detail` namespaces, **zero** `inline` on module functions. Namespace-scope constants are plain `constexpr` throughout. Comments appear in exactly two places — six face labels in `3D/Mesh.cppm` and fifteen section banners in `2D/Gui/Styles.cppm`.

Unit-type discipline is good and in places exemplary: `atmosphere_inverse_length` for scattering coefficients, `irradiance` for sun intensity, same-dimension division for dimensionless ratios (`WorldTextRenderer.cpp:203-204`, `PhysicsDebugRenderer.cpp:254`), and `Clip.cppm`'s sampling path throughout. The dimensional defects that exist are concentrated and named above: pixel extents constructed as `meters` in `UiRenderer`, raw-float angles and per-frame-rather-than-per-second timers in the widgets, hand-converted time units in the profiler overlay, and the `quadratic` attenuation coefficient in `SharedShaders.cppm`.

The recurring style deviations are: definitions written inside `export namespace` blocks, multiple export blocks per file, positional aggregate initialization where designated initializers apply, vertical alignment padding on continuation lines, and a double blank line after the import block (which is consistent enough across the renderer directory to be a habit rather than an accident).

### Verification status

**Nothing in this document was compiled.** The repository owner performs all builds. Findings marked **[verified]** were re-checked by hand against the source after the originating pass reported them; the rest carry the originating pass's stated confidence. Where a claim depends on a fact that could not be established without building — barrier derivation behaviour, replication encoder details, scheduling reachability — that is stated at the finding rather than assumed either way.
