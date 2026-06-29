module gse.dx12:device_impl;

import std;
import gse.gpu_backend;
import gse.core;
import gse.os;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.win32;
import gse.directx;
import gse.log;

import :conversions;
import :pipeline;
import :device;

auto gse::dx12::build_graphics_pipeline_desc(const gfx_template& tmpl, const graphics_pass_state& pass, directx::ID3D12RootSignature* root_signature) -> directx::graphics_pipeline_desc {
	const auto& s = tmpl.state;

	const auto fill_of = [](const gpu::polygon_mode m) -> directx::D3D12_FILL_MODE {
		return m == gpu::polygon_mode::fill ? directx::fill_solid : directx::fill_wireframe;
	};
	const auto cull_of = [](const gpu::cull_mode m) -> directx::D3D12_CULL_MODE {
		switch (m) {
			case gpu::cull_mode::front: return directx::cull_front;
			case gpu::cull_mode::back: return directx::cull_back;
			default: return directx::cull_none;
		}
	};
	const auto compare_of = [](const gpu::compare_op op) -> directx::D3D12_COMPARISON_FUNC {
		switch (op) {
			case gpu::compare_op::never: return directx::compare_never;
			case gpu::compare_op::less: return directx::compare_less;
			case gpu::compare_op::equal: return directx::compare_equal;
			case gpu::compare_op::less_or_equal: return directx::compare_less_equal;
			case gpu::compare_op::greater: return directx::compare_greater;
			case gpu::compare_op::not_equal: return directx::compare_not_equal;
			case gpu::compare_op::greater_or_equal: return directx::compare_greater_equal;
			default: return directx::compare_always;
		}
	};
	const auto factor_of = [](const gpu::blend_factor f) -> directx::D3D12_BLEND {
		switch (f) {
			case gpu::blend_factor::zero: return directx::blend_zero;
			case gpu::blend_factor::one: return directx::blend_one;
			case gpu::blend_factor::src_color: return directx::blend_src_color;
			case gpu::blend_factor::one_minus_src_color: return directx::blend_inv_src_color;
			case gpu::blend_factor::dst_color: return directx::blend_dst_color;
			case gpu::blend_factor::one_minus_dst_color: return directx::blend_inv_dst_color;
			case gpu::blend_factor::src_alpha: return directx::blend_src_alpha;
			case gpu::blend_factor::one_minus_src_alpha: return directx::blend_inv_src_alpha;
			case gpu::blend_factor::dst_alpha: return directx::blend_dst_alpha;
			default: return directx::blend_inv_dst_alpha;
		}
	};
	const auto op_of = [](const gpu::blend_op o) -> directx::D3D12_BLEND_OP {
		switch (o) {
			case gpu::blend_op::add: return directx::blend_op_add;
			case gpu::blend_op::subtract: return directx::blend_op_subtract;
			case gpu::blend_op::reverse_subtract: return directx::blend_op_reverse_subtract;
			case gpu::blend_op::min: return directx::blend_op_min;
			default: return directx::blend_op_max;
		}
	};
	const auto topology_type_of = [](const gpu::topology t) -> directx::D3D12_PRIMITIVE_TOPOLOGY_TYPE {
		switch (t) {
			case gpu::topology::line_list: return directx::topology_type_line;
			case gpu::topology::point_list: return directx::topology_type_point;
			default: return directx::topology_type_triangle;
		}
	};
	const auto sample_count_of = [](const gpu::sample_count c) -> std::uint32_t {
		switch (c) {
			case gpu::sample_count::e2: return 2;
			case gpu::sample_count::e4: return 4;
			case gpu::sample_count::e8: return 8;
			case gpu::sample_count::e16: return 16;
			default: return 1;
		}
	};

	const bool has_depth = pass.dsv_format != directx::format_unknown;

	directx::graphics_pipeline_desc desc = {
		.root_signature = root_signature,
		.vs = tmpl.vs.data(),
		.vs_size = tmpl.vs.size(),
		.ps = tmpl.ps.data(),
		.ps_size = tmpl.ps.size(),
		.as = tmpl.task.data(),
		.as_size = tmpl.task.size(),
		.ms = tmpl.mesh.data(),
		.ms_size = tmpl.mesh.size(),
		.fill_mode = fill_of(s.polygon),
		.cull_mode = cull_of(s.cull),
		.front_counter_clockwise = s.front == gpu::front_face::counter_clockwise,
		.depth_clip_enable = !s.depth_clamp_enable,
		.depth_bias = static_cast<std::int32_t>(s.depth_bias_constant),
		.depth_bias_clamp = s.depth_bias_clamp,
		.depth_bias_slope = s.depth_bias_slope,
		.depth_enable = has_depth && s.depth.test,
		.depth_write = has_depth && s.depth.write,
		.depth_func = compare_of(s.depth.compare),
		.topology_type = topology_type_of(s.topology),
		.rtv_count = pass.rtv_count,
		.dsv_format = pass.dsv_format,
		.sample_count = sample_count_of(s.samples),
		.alpha_to_coverage = s.alpha_to_coverage_enable,
	};

	for (std::uint32_t i = 0; i < pass.rtv_count && i < 8; ++i) {
		desc.rtv_formats[i] = pass.rtv_formats[i];
		directx::render_target_blend blend;
		blend.blend_enable = i < s.blend_enables.size() && s.blend_enables[i] != 0;
		if (i < s.blend_equations.size()) {
			const auto& eq = s.blend_equations[i];
			blend.src_blend = factor_of(eq.src_color);
			blend.dst_blend = factor_of(eq.dst_color);
			blend.blend_op = op_of(eq.color_op);
			blend.src_blend_alpha = factor_of(eq.src_alpha);
			blend.dst_blend_alpha = factor_of(eq.dst_alpha);
			blend.blend_op_alpha = op_of(eq.alpha_op);
		}
		if (i < s.color_write_masks.size()) {
			blend.write_mask = static_cast<std::uint8_t>(s.color_write_masks[i].bits());
		}
		desc.rtv_blends[i] = blend;
	}

	return desc;
}

gse::dx12::device::device(const shared_view<window::data> win, const bool enable_validation, gpu::device_settings&) {
	m_validation_enabled = enable_validation;
	log::println(log::category::dx12, "ctor begin");

	if (m_validation_enabled) {
		directx::enable_debug_layer();
	}
	m_factory = directx::create_factory();
	m_device = directx::create_device();
	if (m_validation_enabled) {
		directx::disable_debug_break(m_device.get());
	}
	log::println(log::category::dx12, "factory={} device={}", static_cast<void*>(m_factory.get()), static_cast<void*>(m_device.get()));

	m_graphics_queue = directx::create_direct_queue(m_device.get());
	m_hwnd = win32::hwnd_from_glfw_window(window::raw_handle(win).value);
	log::println(log::category::dx12, "queue={} hwnd={}", static_cast<void*>(m_graphics_queue.get()), m_hwnd);

	m_idle_fence = directx::create_fence(m_device.get(), 0);
	m_idle_event = directx::create_wait_event();

	m_frames.resize(3);
	for (auto& f : m_frames) {
		f.allocator = directx::create_command_allocator(m_device.get());
		f.list = directx::create_command_list(m_device.get(), f.allocator.get());
	}

	init_bindless();

	constexpr std::uint32_t rtv_view_capacity = 1024;
	constexpr std::uint32_t dsv_view_capacity = 256;
	m_rtv_view_heap = directx::create_rtv_heap(m_device.get(), rtv_view_capacity);
	m_dsv_view_heap = directx::create_dsv_heap(m_device.get(), dsv_view_capacity);
	m_rtv_size = directx::rtv_descriptor_size(m_device.get());
	m_dsv_size = directx::dsv_descriptor_size(m_device.get());

	active_device = this;

	log::println(log::category::dx12, "ctor end");
}

auto gse::dx12::device::init_bindless() -> void {
	{
		const auto probe = directx::create_gpu_upload_buffer(m_device.get(), 256);
		m_gpu_upload_supported = static_cast<bool>(probe);
	}
	log::println(log::category::dx12, "gpu_upload_supported={}", m_gpu_upload_supported);

	constexpr std::uint32_t texture_capacity = 1024;
	constexpr std::uint32_t image_capacity = 65536;
	constexpr std::uint32_t buffer_capacity = 16384;
	constexpr std::uint32_t sampler_capacity = 512;

	m_cbv_srv_uav_size = directx::cbv_srv_uav_descriptor_size(m_device.get());
	m_sampler_size = directx::sampler_descriptor_size(m_device.get());

	const auto image_size = static_cast<gpu::device_size>(m_cbv_srv_uav_size);
	const auto sampler_size = static_cast<gpu::device_size>(m_sampler_size);

	const gpu::device_size texture_image_offset = 0;
	const gpu::device_size image_range_offset = texture_image_offset + texture_capacity * image_size;
	const gpu::device_size buffer_range_offset = image_range_offset + image_capacity * image_size;
	const auto resource_count = texture_capacity + image_capacity + buffer_capacity;

	const gpu::device_size texture_sampler_offset = 0;
	const gpu::device_size sampler_range_offset = texture_sampler_offset + texture_capacity * sampler_size;
	const auto sampler_count = texture_capacity + sampler_capacity;

	m_resource_heap = directx::create_cbv_srv_uav_heap(m_device.get(), resource_count, true);
	m_sampler_heap = directx::create_sampler_heap(m_device.get(), sampler_count, true);

	m_bindless_layout = {
		.image_range_offset = image_range_offset,
		.image_stride = image_size,
		.texture_image_offset = texture_image_offset,
		.buffer_range_offset = buffer_range_offset,
		.buffer_stride = image_size,
		.texture_sampler_offset = texture_sampler_offset,
		.sampler_range_offset = sampler_range_offset,
		.sampler_stride = sampler_size,
	};
	m_resource_binding = {
		.address = directx::descriptor_heap_gpu_start(m_resource_heap.get()).ptr,
		.size = resource_count * image_size,
		.reserved_offset = 0,
		.reserved_size = 0,
	};
	m_sampler_binding = {
		.address = directx::descriptor_heap_gpu_start(m_sampler_heap.get()).ptr,
		.size = sampler_count * sampler_size,
		.reserved_offset = 0,
		.reserved_size = 0,
	};

	m_image_pool.base_offset = image_range_offset;
	m_image_pool.stride = image_size;
	m_image_pool.base_index = texture_capacity;
	m_image_pool.reset(image_capacity);

	m_buffer_pool.base_offset = buffer_range_offset;
	m_buffer_pool.stride = image_size;
	m_buffer_pool.base_index = texture_capacity + image_capacity;
	m_buffer_pool.reset(buffer_capacity);

	m_texture_pool.reset(texture_capacity);

	m_sampler_pool.base_offset = sampler_range_offset;
	m_sampler_pool.stride = sampler_size;
	m_sampler_pool.base_index = texture_capacity;
	m_sampler_pool.reset(sampler_capacity);

	m_pipeline_layout = create_bindless_pipeline_layout(m_device.get());
}

auto gse::dx12::device::write_sampler_at(const gpu::device_size byte_offset, const gpu::sampler_desc& desc) const -> void {
	const directx::sampler_params params = {
		.min_linear = desc.min == gpu::sampler_filter::linear,
		.mag_linear = desc.mag == gpu::sampler_filter::linear,
		.mip_linear = desc.min == gpu::sampler_filter::linear,
		.anisotropy = desc.max_anisotropy > 0.0f,
		.comparison = desc.compare_enable,
		.max_anisotropy = desc.max_anisotropy > 0.0f ? std::min(static_cast<std::uint32_t>(desc.max_anisotropy), 16u) : 1u,
		.comparison_func = static_cast<std::uint32_t>(desc.compare),
		.address_u = static_cast<std::uint32_t>(desc.address_u),
		.address_v = static_cast<std::uint32_t>(desc.address_v),
		.address_w = static_cast<std::uint32_t>(desc.address_w),
		.border = static_cast<std::uint32_t>(desc.border),
		.min_lod = desc.min_lod,
		.max_lod = desc.max_lod,
	};
	const directx::D3D12_CPU_DESCRIPTOR_HANDLE handle = {
		.ptr = directx::descriptor_heap_cpu_start(m_sampler_heap.get()).ptr + static_cast<std::size_t>(byte_offset),
	};
	directx::create_sampler_descriptor(m_device.get(), params, handle);
}

auto gse::dx12::device::find_buffer(const gpu::device_address address) const -> std::pair<directx::ID3D12Resource*, gpu::device_address> {
	auto it = m_buffer_by_address.upper_bound(address);
	if (it == m_buffer_by_address.begin()) {
		return {};
	}
	--it;
	const auto base = it->first;
	const auto& [resource, buffer_size] = it->second;
	if (address >= base && address < base + buffer_size) {
		return { resource, base };
	}
	return {};
}

auto gse::dx12::device::graphics_state(directx::ID3D12GraphicsCommandList* list) -> graphics_pass_state& {
	static thread_local std::unordered_map<directx::ID3D12GraphicsCommandList*, graphics_pass_state> states;
	return states[list];
}

auto gse::dx12::device::resolve_graphics_pso(graphics_pass_state& pass) -> directx::ID3D12PipelineState* {
	if (pass.resolved_pso) {
		return pass.resolved_pso;
	}
	if (!pass.pending) {
		return nullptr;
	}
	const std::lock_guard lock(m_mutex);
	for (const auto& entry : m_graphics_psos) {
		if (entry.tmpl == pass.pending && entry.rtv_count == pass.rtv_count && entry.dsv_format == pass.dsv_format && entry.rtv_formats == pass.rtv_formats) {
			pass.resolved_pso = entry.pso.get();
			return pass.resolved_pso;
		}
	}
	const auto desc = build_graphics_pipeline_desc(*pass.pending, pass, m_pipeline_layout.root_signature());
	auto pso = pass.pending->is_mesh
		? directx::create_mesh_pipeline_state(m_device.get(), desc)
		: directx::create_graphics_pipeline_state(m_device.get(), desc);
	if (!pso) {
		log::println(log::level::error, log::category::dx12, "graphics PSO creation FAILED mesh={} rtv_count={} removed=0x{:08x}", pass.pending->is_mesh, pass.rtv_count, static_cast<std::uint32_t>(m_device->GetDeviceRemovedReason()));
		directx::drain_debug_messages(m_device.get(), [](void*, const char* message) {
			log::println(log::level::warning, log::category::dx12_validation, "{}", message);
		}, nullptr);
		return nullptr;
	}
	auto* raw = pso.get();
	m_graphics_psos.push_back(graphics_pso_entry{
		.tmpl = pass.pending,
		.rtv_count = pass.rtv_count,
		.rtv_formats = pass.rtv_formats,
		.dsv_format = pass.dsv_format,
		.pso = std::move(pso),
	});
	pass.resolved_pso = raw;
	return raw;
}

auto gse::dx12::device::view_format(const std::size_t descriptor_ptr) const -> directx::DXGI_FORMAT {
	const auto it = m_view_format.find(descriptor_ptr);
	return it == m_view_format.end() ? directx::format_unknown : it->second;
}

auto gse::dx12::device::handle() const -> gpu::device_handle {
	return std::bit_cast<gpu::device_handle>(m_device.get());
}

auto gse::dx12::device::queue_family(gpu::queue_type) const -> std::uint32_t {
	return 0;
}

auto gse::dx12::device::wait_idle() const -> void {
	const auto target = m_idle_fence->GetCompletedValue() + 1;
	m_graphics_queue->Signal(m_idle_fence.get(), target);
	directx::wait_fence(m_idle_fence.get(), target, m_idle_event);
}

auto gse::dx12::device::timestamp_period() const -> float {
	return 1.0f;
}

auto gse::dx12::device::record_buffer_fill_u32(gpu::command_buffer_handle, gpu::handle<gpu::buffer>, gpu::device_size, std::uint32_t) -> void {}

auto gse::dx12::device::cmd_reset(const gpu::command_buffer_handle cmd) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	for (auto& f : m_frames) {
		if (f.list.get() == list) {
			f.allocator->Reset();
			list->Reset(f.allocator.get(), nullptr);
			return;
		}
	}
}

auto gse::dx12::device::cmd_begin(gpu::command_buffer_handle) -> void {}

auto gse::dx12::device::cmd_end(const gpu::command_buffer_handle cmd) -> void {
	std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd)->Close();
}

auto gse::dx12::device::cmd_pipeline_barrier(const gpu::command_buffer_handle cmd, const gpu::dependency_info& dep) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	if (!list) {
		return;
	}
	static thread_local std::vector<directx::D3D12_RESOURCE_BARRIER> barriers;
	barriers.clear();
	barriers.reserve(dep.image_barriers.size() + dep.memory_barriers.size());
	for (const auto& mb : dep.memory_barriers) {
		const auto src = mb.src_access;
		const auto dst = mb.dst_access;
		const bool unordered_hazard =
			src.test(gpu::access_flag::acceleration_structure_write) ||
			src.test(gpu::access_flag::shader_storage_write) ||
			src.test(gpu::access_flag::shader_write) ||
			dst.test(gpu::access_flag::acceleration_structure_read) ||
			dst.test(gpu::access_flag::shader_storage_read) ||
			dst.test(gpu::access_flag::shader_storage_write) ||
			dst.test(gpu::access_flag::shader_write);
		if (!unordered_hazard) {
			continue;
		}
		barriers.push_back({
			.Type = directx::barrier_type_uav,
			.UAV = {
				.pResource = nullptr,
			},
		});
	}
	for (const auto& ib : dep.image_barriers) {
		auto* res = std::bit_cast<directx::ID3D12Resource*>(ib.image);
		if (!res) {
			continue;
		}
		const auto tracked = m_resource_states.find(res);
		const auto before = tracked != m_resource_states.end() ? tracked->second : directx::resource_state_common;
		const auto after = state_from_access(ib.dst_access);
		if (before == after) {
			continue;
		}
		barriers.push_back({
			.Type = directx::barrier_type_transition,
			.Transition = {
				.pResource = res,
				.Subresource = directx::resource_barrier_all_subresources,
				.StateBefore = before,
				.StateAfter = after,
			},
		});
		m_resource_states[res] = after;
	}
	if (!barriers.empty()) {
		list->ResourceBarrier(static_cast<std::uint32_t>(barriers.size()), barriers.data());
	}
}

auto gse::dx12::device::cmd_release_swapchain_to_present(const gpu::command_buffer_handle cmd, const gpu::handle<gpu::image> img, gpu::pipeline_stage_flags, gpu::access_flags) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	auto* res = std::bit_cast<directx::ID3D12Resource*>(img);
	const directx::D3D12_RESOURCE_BARRIER b = {
		.Type = directx::barrier_type_transition,
		.Transition = {
			.pResource = res,
			.Subresource = directx::resource_barrier_all_subresources,
			.StateBefore = directx::resource_state_render_target,
			.StateAfter = directx::resource_state_present,
		},
	};
	list->ResourceBarrier(1, &b);
	m_resource_states[res] = directx::resource_state_present;
}

auto gse::dx12::device::begin_debug_event(const gpu::command_buffer_handle cmd, const std::string_view label) -> void {
	if (auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd)) {
		directx::begin_event(list, label.data(), label.size());
	}
}

auto gse::dx12::device::end_debug_event(const gpu::command_buffer_handle cmd) -> void {
	if (auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd)) {
		directx::end_event(list);
	}
}

auto gse::dx12::device::cmd_begin_rendering(const gpu::command_buffer_handle cmd, const gpu::rendering_info& info) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	if (!list) {
		return;
	}
	static thread_local std::vector<directx::D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
	rtvs.clear();
	rtvs.reserve(info.color_attachments.size());

	graphics_pass_state pass;
	pass.rtv_count = static_cast<std::uint32_t>(info.color_attachments.size());
	for (std::size_t i = 0; i < info.color_attachments.size(); ++i) {
		const auto& att = info.color_attachments[i];
		const auto ptr = std::bit_cast<std::size_t>(att.image_view);
		const directx::D3D12_CPU_DESCRIPTOR_HANDLE rtv = { .ptr = ptr };
		rtvs.push_back(rtv);
		if (i < 8) {
			pass.rtv_formats[i] = view_format(ptr);
		}
		if (att.load == gpu::load_op::clear) {
			const std::array<float, 4> color = { att.color_clear_value.r, att.color_clear_value.g, att.color_clear_value.b, att.color_clear_value.a };
			list->ClearRenderTargetView(rtv, color.data(), 0, nullptr);
		}
	}

	directx::D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
	const directx::D3D12_CPU_DESCRIPTOR_HANDLE* dsv_ptr = nullptr;
	if (info.depth_attachment) {
		const auto ptr = std::bit_cast<std::size_t>(info.depth_attachment->image_view);
		dsv = { .ptr = ptr };
		dsv_ptr = &dsv;
		pass.dsv_format = view_format(ptr);
		if (info.depth_attachment->load == gpu::load_op::clear) {
			list->ClearDepthStencilView(dsv, directx::clear_flag_depth, info.depth_attachment->depth_clear_value.depth, 0, 0, nullptr);
		}
	}

	list->OMSetRenderTargets(static_cast<std::uint32_t>(rtvs.size()), rtvs.empty() ? nullptr : rtvs.data(), false, dsv_ptr);

	auto& state = graphics_state(list);
	state.rtv_count = pass.rtv_count;
	state.rtv_formats = pass.rtv_formats;
	state.dsv_format = pass.dsv_format;
	state.resolved_pso = nullptr;
}

auto gse::dx12::device::cmd_bind_graphics_shaders(const gpu::command_buffer_handle cmd, const gpu::handle<gpu::shader_object> shader) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	if (!list) {
		return;
	}
	auto* tmpl = std::bit_cast<gfx_template*>(shader);
	auto& state = graphics_state(list);
	state.pending = tmpl;
	state.push_size = tmpl ? tmpl->push_size : 0;
	state.resolved_pso = nullptr;
}

auto gse::dx12::device::note_compute_push_size(const gpu::command_buffer_handle cmd, const gpu::handle<gpu::shader_object> shader) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	if (!list) {
		return;
	}
	auto* pso = std::bit_cast<directx::ID3D12PipelineState*>(shader);
	std::uint32_t size = 0;
	if (const auto it = m_pso_push_size.find(pso); it != m_pso_push_size.end()) {
		size = it->second;
	}
	auto& state = graphics_state(list);
	state.push_size = size;
	state.compute_pso_bound = true;
}

auto gse::dx12::device::compute_pso_bound(const gpu::command_buffer_handle cmd) -> bool {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	if (!list) {
		return false;
	}
	return graphics_state(list).compute_pso_bound;
}

auto gse::dx12::device::list_push_size(const gpu::command_buffer_handle cmd) -> std::uint32_t {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	if (!list) {
		return 0;
	}
	return graphics_state(list).push_size;
}

auto gse::dx12::device::cmd_set_viewport(const gpu::command_buffer_handle cmd, const gpu::viewport& viewport) -> void {
	if (auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd)) {
		directx::set_viewport(list, viewport.x, viewport.y + viewport.height, viewport.width, -viewport.height, viewport.min_depth, viewport.max_depth);
	}
}

auto gse::dx12::device::cmd_set_scissor(const gpu::command_buffer_handle cmd, const gse::rect_t<vec2i>& scissor) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	if (!list) {
		return;
	}
	const auto min = scissor.min();
	const auto size = scissor.size();
	directx::set_scissor(list, min.x(), min.y(), min.x() + size.x(), min.y() + size.y());
}

auto gse::dx12::device::cmd_bind_index_buffer(const gpu::command_buffer_handle cmd, const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset, const gpu::device_size size, const gpu::index_type type) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	auto* resource = std::bit_cast<directx::ID3D12Resource*>(buffer);
	if (!list || !resource) {
		return;
	}
	const auto base = directx::gpu_address(resource);
	const auto width = size == gpu::whole_size ? directx::resource_byte_width(resource) - offset : size;
	directx::set_index_buffer(list, base + offset, static_cast<std::uint32_t>(width), type == gpu::index_type::uint32);
}

auto gse::dx12::device::cmd_draw(const gpu::command_buffer_handle cmd, const std::uint32_t vertex_count, const std::uint32_t instance_count, const std::uint32_t first_vertex, const std::uint32_t first_instance) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	if (!list) {
		return;
	}
	auto& state = graphics_state(list);
	auto* pso = resolve_graphics_pso(state);
	if (!pso) {
		return;
	}
	list->SetPipelineState(pso);
	list->IASetPrimitiveTopology(primitive_topology_of(state.pending->state.topology));
	list->DrawInstanced(vertex_count, instance_count, first_vertex, first_instance);
}

auto gse::dx12::device::cmd_draw_indexed(const gpu::command_buffer_handle cmd, const std::uint32_t index_count, const std::uint32_t instance_count, const std::uint32_t first_index, const std::int32_t vertex_offset, const std::uint32_t first_instance) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	if (!list) {
		return;
	}
	auto& state = graphics_state(list);
	auto* pso = resolve_graphics_pso(state);
	if (!pso) {
		return;
	}
	list->SetPipelineState(pso);
	list->IASetPrimitiveTopology(primitive_topology_of(state.pending->state.topology));
	list->DrawIndexedInstanced(index_count, instance_count, first_index, vertex_offset, first_instance);
}

auto gse::dx12::device::cmd_draw_indexed_indirect(const gpu::command_buffer_handle cmd, const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset, const std::uint32_t draw_count, const std::uint32_t stride) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	auto* resource = std::bit_cast<directx::ID3D12Resource*>(buffer);
	if (!list || !resource) {
		return;
	}
	auto& state = graphics_state(list);
	auto* pso = resolve_graphics_pso(state);
	if (!pso) {
		return;
	}
	if (!m_draw_indexed_signature) {
		m_draw_indexed_signature = directx::create_draw_indexed_command_signature(m_device.get(), stride);
	}
	list->SetPipelineState(pso);
	list->IASetPrimitiveTopology(primitive_topology_of(state.pending->state.topology));
	directx::execute_indirect(list, m_draw_indexed_signature.get(), draw_count, resource, offset);
}

auto gse::dx12::device::cmd_draw_mesh_tasks(const gpu::command_buffer_handle cmd, const std::uint32_t group_count_x, const std::uint32_t group_count_y, const std::uint32_t group_count_z) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	if (!list) {
		return;
	}
	auto& state = graphics_state(list);
	auto* pso = resolve_graphics_pso(state);
	if (!pso) {
		return;
	}
	list->SetPipelineState(pso);
	directx::dispatch_mesh(list, group_count_x, group_count_y, group_count_z);
}

auto gse::dx12::device::cmd_draw_mesh_tasks_indirect(const gpu::command_buffer_handle cmd, const gpu::handle<gpu::buffer> buffer, const gpu::device_size offset, const std::uint32_t draw_count, const std::uint32_t stride) -> void {
	auto* list = std::bit_cast<directx::ID3D12GraphicsCommandList*>(cmd);
	auto* resource = std::bit_cast<directx::ID3D12Resource*>(buffer);
	if (!list || !resource) {
		return;
	}
	auto& state = graphics_state(list);
	auto* pso = resolve_graphics_pso(state);
	if (!pso) {
		return;
	}
	if (!m_dispatch_mesh_signature) {
		m_dispatch_mesh_signature = directx::create_dispatch_mesh_command_signature(m_device.get(), stride);
	}
	list->SetPipelineState(pso);
	directx::execute_indirect(list, m_dispatch_mesh_signature.get(), draw_count, resource, offset);
}

auto gse::dx12::device::frame_command_buffer(gpu::queue_type, const std::uint32_t frame_index) const -> gpu::command_buffer_handle {
	if (m_frames.empty()) {
		return {};
	}
	return std::bit_cast<gpu::command_buffer_handle>(m_frames[frame_index % m_frames.size()].list.get());
}

auto gse::dx12::device::graphics_queue() const -> directx::ID3D12CommandQueue* {
	return m_graphics_queue.get();
}

auto gse::dx12::device::validation_enabled() const -> bool {
	return m_validation_enabled;
}

auto gse::dx12::device::idle_event() const -> void* {
	return m_idle_event;
}

auto gse::dx12::device::factory() const -> directx::IDXGIFactory4* {
	return m_factory.get();
}

auto gse::dx12::device::hwnd() const -> void* {
	return m_hwnd;
}

auto gse::dx12::device::register_view_format(const std::size_t descriptor_ptr, const directx::DXGI_FORMAT format) -> void {
	m_view_format[descriptor_ptr] = format;
}

auto gse::dx12::device::create_image_unbound(const gpu::image_create_info& info) const -> std::pair<gpu::handle<gpu::image>, gpu::memory_requirements> {
	const std::lock_guard lock(m_mutex);
	const bool is_3d = info.extent.z() > 1;
	const auto dimension = is_3d ? directx::dimension_texture_3d : directx::dimension_texture_2d;
	const std::uint32_t depth_or_layers = is_3d ? info.extent.z() : info.array_layers;

	int flag_bits = static_cast<int>(directx::resource_flag_none);
	if (info.usage.test(gpu::image_flag::color_attachment)) {
		flag_bits |= static_cast<int>(directx::resource_flag_allow_render_target);
	}
	if (info.usage.test(gpu::image_flag::depth_attachment)) {
		flag_bits |= static_cast<int>(directx::resource_flag_allow_depth_stencil);
	}
	if (info.usage.test(gpu::image_flag::storage)) {
		flag_bits |= static_cast<int>(directx::resource_flag_allow_unordered_access);
	}
	const auto flags = static_cast<directx::D3D12_RESOURCE_FLAGS>(flag_bits);

	auto resource = directx::create_committed_texture(m_device.get(), dimension, resource_format_of(info.format), info.extent.x(), info.extent.y(), depth_or_layers, info.mip_levels, flags);
	if (!resource) {
		return {};
	}
	auto* raw = resource.get();
	m_owned_images.push_back(std::move(resource));

	const auto texels = static_cast<gpu::device_size>(info.extent.x()) * info.extent.y() * std::max(info.extent.z(), info.array_layers);
	return {
		std::bit_cast<gpu::handle<gpu::image>>(raw),
		gpu::memory_requirements{
			.size = texels * 8 * info.mip_levels,
			.alignment = 65536,
			.memory_type_bits = 1,
		},
	};
}

auto gse::dx12::device::create_buffer_unbound(const gpu::buffer_desc& info) const -> std::pair<gpu::handle<gpu::buffer>, gpu::memory_requirements> {
	const std::lock_guard lock(m_mutex);
	auto resource = m_gpu_upload_supported
		? directx::create_gpu_upload_buffer(m_device.get(), info.size)
		: directx::create_upload_buffer(m_device.get(), info.size);
	if (!resource) {
		return {};
	}
	const auto address = directx::gpu_address(resource.get());
	auto* raw = resource.get();
	m_buffer_by_address.emplace(address, std::pair{ raw, info.size });
	m_owned_buffers.push_back(std::move(resource));
	return {
		std::bit_cast<gpu::handle<gpu::buffer>>(raw),
		gpu::memory_requirements{
			.size = info.size,
			.alignment = 65536,
			.memory_type_bits = 1,
		},
	};
}

auto gse::dx12::device::bind_image_memory(gpu::handle<gpu::image>, gpu::device_memory, gpu::device_size) const -> void {}

auto gse::dx12::device::bind_buffer_memory(gpu::handle<gpu::buffer>, gpu::device_memory, gpu::device_size) const -> void {}

auto gse::dx12::device::create_image_view(const gpu::handle<gpu::image> img, const gpu::image_view_create_info& info) const -> gpu::handle<gpu::image_view> {
	auto* resource = std::bit_cast<directx::ID3D12Resource*>(img);
	if (!resource) {
		return {};
	}
	const std::lock_guard lock(m_mutex);
	const auto format = dxgi_format_of(info.format);
	if (info.aspects.test(gpu::image_aspect_flag::depth)) {
		const auto handle = directx::offset_cpu_handle(directx::descriptor_heap_cpu_start(m_dsv_view_heap.get()), m_dsv_view_next++, m_dsv_size);
		directx::create_depth_stencil_view(m_device.get(), resource, format, handle);
		m_view_format[handle.ptr] = format;
		return std::bit_cast<gpu::handle<gpu::image_view>>(handle.ptr);
	}
	const auto handle = directx::offset_cpu_handle(directx::descriptor_heap_cpu_start(m_rtv_view_heap.get()), m_rtv_view_next++, m_rtv_size);
	directx::create_render_target_view(m_device.get(), resource, handle);
	m_view_format[handle.ptr] = format;
	return std::bit_cast<gpu::handle<gpu::image_view>>(handle.ptr);
}

auto gse::dx12::device::allocate_aliased_memory(gpu::device_size, std::uint32_t) const -> gpu::device_memory {
	const std::lock_guard lock(m_mutex);
	return gpu::device_memory{ .value = ++m_aliased_counter };
}

auto gse::dx12::device::free_aliased_memory(gpu::device_memory) const -> void {}

auto gse::dx12::device::find_memory_type_index(std::uint32_t, gpu::memory_property_flags) const -> std::uint32_t {
	return 0;
}

auto gse::dx12::device::host_upload_image_layers(const gpu::handle<gpu::image> img, const std::span<const void* const> layer_pointers, vec2u) const -> void {
	const std::lock_guard lock(m_mutex);
	auto* resource = std::bit_cast<directx::ID3D12Resource*>(img);
	if (!resource || layer_pointers.empty()) {
		return;
	}
	directx::upload_texture(m_device.get(), m_graphics_queue.get(), m_idle_fence.get(), m_idle_event, resource, layer_pointers.data(), static_cast<std::uint32_t>(layer_pointers.size()));
}

auto gse::dx12::device::raw_device() const -> directx::ID3D12Device* {
	return m_device.get();
}

auto gse::dx12::device::reset_acquired_list(directx::ID3D12GraphicsCommandList* list) -> void {
	graphics_state(list).compute_pso_bound = false;
}

auto gse::dx12::device::create_shader_program(const gpu::shader_program_create_info& info) -> gpu::shader_program {
	const std::lock_guard lock(m_mutex);
	std::vector<gpu::stage_flag> stages;
	std::vector<gpu::handle<gpu::shader_object>> shader_handles;
	if (info.is_compute && !info.stages.empty()) {
		const auto& cs = info.stages[0];
		auto pso = directx::create_compute_pipeline_state(m_device.get(), m_pipeline_layout.root_signature(), cs.spirv.data(), cs.spirv.size() * sizeof(std::uint32_t));
		if (pso) {
			stages.push_back(gpu::stage_flag::compute);
			shader_handles.push_back(std::bit_cast<gpu::handle<gpu::shader_object>>(pso.get()));
			m_pso_push_size[pso.get()] = info.push_offset_start;
			m_owned_psos.push_back(std::move(pso));
		}
		else {
			log::println(log::level::error, log::category::dx12, "compute PSO creation FAILED removed=0x{:08x}", static_cast<std::uint32_t>(m_device->GetDeviceRemovedReason()));
			directx::drain_debug_messages(m_device.get(), [](void*, const char* message) {
				log::println(log::level::warning, log::category::dx12_validation, "{}", message);
			}, nullptr);
		}
	}
	else if (!info.stages.empty()) {
		gfx_template tmpl;
		tmpl.state = info.state;
		tmpl.is_mesh = info.is_mesh;
		tmpl.push_size = info.push_offset_start;
		for (const auto& stage : info.stages) {
			const auto* bytes = reinterpret_cast<const std::byte*>(stage.spirv.data());
			std::vector<std::byte> blob(bytes, bytes + stage.spirv.size() * sizeof(std::uint32_t));
			switch (stage.stage) {
				case gpu::stage_flag::vertex: tmpl.vs = std::move(blob); break;
				case gpu::stage_flag::fragment: tmpl.ps = std::move(blob); break;
				case gpu::stage_flag::mesh: tmpl.mesh = std::move(blob); break;
				case gpu::stage_flag::task: tmpl.task = std::move(blob); break;
				default: break;
			}
		}
		m_gfx_templates.push_back(std::move(tmpl));
		const auto handle = std::bit_cast<gpu::handle<gpu::shader_object>>(&m_gfx_templates.back());
		for (const auto& stage : info.stages) {
			stages.push_back(stage.stage);
			shader_handles.push_back(handle);
		}
	}
	return gpu::shader_program(
		std::bit_cast<gpu::handle<gpu::pipeline_layout>>(root_signature()),
		std::move(stages),
		std::move(shader_handles),
		info.state,
		info.is_compute,
		info.is_mesh
	);
}

auto gse::dx12::device::create_semaphore() -> gpu::handle<gpu::semaphore> {
	const std::lock_guard lock(m_mutex);
	m_sync_points.push_back({
		.fence = directx::create_fence(m_device.get(), 0),
		.value = 0,
	});
	return std::bit_cast<gpu::handle<gpu::semaphore>>(&m_sync_points.back());
}

auto gse::dx12::device::create_timeline_semaphore(const std::uint64_t initial_value) -> gpu::handle<gpu::semaphore> {
	const std::lock_guard lock(m_mutex);
	m_sync_points.push_back({
		.fence = directx::create_fence(m_device.get(), initial_value),
		.value = initial_value,
	});
	return std::bit_cast<gpu::handle<gpu::semaphore>>(&m_sync_points.back());
}

auto gse::dx12::device::create_fence(const bool signaled) -> gpu::handle<gpu::fence> {
	const std::lock_guard lock(m_mutex);
	m_sync_points.push_back({
		.fence = directx::create_fence(m_device.get(), 0),
		.value = 0,
	});
	auto& sp = m_sync_points.back();
	if (signaled && sp.fence) {
		sp.value = 1;
		sp.fence->Signal(1);
	}
	return std::bit_cast<gpu::handle<gpu::fence>>(&sp);
}

auto gse::dx12::device::retire_semaphore(const gpu::handle<gpu::semaphore> semaphore) -> void {
	if (auto* sp = std::bit_cast<sync_point*>(semaphore)) {
		sp->fence.reset();
	}
}

auto gse::dx12::device::retire_fence(const gpu::handle<gpu::fence> fence) -> void {
	if (auto* sp = std::bit_cast<sync_point*>(fence)) {
		sp->fence.reset();
	}
}

auto gse::dx12::device::semaphore_counter_value(const gpu::handle<gpu::semaphore> semaphore) const -> std::uint64_t {
	auto* sp = std::bit_cast<sync_point*>(semaphore);
	return sp && sp->fence ? sp->fence->GetCompletedValue() : 0;
}

auto gse::dx12::device::wait_semaphore(const gpu::handle<gpu::semaphore> semaphore, const std::uint64_t value) const -> void {
	if (auto* sp = std::bit_cast<sync_point*>(semaphore); sp && sp->fence) {
		directx::wait_fence(sp->fence.get(), value, m_idle_event);
	}
}

auto gse::dx12::device::create_timestamp_query_pool(std::uint32_t, std::string_view) -> gpu::handle<gpu::query_pool> {
	return {};
}

auto gse::dx12::device::create_pipeline_stats_query_pool(std::uint32_t, gpu::pipeline_statistic_flags, std::string_view) -> gpu::handle<gpu::query_pool> {
	return {};
}

auto gse::dx12::device::query_pool_results(gpu::handle<gpu::query_pool>, std::uint32_t, std::uint32_t, std::uint64_t) const -> std::pair<gpu::query_status, std::vector<std::uint64_t>> {
	return { gpu::query_status::error, {} };
}

auto gse::dx12::device::create_blas(const gpu::acceleration_structure_geometry& geometry, const std::uint32_t prim_count) -> gpu::blas {
	const auto sizes = query_blas_build_sizes(geometry, prim_count);

	auto storage = create_buffer(
		gpu::buffer_desc{
			.size = sizes.acceleration_structure_size,
			.usage = gpu::buffer_flag::acceleration_structure_storage,
		},
		{},
		std::source_location::current()
	);

	const auto address = storage.device_address();

	return gpu::blas{
		std::move(storage),
		gpu::acceleration_structure{ address },
		address,
	};
}

auto gse::dx12::device::create_tlas(const std::uint32_t max_instances) -> gpu::tlas {
	std::uint64_t as_size = 0;
	std::uint64_t build_scratch = 0;
	std::uint64_t update_scratch = 0;
	directx::tlas_prebuild_info(m_device.get(), max_instances, &as_size, &build_scratch, &update_scratch);

	const auto alignment = acceleration_structure_scratch_alignment();

	auto storage = create_buffer(
		gpu::buffer_desc{
			.size = as_size,
			.usage = gpu::buffer_flag::acceleration_structure_storage,
		},
		{},
		std::source_location::current()
	);

	auto scratch = create_buffer(
		gpu::buffer_desc{
			.size = std::max(build_scratch, update_scratch) + alignment,
			.usage = gpu::buffer_flag::acceleration_structure_scratch,
		},
		{},
		std::source_location::current()
	);

	auto instance_buffer = create_buffer(
		gpu::buffer_desc{
			.size = static_cast<gpu::device_size>(max_instances) * sizeof(gpu::acceleration_structure_instance),
			.usage = gpu::buffer_flag::acceleration_structure_build_input | gpu::buffer_flag::storage,
		},
		{},
		std::source_location::current()
	);

	const auto address = storage.device_address();

	return gpu::tlas{
		std::move(storage),
		std::move(scratch),
		std::move(instance_buffer),
		gpu::acceleration_structure{ address },
		address,
	};
}

auto gse::dx12::device::query_blas_build_sizes(const gpu::acceleration_structure_geometry& geometry, const std::uint32_t prim_count) const -> gpu::acceleration_structure_build_sizes {
	const directx::blas_triangles triangles{
		.vertex_format = directx::format_r32g32b32_float,
		.vertex_address = geometry.triangles.vertex_data,
		.vertex_stride = geometry.triangles.vertex_stride,
		.vertex_count = geometry.triangles.max_vertex + 1,
		.index_format = directx::format_r32_uint,
		.index_address = geometry.triangles.index_data,
		.prim_count = prim_count,
	};

	std::uint64_t as_size = 0;
	std::uint64_t scratch_size = 0;
	directx::blas_prebuild_info(m_device.get(), triangles, &as_size, &scratch_size);

	return {
		.acceleration_structure_size = as_size,
		.build_scratch_size = scratch_size,
		.update_scratch_size = scratch_size,
	};
}

auto gse::dx12::device::acceleration_structure_scratch_alignment() const -> gpu::device_size {
	return 256;
}

auto gse::dx12::device::create_buffer(const gpu::buffer_desc& desc, std::string_view, const std::source_location&) -> gpu::buffer {
	const std::lock_guard lock(m_mutex);
	if (desc.usage.test(gpu::buffer_flag::acceleration_structure_storage) || desc.usage.test(gpu::buffer_flag::acceleration_structure_scratch)) {
		const auto state = desc.usage.test(gpu::buffer_flag::acceleration_structure_storage)
			? directx::resource_state_raytracing_acceleration_structure
			: directx::resource_state_unordered_access;
		auto as_resource = directx::create_default_buffer(m_device.get(), desc.size, state);
		if (!as_resource) {
			return {};
		}
		auto* as_raw = as_resource.get();
		const auto as_address = directx::gpu_address(as_raw);
		m_buffer_by_address.emplace(as_address, std::pair{ as_raw, desc.size });
		m_owned_buffers.push_back(std::move(as_resource));
		return gpu::buffer(
			std::bit_cast<gpu::handle<gpu::buffer>>(as_raw),
			desc.size,
			as_address,
			nullptr,
			{}
		);
	}
	auto resource = m_gpu_upload_supported
		? directx::create_gpu_upload_buffer(m_device.get(), desc.size)
		: directx::create_upload_buffer(m_device.get(), desc.size);
	if (!resource) {
		log::println(log::level::error, log::category::dx12, "create_buffer FAILED size={} removed=0x{:08x}", desc.size, static_cast<std::uint32_t>(m_device->GetDeviceRemovedReason()));
		directx::drain_debug_messages(m_device.get(), [](void*, const char* message) {
			log::println(log::level::warning, log::category::dx12_validation, "{}", message);
		}, nullptr);
		return {};
	}
	auto* mapped = static_cast<std::byte*>(directx::map_buffer(resource.get()));
	const auto address = directx::gpu_address(resource.get());
	if (desc.data && mapped) {
		std::memcpy(mapped, desc.data, desc.size);
	}
	auto* raw = resource.get();
	m_buffer_by_address.emplace(address, std::pair{ raw, desc.size });
	m_owned_buffers.push_back(std::move(resource));

	gpu::bindless_slot slot;
	if (desc.bindless) {
		slot = m_buffer_pool.allocate();
		const directx::D3D12_CPU_DESCRIPTOR_HANDLE handle = {
			.ptr = directx::descriptor_heap_cpu_start(m_resource_heap.get()).ptr + static_cast<std::size_t>(m_buffer_pool.offset(slot)),
		};
		if (desc.usage.test(gpu::buffer_flag::byte_address)) {
			const auto num_elements = static_cast<std::uint32_t>((desc.size + 3) / 4);
			directx::create_raw_buffer_srv(m_device.get(), raw, 0, num_elements, handle);
		}
		else {
			const auto stride = desc.stride ? desc.stride : desc.size;
			const auto element_count = stride ? desc.size / stride : 1;
			directx::create_structured_buffer_srv(m_device.get(), raw, 0, static_cast<std::uint32_t>(element_count), static_cast<std::uint32_t>(stride), handle);
		}
	}

	return gpu::buffer(
		std::bit_cast<gpu::handle<gpu::buffer>>(raw),
		desc.size,
		address,
		mapped,
		slot
	);
}

auto gse::dx12::device::create_image(const gpu::image_desc& desc, std::string_view) -> gpu::image {
	const std::lock_guard lock(m_mutex);
	const auto dimension = desc.depth > 1 ? directx::dimension_texture_3d : directx::dimension_texture_2d;
	const std::uint32_t depth_or_layers = desc.depth > 1 ? desc.depth : 1;

	int flag_bits = static_cast<int>(directx::resource_flag_none);
	if (desc.usage.test(gpu::image_flag::color_attachment)) {
		flag_bits |= static_cast<int>(directx::resource_flag_allow_render_target);
	}
	if (desc.usage.test(gpu::image_flag::depth_attachment)) {
		flag_bits |= static_cast<int>(directx::resource_flag_allow_depth_stencil);
	}
	if (desc.usage.test(gpu::image_flag::storage)) {
		flag_bits |= static_cast<int>(directx::resource_flag_allow_unordered_access);
	}
	const auto flags = static_cast<directx::D3D12_RESOURCE_FLAGS>(flag_bits);

	auto resource = directx::create_committed_texture(m_device.get(), dimension, resource_format_of(desc.format), desc.size.x(), desc.size.y(), depth_or_layers, 1, flags);
	if (!resource) {
		log::println(log::level::error, log::category::dx12, "create_image FAILED size={} depth={} fmt={} flags={} removed=0x{:08x}", desc.size, depth_or_layers, desc.format, flags, static_cast<std::uint32_t>(m_device->GetDeviceRemovedReason()));
		directx::drain_debug_messages(m_device.get(), [](void*, const char* message) {
			log::println(log::level::warning, log::category::dx12_validation, "{}", message);
		}, nullptr);
		return {};
	}
	auto* raw = resource.get();
	m_owned_images.push_back(std::move(resource));

	auto view = std::bit_cast<gpu::handle<gpu::image_view>>(raw);
	if (desc.usage.test(gpu::image_flag::depth_attachment)) {
		const auto handle = directx::offset_cpu_handle(directx::descriptor_heap_cpu_start(m_dsv_view_heap.get()), m_dsv_view_next++, m_dsv_size);
		directx::create_depth_stencil_view(m_device.get(), raw, dxgi_format_of(desc.format), handle);
		m_view_format[handle.ptr] = dxgi_format_of(desc.format);
		view = std::bit_cast<gpu::handle<gpu::image_view>>(handle.ptr);
	}
	else if (desc.usage.test(gpu::image_flag::color_attachment)) {
		const auto handle = directx::offset_cpu_handle(directx::descriptor_heap_cpu_start(m_rtv_view_heap.get()), m_rtv_view_next++, m_rtv_size);
		directx::create_render_target_view(m_device.get(), raw, handle);
		m_view_format[handle.ptr] = dxgi_format_of(desc.format);
		view = std::bit_cast<gpu::handle<gpu::image_view>>(handle.ptr);
	}

	gpu::bindless_slot storage_slot;
	gpu::bindless_slot sampled_slot;
	if (desc.bindless) {
		storage_slot = m_image_pool.allocate();
		sampled_slot = m_image_pool.allocate();
		const bool is_3d = desc.depth > 1;
		if (desc.usage.test(gpu::image_flag::sampled)) {
			const directx::D3D12_CPU_DESCRIPTOR_HANDLE srv = {
				.ptr = directx::descriptor_heap_cpu_start(m_resource_heap.get()).ptr + static_cast<std::size_t>(m_image_pool.offset(sampled_slot)),
			};
			if (is_3d) {
				directx::create_texture_srv_3d(m_device.get(), raw, srv_format_of(desc.format), srv);
			}
			else {
				directx::create_texture_srv(m_device.get(), raw, srv_format_of(desc.format), srv);
			}
		}
		if (desc.usage.test(gpu::image_flag::storage)) {
			const directx::D3D12_CPU_DESCRIPTOR_HANDLE uav = {
				.ptr = directx::descriptor_heap_cpu_start(m_resource_heap.get()).ptr + static_cast<std::size_t>(m_image_pool.offset(storage_slot)),
			};
			if (is_3d) {
				directx::create_texture_uav_3d(m_device.get(), raw, srv_format_of(desc.format), desc.depth, uav);
			}
			else {
				directx::create_texture_uav(m_device.get(), raw, srv_format_of(desc.format), uav);
			}
		}
	}

	return gpu::image(
		std::bit_cast<gpu::handle<gpu::image>>(raw),
		view,
		static_cast<gpu::image_format_value>(desc.format),
		vec3u{ desc.size.x(), desc.size.y(), desc.depth },
		{},
		storage_slot,
		sampled_slot
	);
}

auto gse::dx12::device::allocate_buffer_slot() -> gpu::bindless_handle {
	const std::lock_guard lock(m_mutex);
	return gpu::bindless_handle(&m_buffer_pool, m_buffer_pool.allocate());
}

auto gse::dx12::device::allocate_image_slot() -> gpu::bindless_handle {
	const std::lock_guard lock(m_mutex);
	return gpu::bindless_handle(&m_image_pool, m_image_pool.allocate());
}

auto gse::dx12::device::allocate_acceleration_structure_slot() -> gpu::bindless_handle {
	const std::lock_guard lock(m_mutex);
	return gpu::bindless_handle(&m_buffer_pool, m_buffer_pool.allocate());
}

auto gse::dx12::device::write_storage_buffer(const gpu::bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	const std::lock_guard lock(m_mutex);
	const auto [resource, base] = find_buffer(address);
	if (!resource) {
		return;
	}
	const auto first_element = static_cast<std::uint32_t>((address - base) / 4);
	const auto num_elements = static_cast<std::uint32_t>((size + 3) / 4);
	const directx::D3D12_CPU_DESCRIPTOR_HANDLE handle = {
		.ptr = directx::descriptor_heap_cpu_start(m_resource_heap.get()).ptr + static_cast<std::size_t>(m_buffer_pool.offset(slot)),
	};
	directx::create_raw_buffer_uav(m_device.get(), resource, first_element, num_elements, handle);
}

auto gse::dx12::device::write_uniform_buffer(const gpu::bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	const std::lock_guard lock(m_mutex);
	const directx::D3D12_CPU_DESCRIPTOR_HANDLE handle = {
		.ptr = directx::descriptor_heap_cpu_start(m_resource_heap.get()).ptr + static_cast<std::size_t>(m_buffer_pool.offset(slot)),
	};
	directx::create_constant_buffer_view(m_device.get(), address, static_cast<std::uint32_t>(size), handle);
}

auto gse::dx12::device::write_acceleration_structure(const gpu::bindless_slot slot, const gpu::device_address as_address) -> void {
	const std::lock_guard lock(m_mutex);
	const directx::D3D12_CPU_DESCRIPTOR_HANDLE handle = {
		.ptr = directx::descriptor_heap_cpu_start(m_resource_heap.get()).ptr + static_cast<std::size_t>(m_buffer_pool.offset(slot)),
	};
	directx::create_acceleration_structure_srv(m_device.get(), as_address, handle);
}

auto gse::dx12::device::write_sampled_image(const gpu::bindless_slot slot, const gpu::image& img) -> void {
	auto* resource = std::bit_cast<directx::ID3D12Resource*>(img.handle());
	if (!resource) {
		return;
	}
	const std::lock_guard lock(m_mutex);
	const directx::D3D12_CPU_DESCRIPTOR_HANDLE handle = {
		.ptr = directx::descriptor_heap_cpu_start(m_resource_heap.get()).ptr + static_cast<std::size_t>(m_image_pool.offset(slot)),
	};
	directx::create_texture_srv(m_device.get(), resource, srv_format_of(static_cast<gpu::image_format>(img.format())), handle);
}

auto gse::dx12::device::register_sampler(const gpu::sampler_desc& desc) -> gpu::bindless_handle {
	const std::lock_guard lock(m_mutex);
	const auto slot = m_sampler_pool.allocate();
	write_sampler_at(m_sampler_pool.offset(slot), desc);
	return gpu::bindless_handle(&m_sampler_pool, slot);
}

auto gse::dx12::device::register_texture(const gpu::image& img, const gpu::sampler_desc& desc) -> gpu::bindless_handle {
	const std::lock_guard lock(m_mutex);
	const auto slot = m_texture_pool.allocate();
	if (auto* resource = std::bit_cast<directx::ID3D12Resource*>(img.handle())) {
		const directx::D3D12_CPU_DESCRIPTOR_HANDLE srv = {
			.ptr = directx::descriptor_heap_cpu_start(m_resource_heap.get()).ptr + static_cast<std::size_t>(m_bindless_layout.texture_image_offset + slot.index * m_bindless_layout.image_stride),
		};
		directx::create_texture_srv(m_device.get(), resource, srv_format_of(static_cast<gpu::image_format>(img.format())), srv);
	}
	write_sampler_at(m_bindless_layout.texture_sampler_offset + slot.index * m_bindless_layout.sampler_stride, desc);
	return gpu::bindless_handle(&m_texture_pool, slot);
}

auto gse::dx12::device::bindless_layout() const -> gpu::bindless_layout {
	return m_bindless_layout;
}

auto gse::dx12::device::bindless_resource_heap_binding() const -> gpu::bindless_heap_binding {
	return m_resource_binding;
}

auto gse::dx12::device::bindless_sampler_heap_binding() const -> gpu::bindless_heap_binding {
	return m_sampler_binding;
}

auto gse::dx12::device::create_sampler(const gpu::sampler_desc& desc) -> gpu::handle<gpu::sampler> {
	const std::lock_guard lock(m_mutex);
	const auto slot = m_sampler_pool.allocate();
	const auto offset = m_sampler_pool.offset(slot);
	write_sampler_at(offset, desc);
	const directx::D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = {
		.ptr = directx::descriptor_heap_gpu_start(m_sampler_heap.get()).ptr + offset,
	};
	return std::bit_cast<gpu::handle<gpu::sampler>>(gpu_handle.ptr);
}

auto gse::dx12::device::collect_garbage() -> void {}

auto gse::dx12::device::root_signature() const -> directx::ID3D12RootSignature* {
	return m_pipeline_layout.root_signature();
}

auto gse::dx12::device::resource_heap() const -> directx::ID3D12DescriptorHeap* {
	return m_resource_heap.get();
}

auto gse::dx12::device::sampler_heap() const -> directx::ID3D12DescriptorHeap* {
	return m_sampler_heap.get();
}
