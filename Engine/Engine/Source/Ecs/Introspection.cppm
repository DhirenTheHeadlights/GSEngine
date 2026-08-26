export module gse.introspection;

import std;

import gse.meta;

export namespace gse::introspection {
	constexpr std::uint32_t system_graph_magic = 0x47535347;
	constexpr std::uint32_t system_graph_version = 3;

	struct edge_kind_info {
		char label[48];
		std::uint32_t color = 0;
		float alpha = 1.f;
	};

	enum class edge_kind : std::uint8_t {
		data_raw [[= edge_kind_info{
			.label = "read after write",
			.color = 0xd98c47,
			.alpha = 0.85f,
		}]],
		data_waw [[= edge_kind_info{
			.label = "write after write",
			.color = 0xd15959,
			.alpha = 0.85f,
		}]],
		data_war [[= edge_kind_info{
			.label = "write after read",
			.color = 0xccb85a,
			.alpha = 0.85f,
		}]],
		structural [[= edge_kind_info{
			.label = "adds / removes",
			.color = 0xa678d1,
			.alpha = 0.90f,
		}]],
		shared_view [[= edge_kind_info{
			.label = "shared view",
			.color = 0x5c99d9,
			.alpha = 0.90f,
		}]],
		lifecycle [[= edge_kind_info{
			.label = "init / frame order",
			.color = 0x737885,
			.alpha = 0.70f,
		}]],
		channel [[= edge_kind_info{
			.label = "channel (on select)",
			.color = 0x66b87a,
			.alpha = 0.85f,
		}]]
	};

	constexpr std::size_t edge_kind_count = std::define_static_array(std::meta::enumerators_of(^^edge_kind)).size();

	constexpr auto edge_info(
		edge_kind kind
	) -> edge_kind_info;

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
		std::vector<std::string> publishes;
		std::vector<std::string> consumes;
	};

	struct system_graph {
		std::vector<graph_node> nodes;
		std::vector<graph_edge> edges;
	};
}

constexpr auto gse::introspection::edge_info(const edge_kind kind) -> edge_kind_info {
	return annotation_from_enum(kind, edge_kind_info{
		.label = "unknown",
		.color = 0x808080,
		.alpha = 0.8f,
	});
}