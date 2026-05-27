export module gse.runtime:world_system;

import std;

import gse.assert;
import gse.concurrency;
import gse.core;
import gse.ecs;
import gse.log;
import gse.network;
import gse.os;

import :scene;

export namespace gse {
	struct evaluation_context {
		std::optional<id> client_id = std::nullopt;
		const actions::state* input = nullptr;
		const actions::system::data* actions_sys = nullptr;
		registry* registry = nullptr;
	};

	struct trigger {
		id scene_id;
		bool (
			*condition
		)(
			const evaluation_context&
		) = nullptr;
	};

	struct world_system {
		struct data {
			std::unordered_map<id, std::unique_ptr<scene>> scenes;
			std::vector<trigger> triggers;
			std::optional<id> active_scene;
			bool networked = false;
			bool authoritative = true;
			std::optional<id> client_id;
			id local_controlled_entity{};
			id local_controller_id{};

			std::unordered_set<id> pc_processed;
			std::unordered_map<id, id> pc_controller_to_local_player;
			bool pc_local_player_created = false;
		};

		static auto run(
			run_context& ctx,
			data& d,
			const actions::system::data& actions_d
		) -> async::task<>;

		static auto shutdown(
			shutdown_context& phase,
			data& d
		) -> void;
	};

	auto add_scene(
		world_system::data& d,
		registry& reg,
		std::string_view name,
		scene::setup_fn setup = {}
	) -> scene*;

	auto find_scene(
		world_system::data& d,
		const id& scene_id
	) -> scene*;

	auto current_scene(
		world_system::data& d
	) -> scene*;

	auto activate_scene(
		world_system::data& d,
		const id& scene_id
	) -> void;

	auto deactivate_active_scene(
		world_system::data& d
	) -> void;

	class director {
	public:
		explicit director(
			world_system::data* state = nullptr
		);

		auto when(
			const trigger& trigger
		) -> director&;

	private:
		world_system::data* m_state = nullptr;
	};
}

namespace gse {
	auto update_player_controllers(
		world_system::data& d,
		registry& reg
	) -> void;
}

gse::director::director(world_system::data* state) : m_state(state) {
}

auto gse::director::when(const trigger& trigger) -> director& {
	m_state->triggers.push_back(trigger);
	return *this;
}

auto gse::add_scene(world_system::data& d, registry& reg, std::string_view name, scene::setup_fn setup) -> scene* {
	auto new_scene = std::make_unique<gse::scene>(reg, name);
	if (setup) {
		new_scene->set_setup(setup);
	}
	auto* scene_ptr = new_scene.get();
	d.scenes[scene_ptr->id()] = std::move(new_scene);
	return scene_ptr;
}

auto gse::find_scene(world_system::data& d, const id& scene_id) -> scene* {
	if (const auto it = d.scenes.find(scene_id); it != d.scenes.end()) {
		return it->second.get();
	}
	return nullptr;
}

auto gse::current_scene(world_system::data& d) -> scene* {
	if (d.active_scene.has_value()) {
		return find_scene(d, d.active_scene.value());
	}
	return nullptr;
}

auto gse::activate_scene(world_system::data& d, const id& scene_id) -> void {
	if (d.active_scene.has_value()) {
		if (auto* old_scene = find_scene(d, d.active_scene.value())) {
			old_scene->set_active(false);
		}
	}
	if (auto* new_scene = find_scene(d, scene_id)) {
		new_scene->set_active(true);
		d.active_scene = new_scene->id();
	}
}

auto gse::deactivate_active_scene(world_system::data& d) -> void {
	if (d.active_scene.has_value()) {
		if (auto* old_scene = find_scene(d, d.active_scene.value())) {
			old_scene->set_active(false);
		}
	}
	d.active_scene = std::nullopt;
}

auto gse::update_player_controllers(world_system::data& d, registry& reg) -> void {
	auto* current = current_scene(d);
	if (!current) {
		return;
	}

	const auto& factory = current->player_factory();
	if (!factory) {
		return;
	}

	if (!d.networked) {
		if (!d.pc_local_player_created) {
			const auto player_id = factory(*current, std::nullopt);
			d.local_controlled_entity = player_id;
			d.pc_local_player_created = true;
		}
		return;
	}

	const bool is_server = d.authoritative;

	if (!is_server) {
		const auto current_local = d.local_controlled_entity;
		if (current_local.exists()) {
			id our_controller{};
			for (const auto& [ctrl_id, local_id] : d.pc_controller_to_local_player) {
				if (local_id == current_local) {
					our_controller = ctrl_id;
					break;
				}
			}

			if (our_controller.exists() && !reg.try_component<player_controller>(our_controller)) {
				if (reg.exists(current_local)) {
					reg.remove(current_local);
				}
				d.local_controlled_entity = {};
				d.pc_processed.erase(our_controller);
				d.pc_controller_to_local_player.erase(our_controller);
			}
		}
	}

	const auto pc_components = reg.components<player_controller>();
	const auto pc_ids = reg.owner_ids<player_controller>();
	for (std::size_t i = 0; i < pc_components.size(); ++i) {
		auto& pc = pc_components[i];
		const auto controller_id = pc_ids[i];

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

			if (d.pc_processed.contains(controller_id)) {
				continue;
			}

			const auto our_controller = d.local_controller_id;
			if (!our_controller.exists() || controller_id != our_controller) {
				d.pc_processed.insert(controller_id);
				continue;
			}

			const auto current_local = d.local_controlled_entity;
			if (current_local.exists() && reg.exists(current_local)) {
				d.pc_processed.insert(controller_id);
				continue;
			}

			if (current_local.exists()) {
				for (auto it = d.pc_controller_to_local_player.begin(); it != d.pc_controller_to_local_player.end();) {
					if (it->second == current_local) {
						d.pc_processed.erase(it->first);
						it = d.pc_controller_to_local_player.erase(it);
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
			d.local_controlled_entity = local_player_id;
			d.pc_processed.insert(controller_id);
			d.pc_controller_to_local_player[controller_id] = local_player_id;
		}
	}
}

auto gse::world_system::run(run_context& ctx, data& d, const actions::system::data& actions_d) -> async::task<> {
	while (true) {
		for (const auto& r : ctx.read_channel<set_networked_request>()) {
			d.networked = r.value;
		}
		for (const auto& r : ctx.read_channel<set_authoritative_request>()) {
			d.authoritative = r.value;
		}
		for (const auto& r : ctx.read_channel<set_local_controller_id_request>()) {
			d.local_controller_id = r.controller_id;
		}
		if (!ctx.read_channel<deactivate_active_scene_request>().empty()) {
			deactivate_active_scene(d);
		}
		for (const auto& r : ctx.read_channel<activate_scene_request>()) {
			activate_scene(d, r.scene_id);
		}

		if (!d.networked) {
			const auto& s = actions::system::current_state(actions_d);

			for (const auto& [scene_id, condition] : d.triggers) {
				const evaluation_context ec{
					.client_id = d.client_id,
					.input = std::addressof(s),
					.actions_sys = &actions_d,
					.registry = &ctx.registry(),
				};

				if (condition(ec) && scene_id != d.active_scene) {
					if (d.active_scene.has_value()) {
						if (auto* old_scene = find_scene(d, d.active_scene.value())) {
							old_scene->set_active(false);
						}
					}

					if (auto* new_scene = find_scene(d, scene_id)) {
						new_scene->set_active(true);
						d.active_scene = new_scene->id();
						break;
					}
				}
			}
		}

		update_player_controllers(d, ctx.registry());

		co_await ctx.next_tick();
	}
}

auto gse::world_system::shutdown(shutdown_context&, data& d) -> void {
	for (const auto& s : std::views::values(d.scenes)) {
		if (s->active()) {
			s->set_active(false);
		}
	}
	d.scenes.clear();
	d.active_scene.reset();
}
