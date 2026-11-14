module sandbox:grass_system_impl;

import std;

import gse;
import gse.gpu;
import gse.gpu_record;

import :grass_field;
import :grass_system;

namespace sandbox::grass {
	using namespace gse;
	using namespace gse::physics;

	constexpr std::uint32_t solver_iterations = 4;

	auto grid_cell_size() -> length;

	auto create_field_buffers(
		gpu::device& device,
		const grass_field& field,
		field_buffers& fb
	) -> void;

	struct grid_bindings {
		shaders::ssbo_readonly<grass_params> params_data;
		shaders::ssbo_readonly<vbd::body_state> body_data;
		shaders::ssbo_readonly<std::uint32_t> passthrough_data;
		shaders::ssbo_readwrite<std::uint32_t> grid_data;
		shaders::ssbo_readwrite<vbd::body_state> body_copy_data;
	};

	struct wake_bindings {
		shaders::ssbo_readonly<grass_params> params_data;
		shaders::ssbo_readonly<std::uint32_t> grid_read_data;
		shaders::ssbo_readwrite<grass_tile> tile_data;
	};

	struct compact_bindings {
		shaders::ssbo_readonly<grass_params> params_data;
		shaders::ssbo_readwrite<grass_tile> tile_data;
		shaders::ssbo_readwrite<grass_slot> slot_data;
		shaders::ssbo_readwrite<std::uint32_t> awake_data;
		shaders::ssbo_readwrite<grass_status> status_data;
	};

	struct simulate_bindings {
		shaders::ssbo_readonly<grass_params> params_data;
		shaders::ssbo_readonly<vbd::body_state> body_data;
		shaders::ssbo_readonly<std::uint32_t> grid_read_data;
		shaders::ssbo_readwrite<grass_node> node_data;
		shaders::ssbo_readwrite<grass_slot> slot_data;
		shaders::ssbo_readonly<std::uint32_t> awake_read_data;
		shaders::ssbo_readwrite<std::uint32_t> contact_data;
		shaders::ssbo_readwrite<std::uint32_t> reaction_data;
		shaders::ssbo_readonly<std::uint32_t> route_data;
	};

	struct settle_bindings {
		shaders::ssbo_readonly<grass_params> params_data;
		shaders::ssbo_readwrite<grass_tile> tile_data;
		shaders::ssbo_readwrite<grass_slot> slot_data;
	};

	using grid_entry = gpu::compute_entry<
		gpu::body_path<"Grass/grass_grid_build">,
		gpu::types<vbd::shader_types, shader_types>,
		gpu::bindings<grid_bindings>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<64>
	>;

	using wake_entry = gpu::compute_entry<
		gpu::body_path<"Grass/grass_tile_wake">,
		gpu::types<shader_types>,
		gpu::bindings<wake_bindings>,
		gpu::threads<64>
	>;

	using compact_entry = gpu::compute_entry<
		gpu::body_path<"Grass/grass_tile_compact">,
		gpu::types<shader_types>,
		gpu::bindings<compact_bindings>,
		gpu::threads<layout.grass_compact_threads>
	>;

	using simulate_entry = gpu::compute_entry<
		gpu::body_path<"Grass/grass_simulate">,
		gpu::types<vbd::shader_types, shader_types>,
		gpu::bindings<simulate_bindings>,
		gpu::helpers<"VBDPhysics/vbd_shared", "Grass/grass_shape", "Grass/grass_shared">,
		gpu::threads<layout.grass_sim_threads>
	>;

	using settle_entry = gpu::compute_entry<
		gpu::body_path<"Grass/grass_tile_settle">,
		gpu::types<shader_types>,
		gpu::bindings<settle_bindings>,
		gpu::threads<64>
	>;
}

auto sandbox::grass::grid_cell_size() -> length {
	return meters(0.5f);
}

auto sandbox::grass::pool_bytes() -> std::size_t {
	return static_cast<std::size_t>(layout.grass_slot_count) * layout.grass_tile_blade_count * layout.grass_nodes * sizeof(grass_node);
}

auto sandbox::grass::create_field_buffers(gpu::device& device, const grass_field& field, field_buffers& fb) -> void {
	constexpr gpu::buffer_usage state_usage{ gpu::buffer_flag::storage, gpu::buffer_flag::transfer_dst };
	constexpr gpu::buffer_usage slot_usage{ gpu::buffer_flag::storage, gpu::buffer_flag::transfer_dst, gpu::buffer_flag::transfer_src };
	constexpr gpu::buffer_usage status_usage{ gpu::buffer_flag::storage, gpu::buffer_flag::transfer_dst, gpu::buffer_flag::transfer_src, gpu::buffer_flag::indirect };

	fb.tile_count = tile_count(field);

	fb.nodes = device.create_buffer(
		{
			.size = pool_bytes(),
			.stride = sizeof(grass_node),
			.usage = gpu::buffer_flag::storage,
			.writable = true,
			.device_local = true,
		},
		"grass.pool"
	);
	fb.tiles = device.create_buffer(
		{
			.size = static_cast<std::size_t>(fb.tile_count) * sizeof(grass_tile),
			.stride = sizeof(grass_tile),
			.usage = state_usage,
			.writable = true,
			.device_local = true,
		},
		"grass.tiles"
	);
	fb.slots = device.create_buffer(
		{
			.size = static_cast<std::size_t>(layout.grass_slot_count) * sizeof(grass_slot),
			.stride = sizeof(grass_slot),
			.usage = slot_usage,
			.writable = true,
			.device_local = true,
		},
		"grass.slots"
	);
	fb.awake = device.create_buffer(
		{
			.size = static_cast<std::size_t>(layout.grass_slot_count) * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = state_usage,
			.writable = true,
			.device_local = true,
		},
		"grass.awake"
	);
	fb.status = device.create_buffer(
		{
			.size = sizeof(grass_status),
			.stride = sizeof(grass_status),
			.usage = status_usage,
			.writable = true,
			.device_local = true,
		},
		"grass.status"
	);
	fb.contact = device.create_buffer(
		{
			.size = static_cast<std::size_t>(layout.grass_slot_count) * layout.grass_tile_blade_count * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage,
			.writable = true,
			.device_local = true,
		},
		"grass.contact"
	);

	const auto extent = field_extent(field);
	fb.cells_per_side = std::max(1u, static_cast<std::uint32_t>(std::ceil(static_cast<float>(extent / grid_cell_size()))));
	const auto cells = static_cast<std::size_t>(fb.cells_per_side) * fb.cells_per_side;
	fb.grid = device.create_buffer(
		{
			.size = (cells + cells * cell_capacity) * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = state_usage,
			.writable = true,
			.device_local = true,
		},
		"grass.grid"
	);

	for (auto& params : fb.params) {
		params = device.create_buffer(
			{
				.size = sizeof(grass_params),
				.stride = sizeof(grass_params),
				.usage = gpu::buffer_flag::storage,
			},
			"grass.params"
		);
	}

	fb.field = field;
	fb.blade_count = blade_count(field);
	fb.cleared = false;

	log::println(
		log::category::physics,
		"grass: field of {} blades in {} tiles, slot pool {} MiB, grid {} KiB",
		fb.blade_count,
		fb.tile_count,
		fb.nodes.size() / (1024 * 1024),
		fb.grid.size() / 1024
	);
}

auto sandbox::grass::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	{
		gpu::program_batch batch(*gpu_s.device);
		batch.add(d.grid_pipeline, grid_entry::pod);
		batch.add(d.wake_pipeline, wake_entry::pod);
		batch.add(d.compact_pipeline, compact_entry::pod);
		batch.add(d.simulate_pipeline, simulate_entry::pod);
		batch.add(d.settle_pipeline, settle_entry::pod);
	}
	return {};
}

auto sandbox::grass::collect(context& ctx, data& d, const shared_view<physics::data> phys, const channel_read<interpolation_state> interp_in, read<grass_field> fields, read<transform_component> transforms, read<grass_passthrough> passthrough, read<grass_reaction_target> reaction_targets) -> async::task<> {
	d.fields.clear();
	d.cpu_bodies.clear();
	d.passthrough_bits.clear();
	d.reaction_routes.clear();

	const auto owners = fields.owner_ids();
	for (std::size_t i = 0; i < fields.size(); ++i) {
		const auto* tc = transforms.find(owners[i]);
		d.fields.push_back({
			.owner = owners[i],
			.field = fields[i],
			.transform = tc != nullptr ? *tc : transform_component{},
		});
	}

	if (d.fields.empty()) {
		d.substeps = 0;
		return {};
	}

	interpolation_state interpolation;
	if (const auto& published = interp_in.of<interpolation_state>(); !published.empty()) {
		interpolation = published[0];
	}
	const auto substeps_per_tick = std::max(phys.physics_substeps, 1);
	d.substeps = static_cast<std::uint32_t>(std::max(interpolation.steps, 0) * substeps_per_tick);
	d.substep_dt = system_clock::fixed_dt<time_t<float, seconds>>() / static_cast<float>(substeps_per_tick);

	d.passthrough_bits.assign((phys.id_to_body_index.size() + 31) / 32 + 1, 0u);
	for (const auto owner : passthrough.owner_ids()) {
		if (const auto it = phys.id_to_body_index.find(owner); it != phys.id_to_body_index.end() && it->second / 32 < d.passthrough_bits.size()) {
			d.passthrough_bits[it->second / 32] |= 1u << (it->second % 32);
		}
	}

	const auto reaction_owners = reaction_targets.owner_ids();
	for (std::size_t i = 0; i < reaction_targets.size(); ++i) {
		const auto from = phys.id_to_body_index.find(reaction_owners[i]);
		const auto to = phys.id_to_body_index.find(reaction_targets[i].body);
		if (from == phys.id_to_body_index.end() || to == phys.id_to_body_index.end()) {
			continue;
		}
		if (d.reaction_routes.empty()) {
			d.reaction_routes.resize(std::ranges::max(phys.id_to_body_index | std::views::values) + 1);
			std::ranges::iota(d.reaction_routes, 0u);
		}
		d.reaction_routes[from->second] = to->second;
	}

	d.solver_bodies = gpu_solver_active(phys);
	if (!d.solver_bodies) {
		const auto bodies = phys.vbd_solver.body_states();
		d.cpu_bodies.assign(bodies.begin(), bodies.end());
	}

	return {};
}

auto sandbox::grass::frame(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d, const channel_read<gpu_solver_frame_info> solver_in, const channel_write<gpu::render_pass_request> pass_out) -> async::task<> {
	std::erase_if(d.buffers, [&](const auto& entry) {
		return std::ranges::none_of(d.fields, [&](const field_instance& f) {
			return f.owner == entry.first;
		});
	});

	if (d.fields.empty()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();

	const auto& solver_infos = solver_in.of<gpu_solver_frame_info>();
	const gpu_solver_frame_info* solver_info = d.solver_bodies && !solver_infos.empty() && solver_infos[0].body_count > 0 ? &solver_infos[0] : nullptr;

	body_binding source;
	if (solver_info && solver_info->snapshot) {
		if (!d.solver_body_views[frame_index].valid()) {
			d.solver_body_views[frame_index] = gpu_s.device->allocate_buffer_slot();
		}
		gpu_s.device->write_storage_buffer(d.solver_body_views[frame_index].slot(), solver_info->snapshot, solver_info->body_count * solver_info->body_stride);
		source = {
			.slot = d.solver_body_views[frame_index].slot(),
			.count = solver_info->body_count,
		};
	}
	if (source.count == 0) {
		const auto cpu_count = static_cast<std::uint32_t>(d.cpu_bodies.size());
		const auto required = std::max<std::size_t>(cpu_count, 1) * sizeof(vbd::body_state);
		if (d.cpu_body_capacity[frame_index] < required) {
			d.cpu_body_buffers[frame_index] = gpu_s.device->create_buffer(
				{
					.size = required,
					.stride = sizeof(vbd::body_state),
					.usage = gpu::buffer_flag::storage,
				},
				"grass.cpu_bodies"
			);
			d.cpu_body_capacity[frame_index] = required;
		}
		if (cpu_count > 0) {
			d.cpu_body_buffers[frame_index].host_write(d.cpu_bodies);
		}
		source = {
			.slot = d.cpu_body_buffers[frame_index].slot(),
			.count = cpu_count,
		};
	}

	const auto copy_bytes = std::max<std::size_t>(source.count, 1) * sizeof(vbd::body_state);
	if (d.body_copy_capacity[frame_index] < copy_bytes) {
		d.body_copies[frame_index] = gpu_s.device->create_buffer(
			{
				.size = copy_bytes,
				.stride = sizeof(vbd::body_state),
				.usage = gpu::buffer_flag::storage,
				.writable = true,
				.device_local = true,
			},
			"grass.bodies"
		);
		d.body_copy_capacity[frame_index] = copy_bytes;
	}
	d.bodies = {
		.slot = d.body_copies[frame_index].slot(),
		.count = source.count,
	};

	const auto passthrough_bytes = d.passthrough_bits.size() * sizeof(std::uint32_t);
	if (d.passthrough_capacity[frame_index] < passthrough_bytes) {
		d.passthrough_buffers[frame_index] = gpu_s.device->create_buffer(
			{
				.size = passthrough_bytes,
				.stride = sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage,
			},
			"grass.passthrough"
		);
		d.passthrough_capacity[frame_index] = passthrough_bytes;
	}
	d.passthrough_buffers[frame_index].host_write(d.passthrough_bits);

	const auto route_bytes = std::max<std::size_t>(d.reaction_routes.size(), 1) * sizeof(std::uint32_t);
	if (d.route_capacity[frame_index] < route_bytes) {
		d.route_buffers[frame_index] = gpu_s.device->create_buffer(
			{
				.size = route_bytes,
				.stride = sizeof(std::uint32_t),
				.usage = gpu::buffer_flag::storage,
			},
			"grass.reaction_routes"
		);
		d.route_capacity[frame_index] = route_bytes;
	}
	if (!d.reaction_routes.empty()) {
		d.route_buffers[frame_index].host_write(d.reaction_routes);
	}

	for (auto& view : d.reaction_views) {
		if (!view.valid()) {
			view = gpu_s.device->allocate_buffer_slot();
		}
	}
	std::uint32_t reaction_bodies = 0;
	std::uint32_t reaction_stamp_word = 0;
	if (solver_info && solver_info->external_force[frame_index]) {
		reaction_bodies = std::min(solver_info->body_count, d.bodies.count);
		reaction_stamp_word = solver_info->external_force_stamp_word;
		gpu_s.device->write_storage_buffer(d.reaction_views[frame_index].slot(), solver_info->external_force[frame_index], (static_cast<std::size_t>(reaction_stamp_word) + 1) * sizeof(std::uint32_t));
	}
	else {
		gpu_s.device->write_storage_buffer(d.reaction_views[frame_index].slot(), d.route_buffers[frame_index].handle(), d.route_capacity[frame_index]);
	}

	const auto world_gravity = vec3<acceleration>(meters_per_second_squared(0.f), -vbd::gravity_magnitude(), meters_per_second_squared(0.f));

	std::vector<id> active;
	active.reserve(d.fields.size());
	for (const auto& instance : d.fields) {
		if (blade_count(instance.field) == 0) {
			continue;
		}

		auto& fb = d.buffers[instance.owner];
		if (fb.blade_count == 0 || rest_shape_changed(fb.field, instance.field)) {
			create_field_buffers(*gpu_s.device, instance.field, fb);
		}
		fb.field = instance.field;
		fb.transform = instance.transform;

		const auto& field = instance.field;
		fb.params[frame_index].host_write(grass_params{
			.field_orientation = instance.transform.orientation,
			.field_position = instance.transform.position,
			.local_gravity = inverse_rotate_vector(instance.transform.orientation, world_gravity),
			.half_extent = field_extent(field) * 0.5f,
			.spacing = field.spacing,
			.blade_height = field.blade_height,
			.root_width = field.root_width,
			.tip_width = field.tip_width,
			.cell_size = grid_cell_size(),
			.contact_margin = field.contact_margin,
			.blade_reach = field.blade_height * layout.grass_reach_factor + field.contact_margin,
			.sleep_tolerance = field.sleep_tolerance,
			.sleep_speed = field.sleep_speed,
			.substep_dt = d.substep_dt,
			.node_mass = field.blade_mass / static_cast<float>(layout.grass_segments),
			.stretch_stiffness = field.stretch_stiffness,
			.bend_stiffness = field.bend_stiffness,
			.contact_stiffness = field.contact_stiffness,
			.damping = field.damping,
			.yield_angle = field.yield_angle,
			.yield_rate = field.yield_rate,
			.recovery_rate = field.recovery_rate,
			.blade_count = fb.blade_count,
			.blades_per_side = static_cast<std::uint32_t>(field.blades_per_side),
			.tiles_per_side = tiles_per_side(field),
			.tile_count = fb.tile_count,
			.cells_per_side = fb.cells_per_side,
			.cell_capacity = cell_capacity,
			.substeps = d.substeps,
			.sleep_steps = static_cast<std::uint32_t>(std::ceil(field.sleep_delay / d.substep_dt)),
			.iterations = solver_iterations,
			.body_count = d.bodies.count,
			.passthrough_words = static_cast<std::uint32_t>(d.passthrough_bits.size()),
			.reaction_bodies = reaction_bodies,
			.reaction_stamp_word = reaction_stamp_word,
			.reaction_stamp = static_cast<std::uint32_t>(gpu_s.frame->frame_count()),
			.route_count = static_cast<std::uint32_t>(d.reaction_routes.size()),
		});
		active.push_back(instance.owner);
	}

	if (active.empty()) {
		co_return;
	}

	auto grid_rec = co_await gpu::pass<^^grid_pass>(pass_out)
		.pipeline(d.grid_pipeline)
		.after<^^vbd::vbd_render_mirror_stage>();

	for (const auto owner : active) {
		auto& fb = d.buffers.at(owner);
		if (!fb.cleared) {
			grid_rec.fill_buffer(fb.tiles, 0, fb.tiles.size());
			grid_rec.fill_buffer(fb.slots, 0, fb.slots.size());
			grid_rec.fill_buffer(fb.status, 0, fb.status.size());
			fb.cleared = true;
		}
		grid_rec.fill_buffer(fb.grid, 0, fb.grid.size());
		if (d.bodies.count > 0) {
			grid_rec.dispatch<grid_entry>(
				{
					.params_data = fb.params[frame_index],
					.body_data = source.slot,
					.passthrough_data = d.passthrough_buffers[frame_index],
					.grid_data = fb.grid,
					.body_copy_data = d.bodies.slot,
				},
				vec3u{ (d.bodies.count + 63) / 64, 1u, 1u }
			);
		}
	}

	if (d.substeps == 0) {
		co_return;
	}

	grid_rec.bind(d.wake_pipeline);
	for (const auto owner : active) {
		const auto& fb = d.buffers.at(owner);
		grid_rec.dispatch<wake_entry>(
			{
				.params_data = fb.params[frame_index],
				.grid_read_data = fb.grid,
				.tile_data = fb.tiles,
			},
			vec3u{ (fb.tile_count + 63) / 64, 1u, 1u }
		);
	}

	grid_rec.bind(d.compact_pipeline);
	for (const auto owner : active) {
		const auto& fb = d.buffers.at(owner);
		grid_rec.dispatch<compact_entry>(
			{
				.params_data = fb.params[frame_index],
				.tile_data = fb.tiles,
				.slot_data = fb.slots,
				.awake_data = fb.awake,
				.status_data = fb.status,
			},
			vec3u{ 1u, 1u, 1u }
		);
	}

	auto sim_rec = co_await gpu::pass<^^simulate_pass>(pass_out)
		.pipeline(d.simulate_pipeline)
		.after<^^grid_pass>();

	for (const auto owner : active) {
		const auto& fb = d.buffers.at(owner);
		sim_rec.push_bindings<simulate_entry>({
			.params_data = fb.params[frame_index],
			.body_data = d.bodies.slot,
			.grid_read_data = fb.grid,
			.node_data = fb.nodes,
			.slot_data = fb.slots,
			.awake_read_data = fb.awake,
			.contact_data = fb.contact,
			.reaction_data = d.reaction_views[frame_index],
			.route_data = d.route_buffers[frame_index],
		});
		sim_rec.dispatch_indirect(fb.status, 0);
	}

	sim_rec.bind(d.settle_pipeline);
	for (const auto owner : active) {
		const auto& fb = d.buffers.at(owner);
		sim_rec.dispatch<settle_entry>(
			{
				.params_data = fb.params[frame_index],
				.tile_data = fb.tiles,
				.slot_data = fb.slots,
			},
			vec3u{ (layout.grass_slot_count + 63) / 64, 1u, 1u }
		);
	}
}
