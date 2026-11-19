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

namespace gse::graph {
	auto forward_adjacency(
		std::size_t node_count,
		std::span<const std::pair<std::uint32_t, std::uint32_t>> edges
	) -> std::vector<std::vector<std::uint32_t>>;

	auto reorder_layers(
		std::vector<std::vector<std::uint32_t>>& by_layer,
		const std::vector<std::vector<std::uint32_t>>& out,
		std::size_t node_count
	) -> void;
}

auto gse::graph::forward_adjacency(const std::size_t node_count, const std::span<const std::pair<std::uint32_t, std::uint32_t>> edges) -> std::vector<std::vector<std::uint32_t>> {
	std::vector<std::vector<std::uint32_t>> all(node_count);
	for (const auto& e : edges) {
		if (e.first >= node_count || e.second >= node_count || e.first == e.second) {
			continue;
		}
		all[e.first].push_back(e.second);
	}

	enum class mark : std::uint8_t {
		unvisited,
		open,
		closed
	};
	std::vector<mark> marks(node_count, mark::unvisited);
	std::vector<std::vector<std::uint32_t>> out(node_count);
	std::vector<std::pair<std::uint32_t, std::size_t>> stack;
	for (std::uint32_t root = 0; root < node_count; ++root) {
		if (marks[root] != mark::unvisited) {
			continue;
		}
		marks[root] = mark::open;
		stack.emplace_back(root, 0);
		while (!stack.empty()) {
			auto& [u, next] = stack.back();
			if (next == all[u].size()) {
				marks[u] = mark::closed;
				stack.pop_back();
				continue;
			}
			const std::uint32_t v = all[u][next];
			++next;
			if (marks[v] == mark::open) {
				continue;
			}
			out[u].push_back(v);
			if (marks[v] == mark::unvisited) {
				marks[v] = mark::open;
				stack.emplace_back(v, 0);
			}
		}
	}
	return out;
}

auto gse::graph::reorder_layers(std::vector<std::vector<std::uint32_t>>& by_layer, const std::vector<std::vector<std::uint32_t>>& out, const std::size_t node_count) -> void {
	std::vector<std::vector<std::uint32_t>> in(node_count);
	for (std::uint32_t u = 0; u < node_count; ++u) {
		for (const std::uint32_t v : out[u]) {
			in[v].push_back(u);
		}
	}

	std::vector<float> position(node_count, 0.f);
	const auto record = [&](const std::vector<std::uint32_t>& row) {
		for (std::size_t i = 0; i < row.size(); ++i) {
			position[row[i]] = static_cast<float>(i);
		}
	};
	for (const std::vector<std::uint32_t>& row : by_layer) {
		record(row);
	}

	std::vector<float> key(node_count, 0.f);
	const auto sweep = [&](std::vector<std::uint32_t>& row, const std::vector<std::vector<std::uint32_t>>& neighbours) {
		for (const std::uint32_t v : row) {
			key[v] = position[v];
			if (neighbours[v].empty()) {
				continue;
			}
			float sum = 0.f;
			for (const std::uint32_t w : neighbours[v]) {
				sum += position[w];
			}
			key[v] = sum / static_cast<float>(neighbours[v].size());
		}
		std::ranges::stable_sort(row, [&](const std::uint32_t a, const std::uint32_t b) {
			return key[a] < key[b];
		});
		record(row);
	};

	constexpr int passes = 4;
	for (int pass = 0; pass < passes; ++pass) {
		for (std::size_t l = 1; l < by_layer.size(); ++l) {
			sweep(by_layer[l], in);
		}
		for (std::size_t l = by_layer.size(); l-- > 1;) {
			sweep(by_layer[l - 1], out);
		}
	}
}

auto gse::graph::layered_layout(const std::size_t node_count, const std::span<const std::pair<std::uint32_t, std::uint32_t>> edges) -> layout {
	layout result;
	result.placements.resize(node_count);
	if (node_count == 0) {
		return result;
	}

	const std::vector<std::vector<std::uint32_t>> out = forward_adjacency(node_count, edges);
	std::vector<std::uint32_t> remaining(node_count, 0);
	for (std::uint32_t u = 0; u < node_count; ++u) {
		for (const std::uint32_t v : out[u]) {
			remaining[v] += 1;
		}
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

	const std::uint32_t max_layer = *std::ranges::max_element(layer_of);
	result.layer_count = max_layer + 1;

	std::vector<std::vector<std::uint32_t>> by_layer(result.layer_count);
	for (std::uint32_t i = 0; i < node_count; ++i) {
		by_layer[layer_of[i]].push_back(i);
	}
	reorder_layers(by_layer, out, node_count);

	result.layer_sizes.resize(result.layer_count);
	for (std::uint32_t l = 0; l < result.layer_count; ++l) {
		result.layer_sizes[l] = static_cast<std::uint32_t>(by_layer[l].size());
		for (std::uint32_t order = 0; order < by_layer[l].size(); ++order) {
			result.placements[by_layer[l][order]] = {
				.layer = l,
				.order = order,
			};
		}
	}

	return result;
}
