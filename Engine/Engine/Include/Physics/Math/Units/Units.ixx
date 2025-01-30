export module gse.physics.math.units;

export import gse.physics.math.units.angle;
export import gse.physics.math.units.duration;
export import gse.physics.math.units.energy_and_power;
export import gse.physics.math.units.length;
export import gse.physics.math.units.mass_and_force;
export import gse.physics.math.units.movement;

export namespace gse {
	template <typename T>
		requires is_quantity<T>
	auto abs(const T& value) -> T {
		return value.template as<T::default_unit>() < 0 ? -value : value;
	}
}