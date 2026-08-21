export module gse.ide.graph;

import std;

import gse;
import gse.graph;

import gse.ide.search;
import gse.ide.navigation;

export namespace gse::ide {
	enum class graph_load_error {
		unavailable,
		incompatible,
		incomplete
	};

	struct edge_draw {
		std::uint32_t a = 0;
		std::uint32_t b = 0;
		introspection::edge_kind kind = introspection::edge_kind::data_raw;
	};

	struct channel_link_draw {
		std::uint32_t a = 0;
		std::uint32_t b = 0;
	};

	struct channel_use_draw {
		std::uint32_t node = 0;
		bool produce = false;
		std::string qualified;
	};

	struct list_item {
		std::optional<std::uint64_t> node_id;
		std::string display;
		std::string qualified;
		std::vector<std::string> via;
		std::vector<std::string> via_lines;
		std::optional<search::location> target;
		bool linkable = false;
	};

	struct node_relations {
		std::vector<list_item> reads;
		std::vector<list_item> writes;
		std::vector<list_item> depends;
		std::vector<list_item> feeds;
		std::vector<list_item> publishes;
		std::vector<list_item> consumes;
		std::string reads_header;
		std::string writes_header;
		std::string depends_header;
		std::string feeds_header;
		std::string publishes_header;
		std::string consumes_header;
	};

	struct graph_text_line {
		std::string text;
		vec4f color;
		float indent = 0.f;
	};

	struct graph_data {
		introspection::system_graph snapshot;
		graph::layout layout;
		std::unordered_map<std::uint64_t, std::uint32_t> index_of;
		std::vector<edge_draw> edges_draw;
		std::vector<channel_use_draw> channel_uses;
		std::vector<channel_link_draw> channel_edges;
		std::vector<std::string> labels;
		std::vector<float> widths;
		std::vector<vec4f> base_colors;
		std::vector<vec2f> world;
		std::vector<vec2f> centers;
		std::vector<gui::graph_canvas::node> nodes;
		std::vector<gui::graph_canvas::edge> edges;
		std::vector<bool> highlighted;
		std::vector<bool> isolate_set;
		std::vector<bool> edge_lit;
		vec2f world_min{ 0.f, 0.f };
		vec2f world_max{ 0.f, 0.f };
		id presentation_font;
		std::uint32_t presentation_font_version = 0;
		float presentation_font_size = 0.f;
		bool presentation_ready = false;
		vec2f pan{ 0.f, 0.f };
		float zoom = 1.f;
		vec2f pan_last{ 0.f, 0.f };
		vec2f pan_press{ 0.f, 0.f };
		bool panning = false;
		bool dragged = false;
		std::optional<std::uint64_t> selected;
		std::optional<std::uint64_t> isolated;
		gui::interaction::click_state click;
		std::array<bool, introspection::edge_kind_count> kind_visible = [] {
			std::array<bool, introspection::edge_kind_count> visible;
			visible.fill(true);
			return visible;
		}();
		float panel_ratio = 0.3f;
		gui::layout::split_drag_state resizing_panel;
		bool built = false;
		node_relations detail;
		std::optional<std::uint64_t> detail_for;
		std::optional<std::uint64_t> detail_generation;
		float detail_wrap_width = 0.f;
		float detail_wrap_font_size = 0.f;
		std::vector<graph_text_line> tooltip_lines;
		std::optional<std::uint64_t> tooltip_for;
		std::optional<std::uint64_t> tooltip_generation;
		float tooltip_width = 0.f;
		float tooltip_font_size = 0.f;
		vec4f tooltip_text_color;
		vec4f tooltip_secondary_color;
		vec4f tooltip_accent_color;
	};

	auto build_graph(
		const scheduler& sched
	) -> graph_data;

	auto build_graph_from_file(
		const std::filesystem::path& path
	) -> std::expected<graph_data, graph_load_error>;

	auto draw_graph(
		gui::builder& ui,
		const input::state& input,
		const rectf& area,
		graph_data& gd,
		const search::index_state* index,
		channel_write<jump_to_request, set_cursor_shape_request> channels
	) -> void;
}
