# GSE Code Review Guide

This document is the durable review contract for code produced by humans or language models. Apply it together with `docs/STYLEGUIDE.md`. Keep it foundational: record stable engineering values and approved idioms, not details likely to become stale after a refactor.

## Review Standard

A change should feel inevitable in the surrounding code: it uses the engine's existing vocabulary, ownership model, helpers, and composition patterns; introduces no parallel abstraction without a demonstrated need; and reaches the simplest cohesive design, even when that requires a broad refactor. Simplicity does not mean minimizing the diff or the number of types. It means minimizing concepts, exceptional paths, duplicated knowledge, and hidden coupling in the resulting system.

Review in this order:

1. Correctness: find latent bugs, invalid states, lifetime hazards, concurrency errors, lossy conversions, and unhandled failure paths.
2. Architecture: enforce ownership and producer-consumer semantics. Separate independently scheduled or independently owned features into systems; directly compose behavior that shares one responsibility and lifecycle.
3. Engine congruence: prefer existing engine types, IDs, reflection facilities, helpers, channels, and established idioms over local substitutes.
4. Dimensional correctness: use strong unit types for every physical quantity. Require a concrete dimensional derivation for any raw numeric math that resembles a physical quantity, ratio, conversion, or integration step.
5. Runtime cost: avoid per-frame string construction and allocation, repeated formatting, avoidable container churn, redundant work, and synchronization in hot paths. Verify the actual lifetime and call frequency rather than assuming a function is cold.
6. Complexity: challenge every new state variable, branch, cache, adapter, mapping, and abstraction. Prefer designs where invalid combinations are unrepresentable and behavior follows from one source of truth.
7. Style and API shape: enforce every rule in `docs/STYLEGUIDE.md`, including naming, file organization, module visibility, unit types, channel pushes, spans, ranges, and declaration/definition structure.

## Behavioral Consolidation

Treat repeated state transitions and interaction rules as duplicated knowledge even when their drawing, data, or call sites look different. Selection, focus, activation, dismissal, dragging, and replacement of indexed data are examples of behavior whose invariants should have one authoritative implementation once they recur. Consolidate them into the lowest reusable helper or state type that can own the complete transition, rather than leaving each feature to coordinate related fields and edge cases independently.

Prefer small composable behavior helpers over manager hierarchies, visual base classes, or wrappers that merely shorten code. A useful helper centralizes an invariant, gives callers the result needed for custom presentation, and makes invalid transitions difficult to express. Similar-looking rendering without shared behavioral rules is not by itself a reason to abstract.

## Parameter Objects

When a function signature is long and the meaning of each argument is not obvious at the call site, group the cohesive operation inputs into a named aggregate and pass it with designated initialization. This is especially important for adjacent booleans, numeric values, IDs, or parameters of the same type, where positional calls conceal intent and permit valid-looking argument swaps. Field names should make the call self-documenting and defaults should represent safe, unsurprising behavior.

The aggregate must describe one cohesive operation rather than become a catch-all options bag or hide ownership and lifetime requirements. Keep independently owned services, mutable output, and unrelated concerns explicit when grouping them would weaken the API contract.

## Mandatory Questions

- Can an identifier be a `gse::id` or an existing typed ID instead of a string, integer, index, or sentinel?
- Does contiguous storage maintain a parallel ID-to-index map or hand-roll swap removal? Use `id_mapped_collection` unless the dense indices are themselves stable identities stored outside the collection.
- Are aggregates initialized with designated initializers whenever the language permits? Positional aggregate initialization requires a concrete reason.
- Can reflection derive this table, mapping, label, dispatch, or serialization behavior? If so, use annotations for exceptional metadata and existing reflection helpers before adding new machinery.
- Does a switch or parallel table map every enumerator to fixed metadata such as a color, label, priority, capability, or policy? Put that metadata on the enumerators as annotations and derive the lookup with the existing reflection helpers.
- Does each mutation happen through the owning system, with other systems acting as producers through channels?
- Does any deferred callback, task, or channel payload retain a raw pointer or reference obtained from a shared view? Retain an immutable owning snapshot instead.
- Is a shared owning pointer stable for the entire published lifetime? Use `stable_shared` only for write-once `unique_ptr` resources; publish replaceable generations as `shared_ptr<const T>` snapshots or through a producer-consumer channel.
- Are separate features incorrectly sharing state or scheduling merely because they are displayed together?
- Is any physical or time-dependent expression weakly typed? If unavoidable at a foreign boundary, is the dimensional argument local and mechanically obvious?
- Does a per-frame path allocate strings, rebuild stable data, scan an unchanged collection, or create transient ownership?
- Does a subprocess require a different environment? Construct that environment for the child without mutating process-wide state, and keep launch-only variables out of tracked build ABI inputs.
- Is state duplicated such that two fields can disagree? Can one be derived or replaced by a stronger representation?
- Is the change reimplementing a state transition or interaction rule that already appears elsewhere? If it recurs, can the invariant move into a shared behavior helper while presentation remains feature-owned?
- Is a long positional call difficult to understand or easy to misorder? Can cohesive inputs become a named aggregate passed with designated initialization?
- Does the solution reuse the codebase's established helper and idiom, or create a second way to express the same operation?
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
