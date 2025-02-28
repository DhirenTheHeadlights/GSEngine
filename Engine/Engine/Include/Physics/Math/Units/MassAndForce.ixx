export module gse.physics.math.units.mass_and_force;

import std;

import gse.physics.math.units.quant;
import gse.physics.math.unit_vec;

namespace gse::units {
	struct mass_tag {};
	
	constexpr char kilograms_units[] = "kg";
	constexpr char grams_units[] = "g";
	constexpr char milligrams_units[] = "mg";
	constexpr char pounds_units[] = "lb";
	constexpr char ounces_units[] = "oz";
	
	export using kilograms = internal::unit<mass_tag, 1.0f, kilograms_units>;
	export using grams = internal::unit<mass_tag, 0.001f, grams_units>;
	export using milligrams = internal::unit<mass_tag, 0.000001f, milligrams_units>;
	export using pounds = internal::unit<mass_tag, 0.45359237f, pounds_units>;
	export using ounces = internal::unit<mass_tag, 0.0283495231f, ounces_units>;
	
	using mass_units = internal::unit_list <
		kilograms,
		grams,
		milligrams,
		pounds,
		ounces
	>;
}

export namespace gse {
	template <typename T = float>
	struct mass_t : internal::quantity<mass_t<T>, T, units::mass_tag, units::kilograms, units::mass_units> {
		using internal::quantity<mass_t, T, units::mass_tag, units::kilograms, units::mass_units>::quantity;
	};

	using mass = mass_t<>;
	
	template <typename T> constexpr auto kilograms_t(T value) -> mass_t<T>;
	template <typename T> constexpr auto grams_t(T value) -> mass_t<T>;
	template <typename T> constexpr auto milligrams_t(T value) -> mass_t<T>;
	template <typename T> constexpr auto pounds_t(T value) -> mass_t<T>;
	template <typename T> constexpr auto ounces_t(T value) -> mass_t<T>;
	
	constexpr auto kilograms(float value) -> mass;
	constexpr auto grams(float value) -> mass;
	constexpr auto milligrams(float value) -> mass;
	constexpr auto pounds(float value) -> mass;
	constexpr auto ounces(float value) -> mass;
}

export namespace gse::vec {
	template <internal::is_unit U, typename... Args>
	constexpr auto mass(Args&&... args) -> vec_t<mass_t<std::common_type_t<Args...>>, sizeof...(Args)>;

	template <typename... Args> constexpr auto kilograms(Args&&... args) -> vec_t<mass_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto grams(Args&&... args) -> vec_t<mass_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto milligrams(Args&&... args) -> vec_t<mass_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto pounds(Args&&... args) -> vec_t<mass_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto ounces(Args&&... args) -> vec_t<mass_t<std::common_type_t<Args...>>, sizeof...(Args)>;
}

template <typename T>
constexpr auto gse::kilograms_t(const T value) -> mass_t<T> {
	return mass_t<T>::template from<units::kilograms>(value);
}

template <typename T>
constexpr auto gse::grams_t(const T value) -> mass_t<T> {
	return mass_t<T>::template from<units::grams>(value);
}

template <typename T>
constexpr auto gse::milligrams_t(const T value) -> mass_t<T> {
	return mass_t<T>::template from<units::milligrams>(value);
}

template <typename T>
constexpr auto gse::pounds_t(const T value) -> mass_t<T> {
	return mass_t<T>::template from<units::pounds>(value);
}

template <typename T>
constexpr auto gse::ounces_t(const T value) -> mass_t<T> {
	return mass_t<T>::template from<units::ounces>(value);
}

constexpr auto gse::kilograms(const float value) -> mass {
	return kilograms_t<float>(value);
}

constexpr auto gse::grams(const float value) -> mass {
	return grams_t<float>(value);
}

constexpr auto gse::milligrams(const float value) -> mass {
	return milligrams_t<float>(value);
}

constexpr auto gse::pounds(const float value) -> mass {
	return pounds_t<float>(value);
}

constexpr auto gse::ounces(const float value) -> mass {
	return ounces_t<float>(value);
}

template <gse::internal::is_unit U, typename... Args>
constexpr auto gse::vec::mass(Args&&... args) -> vec_t<mass_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { mass_t<std::common_type_t<Args...>>::template from<U>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::kilograms(Args&&... args) -> vec_t<mass_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { mass_t<std::common_type_t<Args...>>::template from<units::kilograms>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::grams(Args&&... args) -> vec_t<mass_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { mass_t<std::common_type_t<Args...>>::template from<units::grams>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::milligrams(Args&&... args) -> vec_t<mass_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { mass_t<std::common_type_t<Args...>>::template from<units::milligrams>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::pounds(Args&&... args) -> vec_t<mass_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { mass_t<std::common_type_t<Args...>>::template from<units::pounds>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::ounces(Args&&... args) -> vec_t<mass_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { mass_t<std::common_type_t<Args...>>::template from<units::ounces>(args)... };
}

namespace gse::units {
	struct force_tag {};

	constexpr char newtons_units[] = "N";
	constexpr char pounds_force_units[] = "lbf";

	export using newtons = internal::unit<force_tag, 1.0f, newtons_units>;
	export using pounds_force = internal::unit<force_tag, 4.44822f, pounds_force_units>;

	using force_units = internal::unit_list <
		newtons,
		pounds_force
	>;

	struct torque_tag {};

	constexpr char newton_meters_units[] = "N-m";
	constexpr char pound_feet_units[] = "lbf-ft";

	export using newton_meters = internal::unit<torque_tag, 1.0f, newton_meters_units>;
	export using pound_feet = internal::unit<torque_tag, 1.35582f, pound_feet_units>;

	using torque_units = internal::unit_list <
		newton_meters,
		pound_feet
	>;
}

export namespace gse {
	template <typename T = float>
	struct force_t : internal::quantity<force_t<T>, T, units::force_tag, units::newtons, units::force_units> {
		using internal::quantity<force_t, T, units::force_tag, units::newtons, units::force_units>::quantity;
	};
	
	using force = force_t<>;
	
	template <typename T> constexpr auto newtons_t(T value) -> force_t<T>;
	template <typename T> constexpr auto pounds_force_t(T value) -> force_t<T>;
	
	constexpr auto newtons(float value) -> force;
	constexpr auto pounds_force(float value) -> force;

	template <typename T = float>
	struct torque_t : internal::quantity<torque_t<T>, T, units::torque_tag, units::newton_meters, units::torque_units> {
		using internal::quantity<torque_t, T, units::torque_tag, units::newton_meters, units::torque_units>::quantity;
	};

	using torque = torque_t<>;

	template <typename T> constexpr auto newton_meters_t(T value) -> torque_t<T>;
	template <typename T> constexpr auto pound_feet_t(T value) -> torque_t<T>;

	constexpr auto newton_meters(float value) -> torque;
	constexpr auto pound_feet(float value) -> torque;
}

export namespace gse::vec {
	template <internal::is_unit U, typename... Args>
	constexpr auto force(Args&&... args) -> vec_t<force_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	
	template <typename... Args> constexpr auto newtons(Args&&... args) -> vec_t<force_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto pounds_force(Args&&... args) -> vec_t<force_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	
	template <internal::is_unit U, typename... Args>
	constexpr auto torque(Args&&... args) -> vec_t<torque_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	
	template <typename... Args> constexpr auto newton_meters(Args&&... args) -> vec_t<torque_t<std::common_type_t<Args...>>, sizeof...(Args)>;
	template <typename... Args> constexpr auto pound_feet(Args&&... args) -> vec_t<torque_t<std::common_type_t<Args...>>, sizeof...(Args)>;
}

template <typename T>
constexpr auto gse::newtons_t(const T value) -> force_t<T> {
	return force_t<T>::template from<units::newtons>(value);
}

template <typename T>
constexpr auto gse::pounds_force_t(const T value) -> force_t<T> {
	return force_t<T>::template from<units::pounds_force>(value);
}

constexpr auto gse::newtons(const float value) -> force {
	return newtons_t<float>(value);
}

constexpr auto gse::pounds_force(const float value) -> force {
	return pounds_force_t<float>(value);
}

template <typename T>
constexpr auto gse::newton_meters_t(const T value) -> torque_t<T> {
	return torque_t<T>::template from<units::newton_meters>(value);
}

template <typename T>
constexpr auto gse::pound_feet_t(const T value) -> torque_t<T> {
	return torque_t<T>::template from<units::pound_feet>(value);
}

constexpr auto gse::newton_meters(const float value) -> torque {
	return newton_meters_t<float>(value);
}

constexpr auto gse::pound_feet(const float value) -> torque {
	return pound_feet_t<float>(value);
}

template <gse::internal::is_unit U, typename... Args>
constexpr auto gse::vec::force(Args&&... args) -> vec_t<force_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { force_t<std::common_type_t<Args...>>::template from<U>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::newtons(Args&&... args) -> vec_t<force_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { force_t<std::common_type_t<Args...>>::template from<units::newtons>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::pounds_force(Args&&... args) -> vec_t<force_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { force_t<std::common_type_t<Args...>>::template from<units::pounds_force>(args)... };
}

template <gse::internal::is_unit U, typename... Args>
constexpr auto gse::vec::torque(Args&&... args) -> vec_t<torque_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { torque_t<std::common_type_t<Args...>>::template from<U>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::newton_meters(Args&&... args) -> vec_t<torque_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { torque_t<std::common_type_t<Args...>>::template from<units::newton_meters>(args)... };
}

template <typename... Args>
constexpr auto gse::vec::pound_feet(Args&&... args) -> vec_t<torque_t<std::common_type_t<Args...>>, sizeof...(Args)> {
	return { torque_t<std::common_type_t<Args...>>::template from<units::pound_feet>(args)... };
}

