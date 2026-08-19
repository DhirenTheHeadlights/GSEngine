# GSE Code Review Guide

This document is the durable review contract for code produced by humans or language models. Apply it together with `docs/STYLEGUIDE.md`. Keep it foundational: record stable engineering values and approved idioms, not details likely to become stale after a refactor.

## Review Standard

A change should feel inevitable in the surrounding code: it uses the engine's existing vocabulary, ownership model, helpers, and composition patterns; introduces no parallel abstraction without a demonstrated need; and reaches the simplest cohesive design, even when that requires a broad refactor. Simplicity does not mean minimizing the diff or the number of types. It means minimizing concepts, exceptional paths, duplicated knowledge, and hidden coupling in the resulting system.

Review in this order:

1. Correctness: find latent bugs, invalid states, lifetime hazards, concurrency errors, lossy conversions, and unhandled failure paths.
2. Architecture: enforce ownership and producer-consumer semantics. Separate independently scheduled or independently owned features into systems; directly compose behavior that shares one responsibility and lifecycle.
3. Engine congruence: prefer existing engine types, IDs, reflection facilities, helpers, channels, and established idioms over local substitutes.
4. Dimensional correctness: use strong unit types for every physical quantity. Require a concrete dimensional derivation for any raw numeric math that resembles a physical quantity, ratio, conversion, or integration step. Audit the *exits* from the type, not merely its presence — a conversion that names a unit still leaves the type system, and the arithmetic after it is unchecked.
5. Runtime cost: avoid per-frame string construction and allocation, repeated formatting, avoidable container churn, redundant work, and synchronization in hot paths. Verify the actual lifetime and call frequency rather than assuming a function is cold.
6. Complexity: challenge every new state variable, branch, cache, adapter, mapping, and abstraction. Establish that each branch is reachable before accepting it. Prefer designs where invalid combinations are unrepresentable and behavior follows from one source of truth.
7. Style and API shape: enforce every rule in `docs/STYLEGUIDE.md`, including naming, file organization, module visibility, unit types, channel pushes, spans, ranges, and declaration/definition structure.

## Behavioral Consolidation

Treat repeated state transitions and interaction rules as duplicated knowledge even when their drawing, data, or call sites look different. Selection, focus, activation, dismissal, dragging, and replacement of indexed data are examples of behavior whose invariants should have one authoritative implementation once they recur. Consolidate them into the lowest reusable helper or state type that can own the complete transition, rather than leaving each feature to coordinate related fields and edge cases independently.

Prefer small composable behavior helpers over manager hierarchies, visual base classes, or wrappers that merely shorten code. A useful helper centralizes an invariant, gives callers the result needed for custom presentation, and makes invalid transitions difficult to express. Similar-looking rendering without shared behavioral rules is not by itself a reason to abstract.

## Paired Derivations

When two paths derive behavior from the same underlying fact — what is drawn and what is interactive, what is validated and what is executed, what is written and what is read back — they must obtain that fact from one authority. A pair that computes it independently diverges silently: nothing fails, and the system enters a state its own design treats as impossible, so the symptom points away from the cause. Interactivity outliving presentation is the canonical case: an element is drawn through a clip, ownership, visibility, or enablement path that the hit test never consults, so it becomes invisible yet live and reads as a rendering fault rather than an input one.

Route both halves through the type that owns the fact instead of repeating the check at each call site, so the invalid pairing cannot be written. Where a shared path is genuinely impossible, prefer the failure that is conspicuous over the one that is merely consistent. Duplicated derivation is the defect even when both copies are currently correct, because only one of them will be updated.

## Escape Hatches Read As Compliance

Every strong abstraction ships a sanctioned way out: `.as<Unit>()` for quantities, `id::tag()` for identities, component access for vectors, a raw handle for a wrapper. Reviewers check whether the strong type is *used* and stop there, so an escape hatch invoked fluently — correct syntax, plausible argument, a unit or field actually named — reads as evidence of care. It is the opposite: it is the point where the checking stops and ordinary untyped code resumes, and everything downstream of it is unverified.

Review the conversions, not the declarations. For each exit ask what external contract forces it. A wire or file format, an OS or driver API, and a shader constant layout are contracts; "I needed a number to do maths with" and "I needed to print it" are not — the type supports arithmetic and ships a formatter. An exit with no external contract behind it is a defect even when the result is numerically correct today, because the next edit to that expression has no dimensional check behind it.

Two shapes recur and are decidable without judgement. Arithmetic that converts before combining same-typed values is always wrong: the operation is defined on the type, and for a ratio the units cancel regardless, so converting only discards the guarantee. Display that converts is always wrong: the formatter's spec performs the conversion, so a unit appearing in a format literal, a heading, or a label is the tell that a value was converted by hand.

The tell is the hand conversion, not the word. A column heading or axis label may name a unit when the values under it are produced by an explicit unit spec — the formatter still owns the conversion, and stating the unit once beats repeating a suffix on every cell. What is never acceptable is reaching for `.as<Unit>()` because the formatter could not express the output you wanted. Extend the format spec instead: the capability belongs next to the conversion, where every future caller inherits it.

## Branches For States The Infrastructure Excludes

Defensive code reads as care. A null check, an emptiness guard, a re-validated precondition — each looks like a reviewer's win, and none is ever challenged, because a branch that cannot fire appears to cost nothing. It is not free. A guard is a claim about the surrounding system: that this state is reachable. When the claim is false the branch becomes documentation that contradicts the code, and every later reader must re-derive the invariant the guard denies before touching anything near it. The dead path is also the one path nothing exercises, so if the invariant ever does break, what runs is whatever the author guessed — usually a silent early return that converts a broken invariant into a missing feature, reported nowhere.

Establish reachability from the write site, not the read site. For a container, find every write; for a pointer parameter, find every call site; for system state, find the lifecycle rule that sequences it. The burden is to name the construction that makes the state possible. When you cannot name it, the branch goes.

Several shapes are decidable without judgement:

- A map reached only through `map[key].push_back(value)` cannot hold an empty mapped value, so an `.empty()` check on one of its entries is dead. Contrast a map whose entries are created and then cleared, where the same check carries the whole meaning. The two are told apart by reading the writes, never by the type.
- A pointer parameter is a claim that absence is meaningful. When every call site passes a known address the claim is false and the null checks behind it are dead; take a reference. The strong tell is a function receiving both an owner by reference and a pointer to one of that owner's members — the parameter re-expresses something already in scope, and the two names must then be kept in agreement by hand.
- A defaulted `nullptr` argument that no caller omits is the same defect with the branch hidden in the signature.
- A precondition re-checked by a callee that only one caller reaches is duplicated knowledge, not defence in depth, and it is the copy that will not be updated when the caller's rule changes.
- State assigned unconditionally in `init` is non-null in `frame`, because the scheduler sequences them. A guard on it is unreachable, its `else` is dead code, and it costs a level of indentation across the whole function.
- A clause repeating a condition the short-circuit already decided is tautological.

Resolve these by removing the state, not the branch: narrow the signature, delete the redundant parameter, take the reference. Hardening a value that cannot be invalid is the symptom-only patch described under Findings Format — it leaves the invalid construction exactly as easy to write. The asymmetry is what makes the rule cheap to apply: deleting a dead guard is checked by the compiler and by the invariant, while adding one is checked by nothing.

None of this licenses removing checks at genuine boundaries. A filesystem call, a subprocess result, a parsed file, a driver or OS return, a network payload, and anything a user typed all sit outside the invariant, and their failure paths must be handled and surfaced rather than swallowed. A trailing `return` after a fully covered `switch` is required by `-Wreturn-type` and is not a defensive branch. The distinction is ownership of the invariant: what the engine constructs, it may rely on; what it merely receives, it must check.

## Machinery Without A Caller

Caching, published generations, pointer indirection, and synchronisation primitives all read as rigor. They are the shapes experienced code has, so a reviewer recognises them as competence and stops. They are the opposite of free: each one is a claim about the surrounding system — that a caller is hot enough to need the cache, that a reader and a writer genuinely overlap, that a value changes often enough to need a generation. When the claim is false the machinery is pure cost, and worse, it is load-bearing-looking cost that the next author will preserve and extend rather than question.

Establish the caller before accepting the mechanism, the same way reachability is established from the write site. For a cache, name the call site and read its enclosing scope to confirm the frequency — a function called once from an `init` path or from behind an `if (!initialized)` guard is cold no matter how expensive it looks. For a lock, name the two threads and the window in which they overlap. For a published generation, name the reader that must survive a concurrent write. When the answer is "a future caller might", the mechanism is premature and the future caller will arrive with better information than you have now.

Prefer immutable and plural over mutable and synchronised. Resolving a larger set once at startup is almost always simpler than supporting runtime mutation, and it eliminates the lock, the generation, the stable-address requirement, and the custom view together rather than one at a time. The cost is usually that something new requires a restart to be picked up, which is frequently acceptable and always cheaper than the alternative it replaces. Where an invariant has already been identified as load-bearing — resolved-once, single-owner, append-only — a change that breaks it must justify the break, not quietly compensate for it with machinery.

Two shapes are decidable without judgement. Two accessors over the same data returning different shapes — one a snapshot by value, one a cached view — is not a design, it is evidence that one of them is over-built; unify them and the surplus becomes obvious. A synchronisation primitive whose protected mutation has no caller in the tree is dead machinery guarding a hypothetical, and it should be deleted along with the mutation rather than kept for symmetry.

Machinery also spreads through parameter types, and that is the harder form to see because the author who inherits it did not choose it. When the only entry point to a capability accepts a heavyweight type, every caller must construct one — including callers whose path has none of the properties that type exists to provide. A pipe reader that takes a mutex-protected multi-consumer buffer forces a single-threaded caller to allocate a mutex and a shared owner purely to obtain lines, and because the surrounding type is then immovable, that owner becomes a `shared_ptr` in somebody's system state. The caller has adopted a concurrency model it does not participate in, and nothing in the diff looks wrong. The usual next move makes it worse: the caller notices the mismatch and hand-rolls a private copy of the underlying loop, so the codebase now has both the surplus machinery and the duplication.

The fix belongs upstream, not at the call site. Separate the capability from the publishing: expose the primitive that does the work on plain values, and implement the heavyweight entry point on top of it. Existing callers keep their signature and behaviour, and the next caller gets to choose how much machinery it needs. A private reimplementation of an existing loop is the diagnostic — it means the shared entry point demanded something its caller could not justify, and the shared entry point is what should change.

This is not licence to ignore genuine hot paths or real races. The distinction is evidence: a mechanism introduced because a call site was read and found hot, or a race was identified between two named threads, is engineering. One introduced because the shape felt appropriate to the problem is decoration, and it must be removed with the same prejudice as a dead branch.

## Absence Of Evidence

A search that returns nothing is evidence about vocabulary, not about capability. The author who greps `wrap`, finds only an unrelated `tab_overflow::wrap`, and concludes the codebase cannot wrap text has learned that one word is not the name — nothing more. The correct next question is never "shall I write one", it is "what existing feature already does this, under whatever name it uses". Capabilities are found by asking what already presents this kind of content, measures this kind of thing, or drives this kind of process, and then reading that code.

Reading means reading. Sampling a file through repeated greps builds a false sense of familiarity: each hit answers the narrow question asked and hides everything adjacent to it, so the reader can consult one file a dozen times and still miss that it already solves the whole problem. Before implementing a panel, a render loop, a parser, or a measurement helper, name the nearest existing analogue and read it end to end. That cost is bounded and paid once; the rewrite that follows a miss is neither.

The tells are visible in review without knowing the history. A new render loop that paints text directly, where a sibling feature uses the shared widget. A private line-splitting or buffer-draining loop beside an exported one that differs only in what it does with the result. A helper defined local to a feature whose parameters mention nothing about that feature — `wrap(text, width, scale)` names no caller, so it belongs at the layer that owns text metrics, not in the module that happened to need it first. Treat each as a question about layering: what did the shared version demand that this caller could not give, and should that demand exist at all?

Resolve by moving the capability, not by keeping the copy. A helper that is not specific to its feature goes to the shared layer in the same change that introduces it — "local for now" is how the second copy is born, and the second copy is the one that will not be updated.

## GUI Interaction Authority

UI code with a `draw_context` must use its interaction queries instead of reconstructing them from geometry and raw input. Use `hovers(rect)`, `mouse_pressed_for(rect)`, and `mouse_released_for(rect)` for rectangle-scoped interaction so clipping, render-layer arbitration, availability, and event consumption remain one owned derivation. Do not spell hover as `rect.contains(mouse) && input_available()` or activation as a raw mouse transition gated by a local rectangle test. Stateful controls may observe an unscoped release only through the established interaction behavior that must clear active ownership even when release occurs outside the original rectangle.

Standard controls should use the established widget or behavior implementation so hot, active, press, release, and animation state stay consolidated. Custom presentation may add semantic eligibility or geometry constraints around the context result, but it must not reproduce the context's interaction policy. Lower GUI layers that implement `draw_context` itself are the boundary exception; they own that policy rather than consuming it.

Scrolling and clipping belong to `scroll_region`, not to the feature. A float offset in feature state, a wheel handler reading `scroll_delta`, a manual clamp against content height, and a hand-written cull are together a reimplementation of a widget that already exists — and one that ships without a scrollbar. Pass `scroll_region_info::size` to bind the region to an absolutely-positioned rect instead of the enclosing menu; seed `layout_cursor` before it and write the final cursor position back after, so content extent is measured rather than estimated. A content height computed from row counts is the tell that the region is not doing the measuring.

A widget reports the cursor it wants; it does not set one. `cursor::set_style` is reachable only inside the GUI module, so a widget that calls it works for engine callers and silently does nothing for everyone else — and code outside the GUI must push `set_cursor_shape_request` instead. Two delivery mechanisms for one fact means a widget's behaviour depends on which layer instantiated it. Return the desired shape in the widget's result and let the caller apply it through whatever its layer owns; the widget keeps the decision, the caller keeps the delivery.

A rect never clips itself. `clip_rect` must name the container the content is confined to; passing the element's own rect constrains nothing, and the defect stays invisible until an offset carries that element outside its parent, at which point it draws over unrelated UI. Clip to the region's visible rect, and derive *drawn* and *hit-tested* from one predicate — culling one but not the other leaves rows that are scrolled out of sight still clickable.

`draw_context` must not expose the raw input state. Engine GUI widget implementations that need device-level transitions import the GUI's internal widget-context partition; this capability must not be exported or passed through generic widget dispatch. Code outside the GUI module must declare `input::data` as a system dependency for genuinely non-GUI input behavior and pass a value snapshot into deferred drawing callbacks. Do not retain a raw input reference in a deferred callback. An explicit input dependency does not authorize rebuilding GUI hit testing, layering, capture, or consumption outside `draw_context`.

## Reflection Helpers

Reflection is consumed through the helpers in `gse.meta`, never through raw `std::meta` calls rewritten at the use site. Enumerator metadata is read with `annotation_from_enum`, `enum_from_annotation`, and `enum_has_annotation`; member and type annotations go through `has_annotation`, `first_annotation_of_type`, and `apply_annotations`. A hand-rolled `template for` over `enumerators_of` that reaches into `annotations_of` and `constant_of` is a reimplementation of those helpers and is rejected on sight, even when it works.

`std::define_static_array(std::meta::annotations_of(...))` is banned outright. Two entities sharing an unqualified name and an annotation shape collide in that static storage, and the second silently inherits the first one's values — no error, no diagnostic, wrong data at runtime. Read annotations only through the `gse.meta` helpers, which own the safe read.

Design the annotation to fit the helper rather than the other way round. Annotations are plain aggregates with fixed-size `char` arrays for text, so one concrete type covers every enumerator of an enum and `annotation_from_enum` can find it. Carrying the payload as a template parameter (`my_annotation<"label">`) gives each enumerator a distinct type, makes the helpers inapplicable, and is what pushes authors into hand-rolling the read. Accessors return the annotation aggregate by value; a `std::string_view` into a returned temporary dangles, so callers hold the aggregate or bind it to a `static constexpr` local.

An existing call site is not an API contract. Some predate the helper or the bug it was written to avoid. Read the helper module before copying a usage pattern out of neighbouring code.

Enumerator *names* are already reflected: `enum_to_string` and `enum_from_string` derive them, and the global formatter prints them. Annotations carry metadata the identifier cannot — display text that differs from the identifier, colours, policies. An annotation whose payload merely restates the enumerator's own spelling is machinery with nothing in it: delete the annotation and call the name helper. This is the same defect as a hand-rolled read, one layer earlier — the search stopped at the first mechanism that worked instead of the one that already existed.

## Parameter Objects

When a function signature is long and the meaning of each argument is not obvious at the call site, group the cohesive operation inputs into a named aggregate and pass it with designated initialization. This is especially important for adjacent booleans, numeric values, IDs, or parameters of the same type, where positional calls conceal intent and permit valid-looking argument swaps. Field names should make the call self-documenting and defaults should represent safe, unsurprising behavior.

The aggregate must describe one cohesive operation rather than become a catch-all options bag or hide ownership and lifetime requirements. Keep independently owned services, mutable output, and unrelated concerns explicit when grouping them would weaken the API contract.

## Mandatory Questions

- Can an identifier be a `gse::id` or an existing typed ID instead of a string, integer, index, or sentinel?
- Does contiguous storage maintain a parallel ID-to-index map or hand-roll swap removal? Use `id_mapped_collection` unless the dense indices are themselves stable identities stored outside the collection.
- Are aggregates initialized with designated initializers whenever the language permits? Positional aggregate initialization requires a concrete reason.
- Can reflection derive this table, mapping, label, dispatch, or serialization behavior? If so, use annotations for exceptional metadata and existing reflection helpers before adding new machinery.
- Does a switch or parallel table map every enumerator to fixed metadata such as a color, label, priority, capability, or policy? Put that metadata on the enumerators as annotations and derive the lookup with the existing reflection helpers.
- Does the change call `std::meta` directly where a `gse.meta` helper exists, or spell `std::define_static_array(std::meta::annotations_of(...))` anywhere at all? Both are defects regardless of whether the result is currently correct.
- Is an annotation payload carried as a template parameter, forcing each enumerator to a distinct type and putting `annotation_from_enum` out of reach? Use one concrete aggregate with fixed-size `char` arrays.
- Does each mutation happen through the owning system, with other systems acting as producers through channels?
- Does any deferred callback, task, or channel payload retain a raw pointer or reference obtained from a shared view? Retain an immutable owning snapshot instead.
- Is a shared owning pointer stable for the entire published lifetime? Use `stable_shared` only for write-once `unique_ptr` resources; publish replaceable generations as `shared_ptr<const T>` snapshots or through a producer-consumer channel.
- Are separate features incorrectly sharing state or scheduling merely because they are displayed together?
- Is any physical or time-dependent expression weakly typed? If unavoidable at a foreign boundary, is the dimensional argument local and mechanically obvious?
- Does any value leave a strong type — `.as<Unit>()`, `id::tag()`, component access, a raw handle — and can you name the external contract that forces it? Converting to do arithmetic or to format is not a contract; both are supported on the type.
- Does a unit appear in a format literal, column heading, or label string rather than in the value's format spec?
- Does a per-frame path allocate strings, rebuild stable data, scan an unchanged collection, or create transient ownership?
- Does a new cache, published generation, pointer index, mutex, or atomic have a caller whose frequency or thread overlap you verified by reading the call site? A function reached only from `init` or from behind an `if (!initialized)` guard is cold regardless of its cost.
- Could this data be resolved once at startup and left immutable instead of mutated under synchronisation? Name the event that forces runtime mutation, and prefer requiring a restart over keeping the lock.
- Do two accessors over the same data return different shapes — a snapshot by value and a cached view? Unify them; the inconsistency means one is over-built.
- Does the change break an invariant previously established as load-bearing, then add machinery to compensate? Restore the invariant instead.
- Does a subprocess require a different environment? Construct that environment for the child without mutating process-wide state, and keep launch-only variables out of tracked build ABI inputs.
- Does a branch handle a state the surrounding code makes unreachable — an empty entry in a map written only by `map[key].push_back`, a null pointer parameter every call site fills, a precondition the single caller already established, a tautological short-circuit clause, or system state the scheduler sequenced through `init`? Name the write site or call site that makes it reachable, or delete the branch.
- Does a function take a pointer, an optional, or a defaulted argument to express an absence that never occurs? Take a reference and remove the branches that pointer implies.
- Does a function receive both an owner by reference and a pointer into that owner? Delete the redundant parameter and read through the owner already in scope.
- Is state duplicated such that two fields can disagree? Can one be derived or replaced by a stronger representation?
- Does the change decide visibility, enablement, reachability, or extent on one path and act on it from another? Both must resolve through the owner of that fact, or the system permits states it treats as impossible, most often input that survives after presentation is clipped, hidden, or disabled.
- Does UI code reconstruct hover or activation from rectangle containment, raw mouse transitions, and input availability even though `draw_context` or an established widget already owns that decision?
- Does code outside the GUI module obtain raw input without declaring `input::data` as a system dependency, or capture a raw input reference in deferred work?
- Does an exported GUI type, generic widget path, or public draw context expose the module-internal widget input capability?
- Does new code sit at a lower layer than the call it inherited from its previous home? Express the intent in the receiving layer's own vocabulary rather than reaching upward for the original entry point.
- Is the change reimplementing a state transition or interaction rule that already appears elsewhere? If it recurs, can the invariant move into a shared behavior helper while presentation remains feature-owned?
- Is a long positional call difficult to understand or easy to misorder? Can cohesive inputs become a named aggregate passed with designated initialization?
- Does the solution reuse the codebase's established helper and idiom, or create a second way to express the same operation?
- Before this renderer, parser, or measurement helper was written, which existing feature was read end to end as the nearest analogue? A search that returned nothing is not an answer — name the feature that presents, measures, or drives the same kind of thing.
- Is a helper defined local to a feature whose parameters mention nothing about that feature? It belongs at the layer that owns the concept, in this change rather than a later one.
- For GUI specifically: read-only text with selection is `gui::text_area` with `text_span` colouring and `text_area_position_at` hit-testing; scrolling is `gui::scroll_region`; measurement and wrapping are `gse::font`; press and hover are `draw_context` queries. Painting text directly, or measuring by hand, means one of these was missed.
- Would a larger refactor leave fewer concepts and fewer special cases? Diff size is not a reason to preserve accidental complexity.

## Findings Format

Report findings by severity, with an exact file and line. Each finding must state:

- Impact: the observable failure or maintenance cost.
- Mechanism: the code path or representation that permits it.
- Resolution: the simplest cohesive change that removes the underlying cause. Removing a redundant abstraction or replacing an invalid representation is preferable to hardening code that should not exist.
- Prevention: whether this is genuinely local or belongs to a recurring error class. For a recurring class, identify the strongest practical guardrail that makes repetition impossible or conspicuous: a stronger type, narrower API, ownership boundary, reflection-derived implementation, generated code, compile-time constraint, lint, assertion, or focused test. Prefer enforcement at the lowest reusable layer. If no proportionate guardrail exists, say why and classify the finding as a one-off.

Do not propose a symptom-only patch as the resolution when the same invalid construction remains easy to write. For a recurring error class, the resolution should include its prevention whenever that prevention is proportionate. Conversely, do not add framework machinery for a mistake whose preconditions are unique and unlikely to recur. The prevention must remove more complexity than it introduces across the codebase.

Do not report preference-only churn. Call out missing tests when they leave meaningful behavior unprotected, but do not substitute test requests for identifying the underlying risk.

## Verification

Agents must never invoke a build, configure, compilation, linking, or test command that can trigger a build in this repository. Verification is limited to non-build static inspection, focused diff checks, existing logs and artifacts, and reasoning from the source. State clearly when changes have not been compiled. The repository owner performs builds.

Do not infer style compliance from a formatter, whitespace check, reference search, or successful static analysis. Directly audit the declarations, definitions, initializers, and file organization in every changed file against `docs/STYLEGUIDE.md`. Verify declarations and definitions independently instead of applying a declaration-layout rule to its definition or assuming their layouts should match.

## Evolution

Update this guide when review feedback reveals a stable preference or recurring failure mode. Generalize the lesson so it survives refactors. Do not encode a one-off decision until repeated approvals or an explicit owner decision establishes it as an engine-wide rule. When a new rule overlaps the style guide, update the style guide and keep only the higher-level review principle here.
