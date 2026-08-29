export module gse.graph;

import std;

export namespace gse::graph {
	struct placement {
		std::uint32_t layer = 0;
		std::uint32_t order = 0;
	};

	struct layout {
		std::vector<placement> placements;
		std::uint32_t layer_count = 0;
		std::vector<std::uint32_t> layer_sizes;
	};

	auto layered_layout(std::size_t node_count, std::span<const std::pair<std::uint32_t, std::uint32_t>> edges) -> layout;
}

auto gse::graph::layered_layout(const std::size_t node_count, const std::span<const std::pair<std::uint32_t, std::uint32_t>> edges) -> layout {
	layout result;
	result.placements.resize(node_count);
	if (node_count == 0) {
		return result;
	}

	std::vector<std::vector<std::uint32_t>> out(node_count);
	std::vector<std::uint32_t> remaining(node_count, 0);
	for (const auto& e : edges) {
		if (e.first >= node_count || e.second >= node_count || e.first == e.second) {
			continue;
		}
		out[e.first].push_back(e.second);
		remaining[e.second] += 1;
	}

	std::vector<std::uint32_t> layer_of(node_count, 0);
	std::vector<std::uint32_t> ready;
	ready.reserve(node_count);
	for (std::uint32_t i = 0; i < node_count; ++i) {
		if (remaining[i] == 0) {
			ready.push_back(i);
		}
	}

	std::size_t head = 0;
	while (head < ready.size()) {
		const std::uint32_t u = ready[head];
		++head;
		for (const std::uint32_t v : out[u]) {
			if (layer_of[u] + 1 > layer_of[v]) {
				layer_of[v] = layer_of[u] + 1;
			}
			remaining[v] -= 1;
			if (remaining[v] == 0) {
				ready.push_back(v);
			}
		}
	}

	std::uint32_t max_layer = 0;
	for (std::uint32_t i = 0; i < node_count; ++i) {
		if (layer_of[i] > max_layer) {
			max_layer = layer_of[i];
		}
	}
	result.layer_count = max_layer + 1;
	result.layer_sizes.assign(result.layer_count, 0);

	for (std::uint32_t i = 0; i < node_count; ++i) {
		const std::uint32_t l = layer_of[i];
		result.placements[i].layer = l;
		result.placements[i].order = result.layer_sizes[l];
		result.layer_sizes[l] += 1;
	}

	return result;
}
