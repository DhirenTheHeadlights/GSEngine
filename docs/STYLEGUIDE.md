# GSE Style Guide

Most layout decisions in this codebase are owned by `.clang-format` (e.g. tabs, brace style, pointer alignment, how to indent continuations, where blank lines go inside namespaces, no vertical alignment, always-braces on `if`/`for`/`while`, "fully wrapped or fully inline" arg lists, designated initializers always one per line). `ColumnLimit` is `0` — there is no line-length-based wrapping, so *where* a statement breaks is author-driven; clang-format only normalizes the indentation of a break and keeps argument lists all-or-nothing (`BinPackArguments`/`BinPackParameters` are off). This document covers everything *clang-format can't decide for you* — semantic conventions, naming, API shape, and patterns specific to the engine.

## Naming
- STL style (snake_case for everything).
- snake_case includes compile-time constants, `static constexpr`, enum values, and `shader_constant_block` fields. There is no `SCREAMING_SNAKE_CASE` in this codebase — the HLSL/C habit of uppercase constants does not carry over.
- Private member variables prefixed with `m_`.
- Do not prefix functions with `get_`. The verb is implied — a function that returns a value is already a getter. Use the noun (`name()`, `value()`), `_of` for projections (`type_of(x)`, `annotation_of<A>(m)`), or a verb that describes the action (`fetch_`, `compute_`, `find_`) when the work is non-trivial.

Enforced by `clang-tidy` (`readability-identifier-naming` + custom `gse-no-get-prefix`).

## Comments

Do not add comments. Code should be self-documenting.

---

## Bodies on Their Own Line

A body goes on its own line even when it holds a single statement — never collapse it. clang-format enforces this for `if`/`for`/`while` and for non-empty lambdas, but **not for `struct`/`class`/`union` bodies** — with `ColumnLimit` at `0`, nothing forces a record body to expand, so a one-liner survives clang-format untouched. Keep it expanded:

```cpp
// correct
struct binding {
    using element = T;
};

// wrong
struct binding { using element = T; };
```

Empty bodies stay collapsed (`{}`, `= default`).

---

## Function Declarations and Definitions

**Declarations** go inside the namespace; **definitions** go outside it:

```cpp
export namespace gse::foo {
    auto bar(const type& param1, type param2) -> return_type;
}

auto gse::foo::bar(const type& param1, type param2) -> return_type {
    ...
}
```

clang-format handles all wrapping based on the column limit — short signatures stay on one line, long ones wrap with one parameter per line and `)` on its own line.

Default argument values belong only on declarations, never on definitions.

When a constructor has an initializer list, keep it on the same line as the signature with `{` following immediately. If the body is empty, collapse to `{}`:

```cpp
my_type::my_type(const foo& f) : m_foo(f) {}

my_type::my_type(const foo& f, const bar& b) : m_foo(f), m_bar(b) {
    do_something();
}
```

Prefer `non_copyable` / `non_movable` base classes over writing deleted copy/move declarations by hand. Always declare a destructor in the derived class to suppress virtual destructor warnings from the base. **A user-declared destructor implicitly deletes the move operations**, so when inheriting from `non_copyable` you must also re-declare the move ops as defaulted, otherwise the type silently becomes immovable and standard containers (`std::vector`, `std::pair`, etc.) will refuse to hold it:

```cpp
// correct
class my_type : public non_copyable {
public:
    ~my_type();

    my_type(my_type&&) noexcept = default;
    auto operator=(my_type&&) noexcept -> my_type& = default;
};

// wrong — user-declared destructor kills the implicit move ops; instances
// can't be stored in vector or moved out of pair
class my_type : public non_copyable {
public:
    ~my_type();
};

// wrong — re-implementing what non_copyable already provides
class my_type {
public:
    my_type(const my_type&) = delete;
    auto operator=(const my_type&) -> my_type& = delete;
};
```

Long `if` conditions are not wrapped — keep them on one line even if they exceed the column limit.

---

## File Organization

Each file should have one `export namespace` block containing all declarations, followed by all definitions outside it. Never reopen or add a second `export namespace` block to interleave declarations and definitions.

When a file contains multiple sizeable class definitions, give each sizeable class its own file. Small related types (e.g. a group of phase structs, or a handful of POD structs) can share a file. A class is "sizeable" if its definition section would be long enough to make you scroll past other class declarations to reach it.

---

## Module Visibility

Do not use `detail` namespaces or anonymous namespaces to hide implementation. In C++ modules, anything not in an `export` block is already module-private. Use a plain non-exported namespace block instead:

```cpp
// correct — module-private, no leakage, no noise
namespace gse {
    struct my_impl_helper { ... };
}

// wrong — detail namespace is unnecessary ceremony
namespace gse::detail {
    struct my_impl_helper { ... };
}

// wrong — anonymous namespace is unnecessary in modules
namespace {
    struct my_impl_helper { ... };
}
```

This is not a module-only rule. It holds in every translation unit, including non-module `.cpp` files and single-TU executables (e.g. the codegen tools) — TU-local helpers go at file scope or in a named namespace, never an anonymous one.

Enforced by `clang-tidy` (`gse-no-detail-namespace`, `gse-no-anonymous-namespace`).

---

## `inline` on Functions

Do not mark functions `inline` in module interface or implementation units. In modules, function definitions attached to the module have implicit module linkage — ODR is handled per-module, not via the `inline` mechanism the preprocessor header model needed. Writing `inline` is redundant and signals "I don't know why this is here."

```cpp
// correct
auto simd_cpu_supported() noexcept -> bool {
    return sse2;
}

// wrong — inline is meaningless here
inline auto simd_cpu_supported() noexcept -> bool {
    return sse2;
}
```

The only legitimate use of `inline` inside a module is `inline namespace` (for versioning), which is a different feature sharing the keyword.

**Non-`constexpr` namespace-scope variables are different.** A non-`inline` namespace-scope variable in a module interface (especially `thread_local` ones) gets emitted by every TU that imports the module, producing duplicate-symbol errors at link time. Mark these `inline` — `inline thread_local foo bar;` for TLS, `inline foo bar;` for plain globals. This is the variable-deduplication mechanism, not the function-linkage one — different semantics, same keyword.

```cpp
// correct — single definition shared across importers
inline thread_local thread_buffer tls;
inline std::atomic<bool> trace_enabled = true;

// wrong — every TU that imports this module emits its own copy
thread_local thread_buffer tls;
std::atomic<bool> trace_enabled = true;
```

**`constexpr` variables don't need `inline`.** Since C++17, namespace-scope `constexpr` variables are implicitly `inline`. Writing `inline constexpr` is redundant — plain `constexpr` is the form.

```cpp
// correct
constexpr std::array<std::string_view, 5> exts = { ".png", ".jpg", ".jpeg", ".tga", ".bmp" };

// wrong — inline is implied by constexpr at namespace scope
inline constexpr std::array<std::string_view, 5> exts = { ".png", ".jpg", ".jpeg", ".tga", ".bmp" };
```

Enforced by `clang-tidy` (`gse-no-inline-in-modules`).

---

## Concepts in Template Parameter Lists

When constraining a template parameter on a single concept, put the concept in place of `typename`. Do not write a separate `requires` clause for that constraint:

```cpp
// correct — concept is the parameter introducer
template <has_asset_format T>
auto load_baked(const std::filesystem::path& path, T& out) -> bool;

// wrong — extra requires clause for what is just a single concept
template <typename T> requires has_asset_format<T>
auto load_baked(const std::filesystem::path& path, T& out) -> bool;
```

Reserve `requires` clauses for constraints that genuinely don't fit the parameter slot — multi-parameter relationships, ad-hoc `requires { ... }` expressions, or boolean compositions of concepts.

```cpp
// correct — relationship between two parameters needs a requires clause
template <typename T, typename U> requires std::convertible_to<T, U>
auto coerce(T&& v) -> U;
```

Apply the same rule to abbreviated function templates (`auto` parameters) — use the concept directly:

```cpp
// correct
auto print_one(std::integral auto v) -> void;

// wrong
auto print_one(auto v) -> void requires std::integral<decltype(v)>;
```

Enforced by `clang-tidy` (`gse-concept-in-template-param`).

---

## Namespace Qualifiers

Strip all redundant namespace qualifiers. If a name is already visible in the current scope, do not qualify it:

```cpp
// correct — inside gse::foo context, bar_type is already visible
auto gse::foo::do_thing(const bar_type& x) -> result_type {
    ...
}

// wrong — gse::foo:: prefix is redundant here
auto gse::foo::do_thing(const gse::foo::bar_type& x) -> gse::foo::result_type {
    ...
}
```

This applies to both function signatures and bodies.

Enforced by `clang-tidy` (`gse-redundant-namespace-qualifier`).

---

## Aggregate Initialization

When the type is already known from the assignment target, omit the explicit type name from the brace initializer:

```cpp
// correct — map value type is already known
ctrl.parameters[key] = {
    .value = value,
    .is_trigger = false,
};

// wrong — animation_parameter is redundant
ctrl.parameters[key] = animation_parameter{
    .value = value,
    .is_trigger = false,
};
```

For function call arguments where the type is needed for template deduction, keep the explicit type.

Designated initializers are always one per line, regardless of how short the aggregate is — never pack `.field = value` pairs onto a single line.

---

## Type-Safe Units (`gse.math`)

Always use the unit system for physical quantities. Never represent them with raw `float`, `double`, or integer types. This applies everywhere — member variables, function parameters, return types, local variables.

### Available quantity types

| Quantity | Type alias | Unit constants |
|---|---|---|
| Time | `time` | `nanoseconds`, `microseconds`, `milliseconds`, `seconds`, `minutes`, `hours` |
| Length / distance | `length` | `meters`, `centimeters`, `millimeters`, `kilometers`, `feet`, `inches`, `yards` |
| Displacement (relative) | `displacement` | same as length |
| Position (absolute) | `position` | same as length |
| Angle | `angle` | `radians`, `degrees` |
| Mass | `mass` | `kilograms`, `grams`, `pounds` |
| Velocity | `velocity` | `meters_per_second`, `kilometers_per_hour`, `miles_per_hour` |
| Acceleration | `acceleration` | `meters_per_second_squared` |
| Angular velocity | `angular_velocity` | `radians_per_second`, `degrees_per_second` |
| Force | `force` | `newtons`, `pounds_force` |
| Torque | `torque` | `newton_meters` |
| Energy | `energy` | `joules`, `kilojoules`, `megajoules` |
| Power | `power` | `watts`, `kilowatts` |
| Mass | `mass` | `kilograms`, `grams` |
| Inverse mass | `inverse_mass` | `per_kilograms` |
| Inertia | `inertia` | `kilograms_meters_squared` |
| Stiffness | `stiffness` | `newtons_per_meter` |
| Density | `density` | `kilograms_per_cubic_meter` |
| Area | `area` | `square_meters` |
| Percentage | `percentage<T>` | — |

Construct values using the unit constant as a callable:

```cpp
// correct
const time timeout = seconds(5.f);
const length jump_height = meters(1.2f);
const angle fov = degrees(90.f);
const velocity max_speed = meters_per_second(10.f);

// wrong — raw floats with no unit
const float timeout = 5.f;
const float jump_height = 1.2f;
const float fov = 90.f;
```

Numeric type can be specified via `time_t<T>`, `length_t<T>`, etc. when `float` is not appropriate (e.g. `time_t<std::uint32_t>`).

Unit types are layout-compatible with their underlying arithmetic type and pass through math and GPU push constants directly. `.as<Unit>()` is for converting between units (e.g. `time.as<milliseconds>()` from ns-stored time); identity strip via `.as<DefaultUnit>()` is a compile error. When you genuinely need the raw scalar at a foreign-API boundary, write `static_cast<value_type>(q)`.

---

## Deducing `this`

Use explicit object parameters (deducing `this`) to collapse const/non-const overload pairs into a single function:

```cpp
// correct — one function handles both const and non-const
template <typename Self>
auto networked_data(this Self& self) -> decltype(auto);

// wrong — two identical functions differing only in constness
auto networked_data() -> network_data_t&;
auto networked_data() const -> const network_data_t&;
```

Virtual functions cannot use deducing `this` — keep those as regular const/non-const overloads if needed.

---

## Mutation and `mutable`

Do not use `mutable` to work around `const` on system state or engine objects. If something needs to be mutated, use the engine's deferred mutation mechanism (`defer`) to schedule the write at the correct point in the frame. `mutable` hides the mutation from the type system and bypasses the engine's ownership and scheduling guarantees.

```cpp
// correct — schedule the write through the engine
defer([value](my_system_state& s) {
    s.some_field = value;
});

// wrong — punches a hole in const to sneak in a write
mutable int m_some_field = 0;  // in a system state accessed via const&
```

Enforced by `clang-tidy` (`gse-no-mutable`).

---

## `defer` — State Deduction

`defer` deduces the system state type from the lambda's first parameter. Do not pass it as an explicit template argument:

```cpp
// correct
defer([handle](state& s) {
    s.resume(handle);
});

// wrong
defer<state>([handle](state& s) {
    s.resume(handle);
});
```

---

## Channel Pushes

Pass the message type to `channels.push` as an explicit template argument; do not let it deduce from the argument. The type is the identity of the event — keep it visible at the call site:

```cpp
// correct — reads as "publish a play_request"
ctx.channels.push<play_request>({
    .clip = clip,
    .loop = true,
});

// wrong — the event type hides inside a constructor expression
ctx.channels.push(play_request{ .clip = clip, .loop = true });
ctx.channels.push(std::move(req));
```

For an existing variable, write `channels.push<decltype(req)>(std::move(req))`, or rebuild a fresh aggregate at the push site.

---

## Formatters

Math and engine types ship `std::formatter` specializations — `vec`, `quat`, `mat`, `rect_t`, `quantity` (so every unit type), and `id`. Enums are formattable too, through a reflection-based global formatter in `gse.meta`. Pass the whole value with a single format spec; do not decompose into components or hand-write a switch-based label helper:

```cpp
// correct — the element spec forwards to each component's quantity formatter
gse::log::println("pos={:.2f}", drum_local);   // pos=(2.50 m, 3.20 m, 6.10 m)

// wrong — decomposing what the formatter already handles
gse::log::println("pos=({:.2f}, {:.2f}, {:.2f})", drum_local.x(), drum_local.y(), drum_local.z());
```

Inline per-component labels (`x`/`y`/`z`) are the only reason to decompose — the exception, not the default.

---

## Returning Contiguous Sequences

When a getter exposes a contiguous sequence (vector, array, etc.) that callers only need to iterate or index, return `std::span<const T>` rather than `const std::vector<T>&` (or any other concrete container reference). The span gives callers the same iteration / indexing surface, decouples them from the storage choice, and lets the implementation switch between `std::vector`, `std::array`, a fixed buffer, etc. without breaking the public API.

```cpp
// correct
[[nodiscard]] auto formats() const -> std::span<const vk::SurfaceFormatKHR>;

// wrong — locks the API to a specific container
[[nodiscard]] auto formats() const -> const std::vector<vk::SurfaceFormatKHR>&;
```

Use a concrete container reference only when the caller genuinely needs container-specific operations (e.g. `.reserve()`, `.emplace_back()`, `.size()` returning the container's exact size_type) — and consider whether the API should expose those at all. If the caller just iterates or indexes, return a span.

---

## Ranges and Views

Prefer `std::ranges` algorithms over raw loops where applicable. Use `std::views` (e.g. `std::views::keys`, `std::views::values`) when iterating over only the keys or values of a map instead of iterating the full pair:

```cpp
// correct
for (const auto& key : std::views::keys(my_map)) { ... }
for (const auto& val : std::views::values(my_map)) { ... }

// wrong
for (const auto& [key, val] : my_map) {
    (void)val;
    use(key);
}
```

Prefer `std::ranges::` algorithm versions over `std::` versions:

```cpp
// correct
std::ranges::find(container, value);
std::ranges::sort(container);

// wrong
std::find(container.begin(), container.end(), value);
std::sort(container.begin(), container.end());
```

---

## Structured Bindings

Use structured bindings whenever decomposing a pair, tuple, or aggregate — especially in range-for loops over maps:

```cpp
// correct
for (const auto& [name, state] : states) { ... }
auto [it, inserted] = my_map.emplace(key, value);

// wrong
for (const auto& pair : states) {
    use(pair.first, pair.second);
}
auto result = my_map.emplace(key, value);
use(result.second);
```

---

## Template Definitions

Template function definitions go outside the namespace, same as non-templates:

```cpp
// declaration (inside namespace)
template <fixed_string Tag>
auto add(key default_key) -> handle;

// definition (outside namespace)
template <gse::foo::fixed_string Tag>
auto gse::foo::add(const key default_key) -> handle {
    ...
}
```
