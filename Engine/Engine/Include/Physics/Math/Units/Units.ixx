export module gse.physics.math.units;

export import gse.physics.math.units.angle;
export import gse.physics.math.units.duration;
export import gse.physics.math.units.energy_and_power;
export import gse.physics.math.units.length;
export import gse.physics.math.units.mass_and_force;
export import gse.physics.math.units.movement;
export import gse.physics.math.units.quant;

export namespace gse {
	template <typename T> requires internal::is_quantity<T> auto abs(const T& value) -> T;
	template <typename T> requires internal::is_quantity<T> auto min(const T& a, const T& b) -> T;
	template <typename T> requires internal::is_quantity<T> auto max(const T& a, const T& b) -> T;
}

template <typename T> requires gse::internal::is_quantity<T>
auto gse::abs(const T& value) -> T {
	return value >= T() ? value : -value;
}

template <typename T> requires gse::internal::is_quantity<T>
auto gse::min(const T& a, const T& b) -> T {
	return a < b ? a : b;
}

template <typename T> requires gse::internal::is_quantity<T>
auto gse::max(const T& a, const T& b) -> T {
	return a > b ? a : b;
}