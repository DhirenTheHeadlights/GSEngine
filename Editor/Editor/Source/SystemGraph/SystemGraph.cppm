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

	struct channel_link_draw {
		std::uint32_t a = 0;
		std::uint32_t b = 0;
		std::string qualified;
	};

	struct graph_group {
		std::string name;
		std::uint64_t key = 0;
		std::vector<std::uint32_t> members;
		vec4f color;
		bool expanded = false;
	};

	struct graph_unit {
		std::uint64_t key = 0;
		std::optional<std::uint32_t> system;
		std::uint32_t group = 0;
	};

	struct unit_edge_draw {
		std::uint32_t a = 0;
		std::uint32_t b = 0;
		introspection::edge_kind kind = introspection::edge_kind::data_raw;
		std::string label;
	};

	enum class graph_view : std::uint8_t {
		graph,
		lifetime
	};

	struct data_access_info {
		char verb[24];
		introspection::edge_kind color_of = introspection::edge_kind::lifecycle;
		bool marked = false;
	};

	enum class data_access : std::uint8_t {
		read [[= data_access_info{
			.verb = "reads",
			.color_of = introspection::edge_kind::shared_view,
		}]],
		write [[= data_access_info{
			.verb = "writes",
			.color_of = introspection::edge_kind::data_raw,
			.marked = true,
		}]],
		read_write [[= data_access_info{
			.verb = "reads + writes",
			.color_of = introspection::edge_kind::data_waw,
			.marked = true,
		}]],
		publish [[= data_access_info{
			.verb = "publishes",
			.color_of = introspection::edge_kind::channel,
			.marked = true,
		}]],
		consume [[= data_access_info{
			.verb = "consumes",
			.color_of = introspection::edge_kind::channel,
		}]]
	};

	struct lifetime_cell {
		std::uint32_t column = 0;
		data_access access = data_access::read;
	};

	struct lifetime_row {
		std::string qualified;
		std::string display;
		bool channel = false;
		std::vector<lifetime_cell> cells;
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
		std::vector<channel_use_draw> channel_uses;
		std::vector<channel_link_draw> channel_edges;
		std::vector<graph_group> groups;
		std::vector<std::uint32_t> group_of;
		std::vector<graph_unit> units;
		std::vector<std::uint32_t> unit_of;
		std::unordered_map<std::uint64_t, std::uint32_t> unit_index_of;
		std::vector<unit_edge_draw> unit_edges;
		std::vector<unit_edge_draw> unit_channel_edges;
		std::vector<std::pair<vec2f, vec2f>> group_world;
		std::vector<gui::graph_canvas::group> canvas_groups;
		graph_view view = graph_view::graph;
		std::vector<std::uint32_t> lifetime_columns;
		std::vector<lifetime_row> lifetime_rows;
		std::vector<bool> lifetime_group_visible;
		std::optional<std::size_t> lifetime_selected_row;
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
		const rectf& area,
		graph_data& gd,
		const search::index_state* index,
		channel_write<jump_to_request, set_cursor_shape_request> channels
	) -> void;
}

namespace gse::ide {
	auto category_color(
		std::string_view category
	) -> vec4f;

	auto edge_color(
		introspection::edge_kind kind
	) -> vec4f;

	auto pretty_name(
		std::string_view name
	) -> std::string;

	auto node_label(
		const introspection::graph_node& node
	) -> std::string_view;

	auto draw_text_tooltip(
		const gui::draw_context& ctx,
		const rectf& area,
		std::span<const graph_text_line> lines,
		vec2f anchor
	) -> void;

	auto build_lifetime(
		graph_data& gd
	) -> void;

	auto draw_lifetime(
		gui::builder& ui,
		const rectf& area,
		graph_data& gd,
		channel_write<jump_to_request, set_cursor_shape_request> channels
	) -> void;
}
