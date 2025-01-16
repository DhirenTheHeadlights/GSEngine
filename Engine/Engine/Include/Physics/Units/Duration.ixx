export module gse.physics.units.duration;

import gse.physics.units.quantity;

export namespace gse::units {
	struct time_tag {};

	constexpr char milliseconds_units[] = "ms";
	constexpr char seconds_units[] = "s";
	constexpr char minutes_units[] = "m";
	constexpr char hours_units[] = "h";

	using milliseconds = unit<time_tag, 0.001f, milliseconds_units>;
	using seconds = unit<time_tag, 1.0f, seconds_units>;
	using minutes = unit<time_tag, 60.0f, minutes_units>;
	using hours = unit<time_tag, 3600.0f, hours_units>;

	using time_units = unit_list<
		units::milliseconds,
		units::seconds,
		units::minutes,
		units::hours
	>;
}

export namespace gse {
	struct time : quantity<time, units::seconds, units::time_units> {
		using quantity::quantity;
	};

	time milliseconds(const float value) {
		return time::from<units::milliseconds>(value);
	}

	time seconds(const float value) {
		return time::from<units::seconds>(value);
	}

	time minutes(const float value) {
		return time::from<units::minutes>(value);
	}

	time hours(const float value) {
		return time::from<units::hours>(value);
	}
}