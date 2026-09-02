export module gse.meta:actions_anno;

import std;

import :annotations;
import :fixed_string;
import :settings_anno;

export namespace gse::actions {
	enum struct binding_source : std::uint8_t {
		key,
		mouse_button
	};

	enum struct axis_source : std::uint8_t {
		digital,
		mouse_delta,
		scroll
	};

	template <fixed_string Label, auto Key, auto... Mods>
	struct bind {
		static constexpr std::string_view label = Label;
		static constexpr int code = static_cast<int>(Key);
		static constexpr std::uint8_t mod_bits = (std::uint8_t{ 0 } | ... | static_cast<std::uint8_t>(Mods));
	};

	template <fixed_string Label, auto Button, auto... Mods>
	struct mouse_bind {
		static constexpr std::string_view label = Label;
		static constexpr int code = static_cast<int>(Button);
		static constexpr std::uint8_t mod_bits = (std::uint8_t{ 0 } | ... | static_cast<std::uint8_t>(Mods));
	};

	template <fixed_string Label, fixed_string Left, fixed_string Right, fixed_string Back, fixed_string Fwd, float Scale = 1.f>
	struct axis2 {
		static constexpr std::string_view label = Label;
		static constexpr std::string_view left = Left;
		static constexpr std::string_view right = Right;
		static constexpr std::string_view back = Back;
		static constexpr std::string_view fwd = Fwd;
		static constexpr float scale = Scale;
	};

	template <fixed_string Label, float ScaleX = 1.f, float ScaleY = 1.f>
	struct axis2_mouse {
		static constexpr std::string_view label = Label;
		static constexpr float scale_x = ScaleX;
		static constexpr float scale_y = ScaleY;
	};

	template <fixed_string Label, fixed_string Neg, fixed_string Pos, float Scale = 1.f>
	struct axis1 {
		static constexpr std::string_view label = Label;
		static constexpr std::string_view neg = Neg;
		static constexpr std::string_view pos = Pos;
		static constexpr float scale = Scale;
	};

	template <fixed_string Label, float Scale = 1.f>
	struct axis1_scroll {
		static constexpr std::string_view label = Label;
		static constexpr float scale = Scale;
	};

	template <fixed_string V>
	struct group {
		static constexpr std::string_view value = V;
	};

	struct hidden {};

	struct set {};

	struct binding_spec {
		binding_source kind = binding_source::key;
		int code = -1;
		std::uint8_t mods = 0;
	};

	struct registration {
		std::string key;
		std::string label;
		std::string group;
		std::vector<binding_spec> bindings;
		bool hidden = false;
		void* handle_ptr = nullptr;
	};

	struct axis_registration {
		std::string key;
		std::string label;
		std::string group;
		axis_source source = axis_source::digital;
		std::uint8_t dimensions = 2;
		std::string neg;
		std::string pos;
		std::string left;
		std::string right;
		std::string back;
		std::string fwd;
		float scale = 1.f;
		float scale_y = 1.f;
		bool hidden = false;
		void* axis_id_ptr = nullptr;
	};
}

export namespace gse::meta {
	consteval auto find_bind(
		std::meta::info m
	) -> std::meta::info;

	consteval auto find_mouse_bind(
		std::meta::info m
	) -> std::meta::info;

	consteval auto find_axis2(
		std::meta::info m
	) -> std::meta::info;

	consteval auto find_axis2_mouse(
		std::meta::info m
	) -> std::meta::info;

	consteval auto find_axis1(
		std::meta::info m
	) -> std::meta::info;

	consteval auto find_axis1_scroll(
		std::meta::info m
	) -> std::meta::info;

	consteval auto find_action_group(
		std::meta::info m
	) -> std::meta::info;

	consteval auto is_action_member(
		std::meta::info m
	) -> bool;
}

consteval auto gse::meta::find_bind(const std::meta::info m) -> std::meta::info {
	return find_class_template_annotation(m, ^^actions::bind);
}

consteval auto gse::meta::find_mouse_bind(const std::meta::info m) -> std::meta::info {
	return find_class_template_annotation(m, ^^actions::mouse_bind);
}

consteval auto gse::meta::find_axis2(const std::meta::info m) -> std::meta::info {
	return find_class_template_annotation(m, ^^actions::axis2);
}

consteval auto gse::meta::find_axis2_mouse(const std::meta::info m) -> std::meta::info {
	return find_class_template_annotation(m, ^^actions::axis2_mouse);
}

consteval auto gse::meta::find_axis1(const std::meta::info m) -> std::meta::info {
	return find_class_template_annotation(m, ^^actions::axis1);
}

consteval auto gse::meta::find_axis1_scroll(const std::meta::info m) -> std::meta::info {
	return find_class_template_annotation(m, ^^actions::axis1_scroll);
}

consteval auto gse::meta::find_action_group(const std::meta::info m) -> std::meta::info {
	return find_class_template_annotation(m, ^^actions::group);
}

consteval auto gse::meta::is_action_member(const std::meta::info m) -> bool {
	return find_bind(m) != std::meta::info{} || find_mouse_bind(m) != std::meta::info{};
}
