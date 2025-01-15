export module gse.physics.units.length;

import gse.physics.units.quantity;

export namespace gse::units {
	struct length_tag {};

	constexpr char kilometers_units[] = "km";
	constexpr char meters_units[] = "m";
	constexpr char centimeters_units[] = "cm";
	constexpr char millimeters_units[] = "mm";
	constexpr char yards_units[] = "yd";
	constexpr char feet_units[] = "ft";
	constexpr char inches_units[] = "in";

	using kilometers = unit<length_tag, 1000.0f, kilometers_units>;
	using meters = unit<length_tag, 1.0f, meters_units>;
	using centimeters = unit<length_tag, 0.01f, centimeters_units>;
	using millimeters = unit<length_tag, 0.001f, millimeters_units>;
	using yards = unit<length_tag, 0.9144f, yards_units>;
	using feet = unit<length_tag, 0.3048f, feet_units>;
	using inches = unit<length_tag, 0.0254f, inches_units>;

	using length_units = unit_list<
		units::kilometers,
		units::meters,
		units::centimeters,
		units::millimeters,
		units::yards,
		units::feet,
		units::inches
	>;
}

export namespace gse {
	struct length : quantity<length, units::meters, units::length_units> {
		using quantity::quantity;
	};

	auto kilometers(const float value) -> length {
		return length::from<units::kilometers>(value);
	}

	auto meters(const float value) -> length {
		return length::from<units::meters>(value);
	}

	auto centimeters(const float value) -> length {
		return length::from<units::centimeters>(value);
	}

	auto millimeters(const float value) -> length {
		return length::from<units::millimeters>(value);
	}

	auto yards(const float value) -> length {
		return length::from<units::yards>(value);
	}

	auto feet(const float value) -> length {
		return length::from<units::feet>(value);
	}

	auto inches(const float value) -> length {
		return length::from<units::inches>(value);
	}
}
