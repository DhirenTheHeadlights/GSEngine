export module gse.runtime:world;

import std;

import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.os;
import gse.assets;
import gse.gpu;
import gse.physics;
import gse.graphics;
import gse.scene;

export namespace gse {
	struct evaluation_context {
		std::optional<id> client_id = std::nullopt;
		const actions::state* input = nullptr;
		registry* registry = nullptr;
	};

	struct trigger {
		id scene_id;
		std::function<bool(const evaluation_context&)> condition;
	};

	class world;

	class director {
	public:
		explicit director(
			world* world = nullptr
		);

		auto when(
			const trigger& trigger
		) -> director&;
	private:
		world* m_world = nullptr;
	};

	class world;

	using input_sampler_fn = std::function<void(world&, std::function<void(const evaluation_context&)>)>;

	auto local_input_sampler(
		world& w,
		std::function<void(const evaluation_context&)> fn
	) -> void;

	class world final : public identifiable {
	public:
		explicit world(
			scheduler& sched,
			std::string_view name = "Unnamed World"
		);

		auto update(
		) -> void;

		auto render(
		) -> void;

		auto shutdown(
		) -> void;

		auto set_input_sampler(
			input_sampler_fn fn
		) -> void;

		auto direct(
		) -> director;

		auto add(
			std::string_view name,
			scene::setup_fn setup = {}
		) -> scene*;

		auto activate(
			const gse::id& scene_id
		) -> void;

		auto deactivate(
			const gse::id& scene_id
		) -> void;

		auto scene(
			const gse::id& scene_id
		) -> scene*;

		auto current_scene(
		) -> gse::scene*;

		auto set_networked(
			bool is_networked
		) -> void;

		auto triggers(
		) const -> std::span<const trigger>;

		auto networked(
		) const -> bool;

		auto set_authoritative(
			bool is_authoritative
		) -> void;

		auto authoritative(
		) const -> bool;

		auto set_local_controlled_entity(
			gse::id entity_id
		) -> void;

		auto local_controlled_entity(
		) const -> gse::id;

		auto set_local_controller_id(
			gse::id controller_id
		) -> void;

		auto local_controller_id(
		) const -> gse::id;

		auto registry(
		) -> registry&;

		template <typename State>
		auto state_of(
		) -> State&;
	private:
		friend class director;
		friend auto world_update_player_controllers(world& w) -> void;

		auto add_trigger(
			const trigger& new_trigger
		) -> void;

		scheduler* m_scheduler = nullptr;

		gse::registry m_registry;
		std::unordered_map<gse::id, std::unique_ptr<gse::scene>> m_scenes;
		std::vector<trigger> m_triggers;
		std::optional<gse::id> m_active_scene = std::nullopt;
		bool m_networked = false;
		bool m_authoritative = true;
		std::optional<gse::id> m_client_id = std::nullopt;
		gse::id m_local_controlled_entity{};
		gse::id m_local_controller_id{};

		input_sampler_fn m_input_sampler;
		std::unordered_set<gse::id> m_pc_processed;
		std::unordered_map<gse::id, gse::id> m_pc_controller_to_local_player;
		bool m_pc_local_player_created = false;
	};
}

gse::director::director(world* world) : m_world(world) {}

auto gse::director::when(const trigger& trigger) -> director& {
	m_world->add_trigger(trigger);
	return *this;
}

auto gse::local_input_sampler(world& w, std::function<void(const evaluation_context&)> fn) -> void {
	const auto local_id = w.local_controlled_entity();
	if (!local_id.exists()) {
		return;
	}

	const auto& a = w.state_of<actions::system_state>();

	evaluation_context ctx{
		.client_id = local_id,
		.input = std::addressof(a.current_state()),
		.registry = &w.registry(),
	};

	fn(ctx);
}

gse::world::world(scheduler& sched, const std::string_view name) : identifiable(std::string(name)), m_scheduler(std::addressof(sched)) {}

auto gse::world::set_input_sampler(input_sampler_fn fn) -> void {
	m_input_sampler = std::move(fn);
}

namespace gse {
	auto world_update_player_controllers(world& w) -> void {
		auto* current = w.current_scene();
		if (!current) {
			return;
		}

		const auto& factory = current->player_factory();
		if (!factory) {
			return;
		}

		auto& reg = current->registry();
		auto& processed = w.m_pc_processed;
		auto& controller_to_local_player = w.m_pc_controller_to_local_player;

		if (!w.networked()) {
			if (!w.m_pc_local_player_created) {
				const auto player_id = factory(*current, std::nullopt);
				w.set_local_controlled_entity(player_id);
				w.m_pc_local_player_created = true;
			}
			return;
		}

		const bool is_server = w.authoritative();

		if (!is_server) {
			const auto current_local = w.local_controlled_entity();
			if (current_local.exists()) {
				id our_controller{};
				for (const auto& [ctrl_id, local_id] : controller_to_local_player) {
					if (local_id == current_local) {
						our_controller = ctrl_id;
						break;
					}
				}

				if (our_controller.exists() && !reg.try_component<player_controller>(our_controller)) {
					if (reg.exists(current_local)) {
						reg.remove(current_local);
					}
					w.set_local_controlled_entity({});
					processed.erase(our_controller);
					controller_to_local_player.erase(our_controller);
				}
			}
		}

		for (auto& pc : reg.components<player_controller>()) {
			const auto controller_id = pc.owner_id();

			if (is_server) {
				if (pc.controlled_entity_id.exists()) {
					continue;
				}

				const auto player_id = factory(*current, std::nullopt);
				pc.controlled_entity_id = player_id;
				reg.mark_component_updated<player_controller>(controller_id);
			}
			else {
				if (!pc.controlled_entity_id.exists()) {
					continue;
				}

				if (processed.contains(controller_id)) {
					continue;
				}

				const auto our_controller = w.local_controller_id();
				if (!our_controller.exists() || controller_id != our_controller) {
					processed.insert(controller_id);
					continue;
				}

				const auto current_local = w.local_controlled_entity();
				if (current_local.exists() && reg.exists(current_local)) {
					processed.insert(controller_id);
					continue;
				}

				if (current_local.exists()) {
					for (auto it = controller_to_local_player.begin(); it != controller_to_local_player.end(); ) {
						if (it->second == current_local) {
							processed.erase(it->first);
							it = controller_to_local_player.erase(it);
						}
						else {
							++it;
						}
					}
					if (reg.exists(current_local)) {
						reg.remove(current_local);
					}
				}

				const auto local_player_id = factory(*current, pc.controlled_entity_id);
				w.set_local_controlled_entity(local_player_id);
				processed.insert(controller_id);
				controller_to_local_player[controller_id] = local_player_id;
			}
		}
	}
}

auto gse::world::update() -> void {
	if (m_networked) {
		auto& a = state_of<actions::system_state>();

		if (m_input_sampler) {
			m_input_sampler(*this, [&](const evaluation_context& ctx) {
				if (!ctx.input || !ctx.client_id) {
					return;
				}
				a.sample_for_entity(*ctx.input, *ctx.client_id);
			});
		}
	}
	else {
		const auto& a = state_of<actions::system_state>();
		const auto& s = a.current_state();

		a.sample_all_channels(s);

		for (const auto& [scene_id, condition] : m_triggers) {
			const evaluation_context ctx{
				.client_id = m_client_id,
				.input = std::addressof(s),
				.registry = &m_registry,
			};

			if (condition(ctx) && scene_id != m_active_scene) {
				if (m_active_scene.has_value()) {
					if (auto* old_scene = scene(m_active_scene.value())) {
						old_scene->set_active(false);
					}
				}

				if (auto* new_scene = scene(scene_id)) {
					new_scene->set_active(true);
					m_active_scene = new_scene->id();
					break;
				}
			}
		}
	}

	world_update_player_controllers(*this);
}

auto gse::world::render() -> void {}

auto gse::world::shutdown() -> void {
	for (const auto& scene : m_scenes | std::views::values) {
		if (scene->active()) {
			scene->set_active(false);
		}
	}
	m_scenes.clear();
	m_active_scene.reset();


}

template <typename State>
auto gse::world::state_of() -> State& {
	assert(
		m_scheduler != nullptr,
		std::source_location::current(),
		"world has no scheduler bound."
	);

	return m_scheduler->state<State>();
}

auto gse::world::direct() -> director {
	m_triggers.clear();
	m_active_scene.reset();
	m_scenes.clear();

	return director(this);
}

auto gse::world::add(std::string_view name, scene::setup_fn setup) -> gse::scene* {
	auto new_scene = std::make_unique<gse::scene>(m_registry, name);
	if (setup) {
		new_scene->set_setup(std::move(setup));
	}

	auto* scene_ptr = new_scene.get();
	m_scenes[scene_ptr->id()] = std::move(new_scene);
	return scene_ptr;
}

auto gse::world::activate(const gse::id& scene_id) -> void {
	assert(
		m_networked,
		std::source_location::current(),
		"Cannot force activate scene in a non-networked world."
	);

	if (m_active_scene.has_value() && m_active_scene.value() == scene_id) {
		if (auto* old_scene = scene(m_active_scene.value())) {
			old_scene->set_active(false);
		}
	}

	if (auto* new_scene = scene(scene_id)) {
		new_scene->set_active(true);
		m_active_scene = new_scene->id();
	}
}

auto gse::world::deactivate(const gse::id& scene_id) -> void {
	assert(
		scene_id == m_active_scene,
		std::source_location::current(),
		"Can only force deactivate the currently active scene."
	);

	assert(
		m_networked,
		std::source_location::current(),
		"Cannot force deactivate a scene in a non-networked world"
	);

	if (m_active_scene.has_value()) {
		if (auto* old_scene = scene(m_active_scene.value())) {
			old_scene->set_active(false);
		}
	}

	m_active_scene = std::nullopt;
}

auto gse::world::scene(const gse::id& scene_id) -> gse::scene* {
	if (const auto scene = m_scenes.find(scene_id); scene != m_scenes.end()) {
		return scene->second.get();
	}
	return nullptr;
}

auto gse::world::current_scene() -> gse::scene* {
	if (const auto scene = m_active_scene; scene.has_value()) {
		return this->scene(scene.value());
	}
	return nullptr;
}

auto gse::world::set_networked(const bool is_networked) -> void {
	m_networked = is_networked;
}

auto gse::world::triggers() const -> std::span<const trigger> {
	return m_triggers;
}

auto gse::world::networked() const -> bool {
	return m_networked;
}

auto gse::world::set_authoritative(const bool is_authoritative) -> void {
	m_authoritative = is_authoritative;
}

auto gse::world::authoritative() const -> bool {
	return m_authoritative;
}

auto gse::world::set_local_controlled_entity(const gse::id entity_id) -> void {
	m_local_controlled_entity = entity_id;
}

auto gse::world::local_controlled_entity() const -> gse::id {
	return m_local_controlled_entity;
}

auto gse::world::set_local_controller_id(const gse::id controller_id) -> void {
	m_local_controller_id = controller_id;
}

auto gse::world::local_controller_id() const -> gse::id {
	return m_local_controller_id;
}

auto gse::world::registry() -> gse::registry& {
	return m_registry;
}

auto gse::world::add_trigger(const trigger& new_trigger) -> void {
	m_triggers.push_back(new_trigger);
}
