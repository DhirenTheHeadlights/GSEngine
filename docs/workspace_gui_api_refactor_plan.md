# Workspace and GUI API Refactor Plan

## Goal

Make the intended abstractions the easiest APIs to use, especially for generated code:

- use `gse::id` throughout workspace document identity;
- represent the active game view without a numeric sentinel;
- centralize ownership of open-document state and invariants;
- make governed GUI interaction the default while keeping raw input an explicit dependency outside the GUI module.

The work should be staged so that each phase reduces ambiguity before the next phase begins.

## Design Principles

1. Domain identity must not be represented by storage-sized integers.
2. Distinct domain states must be distinct types rather than reserved values.
3. Related state and its invariants must have one owner.
4. Capabilities should be available only at the layer that owns their semantics.
5. Generic extension points must expose the narrow interface, not privileged implementation details.
6. Raw input outside the GUI module must be visible as an explicit system dependency.

## Current Structural Issues

### Workspace identity

The GUI tab API already uses `gse::id`, but the workspace stores document identity as `std::uint32_t`. Call sites bridge the mismatch with `generate_temp_id(document_id)` and conversions from `gse::id::number()`. Those adapters make the incorrect representation convenient and obscure the identity boundary.

### Active view

The game viewport is represented as a reserved numeric document ID. This mixes two different domain states and requires callers to remember which integer is special.

### Document ownership

The document map, tab order, active selection, and next-ID sequence are independently mutable. Their invariants are distributed through the workspace implementation rather than enforced by one type.

### Input authority

`draw_context` publicly exposes the complete input state alongside governed GUI operations such as hover, press, release, and consumption. That makes bypassing GUI layering and consumption easier than using the GUI interaction model.

## Phase 1: Use `gse::id` End to End

Change every document-identity boundary to `gse::id`, including:

- the document map key;
- tab order;
- active document state;
- navigation history;
- diagnostics and quick-fix requests;
- prompts and deferred actions;
- workspace layout persistence;
- public and private workspace functions.

Use `gse::id{}` as the invalid value where optionality is not represented by `std::optional`.

Temporary document IDs should be generated from a stable namespace plus a monotonic sequence:

```cpp
generate_temp_id(hash_combine(
    stable_id("ide.workspace.document"),
    m_next_document_sequence++
))
```

Keep the sequence as an integer because it is a sequence, not an identity. Do not introduce adapters that convert arbitrary integers into document IDs or recover domain IDs from `gse::id::number()`.

Retain an associative document store and a separate tab-order vector. Their different semantics are meaningful, so replacing them with a dense ID collection is not part of this change.

### Acceptance criteria

- Workspace document identity is `gse::id` at every boundary.
- No workspace call site generates a GUI ID from a numeric document ID.
- No workspace call site converts `gse::id::number()` into a document ID.
- Diagnostics, navigation, prompts, and persistence use the same identity type.

## Phase 2: Replace the Viewport Sentinel

Represent the active view as tagged state:

```cpp
struct game_view {};

struct document_view {
    gse::id document_id;
};

using active_view = std::variant<game_view, document_view>;
```

Keep this representation private. Expose intent-based operations and queries such as:

- `game_active()`;
- `active_document_id()` returning `std::optional<gse::id>`;
- `active_tab_id()` returning `gse::id`;
- `activate_game()`;
- `activate_document(gse::id)`.

The game tab may have a stable GUI ID such as `id_of<"ide.workspace.game_tab">()`, but that ID is only a projection used by the tab strip. It is not a sentinel in workspace domain state.

### Acceptance criteria

- No reserved numeric or `gse::id` value represents the game view.
- Switching on the active view is exhaustive.
- GUI tab identity is translated at the workspace-to-GUI boundary.

## Phase 3: Introduce an `open_documents` Owner

Extract an owning type for the state that must change together:

```cpp
class open_documents {
public:
    std::span<const gse::id> order() const;
    document* find(gse::id document_id);
    const document* find(gse::id document_id) const;
    document* active_document();
    const document* active_document() const;
    std::optional<gse::id> active_document_id() const;
    gse::id active_tab_id() const;

    gse::id open(const std::filesystem::path& path);
    void activate(gse::id document_id);
    void activate_game();
    void reorder(gse::id document_id, std::size_t index);
    void close(gse::id document_id);

private:
    std::unordered_map<gse::id, document> m_documents;
    std::vector<gse::id> m_order;
    active_view m_active = game_view{};
    std::uint64_t m_next_document_sequence = 1;
};
```

The exact declarations must follow the repository signature-layout rules during implementation.

Put this type in its own workspace module partition. `Workspace.cppm` is already large enough that a separate documents partition will make ownership and API boundaries easier to review.

The GUI may inspect a document and edit its buffer, but it must not directly mutate document identity, the map, tab order, or active selection. A later change can introduce an edit transaction or `mark_edited` operation if buffer mutation also needs stronger ownership.

### Invariants owned by the type

- Every ID in tab order exists in the document map.
- Every open document appears exactly once in tab order.
- An active document exists in the map.
- Closing the active document chooses the next active view consistently.
- Reordering changes order without changing identity.
- Opening a document cannot collide with an existing temporary ID.

### Acceptance criteria

- Workspace code cannot mutate the map, order, active view, or ID sequence independently.
- All operations that can affect an invariant are methods of `open_documents`.
- The type has focused tests for opening, activating, reordering, and closing documents.

## Phase 4: Separate GUI Interaction from Raw Input

### Chosen direction

Use capability separation rather than friendship.

`draw_context` should own or reference raw input privately. Its public surface should contain only governed GUI-semantic operations, including layer-aware hover, press, release, scrolling, key handling, and consumption.

GUI construction code may supply raw input to internal widgets that genuinely need it through closed, GUI-owned entry points. Editor systems outside the GUI module must explicitly add input data as a dependency when they need non-GUI input.

The implementation uses a module-internal `widget_context` derived from the public `draw_context`. GUI construction creates the internal context, public builders expose only its non-copyable and non-movable `draw_context` base, and engine widget implementations import the internal partition when they require device-level input. The internal type and accessor are not exported, so editor modules cannot reach them through the drawing API and the privileged context cannot be sliced.

This produces two useful review signals:

- ordinary drawing code naturally reaches for the GUI interaction API already in scope;
- raw input in editor code requires a visible dependency in the system signature and an explicit data path.

### Public `draw_context` capability

Keep or add narrow operations for common interaction behavior:

- input availability at the current layer and at a position;
- layer-aware hover;
- mouse press and release associated with a control ID;
- mouse and keyboard consumption;
- governed scroll and key queries;
- pointer position only where it is needed for presentation or geometry.

Do not expose the raw input state, device transition arrays, or an escape hatch returning the underlying input object.

### Internal widget capability

Most widgets should use only the governed `draw_context` interface. Widgets that truly require lower-level input should receive it through explicit GUI-owned operations such as builder members for text input, text areas, split controls, or other stateful controls.

Do not add raw input to the generic `builder::draw<W>` signature. An external editor type can participate in a generic extension point, so privileged input there would recreate the original ambient capability.

The implementation should prefer this shape:

- generic or render-only widgets receive `draw_context`;
- closed builder members implemented by the GUI module may use the builder's private input reference;
- reusable interaction behavior is centralized in GUI helpers that return semantic results such as hovered, active, or activated;
- internal widgets receive only the raw facts they require when a full input reference is unnecessary.

`draw_context` and `builder` will no longer be aggregates once their input members are private. Add explicit construction through GUI-owned constructors or initialization objects. Their construction is already centralized in the GUI system, so this should have a small creation-site impact.

### Editor input dependencies

Editor systems may explicitly depend on input for behavior that is not owned by GUI interaction, including:

- global or domain keyboard shortcuts;
- continuous game-input forwarding;
- editor tools whose input semantics are independent of GUI controls.

That input must not be used to reproduce GUI hover, click, layer, capture, or consumption logic. If code is implementing a control, the behavior belongs in the GUI module and should be exposed as a semantic GUI operation.

Game-input forwarding should move out of the Code Panel draw callback and into an input- or workspace-owned system. Drawing can emit capture and release intent and render the current state; continuous forwarding should be processed where the explicit input dependency lives.

Avoid retaining references to input data in deferred GUI callbacks. Either process the non-GUI input in the owning system before building the GUI or capture a value snapshot of the specific facts required.

### Migration sequence

1. Inventory every direct `draw_context.input` use and classify it as standard GUI interaction, internal stateful widget behavior, or non-GUI editor input.
2. Add missing governed GUI operations and semantic interaction results.
3. Add closed GUI builder entry points for internal widgets that still need privileged input.
4. Move legitimate non-GUI editor behavior to systems with explicit input dependencies.
5. Make raw input private in `draw_context` and `builder`.
6. Resolve the resulting compile-time boundary violations according to their classification.
7. Add a static review check that rejects direct raw-input access through GUI contexts and rejects raw input in generic widget dispatch.

### Acceptance criteria

- `draw_context` has no public raw-input member or accessor.
- Generic widget dispatch cannot receive raw input.
- Internal raw-input use is reachable only through closed GUI-owned APIs.
- Editor code declares a direct input dependency for legitimate non-GUI input behavior.
- Editor raw input is not used for GUI hit testing or interaction transitions.
- Game-input forwarding no longer depends on a draw callback retaining raw input.
- The review guide describes both the architectural rule and the visible dependency signal.

## Phase Order

Implement the phases in this order:

1. migrate document identity;
2. introduce tagged active-view state;
3. extract `open_documents`;
4. migrate and privatize input capability;
5. update the code-review guide with the finalized APIs and prohibited bypasses;
6. perform a workspace and editor sweep for legacy adapters, sentinels, direct state mutation, and raw GUI input access.

The first three phases are closely related and may be delivered together if the intermediate branch would otherwise require temporary identity adapters. Phase 4 should remain a separate reviewable change because it spans GUI internals and multiple editor systems.

## Phase 4 Decisions to Validate During Implementation

The direction is settled, but these details should be decided from the usage inventory:

1. Which existing widgets can move entirely to governed context operations?
2. Which stateful widgets justify closed builder members?
3. Which pointer-position queries are presentational and safe to expose publicly?
4. Which editor shortcuts are GUI-scoped and which are domain/global input behavior?
5. Which deferred callbacks should be replaced with immediate intent emission rather than input snapshots?

The deciding test is capability ownership: GUI interaction stays inside GUI semantics; non-GUI input is declared by the system that owns the behavior.

## Verification

Review each phase statically against `docs/STYLEGUIDE.md` and `docs/CODE_REVIEW_GUIDE.md`. The implementation handoff should include the exact build and test targets for the repository owner to run, because the code-review workflow does not execute builds or tests.
