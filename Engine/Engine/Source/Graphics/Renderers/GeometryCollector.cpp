module gse.graphics;

import std;

import :geometry_collector;
import :camera_system;
import :mesh;
import :skinned_mesh;
import :model;
import :skinned_model;
import :render_component;
import :animation_component;
import :material;
import :primitive_resolver;
import :skeleton;
import :texture;

import gse.math;
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
import gse.meta;

import :shared_shaders;

namespace gse::renderer::geometry_collector {
	auto material_palette_index(
		render_data& data,
		const material* mat_ptr
	) -> std::uint32_t;

	auto write_instance(
		std::vector<shaders::common::instance_data>& instance_staging,
		const mat4f& model_matrix,
		const mat4f& normal_matrix,
		std::uint32_t skin_offset,
		std::uint32_t joint_count,
		std::uint32_t mat_idx,
		const vec3f& tint
	) -> void;

	template <typename Items, typename Batches, typename KeyOf, typename GetMesh, typename AccumAabb, typename OnInstance>
	auto build_batches(
		render_data& data,
		std::uint32_t& global_instance_offset,
		const Items& items,
		Batches& batches,
		KeyOf key_of,
		GetMesh get_mesh,
		AccumAabb accum_aabb,
		OnInstance on_instance
	) -> void;

	auto read_body_index_map(
		run_context& ctx
	) -> std::unordered_map<id, std::uint32_t>;

	auto collect_static(
		write<render_component>& render,
		read<physics::transform_component>& transform,
		const std::unordered_map<id, std::uint32_t>& body_index_map,
		std::vector<owned_render_queue_entry>& out
	) -> void;

	auto collect_skinned(
		write<render_component>& render,
		read<physics::transform_component>& transform,
		read<animation_component>& anim,
		system::data& d,
		std::vector<skinned_render_queue_entry>& skinned_out,
		std::vector<mat4f>& local_pose_staging
	) -> std::uint32_t;

	auto sort_queues(
		std::vector<owned_render_queue_entry>& out,
		std::vector<skinned_render_queue_entry>& skinned_out
	) -> void;

	auto build_normal_batches(
		render_data& data,
		std::uint32_t& global_instance_offset
	) -> void;

	auto build_skinned_batches(
		render_data& data,
		std::uint32_t& global_instance_offset
	) -> void;

	auto initialize(
		run_context& ctx,
		const gpu::context::data& gpu_s,
		system::data& d
	) -> async::task<>;

	auto tick(
		run_context& ctx,
		system::data& d,
		const camera::system::data& cam_state
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

auto gse::renderer::geometry_collector::write_instance(std::vector<shaders::common::instance_data>& instance_staging, const mat4f& model_matrix, const mat4f& normal_matrix, const std::uint32_t skin_offset, const std::uint32_t joint_count, const std::uint32_t mat_idx, const vec3f& tint) -> void {
	instance_staging.push_back({
		.model_matrix = model_matrix,
		.normal_matrix = normal_matrix,
		.skin_offset = skin_offset,
		.joint_count = joint_count,
		.material_index = mat_idx,
		.tint = tint,
	});
}

template <typename Items, typename Batches, typename KeyOf, typename GetMesh, typename AccumAabb, typename OnInstance>
auto gse::renderer::geometry_collector::build_batches(render_data& data, std::uint32_t& global_instance_offset, const Items& items, Batches& batches, KeyOf key_of, GetMesh get_mesh, AccumAabb accum_aabb, OnInstance on_instance) -> void {
	std::size_t batch_begin = 0;
	while (batch_begin < items.size()) {
		const auto key = key_of(items[batch_begin]);
		std::size_t batch_end = batch_begin + 1;
		while (batch_end < items.size() && key_of(items[batch_end]) == key) {
			++batch_end;
		}

		const auto& mesh = get_mesh(key);
		const auto instance_count = static_cast<std::uint32_t>(batch_end - batch_begin);
		const auto mat_idx = material_palette_index(data, &mesh.material());
		const auto batch = std::span(items.data() + batch_begin, batch_end - batch_begin);

		vec3 world_aabb_min(meters(std::numeric_limits<float>::max()));
		vec3 world_aabb_max(meters(std::numeric_limits<float>::lowest()));
		accum_aabb(world_aabb_min, world_aabb_max, batch, mesh);

		batches.push_back({ .key = key, .first_instance = global_instance_offset, .instance_count = instance_count, .world_aabb_min = world_aabb_min, .world_aabb_max = world_aabb_max });

		for (const auto& item : batch) {
			on_instance(item, key, mat_idx, global_instance_offset++);
		}

		batch_begin = batch_end;
	}
}

auto gse::renderer::geometry_collector::read_body_index_map(run_context& ctx) -> std::unordered_map<id, std::uint32_t> {
	std::unordered_map<id, std::uint32_t> body_index_map;
	for (const auto& [entries] : ctx.read_channel<physics::gpu_body_index_map>()) {
		for (const auto& [eid, idx] : entries) {
			body_index_map[eid] = idx;
		}
	}
	return body_index_map;
}

auto gse::renderer::geometry_collector::collect_static(write<render_component>& render, read<physics::transform_component>& transform, const std::unordered_map<id, std::uint32_t>& body_index_map, std::vector<owned_render_queue_entry>& out) -> void {
	const auto render_size = render.size();
	const auto render_ids = render.owner_ids();
	const bool transform_order_matches = render_size == transform.size() && std::ranges::equal(render_ids, transform.owner_ids());

	for (std::size_t i = 0; i < render_size; ++i) {
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
		if (const auto it = body_index_map.find(eid); it != body_index_map.end()) {
			body_index = it->second;
		}

		for (std::uint32_t j = 0; j < component.model_count; ++j) {
			if (!component.models[j].valid()) {
				continue;
			}
			const auto& mdl = component.models[j].resolve();
			const auto [model_matrix, normal_matrix] = compute_render_transform(*tc, mdl.center_of_mass(), component.sizes[j]);
			for (std::size_t mi = 0; mi < mdl.meshes().size(); ++mi) {
				out.push_back({ .entry = { .model = component.models[j], .index = mi, .model_matrix = model_matrix, .normal_matrix = normal_matrix, .color = component.tints[j] }, .owner = eid, .body_index = body_index });
			}
		}
	}
}

auto gse::renderer::geometry_collector::collect_skinned(write<render_component>& render, read<physics::transform_component>& transform, read<animation_component>& anim, system::data& d, std::vector<skinned_render_queue_entry>& skinned_out, std::vector<mat4f>& local_pose_staging) -> std::uint32_t {
	const auto render_size = render.size();
	const auto render_ids = render.owner_ids();
	const bool transform_order_matches = render_size == transform.size() && std::ranges::equal(render_ids, transform.owner_ids());
	std::uint32_t skinned_instance_count = 0;

	for (std::size_t i = 0; i < render_size; ++i) {
		auto& component = render[i];
		if (component.skinned_model_count == 0) {
			continue;
		}

		const auto eid = render_ids[i];
		const auto* tc = transform_order_matches ? std::addressof(transform[i]) : transform.find(eid);
		if (tc == nullptr) {
			continue;
		}

		const auto* anim_comp = anim.find(eid);
		if (anim_comp == nullptr || anim_comp->local_pose.empty()) {
			continue;
		}

		for (std::uint32_t j = 0; j < component.skinned_model_count; ++j) {
			const auto& handle = component.skinned_models[j];
			if (!handle.valid()) {
				continue;
			}
			const auto& mdl = handle.resolve();

			const std::uint32_t skin_offset = static_cast<std::uint32_t>(local_pose_staging.size());
			local_pose_staging.insert(local_pose_staging.end(), anim_comp->local_pose.begin(), anim_comp->local_pose.end());

			if (anim_comp->skeleton && (!d.current_skeleton.valid() || d.current_skeleton != anim_comp->skeleton)) {
				d.current_skeleton = anim_comp->skeleton;
				d.current_joint_count = static_cast<std::uint32_t>(anim_comp->skeleton->joint_count());
				system::upload_skeleton_data(d, *anim_comp->skeleton);
			}

			++skinned_instance_count;

			const auto [model_matrix, normal_matrix] = compute_render_transform(*tc, mdl.center_of_mass(), vec3<length>(meters(1.0f)));
			for (std::size_t mi = 0; mi < mdl.meshes().size(); ++mi) {
				skinned_out.push_back({ .model = handle, .index = mi, .model_matrix = model_matrix, .normal_matrix = normal_matrix, .color = component.tints[j], .skin_offset = skin_offset, .joint_count = d.current_joint_count });
			}
		}
	}

	return skinned_instance_count;
}

auto gse::renderer::geometry_collector::sort_queues(std::vector<owned_render_queue_entry>& out, std::vector<skinned_render_queue_entry>& skinned_out) -> void {
	trace::scope_guard sg{ trace_id<"geom_collect::sort">() };
	std::ranges::sort(
		out,
		[](const owned_render_queue_entry& a, const owned_render_queue_entry& b) {
			const auto ma = a.entry.model.id();
			const auto mb = b.entry.model.id();

			if (ma != mb) {
				return ma < mb;
			}

			return a.entry.index < b.entry.index;
		}
	);

	std::ranges::sort(
		skinned_out,
		[](const skinned_render_queue_entry& a, const skinned_render_queue_entry& b) {
			const auto ma = a.model.id();
			const auto mb = b.model.id();

			if (ma != mb) {
				return ma < mb;
			}

			return a.index < b.index;
		}
	);
}

auto gse::renderer::geometry_collector::build_normal_batches(render_data& data, std::uint32_t& global_instance_offset) -> void {
	trace::scope_guard sg{ trace_id<"geom_collect::batch_normal">() };
	build_batches(
		data,
		global_instance_offset,
		data.render_queue,
		data.normal_batches,
		[](const owned_render_queue_entry& q) {
			return normal_batch_key{ .model_ptr = &q.entry.model.resolve(), .mesh_index = q.entry.index };
		},
		[](const normal_batch_key& key) -> const mesh& {
			return key.model_ptr->meshes()[key.mesh_index];
		},
		[](vec3<length>& mn, vec3<length>& mx, std::span<const owned_render_queue_entry> batch, const mesh& m) {
			if (std::ranges::any_of(batch, [](const owned_render_queue_entry& q) {
					return q.body_index != owned_render_queue_entry::invalid_body_index;
				})) {
				constexpr float physics_cull_extent = 1.0e9f;
				mn = vec3<length>(meters(-physics_cull_extent));
				mx = vec3<length>(meters(physics_cull_extent));
				return;
			}

			const auto [local_min, local_max] = m.aabb();
			for (const auto& q : batch) {
				const auto [inst_min, inst_max] = transform_aabb(local_min, local_max, q.entry.model_matrix);
				mn = vec3<length>(std::min(mn.x(), inst_min.x()), std::min(mn.y(), inst_min.y()), std::min(mn.z(), inst_min.z()));
				mx = vec3<length>(std::max(mx.x(), inst_max.x()), std::max(mx.y(), inst_max.y()), std::max(mx.z(), inst_max.z()));
			}
		},
		[&](const owned_render_queue_entry& q, const normal_batch_key& key, std::uint32_t mat_idx, std::uint32_t instance_index) {
			const auto& entry = q.entry;
			write_instance(data.instance_staging, entry.model_matrix, entry.normal_matrix, 0, 0, mat_idx, entry.color);
			if (q.body_index != owned_render_queue_entry::invalid_body_index) {
				data.physics_mappings.push_back({ .body_index = q.body_index, .instance_index = instance_index, .center_of_mass = key.model_ptr->center_of_mass() });
			}
		}
	);
}

auto gse::renderer::geometry_collector::build_skinned_batches(render_data& data, std::uint32_t& global_instance_offset) -> void {
	trace::scope_guard sg{ trace_id<"geom_collect::batch_skinned">() };
	build_batches(
		data,
		global_instance_offset,
		data.skinned_render_queue,
		data.skinned_batches,
		[](const skinned_render_queue_entry& s) {
			return skinned_batch_key{ .model_ptr = &s.model.resolve(), .mesh_index = s.index };
		},
		[](const skinned_batch_key& key) -> const skinned_mesh& {
			return key.model_ptr->meshes()[key.mesh_index];
		},
		[](vec3<length>& mn, vec3<length>& mx, std::span<const skinned_render_queue_entry> batch, const skinned_mesh& m) {
			const auto [local_min, local_max] = m.aabb();
			for (const auto& s : batch) {
				const auto [inst_min, inst_max] = transform_aabb(local_min, local_max, s.model_matrix);
				mn = vec3<length>(std::min(mn.x(), inst_min.x()), std::min(mn.y(), inst_min.y()), std::min(mn.z(), inst_min.z()));
				mx = vec3<length>(std::max(mx.x(), inst_max.x()), std::max(mx.y(), inst_max.y()), std::max(mx.z(), inst_max.z()));
			}
		},
		[&](const skinned_render_queue_entry& s, const skinned_batch_key&, std::uint32_t mat_idx, std::uint32_t) {
			write_instance(data.instance_staging, s.model_matrix, s.normal_matrix, s.skin_offset, s.joint_count, mat_idx, s.color);
		}
	);
}

auto gse::renderer::geometry_collector::initialize(run_context& ctx, const gpu::context::data& gpu_s, system::data& d) -> async::task<> {
	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		constexpr std::size_t skin_buffer_size = system::data::max_skin_matrices * sizeof(mat4f);
		d.skin_buffer[i] = gpu::buffer::create(gpu_s.device->allocator(), { .size = skin_buffer_size, .usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst });
		constexpr std::size_t instance_buffer_size = system::data::max_instances * 2 * sizeof(shaders::common::instance_data);
		d.instance_buffer[i] = gpu::buffer::create(gpu_s.device->allocator(), { .size = instance_buffer_size, .usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_src | gpu::buffer_flag::transfer_dst });

		constexpr std::size_t normal_indirect_buffer_size = render_data::max_batches * sizeof(gpu::draw_mesh_tasks_indirect_command);
		d.normal_indirect_commands_buffer[i] = gpu::buffer::create(gpu_s.device->allocator(), { .size = normal_indirect_buffer_size, .usage = gpu::buffer_flag::indirect | gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst });
		constexpr std::size_t skinned_indirect_buffer_size = render_data::max_batches * sizeof(gpu::draw_indexed_indirect_command);
		d.skinned_indirect_commands_buffer[i] = gpu::buffer::create(gpu_s.device->allocator(), { .size = skinned_indirect_buffer_size, .usage = gpu::buffer_flag::indirect | gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst });

		constexpr std::size_t local_pose_size = system::data::max_skin_matrices * sizeof(mat4f);
		d.local_pose_buffer[i] = gpu::buffer::create(gpu_s.device->allocator(), { .size = local_pose_size, .usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst });
	}

	d.skeleton_buffer = gpu::buffer::create(gpu_s.device->allocator(), { .size = system::data::max_joints * sizeof(geometry_collector::joint_data), .usage = gpu::buffer_flag::storage | gpu::buffer_flag::transfer_dst });

	co_return;
}

auto gse::renderer::geometry_collector::tick(run_context& ctx, system::data& d, const camera::system::data& cam_state) -> async::task<> {
	const view_matrix view_matrix = cam_state.view_matrix;
	const projection_matrix proj_matrix = cam_state.projection_matrix;

	auto body_index_map = read_body_index_map(ctx);

	auto [render, transform, anim] = co_await ctx.acquire_with(
		write_v<render_component>,
		read_v<physics::transform_component>,
		read_v<animation_component>
	);

	if (render.empty()) {
		co_return;
	}

	render_data data;
	data.view = view_matrix;
	data.proj = proj_matrix;

	std::uint32_t skinned_instance_count = 0;
	{
		trace::scope_guard sg{ trace_id<"geom_collect::collect">() };
		collect_static(render, transform, body_index_map, data.render_queue);
		skinned_instance_count = collect_skinned(render, transform, anim, d, data.skinned_render_queue, data.local_pose_staging);
	}

	sort_queues(data.render_queue, data.skinned_render_queue);

	data.instance_staging.reserve(data.render_queue.size() + data.skinned_render_queue.size());
	data.physics_mappings.reserve(data.render_queue.size());

	std::uint32_t global_instance_offset = 0;
	build_normal_batches(data, global_instance_offset);
	build_skinned_batches(data, global_instance_offset);

	if (!data.local_pose_staging.empty() && d.current_joint_count > 0) {
		data.pending_compute_instance_count = skinned_instance_count;
	}
	else {
		data.pending_compute_instance_count = 0;
	}

	data.physics_mapping_count = static_cast<std::uint32_t>(data.physics_mappings.size());

	ctx.channels.push<render_data>(std::move(data));
}

auto gse::renderer::geometry_collector::filter_render_queue(const render_data& data, const std::span<const id> exclude_ids) -> std::vector<render_queue_entry> {
	std::vector<render_queue_entry> result;
	result.reserve(data.render_queue.size());

	for (const auto& queue_entry : data.render_queue) {
		const bool excluded = std::ranges::any_of(exclude_ids, [&](const id& ex) {
			return ex == queue_entry.owner;
		});

		if (!excluded) {
			result.push_back(queue_entry.entry);
		}
	}

	return result;
}

auto gse::renderer::geometry_collector::system::upload_skeleton_data(const data& d, const skeleton& skel) -> void {
	using joint_data = geometry_collector::joint_data;
	const auto joint_count = static_cast<std::size_t>(skel.joint_count());
	const auto joints = skel.joints();

	for (std::size_t i = 0; i < joint_count; ++i) {
		d.skeleton_buffer.host_write(
			joint_data{
				.inverse_bind = joints[i].inverse_bind(),
				.parent_index = joints[i].parent_index(),
			},
			i * sizeof(joint_data)
		);
	}
}

auto gse::renderer::geometry_collector::system::run(run_context& ctx, const gpu::context::data& gpu_s, const asset::data& assets_s, data& d, const camera::system::data& cam_state, const primitive_resolver::system& resolver_state) -> async::task<> {
	co_await initialize(ctx, gpu_s, d);

	while (true) {
		co_await tick(ctx, d, cam_state);
		co_await ctx.next_tick();
	}
}

auto gse::renderer::geometry_collector::system::frame(frame_context& ctx, shared_view<gpu::context> gpu_s, const data& d) -> async::task<> {
	const auto& items = ctx.read_channel<render_data>();
	if (items.empty()) {
		co_return;
	}

	const auto& data = items[0];
	const auto frame_index = gpu_s.render_graph->current_frame();

	if (!data.instance_staging.empty()) {
		d.instance_buffer[frame_index].host_write(data.instance_staging);
	}

	if (!data.normal_batches.empty()) {
		static_vector<gpu::draw_mesh_tasks_indirect_command, render_data::max_batches> normal_indirect_commands;

		for (const auto& batch : data.normal_batches) {
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];
			const std::uint32_t task_groups = (mesh.meshlet_count() + 31) / 32;

			normal_indirect_commands.push_back({ .group_count_x = task_groups, .group_count_y = batch.instance_count, .group_count_z = 1 });
		}

		if (!normal_indirect_commands.empty()) {
			d.normal_indirect_commands_buffer[frame_index].host_write(normal_indirect_commands);
		}
	}

	if (!data.skinned_batches.empty()) {
		static_vector<gpu::draw_indexed_indirect_command, render_data::max_batches> skinned_indirect_commands;

		for (const auto& batch : data.skinned_batches) {
			const auto& mesh = batch.key.model_ptr->meshes()[batch.key.mesh_index];

			skinned_indirect_commands.push_back({ .index_count = static_cast<std::uint32_t>(mesh.indices().size()), .instance_count = batch.instance_count, .first_index = 0, .vertex_offset = 0, .first_instance = batch.first_instance });
		}

		if (!skinned_indirect_commands.empty()) {
			d.skinned_indirect_commands_buffer[frame_index].host_write(skinned_indirect_commands);
		}
	}

	if (!data.local_pose_staging.empty() && d.current_joint_count > 0) {
		d.local_pose_buffer[frame_index].host_write(data.local_pose_staging);
	}
	else if (!data.skin_staging.empty()) {
		d.skin_buffer[frame_index].host_write(data.skin_staging);
	}
}
