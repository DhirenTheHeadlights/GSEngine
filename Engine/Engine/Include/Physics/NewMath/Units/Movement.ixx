//export module gse.physics.math.units.move;
//
//import std;
//
//import gse.physics.math.units.quant;
//import gse.physics.math.unit_vec;
//
//namespace gse::units {
//	struct velocity_tag {};
//
//	constexpr char meters_per_second_units[] = "m/s";
//	constexpr char kilometers_per_hour_units[] = "km/h";
//	constexpr char miles_per_hour_units[] = "mph";
//
//	export using meters_per_second = internal::unit<velocity_tag, 1.0f, meters_per_second_units>;
//	export using kilometers_per_hour = internal::unit<velocity_tag, 0.277777778f, kilometers_per_hour_units>;
//	export using miles_per_hour = internal::unit<velocity_tag, 0.44704f, miles_per_hour_units>;
//
//	using velocity_units = internal::unit_list <
//		meters_per_second,
//		kilometers_per_hour,
//		miles_per_hour
//	>;
//}
//
//export namespace gse {
//	template <typename T = float> using velocity_t = internal::quantity<T, units::velocity_tag, units::meters_per_second, units::velocity_units>;
//	
//	using velocity = velocity_t<>;
//	
//	template <typename T> constexpr auto meters_per_second_t(T value) -> velocity_t<T>;
//	template <typename T> constexpr auto kilometers_per_hour_t(T value) -> velocity_t<T>;
//	template <typename T> constexpr auto miles_per_hour_t(T value) -> velocity_t<T>;
//	
//	constexpr auto meters_per_second(float value) -> velocity;
//	constexpr auto kilometers_per_hour(float value) -> velocity;
//	constexpr auto miles_per_hour(float value) -> velocity;
//}
//
//export namespace gse::vec {
//	template <internal::is_unit U, typename... Args>
//	constexpr auto velocity(Args&&... args) -> vec_t<velocity_t<std::common_type_t<Args...>>, sizeof...(Args)>;
//
//	template <typename... Args> constexpr auto meters_per_second(Args&&... args) -> vec_t<velocity_t<std::common_type_t<Args...>>, sizeof...(Args)>;
//	template <typename... Args> constexpr auto kilometers_per_hour(Args&&... args) -> vec_t<velocity_t<std::common_type_t<Args...>>, sizeof...(Args)>;
//	template <typename... Args> constexpr auto miles_per_hour(Args&&... args) -> vec_t<velocity_t<std::common_type_t<Args...>>, sizeof...(Args)>;
//}
//
//template <typename T>
//constexpr auto gse::meters_per_second_t(const T value) -> velocity_t<T> {
//	return velocity_t<T>::template from<units::meters_per_second>(value);
//}
//
//template <typename T>
//constexpr auto gse::kilometers_per_hour_t(const T value) -> velocity_t<T> {
//	return velocity_t<T>::template from<units::kilometers_per_hour>(value);
//}
//
//template <typename T>
//constexpr auto gse::miles_per_hour_t(const T value) -> velocity_t<T> {
//	return velocity_t<T>::template from<units::miles_per_hour>(value);
//}
//
//constexpr auto gse::meters_per_second(const float value) -> velocity {
//	return meters_per_second_t<float>(value);
//}
//
//constexpr auto gse::kilometers_per_hour(const float value) -> velocity {
//	return kilometers_per_hour_t<float>(value);
//}
//
//constexpr auto gse::miles_per_hour(const float value) -> velocity {
//	return miles_per_hour_t<float>(value);
//}
//
//template <gse::internal::is_unit U, typename... Args>
//constexpr auto gse::vec::velocity(Args&&... args) -> vec_t<velocity_t<std::common_type_t<Args...>>, sizeof...(Args)> {
//	return { velocity_t<std::common_type_t<Args...>>::template from<U>(args)... };
//}
//
//template <typename... Args>
//constexpr auto gse::vec::meters_per_second(Args&&... args) -> vec_t<velocity_t<std::common_type_t<Args...>>, sizeof...(Args)> {
//	return { velocity_t<std::common_type_t<Args...>>::template from<units::meters_per_second>(args)... };
//}
//
//template <typename... Args>
//constexpr auto gse::vec::kilometers_per_hour(Args&&... args) -> vec_t<velocity_t<std::common_type_t<Args...>>, sizeof...(Args)> {
//	return { velocity_t<std::common_type_t<Args...>>::template from<units::kilometers_per_hour>(args)... };
//}
//
//template <typename... Args>
//constexpr auto gse::vec::miles_per_hour(Args&&... args) -> vec_t<velocity_t<std::common_type_t<Args...>>, sizeof...(Args)> {
//	return { velocity_t<std::common_type_t<Args...>>::template from<units::miles_per_hour>(args)... };
//}
//
//namespace gse::units {
//	struct acceleration_tag {};
//	
//	constexpr char meters_per_second_squared_units[] = "m/s^2";
//	constexpr char kilometers_per_hour_squared_units[] = "km/h^2";
//	constexpr char miles_per_hour_squared_units[] = "mph^2";
//	
//	export using meters_per_second_squared = internal::unit<acceleration_tag, 1.0f, meters_per_second_squared_units>;
//	export using kilometers_per_hour_squared = internal::unit<acceleration_tag, 0.0000771604938272f, kilometers_per_hour_squared_units>;
//	export using miles_per_hour_squared = internal::unit<acceleration_tag, 0.000124223602484f, miles_per_hour_squared_units>;
//	
//	using acceleration_units = internal::unit_list <
//		meters_per_second_squared,
//		kilometers_per_hour_squared,
//		miles_per_hour_squared
//	>;
//}
//
//export namespace gse {
//	template <typename T = float> using acceleration_t = internal::quantity<T, units::acceleration_tag, units::meters_per_second_squared, units::acceleration_units>;
//
//	using acceleration = acceleration_t<>;
//
//	template <typename T> constexpr auto meters_per_second_squared_t(T value) -> acceleration_t<T>;
//	template <typename T> constexpr auto kilometers_per_hour_squared_t(T value) -> acceleration_t<T>;
//	template <typename T> constexpr auto miles_per_hour_squared_t(T value) -> acceleration_t<T>;
//
//	constexpr auto meters_per_second_squared(float value) -> acceleration;
//	constexpr auto kilometers_per_hour_squared(float value) -> acceleration;
//	constexpr auto miles_per_hour_squared(float value) -> acceleration;
//}
//
//export namespace gse::vec {
//	template <internal::is_unit U, typename... Args>
//	constexpr auto acceleration(Args&&... args) -> vec_t<acceleration_t<std::common_type_t<Args...>>, sizeof...(Args)>;
//	
//	template <typename... Args> constexpr auto meters_per_second_squared(Args&&... args) -> vec_t<acceleration_t<std::common_type_t<Args...>>, sizeof...(Args)>;
//	template <typename... Args> constexpr auto kilometers_per_hour_squared(Args&&... args) -> vec_t<acceleration_t<std::common_type_t<Args...>>, sizeof...(Args)>;
//	template <typename... Args> constexpr auto miles_per_hour_squared(Args&&... args) -> vec_t<acceleration_t<std::common_type_t<Args...>>, sizeof...(Args)>;
//}
//
//template <typename T>
//constexpr auto gse::meters_per_second_squared_t(const T value) -> acceleration_t<T> {
//	return acceleration_t<T>::template from<units::meters_per_second_squared>(value);
//}
//
//template <typename T>
//constexpr auto gse::kilometers_per_hour_squared_t(const T value) -> acceleration_t<T> {
//	return acceleration_t<T>::template from<units::kilometers_per_hour_squared>(value);
//}
//
//template <typename T>
//constexpr auto gse::miles_per_hour_squared_t(const T value) -> acceleration_t<T> {
//	return acceleration_t<T>::template from<units::miles_per_hour_squared>(value);
//}
//
//constexpr auto gse::meters_per_second_squared(const float value) -> acceleration {
//	return meters_per_second_squared_t<float>(value);
//}
//
//constexpr auto gse::kilometers_per_hour_squared(const float value) -> acceleration {
//	return kilometers_per_hour_squared_t<float>(value);
//}
//
//constexpr auto gse::miles_per_hour_squared(const float value) -> acceleration {
//	return miles_per_hour_squared_t<float>(value);
//}
//
//template <gse::internal::is_unit U, typename... Args>
//constexpr auto gse::vec::acceleration(Args&&... args) -> vec_t<acceleration_t<std::common_type_t<Args...>>, sizeof...(Args)> {
//	return { acceleration_t<std::common_type_t<Args...>>::template from<U>(args)... };
//}
//
//template <typename... Args>
//constexpr auto gse::vec::meters_per_second_squared(Args&&... args) -> vec_t<acceleration_t<std::common_type_t<Args...>>, sizeof...(Args)> {
//	return { acceleration_t<std::common_type_t<Args...>>::template from<units::meters_per_second_squared>(args)... };
//}
//
//template <typename... Args>
//constexpr auto gse::vec::kilometers_per_hour_squared(Args&&... args) -> vec_t<acceleration_t<std::common_type_t<Args...>>, sizeof...(Args)> {
//	return { acceleration_t<std::common_type_t<Args...>>::template from<units::kilometers_per_hour_squared>(args)... };
//}
//
//template <typename... Args>
//constexpr auto gse::vec::miles_per_hour_squared(Args&&... args) -> vec_t<acceleration_t<std::common_type_t<Args...>>, sizeof...(Args)> {
//	return { acceleration_t<std::common_type_t<Args...>>::template from<units::miles_per_hour_squared>(args)... };
//}
