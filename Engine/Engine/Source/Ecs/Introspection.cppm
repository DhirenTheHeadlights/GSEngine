export module gse.introspection;

import std;

export namespace gse::introspection {
	constexpr std::uint32_t system_graph_magic = 0x47535347;
	constexpr std::uint32_t system_graph_version = 2;

	enum class edge_kind : std::uint8_t {
		data_raw,
		data_waw,
		data_war,
		shared_view,
		lifecycle,
		channel
	};

	struct graph_edge {
		std::uint64_t from = 0;
		std::uint64_t to = 0;
		edge_kind kind = edge_kind::data_raw;
		std::vector<std::string> via;
	};

	struct graph_node {
		std::uint64_t id = 0;
		std::string name;
		std::string display;
		std::string category;
		std::string file;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
		bool has_init = false;
		bool has_run = false;
		bool has_frame = false;
		bool deferred = false;
		std::vector<std::string> reads;
		std::vector<std::string> writes;
	};

	struct system_graph {
		std::vector<graph_node> nodes;
		std::vector<graph_edge> edges;
	};
}
