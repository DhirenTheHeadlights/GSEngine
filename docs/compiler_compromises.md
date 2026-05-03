# Compiler Compromises

Punch list of places where the codebase ships a hand-written / duplicated / less-elegant design because a toolchain bug or missing language feature blocked the ideal one. Each entry is self-contained — when the blocker is fixed, you should be able to read a single entry and flip the compromise to the ideal without needing the original context.

## How to use

- **When you shipped a compromise:** add an entry. Don't skip "this was annoying" notes — the annoyance is the point.
- **When a compiler/library update drops:** run each entry's *validate the fix* reproducer. Anything that now compiles goes to the top of the backlog.
- **When in doubt about an existing suboptimal pattern:** check here first. If it's listed, it's intentional.

## Entry template

```
## <Short title> (<location:path>)

**Location:** Precise pointer — file + function/call site + line if relevant.

**Ideal:** What we wanted. Include a short code sketch if it helps.

**Shipped:** What we have instead. Why this form specifically, if non-obvious.

**Blocker:** Specific compiler/library bug(s) or language limitation. Include error symptom (diagnostic message or "compiler segfault"), and which toolchain version exhibits it.

**Validate the fix:** Smallest standalone reproducer that, when it compiles, means we can flip to ideal. Short code snippet is ideal.

**Flip cost:** Rough estimate — minutes or hours.

**See also:** Related memory files, deeper bug catalogs, upstream issues.
```

---

## Vulkan enum/struct → vk mapping — clang-p2996 ICE workaround (`Engine/Engine/Source/Gpu/Vulkan/Types.cppm`)

**Status:** Working with reflection, via a workaround pattern. Resolved 2026-05-01.

**Location:** `gse::vulkan::to_vk(...)` / `from_vk(...)` / `reflect_struct_to_vk(...)` templates at the bottom of `Vulkan/Types.cppm`. Covers ~22 scalar enums (`cull_mode`, `compare_op`, `polygon_mode`, `topology`, `sampler_filter`, `sampler_address_mode`, `border_color`, `image_format`, `vertex_format`, `image_view_type`, `image_layout`, `index_type`, `bind_point`, `descriptor_type`, `stage_flag`, `load_op`, `store_op`, `image_type`, `sample_count`, `acceleration_structure_type`, `build_acceleration_structure_mode`, `query_kind` — last one in `Vulkan/QueryPool.cppm`), ~10 bitflag enums (`buffer_usage`, `stage_flags`, `image_usage`, `image_aspect_flags`, `access_flags`, `pipeline_stage_flags`, `memory_property_flags`, `image_create_flags`, `geometry_flags`, `build_acceleration_structure_flags`), 4 struct conversions (`surface_format`, `viewport`, `buffer_copy_region`, `acceleration_structure_build_range_info`), and 4 reverse `from_vk` lookups (`vk::Result`, `vk::PresentModeKHR`, `vk::ColorSpaceKHR`, `vk::Format`). Total cost ~50 lines of generic machinery + per-enum annotations.

**The bug we hit:** clang-p2996 (Bloomberg fork, as of 2026-04-23) rejects three patterns when they appear directly inside a *templated* function body:
- `std::meta::extract<Vk>(first_annotation_of_type(loop_var, ^^Vk))` → "expressions of consteval-only type are only allowed in constant-evaluated contexts" (the `^^Vk` argument).
- `[: std::meta::type_of(member) :]` where `member` came from a `template for` → "splice operand must be a constant expression".
- `out.[: dst_member :] = ...` where `dst_member` was bound inside a lambda inside the `template for` body → "reflection not usable in a splice expression".

The common cause is that `template for` loop variables, while `constexpr`, don't always propagate as constant expressions through nested calls/lambdas inside template bodies. Worked fine in non-template contexts.

**Workaround (the shipped form):** push every reflective splice and `extract` call out of the template function body and into free `consteval` helpers that take `std::meta::info` arguments. The template body only does runtime lookup against a static table built by those helpers.

```cpp
// 1. Scalar: pre-build the {enum, vk} table at consteval time (no template-for, no splices).
template <typename E, typename Vk>
consteval auto build_to_vk_table() {
    std::array<std::pair<E, Vk>, enumerator_count<E>()> result{};
    auto enums = std::meta::enumerators_of(^^E);
    for (std::size_t i = 0; i < enums.size(); ++i) {
        result[i].first = std::meta::extract<E>(enums[i]);
        for (auto ann : std::meta::annotations_of(enums[i])) {
            if (std::meta::type_of(ann) == ^^Vk) {
                result[i].second = std::meta::extract<Vk>(ann);
                break;
            }
        }
    }
    return result;
}

template <typename E>
constexpr auto to_vk(E e) -> vk_type_of<E> {
    constexpr auto table = build_to_vk_table<E, vk_type_of<E>>();
    for (const auto& [k, v] : table) {
        if (k == e) return v;
    }
    std::unreachable();
}

// 2. Bitflag: same idea but table sized by total annotation count, not enumerator count.
//    Multiple annotations per enumerator (e.g. `eShaderDeviceAddress` co-annotated with
//    `eUniformBuffer`/`eStorageBuffer`/`eIndirectBuffer`) get separate table rows.

// 3. Struct walker: keep `template for` over member infos in the template, but pull the
//    type-comparison and member-matching logic into free consteval helpers so the only
//    splices in the template body are field accesses on the loop's `constexpr auto` infos.
consteval auto match_dst_member(std::meta::info src, std::meta::info dst_type) -> std::meta::info;
consteval auto types_match(std::meta::info src, std::meta::info dst) -> bool;

template <typename Dst, typename Src>
constexpr auto reflect_struct_to_vk(const Src& src) -> Dst {
    Dst out{};
    template for (constexpr auto src_member : std::define_static_array(
            std::meta::nonstatic_data_members_of(^^Src, std::meta::access_context::unchecked()))) {
        constexpr auto dst_member = match_dst_member(src_member, ^^Dst);
        if constexpr (types_match(src_member, dst_member)) {
            out.[: dst_member :] = src.[: src_member :];
        } else {
            out.[: dst_member :] = to_vk(src.[: src_member :]);
        }
    }
    return out;
}
```

**Why this dodges the bug:** the consteval helpers (`build_to_vk_table`, `match_dst_member`, `types_match`) take plain `std::meta::info` arguments and return values. Inside them, `^^Vk` etc. are normal uses of types from their own template parameters, not propagated through a `template for` loop variable. The template function body never has to do an `extract` or `type_of` call on a `template for` variable — it only does cheap runtime lookup against the pre-built table or boolean output of `types_match`.

**Caveats/gotchas if you touch this:**
- `to_vk<E>` and `reflect_struct_to_vk` must be `constexpr` (not just `auto`), otherwise `static_assert` validation in tests stops working. The codebase doesn't currently `static_assert` them but the validation tests in `tmp/p2996-reflect/` do.
- `vk_type_of<E>` is a template alias that splices the result of a `consteval auto vk_type_for<E>()` helper — direct splicing of `type_of(annotations_of(...)[0])` in the alias triggered a different ICE; routing through a named consteval call is fine.
- Bitflag annotation count is computed by a separate consteval `annotation_count<E, Vk>()` because it's not the same as `enumerator_count<E>()` — multiple annotations per enumerator inflate the row count.

**The fully-reflective "ideal" (which still doesn't compile):**

```cpp
template <typename Vk, typename E>
auto reflect_to_vk(E e) -> Vk {
    template for (constexpr auto v : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if ([:v:] == e) {
            return std::meta::extract<Vk>(first_annotation_of_type(v, ^^Vk));
        }
    }
    std::unreachable();
}
```

This is what upstream `155d7af0` shipped and what blew up. Once clang-p2996 fixes the constant-expression propagation through `template for` + nested consteval calls, you can collapse the two layers (`build_to_vk_table` + `to_vk`) into the single template above. Same flip applies to the bitflag and struct variants.

**Validate the fix:** standalone reproducers live at `tmp/p2996-reflect/` (workaround_a/b/c/d/e). When the toolchain catches up, port them to inline `template for` + direct extract; if all three patterns compile and the static_asserts hold, you can collapse the helpers.

**Flip cost:** ~30 minutes when the toolchain catches up. The annotations on enumerators are unchanged; only the implementation of `to_vk` / `from_vk` / `reflect_struct_to_vk` collapses.

**See also:** `memory/project_clang_p2996_bugs.md` bug #14 for the full pattern catalog. Bugs #8 and #9 cover the bitflag/struct variants. The companion entries below for blend-preset and unit-system are unrelated to this one — that workaround unblocked enum/bitflag/struct reflection but didn't touch annotation-of-non-structural-type or namespace-injection.

---

## Blend preset → `vk::PipelineColorBlendAttachmentState` (`Engine/Engine/Source/Gpu/GpuPipeline.cppm`)

**Location:** Inline `switch` inside the pipeline creation function, around line 236.

**Ideal:** Annotate each `blend_preset` enumerator with its `vk::PipelineColorBlendAttachmentState` value, use the same reflected mapping as the other vk enums.

```cpp
enum class blend_preset : std::uint8_t {
    none [[= vk::PipelineColorBlendAttachmentState{...}]],
    alpha [[= vk::PipelineColorBlendAttachmentState{...}]],
    alpha_premultiplied [[= vk::PipelineColorBlendAttachmentState{...}]],
};
```

**Shipped:** ~25-line inline `switch` at the single call site. `blend_preset` enum has no annotations.

**Blocker:** P2996 requires annotation values to be *structural type*. `vk::PipelineColorBlendAttachmentState` contains `vk::ColorComponentFlags` (a `vk::Flags<...>` wrapper), which is not structural. Error: `C++26 annotation attribute requires a value of structural type`.

**Validate the fix:** Try annotating an enumerator with a `vk::PipelineColorBlendAttachmentState{...}` literal in a single-file test. If it compiles, either the structural-type restriction was relaxed in the standard, or vulkan-hpp's flags became structural.

**Flip cost:** ~15 minutes. Three `constexpr vk::PipelineColorBlendAttachmentState` constants next to the enum + three annotations + inline switch becomes one reflected call.

**See also:** Bug #11 in `memory/project_clang_p2996_bugs.md`.

---

## Multi-annotation bitflag reflection (`Engine/Engine/Source/Gpu/GpuTypes.cppm` buffer_flag / stage_flag)

**Location:** `buffer_flag` and `stage_flag` enumerators in `GpuTypes.cppm`; `to_vk(buffer_usage)` / `to_vk(stage_flags)` in `VulkanReflect.cppm`.

**Ideal:** Multi-annotate each flag bit with its vk counterpart(s), with compound rules (e.g. `eShaderDeviceAddress` on top of `eUniformBuffer`) captured directly in the annotations. One generic `to_vk(flags<E>)` OR-folds every annotation of every set bit.

```cpp
enum class buffer_flag : std::uint32_t {
    uniform [[= vk::BufferUsageFlagBits::eUniformBuffer]]
            [[= vk::BufferUsageFlagBits::eShaderDeviceAddress]] = 0x01,
    // ...
};

template <typename VkBits, typename FlagEnum>
auto to_vk(gse::flags<FlagEnum> fls) -> vk::Flags<VkBits> {
    vk::Flags<VkBits> result{};
    template for (constexpr auto v : ...) {
        if (fls.test([:v:])) {
            template for (constexpr auto a : annotations_of(v)) {
                result |= [:a:];
            }
        }
    }
    return result;
}
```

**Shipped:** Annotations stripped from `buffer_flag` / `stage_flag`. `to_vk(buffer_usage)` and `to_vk(stage_flags)` hand-written with explicit `if (test(bit)) result |= VkBit;` chains, and a separate post-fold for the `eShaderDeviceAddress` compound rule.

**Blocker:** Two separate bugs, either one fatal.
1. Nested `template for` (outer over enumerators, inner over annotations per enumerator) OOMs clang-p2996's AST serializer when writing the BMI. Error: `LLVM error: out of memory` with stack showing `ASTWriter::serializing`.
2. `decltype(to_vk(FlagEnum{}))` to auto-deduce the vk flag-bits type from the scalar `to_vk(E)` causes parser-level OOM. Pattern: compile-time overload resolution inside a template body loops.

**Validate the fix:** Compile a two-level `template for` with annotation extraction on a small bitflag enum. If the BMI writes without crashing, bug 1 is fixed. Separately, try the `decltype(scalar_call(X{}))` deduction pattern — if it doesn't OOM, bug 2 is fixed.

**Flip cost:** ~20 minutes. Re-annotate two enums (~25 annotations) + swap two bitflag functions for one generic.

**See also:** Bugs #8 and #9 in `memory/project_clang_p2996_bugs.md`.

---

## Unit-system Phase 2: `_t` alias injection (`Engine/Engine/Source/Math/Units/Units.cppm`)

**Location:** Per-quantity `_t` alias declarations (e.g. `template <typename T = float, auto U = meters> using length_t = ...;`) and the `using length = length_t<>;` line, repeated ~35 times.

**Ideal:** One `consteval { emit_quantity_aliases<^^gse>(); }` block at module end. It scans the namespace for `quantity_spec`-annotated tags and injects both alias template + default alias per tag. Adding a new quantity = one annotated tag struct, nothing else.

**Shipped:** Hand-written 2-3 line alias pair per quantity (with the `quantity_t<Tag, T, U>` generic doing most of the work, so the per-quantity body is minimal).

**Blocker:** clang-p2996 doesn't implement `std::meta::queue_injection` or namespace-scope declaration injection (`<meta>` only exposes `define_aggregate` for class types). Template-alias injection is a P2996 future-work item.

**Validate the fix:** Check if `std::meta::queue_injection` or equivalent appears in `<meta>`. If so, try injecting a single `using X = int;` inside a `consteval {}` block at namespace scope. If that compiles, the alias-injection is unlocked.

**Flip cost:** ~1 hour. Remove ~70 lines of alias-pair declarations, replace with one consteval injection block.

**See also:** `docs/reflection_opportunities.md` section **S1 Phase 2 → 2d**.

---

## Tag recovery via class-member splice workaround (`Engine/Engine/Source/Math/Units/Quantity.cppm`)

**Location:** `tag_recovery<D>` and `tag_recovery_type<D>` class templates in `Quantity.cppm`, used by `operator*` / `operator/` to look up a named tag by dimension.

**Ideal:** Inline the lookup directly in the operator body:

```cpp
template <is_quantity Q1, is_quantity Q2>
constexpr auto operator*(const Q1& lhs, const Q2& rhs) {
    using result_d = decltype(typename Q1::dimension() * typename Q2::dimension());
    constexpr auto tag_info = find_root_tag_by_dim(^^gse, ^^result_d);
    if constexpr (tag_info != std::meta::info{}) {
        using found_tag = typename [: tag_info :];
        // ...
    }
}
```

**Shipped:** Two auxiliary class templates (`tag_recovery<D>` with static info member + `tag_recovery_type<D>` with splice-to-type alias) so the splice happens at class-member scope instead of function-body scope.

**Blocker:** clang-p2996 crashes when `[: function_call_result :]` appears inside a function body with the call result depending on a template parameter. Moving the splice into a class template's static member dodges the bug (that pattern mirrors `quantity_tag_traits<Tag>::parent_tag`, which works).

**Validate the fix:** Put `using X = typename [: some_consteval_call<T>() :];` inside a template function body. If it compiles, the workaround can be collapsed.

**Flip cost:** ~10 minutes. Delete two helper class templates, inline the splice in the two operators.

**See also:** Bugs #3 and #4 in `memory/project_clang_p2996_bugs.md`.

---

## Adding a new entry

Copy the template at the top. Keep the "validate the fix" reproducer as minimal as possible — ideally something that can be pasted into a single .cpp file with just `#include <meta>` and compiled standalone. The whole point is that future-you (or future-me) can run each one and see which ones flipped green on the latest toolchain without re-deriving the context.
