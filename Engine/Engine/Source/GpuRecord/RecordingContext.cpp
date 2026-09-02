module gse.gpu_record:recording_context_impl;

import std;

import :recording_context;
import :pipeline_builder;

import gse.gpu;
import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.log;
import gse.math;
import gse.meta;

namespace gse::gpu {
	inline thread_local recording_context* tl_active_recording_context = nullptr;
}

gse::gpu::recording_context::recording_context(pass_recorder rec, render_pass_data* pass, const transient_pool* transient_pool, device* device)
	: m_recorder(rec), m_pass(pass), m_transient_pool(transient_pool), m_device(device) {
	if (m_recorder.valid()) {
		assert(
			tl_active_recording_context == nullptr,
			"another recording_context is still active on this worker thread; a prior coroutine held its rec across a "
			"co_await that was not gpu::pass<...>(ctx). Scope each rec to end before any non-pass await."
		);
		tl_active_recording_context = this;
		m_origin_thread = std::this_thread::get_id();
	}
}

gse::gpu::recording_context::recording_context(recording_context_init&& init)
	: recording_context(std::move(init.recorder), init.pass, init.transient_pool, init.device) {
	if (init.primary) {
		bind(*init.primary);
	}
	for (const auto& t : init.touches) {
		note_touched(t.ref, t.stages, t.access);
	}
}

gse::gpu::recording_context::recording_context(recording_context&& other) noexcept
	: m_recorder(other.m_recorder), m_pass(other.m_pass), m_transient_pool(other.m_transient_pool), m_device(other.m_device), m_touched(std::move(other.m_touched)), m_last_access(std::move(other.m_last_access)), m_image_states(std::move(other.m_image_states)), m_pending_memory_barriers(std::move(other.m_pending_memory_barriers)), m_pending_buffer_barriers(std::move(other.m_pending_buffer_barriers)), m_pending_image_barriers(std::move(other.m_pending_image_barriers)), m_origin_thread(other.m_origin_thread), m_state_cache(other.m_state_cache), m_bindless_heaps_valid(other.m_bindless_heaps_valid), m_bound_is_compute(other.m_bound_is_compute), m_last_binding_bytes(other.m_last_binding_bytes), m_last_binding_pack(other.m_last_binding_pack), m_last_binding_stages(other.m_last_binding_stages), m_companion_stages(other.m_companion_stages), m_companion_access(other.m_companion_access), m_binding_repeat_valid(other.m_binding_repeat_valid), m_binding_companion_armed(other.m_binding_companion_armed) {
	if (tl_active_recording_context == &other) {
		tl_active_recording_context = this;
	}
	other.m_state_cache.invalidate();
	other.m_recorder = pass_recorder{};
	other.m_pass = nullptr;
	other.m_transient_pool = nullptr;
	other.m_device = nullptr;
	other.m_bindless_heaps_valid = false;
	other.m_bound_is_compute = false;
	other.m_binding_repeat_valid = false;
	other.m_binding_companion_armed = false;
}

auto gse::gpu::recording_context::operator=(recording_context&& other) noexcept -> recording_context& {
	if (this != &other) {
		if (m_recorder.valid()) {
			assert(
				std::this_thread::get_id() == m_origin_thread,
				"recording_context's secondary command buffer is being ended on a thread other than its origin. "
				"This means the rec was held alive across a co_await that was not gpu::pass<...>(ctx) and the "
				"coroutine "
				"resumed on a different worker. Scope the rec to end before any non-pass await."
			);
			finalize_pass();
			m_recorder.end();
		}
		if (tl_active_recording_context == this) {
			tl_active_recording_context = nullptr;
		}
		m_recorder = other.m_recorder;
		m_pass = other.m_pass;
		m_transient_pool = other.m_transient_pool;
		m_device = other.m_device;
		m_touched = std::move(other.m_touched);
		m_last_access = std::move(other.m_last_access);
		m_image_states = std::move(other.m_image_states);
		m_pending_memory_barriers = std::move(other.m_pending_memory_barriers);
		m_pending_buffer_barriers = std::move(other.m_pending_buffer_barriers);
		m_pending_image_barriers = std::move(other.m_pending_image_barriers);
		m_origin_thread = other.m_origin_thread;
		m_state_cache = other.m_state_cache;
		m_bindless_heaps_valid = other.m_bindless_heaps_valid;
		m_bound_is_compute = other.m_bound_is_compute;
		m_last_binding_bytes = other.m_last_binding_bytes;
		m_last_binding_pack = other.m_last_binding_pack;
		m_last_binding_stages = other.m_last_binding_stages;
		m_companion_stages = other.m_companion_stages;
		m_companion_access = other.m_companion_access;
		m_binding_repeat_valid = other.m_binding_repeat_valid;
		m_binding_companion_armed = other.m_binding_companion_armed;
		if (tl_active_recording_context == &other) {
			tl_active_recording_context = this;
		}
		other.m_state_cache.invalidate();
		other.m_recorder = pass_recorder{};
		other.m_pass = nullptr;
		other.m_transient_pool = nullptr;
		other.m_device = nullptr;
		other.m_bindless_heaps_valid = false;
		other.m_bound_is_compute = false;
		other.m_binding_repeat_valid = false;
		other.m_binding_companion_armed = false;
	}
	return *this;
}

auto gse::gpu::recording_context::resolve(const transient_image_handle h) const -> const image& {
	assert(m_transient_pool != nullptr, "recording_context::resolve called but no transient_pool is bound");
	const auto* img = m_transient_pool->resolve_image(h);
	assert(
		img != nullptr,
		"transient_image_handle {} could not be resolved; ensure it was declared via gpu::transient_image before this "
		"pass runs",
		h.key
	);
	return *img;
}

auto gse::gpu::recording_context::resolve(const transient_buffer_handle h) const -> const buffer& {
	assert(m_transient_pool != nullptr, "recording_context::resolve called but no transient_pool is bound");
	const auto* buf = m_transient_pool->resolve_buffer(h);
	assert(
		buf != nullptr,
		"transient_buffer_handle {} could not be resolved; ensure it was declared via gpu::transient_buffer before "
		"this pass runs",
		h.key
	);
	return *buf;
}

gse::gpu::recording_context::~recording_context() {
	if (m_recorder.valid()) {
		assert(
			std::this_thread::get_id() == m_origin_thread,
			"recording_context is being finalized on a thread other than its origin. "
			"This means the rec was held alive across a co_await that was not gpu::pass<...>(ctx) and the coroutine "
			"resumed on a different worker. Scope the rec to end before any non-pass await."
		);
		finalize_pass();
	}
	if (tl_active_recording_context == this) {
		tl_active_recording_context = nullptr;
	}
}

auto gse::gpu::recording_context::finalize_active_on_current_thread() noexcept -> void {
	auto* active = tl_active_recording_context;
	if (active != nullptr && active->m_recorder.valid()) {
		assert(
			std::this_thread::get_id() == active->m_origin_thread,
			"finalize_active_on_current_thread invoked on a thread that does not own the active rec; "
			"tl_active_recording_context was corrupted (probably by a rec being held across a non-pass co_await)."
		);
		active->finalize_pass();
		active->m_recorder = pass_recorder{};
	}
	tl_active_recording_context = nullptr;
}

auto gse::gpu::recording_context::check_active() const -> void {
	assert(
		m_recorder.valid(),
		"recording_context method called after the pass's secondary was finalized. This happens when the previous rec "
		"was implicitly closed by a subsequent `co_await gpu::pass(...)` (each new pass await closes the prior rec on "
		"the suspending thread to keep VkCommandPool access single-threaded). Use only the rec returned by the most "
		"recent co_await, or scope each rec in its own block."
	);
	assert(
		std::this_thread::get_id() == m_origin_thread,
		"recording_context method called from a different thread than the one that constructed it. This means the rec "
		"was held alive across a co_await that was not gpu::pass<...>(ctx) and the coroutine resumed on a different "
		"worker. Recording onto a secondary from a thread that does not own its command pool is undefined; scope the "
		"rec to end before any non-pass await."
	);
}

auto gse::gpu::recording_context::ensure_descriptor_heaps() -> void {
	if (m_bindless_heaps_valid) {
		return;
	}
	assert(m_device != nullptr, "recording_context has no device");
	const auto resource_binding = m_device->bindless_resource_heap_binding();
	m_recorder.bind_resource_heap(
		resource_binding.address,
		resource_binding.size,
		resource_binding.reserved_offset,
		resource_binding.reserved_size
	);
	const auto sampler_binding = m_device->bindless_sampler_heap_binding();
	m_recorder.bind_sampler_heap(
		sampler_binding.address,
		sampler_binding.size,
		sampler_binding.reserved_offset,
		sampler_binding.reserved_size
	);
	m_bindless_heaps_valid = true;
}

auto gse::gpu::recording_context::sample_image(const image& img, const pipeline_stage_flags stages) -> void {
	check_active();
	const resource_ref ref{
		.ptr = std::bit_cast<const void*>(img.handle()),
		.type = resource_type::image,
		.aspects = image_aspect_for(img.format()),
	};
	note_touched(ref, stages, access_flag::shader_sampled_read);
	transition_image_for_binding(ref, resource_state::sampled, stages, access_flag::shader_sampled_read);
}

auto gse::gpu::recording_context::note_touched(const resource_ref ref, const pipeline_stage_flags stages, const access_flags access) -> void {
	if (!ref.ptr) {
		return;
	}
	bool changed = emit_intra_pass_barrier(ref, stages, access);
	for (auto& existing : m_touched) {
		if (existing.ref.ptr == ref.ptr) {
			const auto prev_stages = existing.stages.bits();
			const auto prev_access = existing.access.bits();
			existing.stages |= stages;
			existing.access |= access;
			if (changed || existing.stages.bits() != prev_stages || existing.access.bits() != prev_access) {
				note_binding_mutation(stages, access, true);
			}
			return;
		}
	}
	m_touched.push_back({
		.ref = ref,
		.stages = stages,
		.access = access,
	});
	note_binding_mutation(stages, access, false);
}

auto gse::gpu::recording_context::invalidate_binding_repeat() -> void {
	m_binding_repeat_valid = false;
	m_binding_companion_armed = false;
	m_companion_stages = {};
	m_companion_access = {};
}

auto gse::gpu::recording_context::note_binding_mutation(const pipeline_stage_flags stages, const access_flags access, const bool known_resource) -> void {
	if (!m_binding_repeat_valid) {
		return;
	}
	if (known_resource && !m_binding_companion_armed) {
		m_binding_companion_armed = true;
		m_companion_stages |= stages;
		m_companion_access |= access;
		return;
	}
	invalidate_binding_repeat();
}

auto gse::gpu::recording_context::bound_shader_stages() const -> pipeline_stage_flags {
	if (m_bound_is_compute) {
		return pipeline_stage_flag::compute_shader;
	}
	return { pipeline_stage_flag::vertex_shader, pipeline_stage_flag::fragment_shader, pipeline_stage_flag::mesh_shader, pipeline_stage_flag::task_shader };
}

auto gse::gpu::recording_context::emit_intra_pass_barrier(const resource_ref& ref, const pipeline_stage_flags stages, const access_flags access) -> bool {
	if (!m_recorder.valid()) {
		return false;
	}

	constexpr access_flags write_mask{ access_flag::shader_write, access_flag::shader_storage_write,
		access_flag::color_attachment_write, access_flag::depth_stencil_attachment_write,
		access_flag::transfer_write, access_flag::host_write, access_flag::memory_write,
		access_flag::acceleration_structure_write };

	constexpr pipeline_stage_flags graphics_mask{ pipeline_stage_flag::vertex_input,
		pipeline_stage_flag::vertex_shader, pipeline_stage_flag::tessellation_control,
		pipeline_stage_flag::tessellation_evaluation, pipeline_stage_flag::geometry_shader,
		pipeline_stage_flag::fragment_shader, pipeline_stage_flag::early_fragment_tests,
		pipeline_stage_flag::late_fragment_tests, pipeline_stage_flag::color_attachment_output,
		pipeline_stage_flag::all_graphics, pipeline_stage_flag::index_input,
		pipeline_stage_flag::vertex_attribute_input, pipeline_stage_flag::pre_rasterization_shaders,
		pipeline_stage_flag::mesh_shader, pipeline_stage_flag::task_shader };

	const auto it = m_last_access.find(ref.ptr);
	if (it == m_last_access.end()) {
		m_last_access.emplace(ref.ptr, access_track{ .stages = stages, .access = access });
		return true;
	}

	auto& prev = it->second;
	const bool hazard = (prev.access & write_mask).bits() != 0 || (access & write_mask).bits() != 0;
	if (!hazard || (stages & graphics_mask).bits() != 0) {
		const auto prev_stages = prev.stages.bits();
		const auto prev_access = prev.access.bits();
		prev.stages |= stages;
		prev.access |= access;
		return prev.stages.bits() != prev_stages || prev.access.bits() != prev_access;
	}

	if (ref.type == resource_type::buffer) {
		const auto handle = std::bit_cast<gpu::handle<buffer>>(ref.ptr);
		bool merged = false;
		for (auto& pending : m_pending_buffer_barriers) {
			if (pending.buffer.value == handle.value) {
				pending.src_stages |= prev.stages;
				pending.src_access |= prev.access;
				pending.dst_stages |= stages;
				pending.dst_access |= access;
				merged = true;
				break;
			}
		}
		if (!merged) {
			m_pending_buffer_barriers.push_back({
				.src_stages = prev.stages,
				.src_access = prev.access,
				.dst_stages = stages,
				.dst_access = access,
				.buffer = handle,
				.offset = 0,
				.size = ref.buffer_size,
			});
		}
	}
	else if (ref.type == resource_type::image) {
		const auto tracked = m_image_states.find(ref.ptr);
		const auto state = tracked != m_image_states.end() ? tracked->second.current : resource_state::undefined;
		const auto handle = std::bit_cast<gpu::handle<image>>(ref.ptr);
		bool merged = false;
		for (auto& pending : m_pending_image_barriers) {
			if (pending.image.value == handle.value && pending.prev_state == state && pending.next_state == state) {
				pending.src_stages |= prev.stages;
				pending.src_access |= prev.access;
				pending.dst_stages |= stages;
				pending.dst_access |= access;
				merged = true;
				break;
			}
		}
		if (!merged) {
			m_pending_image_barriers.push_back({
				.src_stages = prev.stages,
				.src_access = prev.access,
				.dst_stages = stages,
				.dst_access = access,
				.prev_state = state,
				.next_state = state,
				.image = handle,
				.aspects = ref.aspects,
			});
		}
	}
	else {
		bool merged = false;
		for (auto& pending : m_pending_memory_barriers) {
			pending.src_stages |= prev.stages;
			pending.src_access |= prev.access;
			pending.dst_stages |= stages;
			pending.dst_access |= access;
			merged = true;
			break;
		}
		if (!merged) {
			m_pending_memory_barriers.push_back({
				.src_stages = prev.stages,
				.src_access = prev.access,
				.dst_stages = stages,
				.dst_access = access,
			});
		}
	}

	prev.stages = stages;
	prev.access = access;
	return true;
}

auto gse::gpu::recording_context::note_bindings_repeat(const pipeline_stage_flags stages, const access_flags access) -> void {
	if (!m_recorder.valid()) {
		return;
	}

	constexpr access_flags write_mask{ access_flag::shader_write, access_flag::shader_storage_write,
		access_flag::color_attachment_write, access_flag::depth_stencil_attachment_write,
		access_flag::transfer_write, access_flag::host_write, access_flag::memory_write,
		access_flag::acceleration_structure_write };

	if ((access & write_mask).bits() == 0 && m_companion_access.bits() == 0) {
		return;
	}

	m_binding_companion_armed = false;

	pipeline_stage_flags cycle_stages = stages;
	cycle_stages |= m_companion_stages;
	access_flags cycle_access = access;
	cycle_access |= m_companion_access;

	for (auto& pending : m_pending_memory_barriers) {
		pending.src_stages |= cycle_stages;
		pending.src_access |= cycle_access;
		pending.dst_stages |= cycle_stages;
		pending.dst_access |= cycle_access;
		return;
	}

	m_pending_memory_barriers.push_back({
		.src_stages = cycle_stages,
		.src_access = cycle_access,
		.dst_stages = cycle_stages,
		.dst_access = cycle_access,
	});
}

auto gse::gpu::recording_context::flush_pending_barriers() -> void {
	if (m_pending_memory_barriers.empty() && m_pending_buffer_barriers.empty() && m_pending_image_barriers.empty()) {
		return;
	}
	if (m_recorder.valid()) {
		m_recorder.pipeline_barrier(dependency_info{
			.memory_barriers = m_pending_memory_barriers,
			.buffer_barriers = m_pending_buffer_barriers,
			.image_barriers = m_pending_image_barriers,
		});
	}
	m_pending_memory_barriers.clear();
	m_pending_buffer_barriers.clear();
	m_pending_image_barriers.clear();
}

auto gse::gpu::recording_context::transition_image_for_binding(const resource_ref& ref, const resource_state target, const pipeline_stage_flags stages, const access_flags access) -> void {
	if (ref.type != resource_type::image || !ref.ptr) {
		return;
	}
	const auto it = m_image_states.find(ref.ptr);
	if (it == m_image_states.end()) {
		invalidate_binding_repeat();
		m_image_states.emplace(ref.ptr, image_state_track{ .aspects = ref.aspects, .first = target, .current = target });
		return;
	}
	if (it->second.current == target) {
		return;
	}
	invalidate_binding_repeat();
	const image_barrier barrier{
		.src_stages = stages,
		.dst_stages = stages,
		.dst_access = access,
		.prev_state = it->second.current,
		.next_state = target,
		.image = std::bit_cast<handle<image>>(ref.ptr),
		.aspects = it->second.aspects,
	};
	flush_pending_barriers();
	m_recorder.transition_image_state(barrier);
	it->second.current = target;
}

auto gse::gpu::recording_context::finalize_pass() -> void {
	if (!m_pass) {
		return;
	}

	flush_pending_barriers();

	if (m_recorder.valid()) {
		for (const auto& [ptr, track] : m_image_states) {
			if (track.current == track.first) {
				continue;
			}
			const image_barrier barrier{
				.prev_state = track.current,
				.next_state = track.first,
				.image = std::bit_cast<handle<image>>(ptr),
				.aspects = track.aspects,
			};
			m_recorder.transition_image_state(barrier);
		}
	}

	constexpr access_flags write_mask{ access_flag::shader_write, access_flag::shader_storage_write,
		access_flag::color_attachment_write, access_flag::depth_stencil_attachment_write,
		access_flag::transfer_write, access_flag::host_write, access_flag::memory_write,
		access_flag::acceleration_structure_write };

	for (const auto& [ref, stages, access] : m_touched) {
		const bool has_writes = (access & write_mask).bits() != 0;
		auto& bucket = has_writes ? m_pass->writes : m_pass->reads;
		bucket.push_back({
			.resource = ref,
			.stage = stages,
			.access = access,
		});
	}
}

auto gse::gpu::recording_context::copy_buffer(const buffer& src, const buffer& dst, const std::size_t size, const std::size_t src_offset, const std::size_t dst_offset) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::bit_cast<const void*>(src.handle()),
			.type = resource_type::buffer,
			.buffer_size = src.size(),
			.host_buffer = std::addressof(src),
		},
		pipeline_stage_flag::copy,
		access_flag::transfer_read
	);
	note_touched(
		{
			.ptr = std::bit_cast<const void*>(dst.handle()),
			.type = resource_type::buffer,
			.buffer_size = dst.size(),
			.host_buffer = std::addressof(dst),
		},
		pipeline_stage_flag::copy,
		access_flag::transfer_write
	);
	flush_pending_barriers();
	m_recorder.copy_buffer(
		src.handle(),
		dst.handle(),
		buffer_copy_region{
			.src_offset = src_offset,
			.dst_offset = dst_offset,
			.size = size
		}
	);
}

auto gse::gpu::recording_context::fill_buffer(const buffer& dst, const std::size_t offset, const std::size_t size, const std::uint32_t data) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::bit_cast<const void*>(dst.handle()),
			.type = resource_type::buffer,
			.buffer_size = dst.size(),
			.host_buffer = std::addressof(dst),
		},
		pipeline_stage_flag::clear,
		access_flag::transfer_write
	);
	flush_pending_barriers();
	m_recorder.fill_buffer(dst.handle(), offset, size, data);
}

auto gse::gpu::recording_context::build_acceleration_structure(const acceleration_structure_build_geometry_info& build_info, const std::span<const acceleration_structure_build_range_info* const> range_infos) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::bit_cast<const void*>(build_info.dst.value),
			.type = resource_type::acceleration_structure,
		},
		pipeline_stage_flag::acceleration_structure_build,
		{ access_flag::acceleration_structure_read, access_flag::acceleration_structure_write }
	);
	flush_pending_barriers();
	m_recorder.build_acceleration_structures(build_info, range_infos);
}

auto gse::gpu::recording_context::pipeline_barrier(const dependency_info& dep) -> void {
	check_active();
	flush_pending_barriers();
	m_recorder.pipeline_barrier(dep);
}

auto gse::gpu::recording_context::copy_target_to_buffer(const image_ref& src, const buffer& dst) -> void {
	check_active();
	flush_pending_barriers();
	const auto ext = src.extent;
	const auto dst_buffer = dst.handle();
	const auto gpu_image = src.image;

	const image_barrier to_transfer{
		.src_stages = pipeline_stage_flag::color_attachment_output,
		.src_access = access_flag::color_attachment_write,
		.dst_stages = pipeline_stage_flag::transfer,
		.dst_access = access_flag::transfer_read,
		.image = gpu_image,
		.aspects = image_aspect_flag::color,
	};
	m_recorder.pipeline_barrier(dependency_info{
		.image_barriers = std::span(&to_transfer, 1)
	});

	const buffer_image_copy_region gpu_region{
		.buffer_offset = 0,
		.buffer_row_length = 0,
		.buffer_image_height = 0,
		.image_subresource = {
			.aspects = image_aspect_flag::color,
			.mip_level = 0,
			.base_array_layer = 0,
			.layer_count = 1,
		},
		.image_offset = vec3i{ 0, 0, 0 },
		.image_extent = vec3u{ ext.x(), ext.y(), 1 },
	};
	m_recorder.copy_image_to_buffer(gpu_image, dst_buffer, std::span(&gpu_region, 1));

	const image_barrier back_to_color{
		.src_stages = pipeline_stage_flag::transfer,
		.src_access = access_flag::transfer_read,
		.dst_stages = pipeline_stage_flag::color_attachment_output,
		.dst_access = { access_flag::color_attachment_write, access_flag::color_attachment_read },
		.image = gpu_image,
		.aspects = image_aspect_flag::color,
	};
	m_recorder.pipeline_barrier(dependency_info{
		.image_barriers = std::span(&back_to_color, 1)
	});
}

auto gse::gpu::recording_context::blit_target_to_image(const image_ref& src, const image& dst, const vec2u dst_extent) -> void {
	check_active();
	flush_pending_barriers();
	const auto src_image = src.image;
	const auto src_ext = src.extent;

	const image_barrier src_to_transfer{
		.src_stages = pipeline_stage_flag::color_attachment_output,
		.src_access = access_flag::color_attachment_write,
		.dst_stages = pipeline_stage_flag::transfer,
		.dst_access = access_flag::transfer_read,
		.image = src_image,
		.aspects = image_aspect_flag::color,
	};

	const image_barrier dst_to_transfer{
		.src_stages = {},
		.src_access = {},
		.dst_stages = pipeline_stage_flag::transfer,
		.dst_access = access_flag::transfer_write,
		.discard_contents = true,
		.image = dst.handle(),
		.aspects = image_aspect_flag::color,
	};

	const std::array pre_barriers = { src_to_transfer, dst_to_transfer };
	m_recorder.pipeline_barrier(dependency_info{
		.image_barriers = pre_barriers
	});

	const image_blit_region gpu_region{
		.src_subresource = {
			.aspects = image_aspect_flag::color,
			.mip_level = 0,
			.base_array_layer = 0,
			.layer_count = 1,
		},
		.src_offsets = {
			vec3i{ 0, 0, 0 },
			vec3i{ static_cast<int>(src_ext.x()), static_cast<int>(src_ext.y()), 1 },
		},
		.dst_subresource = {
			.aspects = image_aspect_flag::color,
			.mip_level = 0,
			.base_array_layer = 0,
			.layer_count = 1,
		},
		.dst_offsets = {
			vec3i{ 0, 0, 0 },
			vec3i{ static_cast<int>(dst_extent.x()), static_cast<int>(dst_extent.y()), 1 },
		},
	};
	m_recorder.blit_image(src_image, dst.handle(), gpu_region, sampler_filter::nearest);

	const image_barrier src_back{
		.src_stages = pipeline_stage_flag::transfer,
		.src_access = access_flag::transfer_read,
		.dst_stages = pipeline_stage_flag::color_attachment_output,
		.dst_access = { access_flag::color_attachment_write, access_flag::color_attachment_read },
		.image = src_image,
		.aspects = image_aspect_flag::color,
	};

	const image_barrier dst_to_read{
		.src_stages = pipeline_stage_flag::transfer,
		.src_access = access_flag::transfer_write,
		.dst_stages = { pipeline_stage_flag::compute_shader, pipeline_stage_flag::fragment_shader },
		.dst_access = access_flag::shader_sampled_read,
		.image = dst.handle(),
		.aspects = image_aspect_flag::color,
	};

	const std::array post_barriers = { src_back, dst_to_read };
	m_recorder.pipeline_barrier(dependency_info{
		.image_barriers = post_barriers
	});
}

auto gse::gpu::recording_context::set_viewport(const float x, const float y, const float width, const float height, const float min_depth, const float max_depth) const -> void {
	check_active();
	m_recorder.set_viewport(
		viewport{
			.x = x,
			.y = y,
			.width = width,
			.height = height,
			.min_depth = min_depth,
			.max_depth = max_depth,
		}
	);
}

auto gse::gpu::recording_context::set_scissor(const std::int32_t x, const std::int32_t y, const std::uint32_t width, const std::uint32_t height) const -> void {
	check_active();
	const rect_t<vec2i> sc{ {
		.min = vec2i{ x, y },
		.max = vec2i{ x + static_cast<int>(width), y + static_cast<int>(height) },
	} };
	m_recorder.set_scissor(sc);
}

auto gse::gpu::recording_context::set_color_blend_enable(const std::uint32_t first_attachment, const std::span<const std::uint8_t> enables) const -> void {
	check_active();
	m_recorder.set_color_blend_enable(first_attachment, enables);
}

auto gse::gpu::recording_context::set_color_blend_equation(const std::uint32_t first_attachment, const std::span<const color_blend_equation> equations) const -> void {
	check_active();
	m_recorder.set_color_blend_equation(first_attachment, equations);
}

auto gse::gpu::recording_context::draw(const std::uint32_t vertex_count, const std::uint32_t instance_count, const std::uint32_t first_vertex, const std::uint32_t first_instance) -> void {
	check_active();
	flush_pending_barriers();
	m_recorder.draw(vertex_count, instance_count, first_vertex, first_instance);
}

auto gse::gpu::recording_context::draw_indexed(const std::uint32_t index_count, const std::uint32_t instance_count, const std::uint32_t first_index, const std::int32_t vertex_offset, const std::uint32_t first_instance) -> void {
	check_active();
	flush_pending_barriers();
	m_recorder.draw_indexed(index_count, instance_count, first_index, vertex_offset, first_instance);
}

auto gse::gpu::recording_context::draw_mesh_tasks(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) -> void {
	check_active();
	flush_pending_barriers();
	m_recorder.draw_mesh_tasks(x, y, z);
}

auto gse::gpu::recording_context::dispatch(const std::uint32_t x, const std::uint32_t y, const std::uint32_t z) -> void {
	check_active();
	flush_pending_barriers();
	m_recorder.dispatch(x, y, z);
}

auto gse::gpu::recording_context::dispatch_indirect(const buffer& buf, const std::size_t offset) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::bit_cast<const void*>(buf.handle()),
			.type = resource_type::buffer,
			.buffer_size = buf.size(),
			.host_buffer = std::addressof(buf),
		},
		pipeline_stage_flag::draw_indirect,
		access_flag::indirect_command_read
	);
	flush_pending_barriers();
	m_recorder.dispatch_indirect(buf.handle(), static_cast<device_size>(offset));
}

auto gse::gpu::recording_context::draw_indirect(const buffer& buf, const std::size_t offset, const std::uint32_t draw_count, const std::uint32_t stride) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::bit_cast<const void*>(buf.handle()),
			.type = resource_type::buffer,
			.buffer_size = buf.size(),
			.host_buffer = std::addressof(buf),
		},
		pipeline_stage_flag::draw_indirect,
		access_flag::indirect_command_read
	);
	flush_pending_barriers();
	m_recorder.draw_indexed_indirect(buf.handle(), offset, draw_count, stride);
}

auto gse::gpu::recording_context::draw_mesh_tasks_indirect(const buffer& buf, const std::size_t offset, const std::uint32_t draw_count, const std::uint32_t stride) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::bit_cast<const void*>(buf.handle()),
			.type = resource_type::buffer,
			.buffer_size = buf.size(),
			.host_buffer = std::addressof(buf),
		},
		pipeline_stage_flag::draw_indirect,
		access_flag::indirect_command_read
	);
	flush_pending_barriers();
	m_recorder.draw_mesh_tasks_indirect(buf.handle(), offset, draw_count, stride);
}

auto gse::gpu::recording_context::bind_index(const buffer& buf, const index_type type, const std::size_t offset) -> void {
	check_active();
	note_touched(
		{
			.ptr = std::bit_cast<const void*>(buf.handle()),
			.type = resource_type::buffer,
			.buffer_size = buf.size(),
			.host_buffer = std::addressof(buf),
		},
		pipeline_stage_flag::index_input,
		access_flag::index_read
	);
	flush_pending_barriers();
	m_recorder.bind_index_buffer_2(buf.handle(), offset, whole_size, type);
}

auto gse::gpu::recording_context::set_viewport(const vec2u extent) const -> void {
	check_active();
	m_recorder.set_viewport(
		viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(extent.x()),
			.height = static_cast<float>(extent.y()),
			.min_depth = 0.0f,
			.max_depth = 1.0f,
		}
	);
}

auto gse::gpu::recording_context::set_scissor(const vec2u extent) const -> void {
	check_active();
	const rect_t<vec2i> sc{ {
		.min = vec2i{ 0, 0 },
		.max = vec2i{ static_cast<int>(extent.x()), static_cast<int>(extent.y()) },
	} };
	m_recorder.set_scissor(sc);
}

auto gse::gpu::recording_context::bind(const shader_program& p) -> void {
	check_active();

	m_bound_is_compute = p.is_compute();

	if (p.is_compute()) {
		m_recorder.bind_shaders(p.stages(), p.shader_handles());
	}
	else {
		constexpr std::array all_graphics_stages = {
			stage_flag::vertex,
			stage_flag::fragment,
			stage_flag::task,
			stage_flag::mesh,
		};
		std::array<handle<shader_object>, 4> bound{};
		const auto stages = p.stages();
		const auto handles = p.shader_handles();
		for (std::size_t i = 0; i < all_graphics_stages.size(); ++i) {
			for (std::size_t j = 0; j < stages.size(); ++j) {
				if (stages[j] == all_graphics_stages[i]) {
					bound[i] = handles[j];
					break;
				}
			}
		}
		m_recorder.bind_shaders(all_graphics_stages, bound);
		if (bound[0] && !m_state_cache.vertex_input_set) {
			m_recorder.set_vertex_input_none();
			m_state_cache.vertex_input_set = true;
		}
		apply_dynamic_state(p.state());
	}

	ensure_descriptor_heaps();
}

auto gse::gpu::recording_context::apply_dynamic_state(const dynamic_pipeline_state& s) -> void {
	if (!m_state_cache.topology || *m_state_cache.topology != s.topology) {
		m_recorder.set_topology(s.topology);
		m_state_cache.topology = s.topology;
	}
	if (!m_state_cache.polygon_mode || *m_state_cache.polygon_mode != s.polygon) {
		m_recorder.set_polygon_mode(s.polygon);
		m_state_cache.polygon_mode = s.polygon;
	}
	if (!m_state_cache.cull_mode || *m_state_cache.cull_mode != s.cull) {
		m_recorder.set_cull_mode(s.cull);
		m_state_cache.cull_mode = s.cull;
	}
	if (!m_state_cache.front_face || *m_state_cache.front_face != s.front) {
		m_recorder.set_front_face(s.front);
		m_state_cache.front_face = s.front;
	}
	if (!m_state_cache.depth_test_enable || *m_state_cache.depth_test_enable != s.depth.test) {
		m_recorder.set_depth_test_enable(s.depth.test);
		m_state_cache.depth_test_enable = s.depth.test;
	}
	if (!m_state_cache.depth_write_enable || *m_state_cache.depth_write_enable != s.depth.write) {
		m_recorder.set_depth_write_enable(s.depth.write);
		m_state_cache.depth_write_enable = s.depth.write;
	}
	if (!m_state_cache.depth_compare_op || *m_state_cache.depth_compare_op != s.depth.compare) {
		m_recorder.set_depth_compare_op(s.depth.compare);
		m_state_cache.depth_compare_op = s.depth.compare;
	}
	if (!m_state_cache.depth_bias_enable || *m_state_cache.depth_bias_enable != s.depth_bias_enable) {
		m_recorder.set_depth_bias_enable(s.depth_bias_enable);
		m_state_cache.depth_bias_enable = s.depth_bias_enable;
	}
	if (s.depth_bias_enable) {
		m_recorder.set_depth_bias(s.depth_bias_constant, s.depth_bias_clamp, s.depth_bias_slope);
	}
	if (!m_state_cache.depth_clamp_enable || *m_state_cache.depth_clamp_enable != s.depth_clamp_enable) {
		m_recorder.set_depth_clamp_enable(s.depth_clamp_enable);
		m_state_cache.depth_clamp_enable = s.depth_clamp_enable;
	}
	if (!m_state_cache.rasterizer_discard_enable || *m_state_cache.rasterizer_discard_enable != s.rasterizer_discard_enable) {
		m_recorder.set_rasterizer_discard_enable(s.rasterizer_discard_enable);
		m_state_cache.rasterizer_discard_enable = s.rasterizer_discard_enable;
	}
	if (!m_state_cache.primitive_restart_enable || *m_state_cache.primitive_restart_enable != s.primitive_restart_enable) {
		m_recorder.set_primitive_restart_enable(s.primitive_restart_enable);
		m_state_cache.primitive_restart_enable = s.primitive_restart_enable;
	}
	if (!m_state_cache.alpha_to_coverage_enable || *m_state_cache.alpha_to_coverage_enable != s.alpha_to_coverage_enable) {
		m_recorder.set_alpha_to_coverage_enable(s.alpha_to_coverage_enable);
		m_state_cache.alpha_to_coverage_enable = s.alpha_to_coverage_enable;
	}
	if (!m_state_cache.alpha_to_one_enable || *m_state_cache.alpha_to_one_enable != s.alpha_to_one_enable) {
		m_recorder.set_alpha_to_one_enable(s.alpha_to_one_enable);
		m_state_cache.alpha_to_one_enable = s.alpha_to_one_enable;
	}
	if (!m_state_cache.logic_op_enable || *m_state_cache.logic_op_enable != s.logic_op_enable) {
		m_recorder.set_logic_op_enable(s.logic_op_enable);
		m_state_cache.logic_op_enable = s.logic_op_enable;
	}

	m_recorder.set_rasterization_samples(s.samples);
	m_recorder.set_sample_mask(s.samples, s.sample_mask);
	m_recorder.set_depth_bounds_test_enable(false);
	m_recorder.set_stencil_test_enable(false);
	m_recorder.set_line_width(1.0f);

	if (!s.blend_enables.empty()) {
		m_recorder.set_color_blend_enable(0, s.blend_enables);
	}
	if (!s.blend_equations.empty()) {
		m_recorder.set_color_blend_equation(0, s.blend_equations);
	}
	if (!s.color_write_masks.empty()) {
		m_recorder.set_color_write_mask(0, s.color_write_masks);
	}
}