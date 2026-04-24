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

## Vulkan enum → vk enum mapping (`Engine/Engine/Source/Gpu/Vulkan/VulkanReflect.cppm`)

**Location:** `gse::vulkan::to_vk(E)` for 11 scalar enums in `GpuTypes.cppm`: `cull_mode`, `compare_op`, `polygon_mode`, `topology`, `sampler_filter`, `sampler_address_mode`, `border_color`, `image_layout`, `image_format`, `vertex_format`, `descriptor_type`.

**Ideal:** One reflected function template reads the `[[= vk::X]]` annotation from each enumerator and returns the matching vk value. Adding a new enum = annotate enumerators, no mapping code.

```cpp
template <typename Vk, typename E>
    requires std::is_enum_v<E>
auto reflect_to_vk(E e) -> Vk {
    template for (constexpr auto v : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if ([:v:] == e) {
            return [: std::define_static_array(std::meta::annotations_of(v, ^^Vk))[0] :];
        }
    }
    std::unreachable();
}
```

**Shipped:** Hand-written `switch` per enum in `VulkanReflect.cppm` (~200 lines). The `[[= vk::X]]` annotations are preserved on the enumerators in `GpuTypes.cppm` as future-proof documentation — they compile fine, they're just not consumed by any code.

**Blocker:** clang-p2996 (Bloomberg fork, commit `9ffb96e3ce362289008e14ad2a79a249f58aa90a` in `clang-p2996-v2`, as of 2026-04-23) crashes during "instantiating function definition" for any template function body that extracts annotation values from an enumerator info. Tried: `[:anns[0]:]` splice, `std::meta::extract<Vk>(ann)`, `template for` loop var, `index_sequence` pack expansion, per-enumerator helper templates. All either segfault or produce "reflection not usable in a splice expression."

**Validate the fix:** Compile the snippet above against a single-file test enum. If `static_assert(reflect_to_vk<int>(E::a) == 1)` holds, the blocker is gone.

```cpp
enum class E : int { a [[= 1]], b [[= 2]] };
static_assert(reflect_to_vk<int>(E::a) == 1);
```

**Flip cost:** ~30 minutes. Replace `VulkanReflect.cppm`'s switches with one ~15-line reflected template + one-liner per-enum wrapper. Annotations already in place on enumerators. Call sites unchanged.

**See also:** `memory/project_clang_p2996_bugs.md` bug #14 for the full pattern catalog.

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
