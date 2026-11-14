module gse.graphics:geometry_collector_impl;

import gse.assets;
import gse.concurrency;
import gse.containers;
import gse.core;
import gse.diag;
import gse.ecs;
import gse.gpu;
import gse.math;
import gse.meta;
import gse.os;
import gse.physics;
import gse.time;
import std;

import :animation_components;
import :camera_system;
import :geometry_collector;
import :material;
import :mesh;
import :model;
import :primitive_resolver;
import :render_component;
import :shared_shaders;
import :skin_renderer;
import :skinned_model;
import :texture;


namespace gse::renderer::geometry_collector {
	auto material_palette_index(
		render_data& data,
		const material* mat_ptr
	) -> std::uint32_t;

	constexpr std::size_t collect_chunk_size = 2048;

	auto entry_bounds(
		const owned_render_queue_entry& q
	) -> std::pair<vec3<length>, vec3<length>>;

	auto group_key_of(
		const owned_render_queue_entry& q
	) -> queue_group_key;

	auto append_entry(
		collect_chunk& chunk,
		owned_render_queue_entry q
	) -> void;

	auto reset_chunk(
		collect_chunk& chunk
	) -> void;

	auto pinned_model(
		std::flat_map<id, std::shared_ptr<const model>>& resolved,
		const resource::handle<model>& handle
	) -> const model*;

	auto collect_static(
		write<render_component>& render,
		read<physics::transform_component>& transform,
		read<physics::motion_component>& motion,
		const std::flat_map<id, std::uint32_t>& body_index_map,
		std::uint32_t gpu_body_count,
		const physics::interpolation_state& interpolation,
		data& d
	) -> void;

	auto collect_skinned(
		read<skeleton_instance_component>& skeletons,
		const skin::skinned_bounds_map& bounds,
		collect_chunk& out
	) -> void;

	auto plan_batches(
		render_data& data,
		std::span<collect_chunk> chunks,
		collect_list collect_chunk::* list,
		std::flat_map<queue_group_key, queue_locator>& batch_groups,
		std::uint32_t& instance_offset,
		std::uint32_t& mapping_offset,
		std::inplace_vector<normal_instance_batch, render_data::max_batches>& batches
	) -> void;

	auto scatter_instances(
		render_data& data,
		std::span<collect_chunk> chunks
	) -> void;

	auto initialize(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	auto tick(
		context& ctx,
		data& d,
		channel_read<physics::interpolation_state> physics_in,
		shared_view<physics::data> phys_s,
		channel_write<render_data> geometry_out,
		shared_view<camera::data> cam_state,
		shared_view<skin::data> skin_s,
		write<render_component>& render,
		read<physics::transform_component>& transform,
		read<physics::motion_component>& motion,
		read<skeleton_instance_component>& skeletons
	) -> async::task<>;
}

auto gse::renderer::geometry_collector::material_palette_index(render_data& data, const material* mat_ptr) -> std::uint32_t {
	if (auto it = data.material_palette_map.find(mat_ptr); it != data.material_palette_map.end()) {
		return it->second;
	}
	const auto index = static_cast<std::uint32_t>(data.material_palette_map.size());
	data.material_palette_map.insert({ mat_ptr, index });
	return index;
}

auto gse::renderer::geometry_collector::entry_bounds(const owned_render_queue_entry& q) -> std::pair<vec3<length>, vec3<length>> {
	if (q.skinned.valid()) {
		return { vec3<length>(q.skinned_bounds.min), vec3<length>(q.skinned_bounds.max) };
	}

	if (q.body_index != owned_render_queue_entry::invalid_body_index) {
		constexpr float physics_cull_extent = 1.0e9f;
		return { vec3<length>(meters(-physics_cull_extent)), vec3<length>(meters(physics_cull_extent)) };
	}

	const auto [local_min, local_max] = q.model_snapshot->meshes()[q.entry.index].aabb();
	return transform_aabb(local_min, local_max, q.entry.model_matrix);
}

auto gse::renderer::geometry_collector::group_key_of(const owned_render_queue_entry& q) -> queue_group_key {
	return {
		.model = q.entry.model,
		.skinned = q.skinned.id(),
		.skinned_owner = q.skinned.valid() ? q.owner : id{},
		.mesh_index = q.entry.index,
		.model_snapshot = q.model_snapshot,
		.skinned_snapshot = q.skinned_snapshot.get(),
	};
}

auto gse::renderer::geometry_collector::append_entry(collect_chunk& chunk, owned_render_queue_entry q) -> void {
	auto& list = q.entry.color.w() < 1.0f ? chunk.transparent : chunk.opaque;
	const auto [entry_min, entry_max] = entry_bounds(q);
	auto [it, inserted] = list.groups.try_emplace(
		group_key_of(q),
		collect_group{
			.first_index = static_cast<std::uint32_t>(list.entries.size()),
			.world_aabb_min = entry_min,
			.world_aabb_max = entry_max,
		}
	);
	auto& group = it->second;
	if (!inserted) {
		group.world_aabb_min = min(group.world_aabb_min, entry_min);
		group.world_aabb_max = max(group.world_aabb_max, entry_max);
	}
	++group.count;
	if (q.body_index != owned_render_queue_entry::invalid_body_index) {
		++group.gpu_count;
	}
	list.entries.push_back(std::move(q));
}

auto gse::renderer::geometry_collector::reset_chunk(collect_chunk& chunk) -> void {
	chunk.opaque.entries.clear();
	chunk.opaque.groups.clear();
	chunk.transparent.entries.clear();
	chunk.transparent.groups.clear();
	chunk.resolved.clear();
}

auto gse::renderer::geometry_collector::pinned_model(std::flat_map<id, std::shared_ptr<const model>>& resolved, const resource::handle<model>& handle) -> const model* {
	const auto handle_id = handle.id();
	if (const auto it = resolved.find(handle_id); it != resolved.end()) {
		return it->second.get();
	}
	if (!handle.valid()) {
		return nullptr;
	}
	return resolved.try_emplace(handle_id, handle.resolve()).first->second.get();
}

auto gse::renderer::geometry_collector::collect_static(write<render_component>& render, read<physics::transform_component>& transform, read<physics::motion_component>& motion, const std::flat_map<id, std::uint32_t>& body_index_map, const std::uint32_t gpu_body_count, const physics::interpolation_state& interpolation, data& d) -> void {
	const auto render_size = render.size();
	const auto render_ids = render.owner_ids();
	const bool transform_order_matches =
		render_size == transform.size() && std::ranges::equal(render_ids, transform.owner_ids());

	std::swap(d.previous_matrices, d.current_matrices);
	const auto& previous = d.previous_matrices;
	auto& current = d.current_matrices;
	current.owners.assign(render_size, id{});
	current.offsets.resize(render_size + 1);
	current.offsets[0] = 0;
	for (std::size_t i = 0; i < render_size; ++i) {
		current.offsets[i + 1] = current.offsets[i] + render[i].model_count;
	}
	current.matrices.resize(current.offsets[render_size]);

	const std::size_t chunk_count = (render_size + collect_chunk_size - 1) / collect_chunk_size;
	d.collect_chunks.resize(chunk_count + 1);

	task::coarse_parallel(
		chunk_count,
		1,
		[&](const std::size_t c) {
			auto& chunk = d.collect_chunks[c];
			reset_chunk(chunk);

			const auto begin = c * collect_chunk_size;
			const auto end = std::min(render_size, begin + collect_chunk_size);
			for (std::size_t i = begin; i < end; ++i) {
				auto& component = render[i];
				if (!component.render) {
					continue;
				}

				const auto eid = render_ids[i];
				const auto* tc = transform_order_matches ? std::addressof(transform[i]) : transform.find(eid);
				if (tc == nullptr) {
					continue;
				}

				std::uint32_t body_index = owned_render_queue_entry::invalid_body_index;
				if (const auto it = body_index_map.find(eid); it != body_index_map.end() && it->second < gpu_body_count) {
					body_index = it->second;
				}
				const bool gpu_driven = body_index != owned_render_queue_entry::invalid_body_index;

				const auto render_tc = gpu_driven ? *tc : physics::render_transform(*tc, motion.find(eid), interpolation);

				const bool has_history = i < previous.owners.size() && previous.owners[i] == eid;
				const auto history_first = has_history ? previous.offsets[i] : 0;
				const auto history_count = has_history ? previous.offsets[i + 1] - history_first : 0;
				current.owners[i] = eid;

				for (std::uint32_t j = 0; j < component.model_count; ++j) {
					auto& stored = current.matrices[current.offsets[i] + j];
					const auto* mdl = pinned_model(chunk.resolved, component.models[j]);
					if (mdl == nullptr) {
						stored.reset();
						continue;
					}
					spatial_matrix model_matrix(mat4f(1.f));
					spatial_matrix normal_matrix(mat4f(1.f));
					spatial_matrix prev_model_matrix(mat4f(1.f));
					if (gpu_driven) {
						stored.reset();
					}
					else {
						std::tie(model_matrix, normal_matrix) = compute_render_transform(render_tc, mdl->render_pivot(), component.sizes[j]);
						prev_model_matrix = j < history_count
							? previous.matrices[history_first + j].value_or(model_matrix)
							: model_matrix;
						stored = model_matrix;
					}
					for (std::size_t mi = 0; mi < mdl->meshes().size(); ++mi) {
						append_entry(chunk, {
							.entry = {
								.model = component.models[j].id(),
								.index = mi,
								.model_matrix = model_matrix,
								.normal_matrix = normal_matrix,
								.prev_model_matrix = prev_model_matrix,
								.color = component.tints[j]
							},
							.owner = eid,
							.body_index = body_index,
							.scale = component.sizes[j],
							.model_snapshot = mdl
						});
					}
				}
			}
		},
		trace_id<"geom_collect::collect_chunk">()
	);
}

auto gse::renderer::geometry_collector::collect_skinned(read<skeleton_instance_component>& skeletons, const skin::skinned_bounds_map& bounds, collect_chunk& out) -> void {
	const auto owners = skeletons.owner_ids();

	for (std::size_t i = 0; i < skeletons.size(); ++i) {
		const auto& skeleton = skeletons[i];
		if (!skeleton.model.valid()) {
			continue;
		}

		const auto mdl = skeleton.model.resolve();
		if (!mdl->uploads_ready()) {
			continue;
		}

		constexpr float unbounded_extent = 1.0e9f;
		const auto bounds_it = bounds.find(owners[i]);
		const auto owner_bounds = bounds_it != bounds.end()
			? bounds_it->second
			: aabb{
			.max = vec3<position>(meters(unbounded_extent)),
			.min = vec3<position>(meters(-unbounded_extent)),
		};

		for (std::size_t mi = 0; mi < mdl->meshes().size(); ++mi) {
			append_entry(out, {
				.entry = {
					.index = mi,
					.model_matrix = spatial_matrix(mat4f(1.f)),
					.normal_matrix = spatial_matrix(mat4f(1.f)),
					.prev_model_matrix = spatial_matrix(mat4f(1.f)),
					.color = vec4f(1.f)
				},
				.owner = owners[i],
				.skinned = skeleton.model,
				.skinned_snapshot = mdl,
				.skinned_bounds = owner_bounds
			});
		}
	}
}

auto gse::renderer::geometry_collector::plan_batches(render_data& data, const std::span<collect_chunk> chunks, collect_list collect_chunk::* list, std::flat_map<queue_group_key, queue_locator>& batch_groups, std::uint32_t& instance_offset, std::uint32_t& mapping_offset, std::inplace_vector<normal_instance_batch, render_data::max_batches>& batches) -> void {
	batch_groups.clear();
	for (std::uint32_t c = 0; c < chunks.size(); ++c) {
		for (const auto& [key, group] : (chunks[c].*list).groups) {
			batch_groups.try_emplace(key, queue_locator{ .chunk = c, .index = group.first_index });
		}
	}

	for (const auto& [key, first] : batch_groups) {
		const auto& q = (chunks[first.chunk].*list).entries[first.index];
		normal_batch_key batch_key{
			.model = q.entry.model,
			.skinned = q.skinned,
			.model_snapshot = q.model_snapshot,
			.skinned_snapshot = q.skinned_snapshot,
			.skinned_owner = q.skinned.valid() ? q.owner : id{},
			.mesh_index = q.entry.index
		};
		const auto material_index = material_palette_index(data, &batch_key.resolve_mesh().material());
		const auto& first_group = (chunks[first.chunk].*list).groups.find(key)->second;

		normal_instance_batch batch{
			.key = std::move(batch_key),
			.first_instance = instance_offset,
			.instance_count = 0,
			.world_aabb_min = first_group.world_aabb_min,
			.world_aabb_max = first_group.world_aabb_max
		};

		for (std::uint32_t c = first.chunk; c < chunks.size(); ++c) {
			auto& groups = (chunks[c].*list).groups;
			const auto it = groups.find(key);
			if (it == groups.end()) {
				continue;
			}
			auto& group = it->second;
			group.material_index = material_index;
			group.instance_cursor = instance_offset;
			group.mapping_cursor = mapping_offset;
			instance_offset += group.count;
			mapping_offset += group.gpu_count;
			batch.world_aabb_min = min(batch.world_aabb_min, group.world_aabb_min);
			batch.world_aabb_max = max(batch.world_aabb_max, group.world_aabb_max);
		}

		batch.instance_count = instance_offset - batch.first_instance;
		batches.push_back(std::move(batch));
	}
}

auto gse::renderer::geometry_collector::scatter_instances(render_data& data, const std::span<collect_chunk> chunks) -> void {
	task::coarse_parallel(
		chunks.size(),
		1,
		[&](const std::size_t c) {
			for (collect_list* list : { &chunks[c].opaque, &chunks[c].transparent }) {
				for (const auto& q : list->entries) {
					auto& group = list->groups.find(group_key_of(q))->second;
					const auto instance_index = group.instance_cursor++;
					data.instance_staging[instance_index] = {
						.model_matrix = q.entry.model_matrix,
						.normal_matrix = q.entry.normal_matrix,
						.prev_model_matrix = q.entry.prev_model_matrix,
						.material_index = group.material_index,
						.tint = q.entry.color,
					};
					if (q.body_index != owned_render_queue_entry::invalid_body_index) {
						data.physics_mappings[group.mapping_cursor++] = {
							.body_index = q.body_index,
							.instance_index = instance_index,
							.pivot = q.model_snapshot->render_pivot(),
							.scale = q.scale
						};
					}
				}
			}
		},
		trace_id<"geom_collect::scatter">()
	);
}

auto gse::renderer::geometry_collector::initialize(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	constexpr std::size_t material_buffer_size = data::max_materials * sizeof(shaders::forward::material_data);
	d.material_staging.reserve(material_buffer_size);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		constexpr std::size_t instance_buffer_size = data::max_instances * sizeof(shaders::common::instance_data);
		d.instance_buffer[i] = gpu_s.device->create_buffer(
			{
				.size = instance_buffer_size,
				.stride = sizeof(shaders::common::instance_data),
				.usage = { gpu::buffer_flag::storage, gpu::buffer_flag::transfer_src, gpu::buffer_flag::transfer_dst },
			},
			"gc_instance_buffer"
		);

		constexpr std::size_t normal_indirect_buffer_size =
			render_data::max_batches * sizeof(gpu::draw_mesh_tasks_indirect_command);
		d.normal_indirect_commands_buffer[i] = gpu_s.device->create_buffer(
			{
				.size = normal_indirect_buffer_size,
				.stride = sizeof(gpu::draw_mesh_tasks_indirect_command),
				.usage = { gpu::buffer_flag::indirect, gpu::buffer_flag::storage, gpu::buffer_flag::transfer_dst, gpu::buffer_flag::byte_address },
				.writable = true
			},
			"gc_indirect_commands"
		);

		d.transparent_indirect_commands_buffer[i] = gpu_s.device->create_buffer(
			{
				.size = normal_indirect_buffer_size,
				.stride = sizeof(gpu::draw_mesh_tasks_indirect_command),
				.usage = { gpu::buffer_flag::indirect, gpu::buffer_flag::storage, gpu::buffer_flag::transfer_dst },
			},
			"gc_transparent_indirect_commands"
		);

		d.material_palette_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = material_buffer_size,
				.stride = sizeof(shaders::forward::material_data),
				.usage = gpu::buffer_flag::storage,
			},
			"gc_material_palette"
		);
	}

	co_return;
}

auto gse::renderer::geometry_collector::tick(context& ctx, data& d, const channel_read<physics::interpolation_state> physics_in, const shared_view<physics::data> phys_s, const channel_write<render_data> geometry_out, const shared_view<camera::data> cam_state, const shared_view<skin::data> skin_s, write<render_component>& render, read<physics::transform_component>& transform, read<physics::motion_component>& motion, read<skeleton_instance_component>& skeletons) -> async::task<> {
	const view_matrix view_matrix = cam_state.view_matrix;
	const projection_matrix proj_matrix = cam_state.projection_matrix;

	if (render.empty()) {
		co_return;
	}

	render_data data;
	data.view = view_matrix;
	data.proj = proj_matrix;

	physics::interpolation_state interpolation;
	if (const auto& published = physics_in.of<physics::interpolation_state>(); !published.empty()) {
		interpolation = published[0];
	}

	{
		trace::scope_guard _{ trace_id<"geom_collect::collect">() };
		const std::uint32_t gpu_body_count = physics::gpu_solver_active(phys_s) ? phys_s.gpu_solver.body_count() : 0;
		collect_static(render, transform, motion, phys_s.id_to_body_index, gpu_body_count, interpolation, d);
		auto& skinned_chunk = d.collect_chunks.back();
		reset_chunk(skinned_chunk);
		collect_skinned(skeletons, skin_s.bounds, skinned_chunk);
	}

	for (const auto& chunk : d.collect_chunks) {
		std::ranges::copy(chunk.resolved.values(), std::back_inserter(data.model_pins));
	}

	std::uint32_t instance_count = 0;
	std::uint32_t mapping_count = 0;
	{
		trace::scope_guard _{ trace_id<"geom_collect::plan">() };
		plan_batches(data, d.collect_chunks, &collect_chunk::opaque, d.batch_groups, instance_count, mapping_count, data.normal_batches);
		plan_batches(data, d.collect_chunks, &collect_chunk::transparent, d.batch_groups, instance_count, mapping_count, data.transparent_batches);
		data.instance_staging.resize(instance_count);
		data.physics_mappings.resize(mapping_count);
	}

	scatter_instances(data, d.collect_chunks);

	for (auto& batch : data.normal_batches) {
		if (batch.key.skinned.valid()) {
			const auto slots = skin::deformed_slots_for(
				skin_s.targets,
				batch.key.skinned_owner,
				static_cast<std::uint32_t>(batch.key.mesh_index)
			);
			batch.deformed_vertices = slots.current;
			batch.prev_deformed_vertices = slots.previous;
		}
	}

	data.physics_mapping_count = mapping_count;

	geometry_out.push<render_data>(std::move(data));
}

auto gse::renderer::geometry_collector::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	co_await initialize(ctx, gpu_s, d);
	co_return;
}

auto gse::renderer::geometry_collector::run(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, data& d, const channel_read<physics::interpolation_state> physics_in, const shared_view<physics::data> phys_s, const channel_write<render_data> geometry_out, const shared_view<camera::data> cam_state, const shared_view<primitive_resolver::data> resolver_state, const shared_view<skin::data> skin_s, write<render_component> render, read<physics::transform_component> transform, read<physics::motion_component> motion, read<skeleton_instance_component> skeletons) -> async::task<> {
	co_await tick(ctx, d, physics_in, phys_s, geometry_out, cam_state, skin_s, render, transform, motion, skeletons);
	co_return;
}

auto gse::renderer::geometry_collector::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_read<render_data, material_reservation> geometry_in) -> async::task<> {
	const auto& items = geometry_in.of<render_data>();
	if (items.empty()) {
		return {};
	}

	const auto& data = items[0];
	const auto frame_index = gpu_s.render_graph->current_frame();

	if (!data.instance_staging.empty()) {
		if (data.instance_staging.size() > d.instance_capacity) {
			d.instance_capacity = data.instance_staging.size();
			for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
				d.instance_buffer[i] = gpu_s.device->create_buffer<shaders::common::instance_data>(
					d.instance_capacity,
					{
						.usage = { gpu::buffer_flag::storage, gpu::buffer_flag::transfer_src, gpu::buffer_flag::transfer_dst },
					},
					"gc_instance_buffer"
				);
			}
		}
		d.instance_buffer[frame_index].host_write(data.instance_staging);
	}

	if (!data.normal_batches.empty()) {
		std::inplace_vector<gpu::draw_mesh_tasks_indirect_command, render_data::max_batches> normal_indirect_commands;

		for (const auto& batch : data.normal_batches) {
			const auto& mesh = batch.key.resolve_mesh();
			const std::uint32_t task_groups = (mesh.meshlet_count() + 31) / 32;

			normal_indirect_commands.push_back({
				.group_count_x = task_groups,
				.group_count_y = batch.instance_count,
				.group_count_z = 1
			});
		}

		if (!normal_indirect_commands.empty()) {
			d.normal_indirect_commands_buffer[frame_index].host_write(normal_indirect_commands);
		}
	}

	if (!data.transparent_batches.empty()) {
		std::inplace_vector<gpu::draw_mesh_tasks_indirect_command, render_data::max_batches> transparent_indirect_commands;

		for (const auto& batch : data.transparent_batches) {
			const auto& mesh = batch.key.resolve_mesh();
			const std::uint32_t task_groups = (mesh.meshlet_count() + 31) / 32;

			transparent_indirect_commands.push_back({
				.group_count_x = task_groups,
				.group_count_y = batch.instance_count,
				.group_count_z = 1
			});
		}

		if (!transparent_indirect_commands.empty()) {
			d.transparent_indirect_commands_buffer[frame_index].host_write(transparent_indirect_commands);
		}
	}

	for (const auto& [owner, material] : geometry_in.of<material_reservation>()) {
		d.reserved_materials.insert_or_assign(owner, material);
	}
	assert(d.reserved_materials.size() <= data::max_reserved_materials, "geometry_collector: {} reserved materials exceed the limit of {}", d.reserved_materials.size(), data::max_reserved_materials);
	constexpr auto first_reserved = static_cast<std::uint32_t>(data::max_materials - data::max_reserved_materials);
	d.reserved_material_indices.clear();
	for (const auto& [owner, material] : d.reserved_materials) {
		const auto index = first_reserved + static_cast<std::uint32_t>(d.reserved_material_indices.size());
		d.reserved_material_indices.emplace(owner, index);
		d.material_palette_buffers[frame_index].host_write(material, index * sizeof(shaders::forward::material_data));
	}

	const auto material_count = std::min<std::size_t>(data.material_palette_map.size(), first_reserved);
	if (material_count > 0) {
		auto& mat_staging = d.material_staging;
		mat_staging.assign(
			material_count * sizeof(shaders::forward::material_data),
			std::byte{ 0 }
		);
		auto* staging_materials = reinterpret_cast<shaders::forward::material_data*>(mat_staging.data());

		for (const auto& [mat_ptr, idx] : data.material_palette_map) {
			if (idx >= first_reserved) {
				continue;
			}
			const auto& diffuse = mat_ptr->diffuse_texture;
			const auto diffuse_view = diffuse.valid() ? diffuse.resolve() : nullptr;
			const auto diffuse_slot = diffuse_view ? diffuse_view->bindless_slot() : gpu::bindless_slot{};
			staging_materials[idx] = {
				.base_color = mat_ptr->base_color,
				.roughness = mat_ptr->roughness,
				.metallic = mat_ptr->metallic,
				.diffuse_index = diffuse_slot.valid() ? diffuse_slot.index : shaders::bindless::invalid_index,
			};
		}

		d.material_palette_buffers[frame_index].host_write(
			mat_staging.data(),
			material_count * sizeof(shaders::forward::material_data)
		);
	}

	return {};
}