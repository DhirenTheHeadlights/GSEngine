module gse.physics:contact_events_impl;

import std;

import gse.core;
import gse.math;

import :contact_events;
import :vbd_constraints;

auto gse::physics::contact_tracker::key_of(const pair_load& load) -> std::pair<std::uint64_t, std::uint64_t> {
	return { load.body_a.number(), load.body_b.number() };
}

auto gse::physics::contact_tracker::key_of(const loaded_pair& pair) -> std::pair<std::uint64_t, std::uint64_t> {
	return { pair.body_a.number(), pair.body_b.number() };
}

auto gse::physics::contact_tracker::set_active(const bool active) -> void {
	if (active && !m_active) {
		clear();
	}
	m_active = active;
}

auto gse::physics::contact_tracker::active() const -> bool {
	return m_active;
}

auto gse::physics::contact_tracker::absorb(pair_load& into, const pair_load& from) -> void {
	into.impulse += from.impulse;
	into.point += (from.point - into.point) * (from.impulse / into.impulse);
	into.relative_speed = std::max(into.relative_speed, from.relative_speed);
}

auto gse::physics::contact_tracker::add(const pair_load& load) -> void {
	if (!m_step.empty() && key_of(m_step.back()) == key_of(load)) {
		absorb(m_step.back(), load);
		return;
	}
	m_step.push_back(load);
}

auto gse::physics::contact_tracker::fold_readback(const std::span<const vbd::contact_constraint> contacts, const std::span<const id> owners, const std::span<const vbd::body_snapshot> bodies, const time_t<float, seconds> dt, const std::function<inverse_mass(id)>& inverse_mass_of) -> void {
	const force bearing = newtons(1e-3f);
	const velocity arrested = meters_per_second(0.1f);
	const std::size_t known = std::min(owners.size(), bodies.size());

	m_arrests.clear();
	for (const auto& c : contacts) {
		if (c.body_a == c.body_b || c.body_a >= known || c.body_b >= known) {
			continue;
		}
		const auto& a = bodies[c.body_a];
		const auto& b = bodies[c.body_b];
		const bool faster_a = magnitude(a.velocity) >= magnitude(b.velocity);
		const auto& body = faster_a ? a : b;
		const bool ordered = owners[c.body_a].number() <= owners[c.body_b].number();
		pair_load load{
			.body_a = owners[ordered ? c.body_a : c.body_b],
			.body_b = owners[ordered ? c.body_b : c.body_a],
			.point = body.position + rotate_vector(body.orientation, faster_a ? c.local_anchor_a : c.local_anchor_b),
			.normal = ordered ? c.normal : -c.normal,
			.impulse = -c.lambda[0] * dt,
			.relative_speed = -c.approach_speed,
			.index_a = ordered ? c.body_a : c.body_b,
			.index_b = ordered ? c.body_b : c.body_a,
		};
		if (-c.lambda[0] > bearing) {
			add(load);
		}

		if (c.body_a >= m_read_bodies.size() || c.body_b >= m_read_bodies.size()) {
			continue;
		}
		const auto& was_a = m_read_bodies[c.body_a];
		const auto& was_b = m_read_bodies[c.body_b];
		if (was_a.owner != owners[c.body_a] || was_b.owner != owners[c.body_b]) {
			continue;
		}
		const velocity closing_before = -dot(c.normal, was_a.velocity - was_b.velocity);
		const velocity lost = closing_before + dot(c.normal, a.velocity - b.velocity);
		if (closing_before <= arrested || lost <= arrested) {
			continue;
		}
		const inverse_mass yielding = inverse_mass_of(owners[c.body_a]) + inverse_mass_of(owners[c.body_b]);
		if (yielding <= inverse_mass{}) {
			continue;
		}
		load.impulse = lost / yielding;
		load.relative_speed = closing_before;
		m_arrests.push_back(load);
	}

	std::ranges::stable_sort(m_arrests, std::ranges::less{}, [](const pair_load& load) {
		return key_of(load);
	});
	for (auto first = m_arrests.begin(); first != m_arrests.end();) {
		const auto next = std::ranges::find_if(first, m_arrests.end(), [&](const pair_load& load) {
			return key_of(load) != key_of(*first);
		});
		const auto rows = static_cast<float>(std::distance(first, next));
		for (; first != next; ++first) {
			first->impulse = first->impulse / rows;
			add(*first);
		}
	}

	m_read_bodies.resize(known);
	for (std::size_t i = 0; i < known; ++i) {
		m_read_bodies[i] = {
			.owner = owners[i],
			.velocity = bodies[i].velocity,
		};
	}
}

auto gse::physics::contact_tracker::merge_step() -> void {
	std::ranges::sort(m_step, std::ranges::less{}, [](const pair_load& load) {
		return key_of(load);
	});

	auto merged = m_step.begin();
	for (auto first = m_step.begin(); first != m_step.end();) {
		pair_load load = *first;
		auto next = std::next(first);
		for (; next != m_step.end() && key_of(*next) == key_of(load); ++next) {
			absorb(load, *next);
		}
		first = next;
		*merged++ = load;
	}
	m_step.erase(merged, m_step.end());
}

auto gse::physics::contact_tracker::close_step(const std::uint64_t step, std::vector<contact_event>& out) -> void {
	++m_closed;
	const bool fresh_step = m_primed && step > m_emitted_step;
	m_emitted_step = std::max(m_emitted_step, step);
	m_primed = true;

	merge_step();

	m_carried.clear();
	auto known = m_loaded.cbegin();
	const auto carry_released = [&](const std::pair<std::uint64_t, std::uint64_t>& below) {
		for (; known != m_loaded.cend() && key_of(*known) < below; ++known) {
			if (m_closed - known->closed <= release_steps) {
				m_carried.push_back(*known);
			}
		}
	};

	for (const pair_load& load : m_step) {
		carry_released(key_of(load));
		const bool held = known != m_loaded.cend() && key_of(*known) == key_of(load);
		const bool bore_load = held && m_closed - known->closed <= release_steps;
		if (held) {
			++known;
		}
		m_carried.push_back({
			.body_a = load.body_a,
			.body_b = load.body_b,
			.closed = m_closed,
		});
		if (fresh_step && !bore_load) {
			out.push_back({
				.step = step,
				.body_a = load.body_a,
				.body_b = load.body_b,
				.point = load.point,
				.normal = load.normal,
				.impulse = load.impulse,
				.relative_speed = load.relative_speed,
			});
		}
	}
	std::ranges::copy_if(known, m_loaded.cend(), std::back_inserter(m_carried), [this](const loaded_pair& pair) {
		return m_closed - pair.closed <= release_steps;
	});

	m_loaded.swap(m_carried);
	m_step.clear();
}

auto gse::physics::contact_tracker::clear() -> void {
	m_step.clear();
	m_loaded.clear();
	m_carried.clear();
	m_arrests.clear();
	m_read_bodies.clear();
	m_primed = false;
}
