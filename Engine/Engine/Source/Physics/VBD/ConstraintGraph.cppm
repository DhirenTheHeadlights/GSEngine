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

		auto add_joint(
			const joint_constraint& j
		) -> std::uint32_t;

		auto remove_joint(
			std::uint32_t index
		) -> void;

		auto compute_coloring(
			std::uint32_t num_bodies,
			std::span<const bool> locked
		) -> void;

		auto clear(
		) -> void;

		auto clear_joints(
		) -> void;

		auto contact_constraints(
		) -> std::vector<contact_constraint>&;

		auto contact_constraints(
		) const -> std::span<const contact_constraint>;

		auto motor_constraints(
		) -> std::vector<velocity_motor_constraint>&;

		auto motor_constraints(
		) const -> std::span<const velocity_motor_constraint>;

		auto joint_constraints(
		) -> std::vector<joint_constraint>&;

		auto joint_constraints(
		) const -> std::span<const joint_constraint>;

		auto body_colors(
		) const -> std::span<const std::vector<std::uint32_t>>;

		auto body_contact_indices(
			std::uint32_t body_idx
		) const -> std::span<const std::uint32_t>;

		auto body_joint_indices(
			std::uint32_t body_idx
		) const -> std::span<const std::uint32_t>;
	private:
		std::vector<contact_constraint> m_contacts;
		std::vector<velocity_motor_constraint> m_motors;
		std::vector<joint_constraint> m_joints;
		static_vector<std::vector<std::uint32_t>, 64> m_body_colors;
		std::vector<std::vector<std::uint32_t>> m_body_contacts;
		std::vector<std::vector<std::uint32_t>> m_body_joints;
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

auto gse::vbd::constraint_graph::add_joint(const joint_constraint& j) -> std::uint32_t {
	const auto idx = static_cast<std::uint32_t>(m_joints.size());
	m_joints.push_back(j);
	return idx;
}

auto gse::vbd::constraint_graph::remove_joint(const std::uint32_t index) -> void {
	if (index < m_joints.size()) {
		m_joints.erase(m_joints.begin() + index);
	}
}

auto gse::vbd::constraint_graph::compute_coloring(const std::uint32_t num_bodies, const std::span<const bool> locked) -> void {
	m_body_colors.clear();
	m_body_contacts.assign(num_bodies, {});
	m_body_joints.assign(num_bodies, {});

	if (m_contacts.empty() && m_joints.empty()) return;

	for (std::uint32_t i = 0; i < m_contacts.size(); ++i) {
		m_body_contacts[m_contacts[i].body_a].push_back(i);
		m_body_contacts[m_contacts[i].body_b].push_back(i);
	}

	for (std::uint32_t i = 0; i < m_joints.size(); ++i) {
		m_body_joints[m_joints[i].body_a].push_back(i);
		m_body_joints[m_joints[i].body_b].push_back(i);
	}

	std::vector<std::vector<std::uint32_t>> adjacency(num_bodies);
	for (const auto& c : m_contacts) {
		if (locked[c.body_a] || locked[c.body_b]) continue;
		adjacency[c.body_a].push_back(c.body_b);
		adjacency[c.body_b].push_back(c.body_a);
	}
	for (const auto& j : m_joints) {
		if (locked[j.body_a] || locked[j.body_b]) continue;
		adjacency[j.body_a].push_back(j.body_b);
		adjacency[j.body_b].push_back(j.body_a);
	}
	for (auto& adj : adjacency) {
		std::ranges::sort(adj);
		adj.erase(std::ranges::unique(adj).begin(), adj.end());
	}

	std::vector body_color(num_bodies, -1);

	for (std::uint32_t bi = 0; bi < num_bodies; ++bi) {
		if (locked[bi] || (m_body_contacts[bi].empty() && m_body_joints[bi].empty())) {
			continue;
		}

		std::uint64_t used_colors = 0;
		for (const auto neighbor : adjacency[bi]) {
			if (body_color[neighbor] >= 0 && body_color[neighbor] < 64) {
				used_colors |= (1ull << body_color[neighbor]);
			}
		}

		int color = 0;
		while (color < 64 && (used_colors & (1ull << color))) {
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
	m_joints.clear();
	m_body_colors.clear();
	m_body_contacts.clear();
	m_body_joints.clear();
}

auto gse::vbd::constraint_graph::clear_joints() -> void {
	m_joints.clear();
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

auto gse::vbd::constraint_graph::joint_constraints() -> std::vector<joint_constraint>& {
	return m_joints;
}

auto gse::vbd::constraint_graph::joint_constraints() const -> std::span<const joint_constraint> {
	return m_joints;
}

auto gse::vbd::constraint_graph::body_colors() const -> std::span<const std::vector<std::uint32_t>> {
	return m_body_colors.span();
}

auto gse::vbd::constraint_graph::body_contact_indices(const std::uint32_t body_idx) const -> std::span<const std::uint32_t> {
	if (body_idx >= m_body_contacts.size()) return {};
	return m_body_contacts[body_idx];
}

auto gse::vbd::constraint_graph::body_joint_indices(const std::uint32_t body_idx) const -> std::span<const std::uint32_t> {
	if (body_idx >= m_body_joints.size()) return {};
	return m_body_joints[body_idx];
}
