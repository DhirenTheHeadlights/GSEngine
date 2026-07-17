export module gse.ide.graph;

import std;

import gse;
import gse.graph;

import gse.ide.search;

export namespace gse::ide {
	struct edge_draw {
		std::uint32_t a = 0;
		std::uint32_t b = 0;
		gse::introspection::edge_kind kind = gse::introspection::edge_kind::data_raw;
	};

	struct channel_link_draw {
		std::uint32_t a = 0;
		std::uint32_t b = 0;
	};

	struct channel_use_draw {
		std::uint32_t node = 0;
		bool produce = false;
		std::string message;
	};

	struct graph_data {
		gse::introspection::system_graph snapshot;
		gse::graph::layout layout;
		std::vector<edge_draw> edges_draw;
		std::vector<channel_use_draw> channel_uses;
		std::vector<channel_link_draw> channel_edges;
		bool channels_merged = false;
		gse::vec2f pan{ 0.f, 0.f };
		float zoom = 1.f;
		gse::vec2f pan_last{ 0.f, 0.f };
		bool panning = false;
		std::optional<std::uint64_t> selected;
		std::optional<std::uint64_t> hovered;
		std::optional<std::uint64_t> isolated;
		gse::gui::interaction::click_state click;
		std::array<bool, 6> kind_visible{ true, true, true, true, true, true };
		float panel_ratio = 0.3f;
		bool resizing_panel = false;
		bool built = false;
	};

	auto build_graph(const gse::scheduler& sched) -> graph_data;

	auto build_graph_from_file(const std::filesystem::path& path) -> graph_data;

	auto draw_graph(gse::gui::builder& ui, const gse::rectf& area, graph_data& gd, const search::index_state* index, gse::channel_writer channels) -> void;
}
