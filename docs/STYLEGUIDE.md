# GoonSquad Style Guide

## Naming
- STL style (snake_case for everything)
- Private member variables prefixed with `m_`

## Comments
Do not add comments. Code should be self-documenting.

---

## Function Declarations and Definitions

**Declarations** go inside the namespace. Each parameter on its own line:

```cpp
export namespace gse::foo {
    auto bar(
        const type& param1,
        type param2
    ) -> return_type;
}
```

**Definitions** go outside the namespace. All parameters on a single line:

```cpp
auto gse::foo::bar(const type& param1, type param2) -> return_type {
    ...
}
```

Zero-parameter functions follow the same split — `()` across two lines for declarations, collapsed for definitions:

```cpp
// declaration (inside namespace)
auto baz(
) -> return_type;

// definition (outside namespace)
auto gse::foo::baz() -> return_type {
    ...
}
```

Default argument values belong only on declarations, never on definitions.

When a constructor has an initializer list, keep it on the same line as the signature with `{` following immediately. If the body is empty, collapse to `{}`:

```cpp
// empty body — all on one line
my_type::my_type(const foo& f) : m_foo(f) {}

// non-empty body — init list and { on same line, body below
my_type::my_type(const foo& f, const bar& b) : m_foo(f), m_bar(b) {
    do_something();
}
```

The per-line parameter rule applies to `= delete` and `= default` declarations as well:

```cpp
// correct
linear_vector(
    const linear_vector&
) = delete;

// wrong
linear_vector(const linear_vector&) = delete;
```

Prefer `non_copyable` / `non_movable` base classes over writing deleted copy/move declarations by hand. Always declare a destructor in the derived class to suppress virtual destructor warnings from the base:

```cpp
// correct
class my_type : public non_copyable {
public:
    ~my_type();
    // custom move ops if needed
};

// wrong
class my_type {
public:
    my_type(const my_type&) = delete;
    auto operator=(const my_type&) -> my_type& = delete;
};
```

---

## Scoped Statements

Always use braces. Always put the body on its own line. No single-line `if`, `for`, `while`, `do`, or lambda bodies:

```cpp
// correct
if (condition) {
    do_something();
}

// wrong
if (condition) do_something();
if (condition)
    do_something();
```

This applies to lambdas as well — no one-liner lambda bodies:

```cpp
// correct
defer<state>([handle](state& s) {
    s.stop(handle);
});

// wrong
defer<state>([handle](state& s) { s.stop(handle); });
```

Long `if` conditions are not wrapped — keep them on one line even if they are long.

---

## File Organization

Each file should have one `export namespace` block containing all declarations, followed by all definitions outside it. Never reopen or add a second `export namespace` block to interleave declarations and definitions.

When a file contains multiple sizeable class definitions, give each sizeable class its own file. Small related types (e.g. a group of phase structs, or a handful of POD structs) can share a file. A class is "sizeable" if its definition section would be long enough to make you scroll past other class declarations to reach it.

---

## Alignment

Do not pad with spaces to align `=` signs, type names, or anything else vertically. Each line stands alone:

```cpp
// correct
using value_type = T;
using size_type = std::size_t;
using reference = T&;

T* m_data = nullptr;
size_type m_size = 0;
size_type m_capacity = 0;

// wrong
using value_type      = T;
using size_type       = std::size_t;
using reference       = T&;

T*        m_data     = nullptr;
size_type m_size     = 0;
size_type m_capacity = 0;
```

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

---

## Aggregate Initialization

When the type is already known from the assignment target, omit the explicit type name from the brace initializer:

```cpp
// correct — map value type is already known
ctrl.parameters[key] = { .value = value, .is_trigger = false };

// wrong — animation_parameter is redundant
ctrl.parameters[key] = animation_parameter{ .value = value, .is_trigger = false };
```

For function call arguments where the type is needed for template deduction, keep the explicit type.

Always put each designated initializer on its own line:

```cpp
// correct
ctrl.parameters[key] = {
    .value = value,
    .is_trigger = false,
};

// wrong
ctrl.parameters[key] = { .value = value, .is_trigger = false };
```

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

To extract a value in a specific unit, pass the unit constant directly as an NTTP:

```cpp
const float ms = t.as<milliseconds>();
const float deg = angle.as<degrees>();
```

---

## Deducing `this`

Use explicit object parameters (deducing `this`) to collapse const/non-const overload pairs into a single function:

```cpp
// correct — one function handles both const and non-const
template <typename Self>
auto networked_data(
    this Self& self
) -> decltype(auto);

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

Template function definitions also go outside the namespace:

```cpp
// declaration (inside namespace)
template <fixed_string Tag>
auto add(
    key default_key
) -> handle;

// definition (outside namespace)
template <gse::foo::fixed_string Tag>
auto gse::foo::add(const key default_key) -> handle {
    ...
}
```
