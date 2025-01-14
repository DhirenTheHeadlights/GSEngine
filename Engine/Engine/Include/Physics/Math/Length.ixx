export module Length;

//import vec;

//namespace gse {
//    namespace gse::units {
//        struct length_tag {};
//
//        constexpr char kilometers_units[] = "km";
//        constexpr char meters_units[] = "m";
//        constexpr char centimeters_units[] = "cm";
//        constexpr char millimeters_units[] = "mm";
//        constexpr char yards_units[] = "yd";
//        constexpr char feet_units[] = "ft";
//        constexpr char inches_units[] = "in";
//
//        export using kilometers = internal::unit<length_tag, 1000.0f, kilometers_units>;
//        export using meters = internal::unit<length_tag, 1.0f, meters_units>;
//        export using centimeters = internal::unit<length_tag, 0.01f, centimeters_units>;
//        export using millimeters = internal::unit<length_tag, 0.001f, millimeters_units>;
//        export using yards = internal::unit<length_tag, 0.9144f, yards_units>;
//        export using feet = internal::unit<length_tag, 0.3048f, feet_units>;
//        export using inches = internal::unit<length_tag, 0.0254f, inches_units>;
//
//        using length_units = internal::unit_list <
//            kilometers,
//            meters,
//            centimeters,
//            millimeters,
//            yards,
//            feet,
//            inches
//        >;
//    }
//
//    export namespace gse {
//        template <typename T = float>
//        using length_t = internal::quantity<T, units::length_tag, units::meters, units::length_units>;
//
//        using length = length_t<>;
//
//        template <typename T>
//        constexpr auto kilometers_t(T value) -> length_t<T>;
//
//        template <typename T>
//        constexpr auto meters_t(T value) -> length_t<T>;
//
//        template <typename T>
//        constexpr auto centimeters_t(T value) -> length_t<T>;
//
//        template <typename T>
//        constexpr auto millimeters_t(T value) -> length_t<T>;
//
//        template <typename T>
//        constexpr auto yards_t(T value) -> length_t<T>;
//
//        template <typename T>
//        constexpr auto feet_t(T value) -> length_t<T>;
//
//        template <typename T>
//        constexpr auto inches_t(T value) -> length_t<T>;
//
//        constexpr auto kilometers(float value) -> length;
//        constexpr auto meters(float value) -> length;
//        constexpr auto centimeters(float value) -> length;
//        constexpr auto millimeters(float value) -> length;
//        constexpr auto yards(float value) -> length;
//        constexpr auto feet(float value) -> length;
//        constexpr auto inches(float value) -> length;
//    }
//
//    namespace gse {
//        template <typename T>
//        constexpr auto kilometers_t(const T value) -> length_t<T> {
//            return length_t<T>::template from<units::kilometers>(value);
//        }
//
//        template <typename T>
//        constexpr auto meters_t(const T value) -> length_t<T> {
//            return length_t<T>::template from<units::meters>(value);
//        }
//
//        template <typename T>
//        constexpr auto centimeters_t(const T value) -> length_t<T> {
//            return length_t<T>::template from<units::centimeters>(value);
//        }
//
//        template <typename T>
//        constexpr auto millimeters_t(const T value) -> length_t<T> {
//            return length_t<T>::template from<units::millimeters>(value);
//        }
//
//        template <typename T>
//        constexpr auto yards_t(const T value) -> length_t<T> {
//            return length_t<T>::template from<units::yards>(value);
//        }
//
//        template <typename T>
//        constexpr auto feet_t(const T value) -> length_t<T> {
//            return length_t<T>::template from<units::feet>(value);
//        }
//
//        template <typename T>
//        constexpr auto inches_t(const T value) -> length_t<T> {
//            return length_t<T>::template from<units::inches>(value);
//        }
//
//        constexpr auto kilometers(const float value) -> length {
//            return kilometers_t<float>(value);
//        }
//
//        constexpr auto meters(const float value) -> length {
//            return meters_t<float>(value);
//        }
//
//        constexpr auto centimeters(const float value) -> length {
//            return centimeters_t<float>(value);
//        }
//
//        constexpr auto millimeters(const float value) -> length {
//            return millimeters_t<float>(value);
//        }
//
//        constexpr auto yards(const float value) -> length {
//            return yards_t<float>(value);
//        }
//
//        constexpr auto feet(const float value) -> length {
//            return feet_t<float>(value);
//        }
//
//        constexpr auto inches(const float value) -> length {
//            return inches_t<float>(value);
//        }
//
//        constexpr auto test() -> void {
//            constexpr auto l1 = kilometers(1.0f);
//            constexpr auto l2 = meters(1.0f);
//            constexpr auto l3 = centimeters(1.0f);
//            constexpr auto l4 = millimeters(1.0f);
//            constexpr auto l5 = yards(1.0f);
//            constexpr auto l6 = feet(1.0f);
//            constexpr auto l7 = inches(1.0f);
//
//            constexpr auto l8 = kilometers_t(1.0f);
//            constexpr auto l9 = meters_t(1.0f);
//            constexpr auto l10 = centimeters_t(1.0f);
//            constexpr auto l11 = millimeters_t(1.0f);
//            constexpr auto l12 = yards_t(1.0f);
//            constexpr auto l13 = feet_t(1.0f);
//            constexpr auto l14 = inches_t(1.0f);
//
//            constexpr auto l15 = l2;
//            constexpr auto l16 = l2 + l3;
//            constexpr auto l17 = l2 - l3;
//            constexpr auto l18 = l2 * 2.0f;
//        }
//    }
//
//    namespace gse {
//        template <typename T>
//        struct unit_to_quantity {
//            using type = T;
//        };
//
//        // Macro to define the relationship between a unit and a quantity
//        // Example: DEFINE_UNIT_TO_QUANTITY(Meters, Length);
//#define DEFINE_UNIT_TO_QUANTITY(unit_type, quantity_type)		    \
//	    template <>                                                 \
//	    struct unit_vec::unit_to_quantity<unit_type> {				\
//	        using type = quantity_type;                             \
//	    }; 													        \
//
//        DEFINE_UNIT_TO_QUANTITY(gse::units::kilometers, gse::length);
//        DEFINE_UNIT_TO_QUANTITY(gse::units::meters, gse::length);
//        DEFINE_UNIT_TO_QUANTITY(gse::units::centimeters, gse::length);
//        DEFINE_UNIT_TO_QUANTITY(gse::units::millimeters, gse::length);
//        DEFINE_UNIT_TO_QUANTITY(gse::units::yards, gse::length);
//        DEFINE_UNIT_TO_QUANTITY(gse::units::feet, gse::length);
//        DEFINE_UNIT_TO_QUANTITY(gse::units::inches, gse::length);
//    }
//
//    export namespace gse {
//        template <typename T>
//        constexpr auto meters(const T& x, const T& y, const T& z) -> vec3<length> {
//            return vec3<length, T>(x, y, z);
//        }
//
//        template <typename T>
//        constexpr auto meters(const T& x) -> vec3<length> {
//            return vec3<length, T>(x);
//        }
//
//        auto test2() -> void {
//            constexpr auto v1 = vec3<length>(meters(1.0f), feet(2.0f), centimeters(3.0f));
//            constexpr auto v2 = vec3<length>(meters(1.0f), meters(2.0f), meters(3.0f));
//            constexpr auto v3 = vec3<length>(meters(1.0f), meters(2.0f), meters(3.0f));
//
//            constexpr auto uv1 = meters(1.0f, 2.0f, 3.0f);
//			constexpr vec3<length> uv2 = meters(1.0f);
//
//
//            constexpr auto x = v1.x;
//
//            /*constexpr auto v4 = v1 + v2;
//            constexpr auto v5 = v1 - v2;
//            constexpr auto v6 = v1 * 2.0f;
//            constexpr auto v7 = v1 / 2.0f;*/
//
//            //constexpr auto v8 = v1.as<units::centimeters>();
//        }
//    }
//}