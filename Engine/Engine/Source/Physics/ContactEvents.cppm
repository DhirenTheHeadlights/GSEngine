export module gse.physics:contact_events;

import std;

import gse.core;
import gse.math;

import :vbd_constraints;

export namespace gse::physics {
	struct contact_event {
		std::uint64_t step = 0;
		id body_a;
		id body_b;
		vec3<position> point;
		vec3f normal;
		impulse impulse;
		velocity relative_speed;
	};

	struct moving_contact {
		std::uint64_t step = 0;
		id body_a;
		id body_b;
		vec3<position> point;
		vec3f normal;
		force load;
		velocity slip_speed;
		velocity roll_speed;
	};

	class contact_tracker {
	public:
		static constexpr std::uint64_t release_steps = 3;

		auto set_active(
			bool active
		) -> void;

		auto active() const -> bool;

		template <typename Body>
		auto fold(
			std::span<const vbd::contact_constraint> contacts,
			std::span<const id> owners,
			std::span<const Body> bodies,
			time_t<float, seconds> dt
		) -> void;

		auto fold_readback(
			std::span<const vbd::contact_constraint> contacts,
			std::span<const id> owners,
			std::span<const vbd::body_snapshot> bodies,
			time_t<float, seconds> dt,
			const std::function<inverse_mass(id)>& inverse_mass_of
		) -> void;

		template <typename Body>
		auto list_moving(
			std::span<const Body> bodies,
			time_t<float, seconds> step_time,
			std::uint64_t step,
			std::vector<moving_contact>& out
		) -> void;

		auto close_step(
			std::uint64_t step,
			std::vector<contact_event>& out
		) -> void;

		auto clear() -> void;

	private:
		struct read_body {
			id owner;
			vec3<velocity> velocity;
		};

		struct pair_load {
			id body_a;
			id body_b;
			vec3<position> point;
			vec3f normal;
			impulse impulse;
			velocity relative_speed;
			std::uint32_t index_a = 0;
			std::uint32_t index_b = 0;
		};

		struct loaded_pair {
			id body_a;
			id body_b;
			std::uint64_t closed = 0;
		};

		static auto key_of(
			const pair_load& load
		) -> std::pair<std::uint64_t, std::uint64_t>;

		static auto key_of(
			const loaded_pair& pair
		) -> std::pair<std::uint64_t, std::uint64_t>;

		static auto absorb(
			pair_load& into,
			const pair_load& from
		) -> void;

		auto add(
			const pair_load& load
		) -> void;

		auto merge_step() -> void;

		std::vector<pair_load> m_step;
		std::vector<loaded_pair> m_loaded;
		std::vector<loaded_pair> m_carried;
		std::vector<pair_load> m_arrests;
		std::vector<read_body> m_read_bodies;
		std::uint64_t m_closed = 0;
		std::uint64_t m_emitted_step = 0;
		bool m_active = false;
		bool m_primed = false;
	};
}

template <typename Body>
auto gse::physics::contact_tracker::fold(const std::span<const vbd::contact_constraint> contacts, const std::span<const id> owners, const std::span<const Body> bodies, const time_t<float, seconds> dt) -> void {
	const force bearing = newtons(1e-3f);
	const std::size_t known = std::min(owners.size(), bodies.size());

	for (const auto& c : contacts) {
		if (c.body_a == c.body_b || c.body_a >= known || c.body_b >= known || -c.lambda[0] <= bearing) {
			continue;
		}
		const bool faster_a = magnitude(bodies[c.body_a].velocity) >= magnitude(bodies[c.body_b].velocity);
		const auto& body = bodies[faster_a ? c.body_a : c.body_b];
		const vec3<position> touched = body.position + rotate_vector(body.orientation, faster_a ? c.local_anchor_a : c.local_anchor_b);
		const bool ordered = owners[c.body_a].number() <= owners[c.body_b].number();
		add({
			.body_a = owners[ordered ? c.body_a : c.body_b],
			.body_b = owners[ordered ? c.body_b : c.body_a],
			.point = touched,
			.normal = ordered ? c.normal : -c.normal,
			.impulse = -c.lambda[0] * dt,
			.relative_speed = -c.approach_speed,
			.index_a = ordered ? c.body_a : c.body_b,
			.index_b = ordered ? c.body_b : c.body_a,
		});
	}
}

template <typename Body>
auto gse::physics::contact_tracker::list_moving(const std::span<const Body> bodies, const time_t<float, seconds> step_time, const std::uint64_t step, std::vector<moving_contact>& out) -> void {
	const velocity moving = meters_per_second(0.01f);

	out.clear();
	merge_step();
	auto held = m_loaded.cbegin();
	for (const auto& load : m_step) {
		held = std::ranges::lower_bound(held, m_loaded.cend(), key_of(load), std::ranges::less{}, [](const loaded_pair& pair) {
			return key_of(pair);
		});
		if (held == m_loaded.cend() || key_of(*held) != key_of(load) || held->closed != m_closed) {
			continue;
		}
		const auto& a = bodies[load.index_a];
		const auto& b = bodies[load.index_b];
		const auto along_surface = [&load](const vec3<velocity>& v) -> vec3<velocity> {
			return v - load.normal * dot(load.normal, v);
		};
		const vec3<velocity> travel = along_surface(a.velocity - b.velocity);
		const vec3<velocity> turning = along_surface(cross(a.angular_velocity, load.point - a.position) / rad - cross(b.angular_velocity, load.point - b.position) / rad);
		const velocity slip_speed = magnitude(travel + turning);
		const velocity roll_speed = std::min(magnitude(travel), magnitude(turning));
		if (slip_speed <= moving && roll_speed <= moving) {
			continue;
		}
		out.push_back({
			.step = step,
			.body_a = load.body_a,
			.body_b = load.body_b,
			.point = load.point,
			.normal = load.normal,
			.load = load.impulse / step_time,
			.slip_speed = slip_speed,
			.roll_speed = roll_speed,
		});
	}
}
