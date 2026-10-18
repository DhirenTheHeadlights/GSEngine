export module gse.graphics:widget_skin;

import std;

import gse.math;

import :text_buffer;

export namespace gse {
	struct scaled_tag {};
	constexpr scaled_tag scaled{};

	struct style_group_tag {};
	constexpr style_group_tag style_group{};
}

export namespace gse::gui::layout {
	enum class halign {
		start,
		center,
		end,
	};

	enum class valign {
		top,
		center,
		bottom,
	};
}

export namespace gse::gui {
	enum class text_case : std::uint8_t {
		as_written,
		upper,
		lower,
	};

	struct [[= style_group]]
	surface {
		vec4f fill{};
		vec4f stroke{};

		[[= scaled]]
		float stroke_width = 0.f;

		[[= scaled]]
		vec4f radii{};
	};

	struct [[= style_group]]
	label_spec {
		text_face face = text_face::text;
		text_case transform = text_case::as_written;
		layout::halign h = layout::halign::center;
		float size_mult = 1.f;
	};

	template <typename T>
	struct [[= style_group]]
	state_set {
		T idle{};
		T hot{};
		T active{};
		T disabled{};
		T selected{};
	};

	[[nodiscard]] constexpr auto uniform_radii(
		float radius
	) -> vec4f;
}

constexpr auto gse::gui::uniform_radii(const float radius) -> vec4f {
	return vec4f(radius);
}
