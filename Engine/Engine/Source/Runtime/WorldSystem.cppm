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
	};

	struct trigger {
		id scene_id;
		bool (
			*condition
		)(
			const evaluation_context&
		) = nullptr;
	};
}

export namespace gse::world_system {
	struct spawn_player_request {
		id entity;
	};

	struct possess_player_request {
		id entity;
	};

	struct scene_catalog {
		std::vector<id> scene_ids;
		std::vector<trigger> triggers;
		scene* active_scene = nullptr;
	};

	struct [[= system_state<"World">{}]] data {
		std::unordered_map<id, std::unique_ptr<scene>> scenes;
		[[= shared]] std::vector<id> scene_ids;
		[[= shared]] std::vector<trigger> triggers;
		[[= shared]] std::optional<id> active_scene;
		[[= shared]] scene* active_scene_ptr = nullptr;
		bool networked = false;
		bool authoritative = true;
		std::optional<id> client_id;
		id local_controlled_entity{};
		id local_controller_id{};

		std::uint32_t next_player = 0;
		bool local_player_spawned = false;

		bool catalog_published = false;
		std::size_t published_scene_count = 0;
		std::size_t published_trigger_count = 0;
		scene* published_active_scene = nullptr;
	};

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		channel_read<set_networked_request, set_authoritative_request, set_local_controller_id_request, deactivate_active_scene_request, activate_scene_request> requests_in,
		channel_write<spawn_player_request, possess_player_request, scene_catalog> player_out,
		shared_view<actions::data> actions_d,
		write<player_controller> controllers,
		entities ents
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}

export namespace gse {
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
		write<player_controller>& controllers,
		const entities& ents,
		channel_write<world_system::spawn_player_request, world_system::possess_player_request> player_out
	) -> void;
}

gse::director::director(world_system::data* state) : m_state(state) {
}

auto gse::director::when(const trigger& trigger) -> director& {
	m_state->triggers.push_back(trigger);
	return *this;
}

auto gse::add_scene(world_system::data& d, registry& reg, std::string_view name, scene::setup_fn setup) -> scene* {
	auto new_scene = std::make_unique<scene>(reg, name);
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

auto gse::update_player_controllers(world_system::data& d, write<player_controller>& controllers, const entities& ents, const channel_write<world_system::spawn_player_request, world_system::possess_player_request> player_out) -> void {
	const auto spawn = [&] {
		const auto player_id = generate_id(std::format("Player_{}", d.next_player++));
		ents.ensure_active(player_id);
		player_out.push<world_system::spawn_player_request>({
			.entity = player_id,
		});
		return player_id;
	};

	if (!d.networked) {
		if (!d.local_player_spawned) {
			d.local_controlled_entity = spawn();
			d.local_player_spawned = true;
			player_out.push<world_system::possess_player_request>({
				.entity = d.local_controlled_entity,
			});
		}
		return;
	}

	if (d.authoritative) {
		const auto owners = controllers.owner_ids();
		for (std::size_t i = 0; i < controllers.size(); ++i) {
			if (auto& pc = controllers[i]; !pc.controlled_entity_id.exists()) {
				pc.controlled_entity_id = spawn();
				controllers.mark_updated(owners[i]);
			}
		}
		return;
	}

	const auto* mine = d.local_controller_id.exists() ? controllers.find(d.local_controller_id) : nullptr;
	const auto target = mine ? mine->controlled_entity_id : id{};

	if (d.local_controlled_entity.exists() && d.local_controlled_entity != target) {
		if (ents.exists(d.local_controlled_entity)) {
			ents.remove(d.local_controlled_entity);
		}
		d.local_controlled_entity = {};
	}

	if (target.exists() && !d.local_controlled_entity.exists() && ents.exists(target)) {
		d.local_controlled_entity = target;
		player_out.push<world_system::possess_player_request>({
			.entity = target,
		});
	}
}

auto gse::world_system::run(context& ctx, data& d, const channel_read<set_networked_request, set_authoritative_request, set_local_controller_id_request, deactivate_active_scene_request, activate_scene_request> requests_in, const channel_write<spawn_player_request, possess_player_request, scene_catalog> player_out, const shared_view<actions::data> actions_d, write<player_controller> controllers, entities ents) -> async::task<> {
	for (const auto& r : requests_in.of<set_networked_request>()) {
		d.networked = r.value;
	}
	for (const auto& r : requests_in.of<set_authoritative_request>()) {
		d.authoritative = r.value;
	}
	for (const auto& r : requests_in.of<set_local_controller_id_request>()) {
		d.local_controller_id = r.controller_id;
	}
	if (!requests_in.of<deactivate_active_scene_request>().empty()) {
		deactivate_active_scene(d);
	}
	for (const auto& r : requests_in.of<activate_scene_request>()) {
		activate_scene(d, r.scene_id);
	}

	d.scene_ids.clear();
	for (const auto& key : std::views::keys(d.scenes)) {
		d.scene_ids.push_back(key);
	}

	if (!d.networked) {
		const auto& s = actions::current_state(actions_d);

		for (const auto& [scene_id, condition] : d.triggers) {
			const evaluation_context ec{
				.client_id = d.client_id,
				.input = std::addressof(s),
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

	update_player_controllers(d, controllers, ents, player_out);

	d.active_scene_ptr = current_scene(d);

	if (!d.catalog_published
		|| d.published_scene_count != d.scene_ids.size()
		|| d.published_trigger_count != d.triggers.size()
		|| d.published_active_scene != d.active_scene_ptr) {
		d.catalog_published = true;
		d.published_scene_count = d.scene_ids.size();
		d.published_trigger_count = d.triggers.size();
		d.published_active_scene = d.active_scene_ptr;
		player_out.push<scene_catalog>({
			.scene_ids = d.scene_ids,
			.triggers = d.triggers,
			.active_scene = d.active_scene_ptr,
		});
	}

	return {};
}

auto gse::world_system::shutdown(data& d) -> void {
	for (const auto& s : std::views::values(d.scenes)) {
		if (s->active()) {
			s->set_active(false);
		}
	}
	d.scenes.clear();
	d.active_scene.reset();
}