export module gse.ide.app:dock;

import std;
import gse;

export namespace gse::ide {
	struct panel_desc {
		gse::id id;
		std::string_view name;
		vec2f min_size{ 180.f, 120.f };
		bool start_hidden = false;
		bool menu_hidden = false;
		std::optional<gui::panel_edge> accent_edge;
	};

	struct dock_metrics {
		float scale = 1.f;
		float divider_thickness = 12.f;
		float header_height = 32.f;
		float tear_threshold = 6.f;
	};

	struct dock_node {
		id parent;
		gui::layout::split_axis axis = gui::layout::split_axis::columns;
		float ratio = 0.5f;
		id first;
		id second;
		std::vector<id> panels;
		std::uint32_t active_panel = 0;
		gui::layout::split_drag_state divider_drag;
	};

	struct dock_tree {
		id_mapped_collection<dock_node, id> nodes;
		id root;
		id maximized;
	};

	struct dock_placement {
		id node;
		rectf rect;
	};

	struct dock_divider {
		id node;
		rectf rect;
		gui::layout::split_axis axis = gui::layout::split_axis::columns;
	};

	struct dock_layout {
		std::vector<dock_placement> leaves;
		std::vector<dock_divider> dividers;
	};

	struct dock_view {
		id window;
		dock_tree tree;
		dock_layout layout;
		dock_metrics metrics;
		rectf frame;
		vec2i window_position;
		vec2i window_size;
	};

	struct dock_drag {
		id panel;
		id group;
		vec2f start;
		rectf header;
		bool torn = false;
	};

	struct dock_drop {
		id node;
		gui::dock::space space;
	};

	struct dock_landing {
		id window;
		dock_drop drop;
	};

	struct dock_popout {
		dock_tree tree;
		id lead;
		vec2i screen_position;
		vec2i size;
	};

	struct dock_window_layout {
		dock_tree tree;
		vec2i position;
		vec2i size;
	};

	struct dock_migration {
		id panel;
		id window;
	};

	struct dock_insert {
		id panel;
		id target;
		gui::dock::location location = gui::dock::location::center;
		float ratio = 0.5f;
	};

	struct dock_tree_sections {
		std::string header;
		std::string node_prefix;
		bool adopt_new_panels = false;
	};

	[[nodiscard]] auto is_leaf(
		const dock_node& node
	) -> bool;

	[[nodiscard]] auto find_leaf(
		const dock_tree& tree,
		id panel
	) -> id;

	[[nodiscard]] auto contains_panel(
		const dock_tree& tree,
		id panel
	) -> bool;

	[[nodiscard]] auto panel_count(
		const dock_tree& tree
	) -> std::size_t;

	[[nodiscard]] auto is_popout(
		const dock_view& v
	) -> bool;

	auto activate_panel(
		dock_tree& tree,
		id panel
	) -> void;

	auto insert_panel(
		dock_tree& tree,
		const dock_insert& what
	) -> void;

	auto remove_panel(
		dock_tree& tree,
		id panel
	) -> void;

	[[nodiscard]] auto resolve(
		const dock_tree& tree,
		const rectf& frame,
		const dock_metrics& metrics,
		std::span<const panel_desc> panels
	) -> dock_layout;

	auto update_dividers(
		dock_tree& tree,
		const rectf& frame,
		const dock_metrics& metrics,
		std::span<const panel_desc> panels,
		const gui::layout::split_drag& drag
	) -> dock_layout;

	[[nodiscard]] auto dragging_axis(
		const dock_tree& tree
	) -> std::optional<gui::layout::split_axis>;

	[[nodiscard]] auto divider_at(
		const dock_layout& layout,
		vec2f mouse
	) -> const dock_divider*;

	[[nodiscard]] auto drop_target(
		const dock_tree& tree,
		const dock_layout& layout,
		const dock_metrics& metrics,
		const dock_drag& drag,
		vec2f mouse
	) -> std::optional<dock_drop>;

	auto insert_group(
		dock_tree& from,
		dock_tree& to,
		id group,
		const dock_drop& where
	) -> void;

	[[nodiscard]] auto panels_of(
		const dock_tree& tree
	) -> std::vector<id>;

	[[nodiscard]] auto primary_tree_sections() -> dock_tree_sections;

	[[nodiscard]] auto window_tree_sections(
		std::size_t index
	) -> dock_tree_sections;

	[[nodiscard]] auto serialize_tree(
		const dock_tree& tree,
		std::span<const panel_desc> panels,
		const dock_tree_sections& where
	) -> std::string;

	[[nodiscard]] auto deserialize_tree(
		std::span<const layout_store::section> sections,
		std::span<const panel_desc> panels,
		const dock_tree_sections& where
	) -> std::optional<dock_tree>;

	[[nodiscard]] auto serialize_windows(
		std::span<const dock_window_layout> windows,
		std::span<const panel_desc> panels
	) -> std::string;

	[[nodiscard]] auto deserialize_windows(
		std::span<const layout_store::section> sections,
		std::span<const panel_desc> panels
	) -> std::vector<dock_window_layout>;
}

namespace gse::ide {
	constexpr std::string_view dock_node_section_prefix = "dock node ";
	constexpr std::string_view dock_window_section_prefix = "dock window ";

	auto make_node(
		dock_tree& tree
	) -> id;

	auto detach_child(
		dock_tree& tree,
		id parent,
		id child
	) -> void;

	auto replace_child(
		dock_tree& tree,
		id parent,
		id from,
		id to
	) -> void;

	auto prune_empty_leaves(
		dock_tree& tree
	) -> void;

	[[nodiscard]] auto min_size_of(
		const dock_tree& tree,
		id node,
		const dock_metrics& metrics,
		std::span<const panel_desc> panels
	) -> vec2f;

	[[nodiscard]] auto split_params_for(
		const dock_tree& tree,
		const dock_node& node,
		const rectf& area,
		const dock_metrics& metrics,
		std::span<const panel_desc> panels
	) -> gui::layout::split_params;

	auto place_node(
		const dock_tree& tree,
		id node,
		const rectf& area,
		const dock_metrics& metrics,
		std::span<const panel_desc> panels,
		dock_layout& out
	) -> void;

	auto drag_split_ratios(
		dock_tree& tree,
		id node,
		const rectf& area,
		const dock_metrics& metrics,
		std::span<const panel_desc> panels,
		const gui::layout::split_drag& drag,
		bool& consumed
	) -> void;

	[[nodiscard]] auto panel_desc_for(
		std::span<const panel_desc> panels,
		id id
	) -> const panel_desc*;

	[[nodiscard]] auto any_leaf(
		const dock_tree& tree
	) -> id;

	auto collect_nodes(
		const dock_tree& tree,
		id node,
		std::vector<id>& out
	) -> void;

	[[nodiscard]] auto slot_of_node(
		const std::unordered_map<id, std::size_t>& index_of,
		id node
	) -> std::string;

	[[nodiscard]] auto node_at_slot(
		const std::unordered_map<std::size_t, id>& id_of,
		const std::string& value
	) -> id;
}