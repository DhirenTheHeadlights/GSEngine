export module gse.ide.agent:canvas;

import std;
import gse;

export namespace gse::ide::agent::drawables {
	struct rect {
		vec2f position;
		vec2f size = { 32.f, 32.f };
		vec4f color = { 1.f, 1.f, 1.f, 1.f };
		float radius = 0.f;
	};

	struct circle {
		vec2f center;
		float radius = 8.f;
		vec4f color = { 1.f, 1.f, 1.f, 1.f };
	};

	struct line {
		vec2f from;
		vec2f to;
		vec4f color = { 1.f, 1.f, 1.f, 1.f };
		float thickness = 2.f;
	};

	struct text {
		vec2f position;
		std::string value;
		vec4f color = { 1.f, 1.f, 1.f, 1.f };
		float scale = 1.f;
		vec2f anchor = { 0.f, 0.f };
	};

	struct label {
		vec2f position;
		vec2f size = { 120.f, 24.f };
		std::string value;
		vec4f color = { 1.f, 1.f, 1.f, 1.f };
		float scale = 1.f;
		gui::layout::halign align = gui::layout::halign::start;
	};

	struct chrome_gallery {
		vec2f position;
		std::string kind;
		float width = 960.f;
	};
}

export namespace gse::ide::agent {
	consteval auto drawable_types() -> std::vector<std::meta::info>;
}

consteval auto gse::ide::agent::drawable_types() -> std::vector<std::meta::info> {
	std::vector<std::meta::info> found;
	for (const auto m : std::meta::members_of(^^drawables, std::meta::access_context::unchecked())) {
		if (std::meta::is_type(m) && std::meta::is_class_type(m)) {
			found.push_back(m);
		}
	}
	return found;
}

export namespace gse::ide::agent {
	constexpr std::span<const std::meta::info> drawable_list = std::define_static_array(drawable_types());

	using canvas_command = [:std::meta::substitute(^^std::variant, drawable_list):];

	struct canvas_motion {
		std::size_t field = 0;
		vec4f from;
		gui::motion_spec spec;
	};

	struct canvas_step {
		canvas_command shape;
		std::vector<canvas_motion> motions;
		std::string_view origin;
	};

	struct sketch {
		char summary[160];
	};
}

export namespace gse::ide::agent::sketches {
	struct flow_params {
		vec2f position = { 8.f, 8.f };
		std::string from = "from";
		std::string to = "to";
		vec4f color = { 0.45f, 0.7f, 1.f, 1.f };
		vec2f box = { 120.f, 48.f };
		float gap = 96.f;
	};

	struct before_after_params {
		vec2f position = { 8.f, 8.f };
		std::string before = "before";
		std::string after = "after";
		vec4f before_color = { 0.9f, 0.45f, 0.4f, 1.f };
		vec4f after_color = { 0.4f, 0.85f, 0.5f, 1.f };
		vec2f panel = { 160.f, 96.f };
	};

	[[= sketch{ "two labelled boxes joined by an arrow that draws itself in" }]]
	auto flow(
		const flow_params& params
	) -> std::vector<canvas_step>;

	[[= sketch{ "two labelled panels side by side with a divider; the after panel fades in" }]]
	auto before_after(
		const before_after_params& params
	) -> std::vector<canvas_step>;
}

export namespace gse::ide::agent {
	consteval auto sketch_members() -> std::vector<std::meta::info>;
}

consteval auto gse::ide::agent::sketch_members() -> std::vector<std::meta::info> {
	std::vector<std::meta::info> found;
	for (const auto m : std::meta::members_of(^^sketches, std::meta::access_context::unchecked())) {
		if (std::meta::is_function(m) && has_annotation<sketch>(m)) {
			found.push_back(m);
		}
	}
	return found;
}

export namespace gse::ide::agent {
	constexpr std::span<const std::meta::info> sketch_list = std::define_static_array(sketch_members());

	struct canvas_record {
		std::string name;
		std::string source;
		vec2f size;
		std::int64_t stamped = 0;
		[[= archive_skip{}]] std::vector<canvas_step> steps;
	};

	struct canvas_frame {
		const gui::draw_context& ctx;
		rectf area;
		rectf clip;
		id canvas;
		float scale = 1.f;
	};

	struct canvas_tally {
		std::uint32_t sprites = 0;
		std::uint32_t texts = 0;
		std::uint32_t dropped = 0;
		std::uint32_t clipped = 0;
	};

	constexpr float max_canvas_extent = 4096.f;

	struct drawable_entry {
		std::string_view name;
		std::string fields;
		std::string_view summary;
		bool sketch = false;
	};

	auto parse_canvas(
		std::string_view source
	) -> std::expected<std::vector<canvas_step>, std::string>;

	auto draw_canvas(
		const canvas_frame& frame,
		std::span<const canvas_step> steps
	) -> canvas_tally;

	auto drawable_catalogue() -> std::span<const drawable_entry>;

	auto draw_chrome_gallery(
		const canvas_frame& frame,
		const drawables::chrome_gallery& gallery
	) -> void;

	auto queue_line(
		const gui::draw_context& ctx,
		vec2f from,
		vec2f to,
		float thickness,
		const vec4f& color,
		const rectf& clip
	) -> void;
}
