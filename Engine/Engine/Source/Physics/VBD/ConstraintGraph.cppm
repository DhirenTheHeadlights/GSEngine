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

		auto colors(
		) const -> std::span<const std::vector<std::uint32_t>>;
	private:
		std::vector<contact_constraint> m_contacts;
		std::vector<velocity_motor_constraint> m_motors;
		std::vector<std::vector<std::uint32_t>> m_colors;
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

auto gse::vbd::constraint_graph::compute_coloring() -> void {
	m_colors.clear();

	if (m_contacts.empty()) return;

	std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> body_to_constraints;

	for (std::uint32_t i = 0; i < m_contacts.size(); ++i) {
		body_to_constraints[m_contacts[i].body_a].push_back(i);
		body_to_constraints[m_contacts[i].body_b].push_back(i);
	}

	std::vector constraint_colors(m_contacts.size(), -1);

	for (std::uint32_t i = 0; i < m_contacts.size(); ++i) {
		std::set<int> neighbor_colors;

		for (const auto body : { m_contacts[i].body_a, m_contacts[i].body_b }) {
			for (const auto neighbor_idx : body_to_constraints[body]) {
				if (neighbor_idx != i && constraint_colors[neighbor_idx] >= 0) {
					neighbor_colors.insert(constraint_colors[neighbor_idx]);
				}
			}
		}

		int color = 0;
		while (neighbor_colors.contains(color)) {
			++color;
		}

		constraint_colors[i] = color;

		while (static_cast<std::size_t>(color) >= m_colors.size()) {
			m_colors.emplace_back();
		}
		m_colors[color].push_back(i);
	}
}

auto gse::vbd::constraint_graph::clear() -> void {
	m_contacts.clear();
	m_motors.clear();
	m_colors.clear();
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

auto gse::vbd::constraint_graph::colors() const -> std::span<const std::vector<std::uint32_t>> {
	return m_colors;
}
