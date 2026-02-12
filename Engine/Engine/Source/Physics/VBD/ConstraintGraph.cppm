export module gse.physics:vbd_constraint_graph;

import std;

import :vbd_constraints;

export namespace gse::vbd {
	class constraint_graph {
	public:
		auto add_contact(
			const contact_constraint& c
		) -> std::uint32_t;

		auto add_motor(
			const velocity_motor_constraint& m
		) -> std::uint32_t;

		auto compute_coloring(
			std::uint32_t num_bodies,
			const std::vector<bool>& locked
		) -> void;

		auto clear(
		) -> void;

		auto contact_constraints(
		) -> std::vector<contact_constraint>&;

		auto contact_constraints(
		) const -> std::span<const contact_constraint>;

		auto motor_constraints(
		) -> std::vector<velocity_motor_constraint>&;

		auto motor_constraints(
		) const -> std::span<const velocity_motor_constraint>;

		auto body_colors(
		) const -> std::span<const std::vector<std::uint32_t>>;

		auto body_contact_indices(
			std::uint32_t body_idx
		) const -> std::span<const std::uint32_t>;
	private:
		std::vector<contact_constraint> m_contacts;
		std::vector<velocity_motor_constraint> m_motors;
		std::vector<std::vector<std::uint32_t>> m_body_colors;
		std::vector<std::vector<std::uint32_t>> m_body_contacts;
	};
}

auto gse::vbd::constraint_graph::add_contact(const contact_constraint& c) -> std::uint32_t {
	const auto idx = static_cast<std::uint32_t>(m_contacts.size());
	m_contacts.push_back(c);
	return idx;
}

auto gse::vbd::constraint_graph::add_motor(const velocity_motor_constraint& m) -> std::uint32_t {
	const auto idx = static_cast<std::uint32_t>(m_motors.size());
	m_motors.push_back(m);
	return idx;
}

auto gse::vbd::constraint_graph::compute_coloring(
	const std::uint32_t num_bodies,
	const std::vector<bool>& locked
) -> void {
	m_body_colors.clear();
	m_body_contacts.assign(num_bodies, {});

	if (m_contacts.empty()) return;

	for (std::uint32_t i = 0; i < m_contacts.size(); ++i) {
		m_body_contacts[m_contacts[i].body_a].push_back(i);
		m_body_contacts[m_contacts[i].body_b].push_back(i);
	}

	std::vector<std::unordered_set<std::uint32_t>> adjacency(num_bodies);
	for (const auto& c : m_contacts) {
		if (locked[c.body_a] || locked[c.body_b]) continue;
		adjacency[c.body_a].insert(c.body_b);
		adjacency[c.body_b].insert(c.body_a);
	}

	std::vector<int> body_color(num_bodies, -1);

	for (std::uint32_t bi = 0; bi < num_bodies; ++bi) {
		if (locked[bi] || m_body_contacts[bi].empty()) continue;

		std::set<int> neighbor_colors;
		for (const auto neighbor : adjacency[bi]) {
			if (body_color[neighbor] >= 0) {
				neighbor_colors.insert(body_color[neighbor]);
			}
		}

		int color = 0;
		while (neighbor_colors.contains(color)) {
			++color;
		}

		body_color[bi] = color;

		while (static_cast<std::size_t>(color) >= m_body_colors.size()) {
			m_body_colors.emplace_back();
		}
		m_body_colors[color].push_back(bi);
	}
}

auto gse::vbd::constraint_graph::clear() -> void {
	m_contacts.clear();
	m_motors.clear();
	m_body_colors.clear();
	m_body_contacts.clear();
}

auto gse::vbd::constraint_graph::contact_constraints() -> std::vector<contact_constraint>& {
	return m_contacts;
}

auto gse::vbd::constraint_graph::contact_constraints() const -> std::span<const contact_constraint> {
	return m_contacts;
}

auto gse::vbd::constraint_graph::motor_constraints() -> std::vector<velocity_motor_constraint>& {
	return m_motors;
}

auto gse::vbd::constraint_graph::motor_constraints() const -> std::span<const velocity_motor_constraint> {
	return m_motors;
}

auto gse::vbd::constraint_graph::body_colors() const -> std::span<const std::vector<std::uint32_t>> {
	return m_body_colors;
}

auto gse::vbd::constraint_graph::body_contact_indices(const std::uint32_t body_idx) const -> std::span<const std::uint32_t> {
	if (body_idx >= m_body_contacts.size()) return {};
	return m_body_contacts[body_idx];
}
