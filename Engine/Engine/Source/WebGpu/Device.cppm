export module gse.webgpu:device;

import std;

import gse.assert;
import gse.ecs;
import gse.gpu_backend;
import gse.log;
import gse.math;
import gse.os;
import gse.time;
import gse.webgpu_api;

import :platform;

export namespace gse::webgpu {
	class device;

	template <typename T>
	[[nodiscard]] auto record_of(
		std::uint64_t value
	) -> T*;

	template <typename T, typename Tag>
	[[nodiscard]] auto record_of(
		gpu::handle<Tag> handle
	) -> T*;

	[[nodiscard]] auto address_of(
		const void* record
	) -> std::uint64_t;

	template <typename H>
	[[nodiscard]] auto handle_of(
		const void* record
	) -> H;

	struct view_record;

	struct texture_record {
		WGPUTexture texture = nullptr;
		WGPUTextureView default_view = nullptr;
		view_record* default_record = nullptr;
		gpu::image_format format = gpu::image_format::undefined;
		vec3u extent;
		gpu::bindless_slot sampled_slot;
		gpu::bindless_slot storage_slot;
	};

	struct view_record {
		WGPUTextureView view = nullptr;
		texture_record* texture = nullptr;
	};

	struct buffer_record {
		WGPUBuffer buffer = nullptr;
		gpu::device_size size = 0;
		gpu::device_address address = 0;
		gpu::bindless_slot slot;
		std::unique_ptr<std::byte[]> shadow;
		std::unique_ptr<std::byte[]> uploaded;
		std::uint64_t mapping_serial = 0;
	};

	struct slot_entry {
		WGPUBuffer buffer = nullptr;
		gpu::device_size offset = 0;
		gpu::device_size size = 0;
		WGPUTextureView view = nullptr;
		gpu::image_format format = gpu::image_format::undefined;
	};

	constexpr std::uint32_t max_color_attachments = 8;

	struct attachment_formats {
		std::array<WGPUTextureFormat, max_color_attachments> colors{};
		std::uint32_t color_count = 0;
		WGPUTextureFormat depth = WGPUTextureFormat_Undefined;

		auto operator==(
			const attachment_formats&
		) const -> bool = default;
	};

	struct program_variant {
		attachment_formats formats;
		std::uint64_t unfilterable = 0;
		WGPUBindGroupLayout group_layout = nullptr;
		WGPUPipelineLayout pipeline_layout = nullptr;
		WGPURenderPipeline render = nullptr;
		WGPUComputePipeline compute = nullptr;
	};

	struct program_stage {
		gpu::stage_flag stage = gpu::stage_flag::vertex;
		WGPUShaderModule module = nullptr;
		std::string entry_point;
	};

	struct program_record {
		std::string label;
		bool compute = false;
		std::vector<program_stage> stages;
		std::vector<gpu::binding_layout_entry> bindings;
		std::uint32_t push_size = 0;
		std::uint32_t binding_offset = 0;
		gpu::dynamic_pipeline_state state;
		std::uint32_t color_target_count = 0;
		WGPUBindGroupLayout push_layout = nullptr;
		WGPUBindGroup push_group = nullptr;
		std::deque<program_variant> variants;
	};

	struct fence_record {
		std::uint64_t serial = 0;
		bool pending = false;
	};

	struct timeline_signal {
		std::uint64_t value = 0;
		std::uint64_t serial = 0;
	};

	struct semaphore_record {
		std::uint64_t reached = 0;
		std::vector<timeline_signal> signals;
	};

	struct command_list {
		device* owner = nullptr;
		WGPUCommandEncoder encoder = nullptr;
		WGPURenderPassEncoder pass = nullptr;
		WGPUCommandBuffer finished = nullptr;
		std::uint64_t in_use_value = 0;
		bool marked = false;
		program_record* program = nullptr;
		std::vector<std::byte> push;
		attachment_formats formats;
		vec2u attachment_extent;
		std::optional<gpu::viewport> viewport;
		std::optional<rect_t<vec2i>> scissor;
		WGPUBuffer index_buffer = nullptr;
		gpu::device_size index_offset = 0;
		WGPUIndexFormat index_format = WGPUIndexFormat_Undefined;
		std::vector<buffer_record*> readbacks;
	};

	struct command_pool {
		std::deque<command_list> lists;
		std::vector<command_list*> free;
		std::vector<command_list*> live;
	};

	constexpr std::uint32_t swapchain_image_count = 3;

	struct surface_record {
		WGPUSurface surface = nullptr;
		bool configured = false;
		vec2u extent;
		gpu::present_mode mode = gpu::present_mode::fifo;
		std::array<texture_record, swapchain_image_count> images;
		std::array<view_record, swapchain_image_count> views;
		std::uint32_t next_image = 0;
	};

	[[nodiscard]] auto texture_format_of(
		gpu::image_format format
	) -> WGPUTextureFormat;

	class device {
	public:
		device(
			std::optional<shared_view<window::data>> win
		);

		~device();

		device(
			const device&
		) = delete;

		auto operator=(
			const device&
		) -> device& = delete;

		[[nodiscard]] auto raw() const -> WGPUDevice;

		[[nodiscard]] auto lost() const -> bool;

		auto wait_idle() -> void;

		[[nodiscard]] auto frame_command_buffer(
			gpu::queue_type queue,
			std::uint32_t frame_index
		) -> gpu::command_buffer_handle;

		auto reset_worker_command_pools(
			std::uint32_t frame_index
		) -> void;

		[[nodiscard]] auto acquire_worker_command_buffer(
			std::uint32_t frame_index
		) -> gpu::command_buffer_handle;

		[[nodiscard]] auto create_transient_command_pool() -> gpu::transient_pool_handle;

		[[nodiscard]] auto allocate_transient_primary(
			gpu::transient_pool_handle pool
		) -> gpu::command_buffer_handle;

		auto transient_pool_try_reset(
			gpu::transient_pool_handle pool,
			std::uint64_t queue_progress
		) -> void;

		auto transient_pool_mark_in_use(
			gpu::transient_pool_handle pool,
			std::uint64_t value
		) -> void;

		auto transient_pool_reset_all(
			gpu::transient_pool_handle pool
		) -> void;

		auto cmd_reset(
			gpu::command_buffer_handle cmd
		) -> void;

		auto cmd_begin(
			gpu::command_buffer_handle cmd
		) -> void;

		auto cmd_end(
			gpu::command_buffer_handle cmd
		) -> void;

		auto begin_debug_event(
			gpu::command_buffer_handle cmd,
			std::string_view label
		) -> void;

		auto end_debug_event(
			gpu::command_buffer_handle cmd
		) -> void;

		[[nodiscard]] auto submit(
			const gpu::submit_info& info,
			gpu::handle<gpu::fence> signal_fence
		) -> gpu::result;

		[[nodiscard]] auto wait_for_fence(
			gpu::handle<gpu::fence> f,
			std::uint64_t timeout_ns
		) -> gpu::result;

		auto reset_fence(
			gpu::handle<gpu::fence> f
		) -> void;

		[[nodiscard]] auto create_fence(
			bool signaled
		) -> gpu::handle<gpu::fence>;

		[[nodiscard]] auto create_semaphore(
			std::uint64_t initial_value
		) -> gpu::handle<gpu::semaphore>;

		auto retire_fence(
			gpu::handle<gpu::fence> f
		) -> void;

		auto retire_semaphore(
			gpu::handle<gpu::semaphore> s
		) -> void;

		[[nodiscard]] auto semaphore_counter_value(
			gpu::handle<gpu::semaphore> s
		) -> std::uint64_t;

		[[nodiscard]] auto wait_semaphore_for(
			gpu::handle<gpu::semaphore> s,
			std::uint64_t value,
			std::optional<time> timeout
		) -> bool;

		[[nodiscard]] auto boot_surface() const -> gpu::surface;

		[[nodiscard]] auto create_surface(
			native_window_handle handle
		) -> gpu::surface;

		auto destroy_surface(
			gpu::surface surface
		) -> void;

		[[nodiscard]] auto configure_surface(
			gpu::surface surface,
			vec2i framebuffer_size,
			gpu::present_mode mode
		) -> gpu::expected<gpu::swap_chain_info>;

		[[nodiscard]] auto acquire_surface_image(
			gpu::swap_chain_handle swapchain
		) -> gpu::acquire_next_image_result;

		[[nodiscard]] auto present(
			const gpu::present_info& info
		) -> gpu::result;

		[[nodiscard]] auto create_buffer(
			const gpu::buffer_desc& desc,
			std::string_view tag
		) -> gpu::buffer;

		[[nodiscard]] auto create_image(
			const gpu::image_desc& desc,
			std::string_view tag
		) -> gpu::image;

		[[nodiscard]] auto create_image_view(
			gpu::handle<gpu::image> img,
			const gpu::image_view_create_info& info
		) -> gpu::handle<gpu::image_view>;

		auto host_upload_image_layers(
			gpu::handle<gpu::image> img,
			std::span<const void* const> layer_pointers,
			vec2u extent
		) const -> void;

		[[nodiscard]] auto allocate_buffer_slot() -> gpu::bindless_handle;

		[[nodiscard]] auto allocate_image_slot() -> gpu::bindless_handle;

		[[nodiscard]] auto register_sampler(
			const gpu::sampler_desc& desc
		) -> gpu::bindless_handle;

		[[nodiscard]] auto register_texture(
			const gpu::image& img
		) -> gpu::bindless_handle;

		auto write_storage_buffer(
			gpu::bindless_slot slot,
			gpu::device_address address,
			gpu::device_size size
		) -> void;

		auto write_sampled_image(
			gpu::bindless_slot slot,
			const gpu::image& img
		) -> void;

		[[nodiscard]] auto create_program(
			const gpu::shader_program_create_info& info
		) -> gpu::shader_program;

		auto bind_for_draw(
			command_list& list
		) -> void;

		auto bind_for_dispatch(
			command_list& list,
			WGPUComputePassEncoder pass
		) -> void;

		auto upload_host_writes() -> void;

		auto write_texture_region(
			const texture_record& texture,
			const std::byte* data,
			const gpu::buffer_image_copy_region& region
		) const -> void;

		auto collect_garbage() -> void;

		auto finish_readback(
			buffer_record& buffer,
			bool mapped
		) -> void;

	private:
		[[nodiscard]] auto readbacks_settled(
			std::uint64_t serial
		) -> bool;

		[[nodiscard]] auto readbacks_settled_locked(
			std::uint64_t serial
		) const -> bool;

		auto process_events() -> void;

		auto wait_serial(
			std::uint64_t serial
		) -> void;

		[[nodiscard]] auto serial_reached(
			std::uint64_t serial
		) const -> bool;

		auto track_submission(
			std::uint64_t serial
		) -> void;

		auto release_surface_image(
			surface_record& surface,
			std::uint32_t index
		) -> void;

		[[nodiscard]] auto new_list() -> command_list*;

		[[nodiscard]] auto unfilterable_mask(
			const program_record& program,
			const command_list& list
		) -> std::uint64_t;

		[[nodiscard]] auto variant_for(
			program_record& program,
			const attachment_formats& formats,
			std::uint64_t unfilterable
		) -> program_variant&;

		auto build_variant(
			const program_record& program,
			program_variant& variant
		) -> void;

		[[nodiscard]] auto make_group(
			const program_record& program,
			const program_variant& variant,
			const command_list& list
		) -> WGPUBindGroup;

		[[nodiscard]] auto push_slice(
			const program_record& program,
			const command_list& list
		) -> std::uint32_t;

		[[nodiscard]] auto dummy_storage_view(
			gpu::binding_kind kind,
			gpu::image_format format
		) -> WGPUTextureView;

		auto create_dummies() -> void;

		WGPUInstance m_instance = nullptr;
		WGPUAdapter m_adapter = nullptr;
		WGPUDevice m_device = nullptr;
		WGPUQueue m_queue = nullptr;
		surface_record* m_boot_surface = nullptr;

		std::atomic<std::uint64_t> m_submitted{ 0 };
		std::atomic<std::uint64_t> m_completed{ 0 };
		std::atomic<bool> m_lost{ false };

		std::mutex m_mutex;
		std::mutex m_events_mutex;
		std::mutex m_scope_mutex;
		std::deque<command_list> m_lists;
		std::array<std::array<command_list*, gpu::max_frames_in_flight>, gpu::queue_type_count> m_frame_lists{};
		std::array<command_pool, gpu::max_frames_in_flight> m_worker_pools;
		std::deque<command_pool> m_transient_pools;
		std::deque<fence_record> m_fences;
		std::deque<semaphore_record> m_semaphores;
		std::deque<surface_record> m_surfaces;
		std::deque<texture_record> m_textures;
		std::deque<view_record> m_views;
		std::deque<buffer_record> m_buffers;
		std::vector<buffer_record*> m_mapping;

		gpu::bindless_slot_pool m_texture_pool;
		gpu::bindless_slot_pool m_image_pool;
		gpu::bindless_slot_pool m_buffer_pool;
		gpu::bindless_slot_pool m_sampler_pool;

		std::vector<slot_entry> m_slots;
		std::vector<WGPUSampler> m_samplers;
		std::vector<bool> m_nearest_samplers;
		std::map<gpu::device_address, buffer_record*> m_addresses;
		gpu::device_address m_next_address = gpu::device_address{ 1 } << 32;
		std::deque<program_record> m_programs;

		WGPUBuffer m_push_ring = nullptr;
		std::atomic<std::uint64_t> m_push_cursor{ 0 };
		std::atomic<std::uint64_t> m_push_submitted_cursor{ 0 };
		std::uint64_t m_push_high_water = 0;
		std::uint32_t m_storage_offset_alignment = 256;

		WGPUBuffer m_dummy_buffer = nullptr;
		WGPUTexture m_dummy_texture_2d = nullptr;
		WGPUTextureView m_dummy_view_2d = nullptr;
		WGPUTexture m_dummy_texture_3d = nullptr;
		WGPUTextureView m_dummy_view_3d = nullptr;
		WGPUSampler m_dummy_sampler = nullptr;
		std::map<std::pair<gpu::binding_kind, gpu::image_format>, std::pair<WGPUTexture, WGPUTextureView>> m_dummy_storage;
	};
}

template <typename T>
auto gse::webgpu::record_of(const std::uint64_t value) -> T* {
	return reinterpret_cast<T*>(static_cast<std::uintptr_t>(value));
}

template <typename T, typename Tag>
auto gse::webgpu::record_of(const gpu::handle<Tag> handle) -> T* {
	return record_of<T>(handle.value);
}

auto gse::webgpu::address_of(const void* record) -> std::uint64_t {
	return reinterpret_cast<std::uintptr_t>(record);
}

template <typename H>
auto gse::webgpu::handle_of(const void* record) -> H {
	return H{ address_of(record) };
}

namespace gse::webgpu {
	constexpr std::uint32_t texture_capacity = 4096;
	constexpr std::uint32_t image_capacity = 4096;
	constexpr std::uint32_t buffer_capacity = 16384;
	constexpr std::uint32_t sampler_capacity = 256;
	constexpr std::uint64_t push_ring_size = 32ull << 20;
	constexpr std::uint32_t push_group_index = 1;
	constexpr std::size_t max_push_bytes = 4096;
	constexpr std::uint64_t dummy_buffer_size = 65536;

	[[nodiscard]] auto pushed_slot(
		const program_record& program,
		const gpu::binding_layout_entry& binding,
		const command_list& list
	) -> std::uint32_t;

	[[nodiscard]] auto string_of(
		WGPUStringView view
	) -> std::string_view;

	[[nodiscard]] auto label_of(
		std::string_view text
	) -> WGPUStringView;

	[[nodiscard]] auto repair_slang_wgsl(
		std::string_view wgsl
	) -> std::string;

	[[nodiscard]] auto backend_name(
		WGPUBackendType type
	) -> std::string_view;

	[[nodiscard]] auto error_name(
		WGPUErrorType type
	) -> std::string_view;

	[[nodiscard]] auto present_mode_of(
		gpu::present_mode mode
	) -> WGPUPresentMode;

	[[nodiscard]] auto list_of(
		gpu::command_buffer_handle cmd
	) -> command_list*;

	[[nodiscard]] auto compare_function_of(
		gpu::compare_op op
	) -> WGPUCompareFunction;

	[[nodiscard]] auto blend_factor_of(
		gpu::blend_factor factor
	) -> WGPUBlendFactor;

	[[nodiscard]] auto blend_operation_of(
		gpu::blend_op op
	) -> WGPUBlendOperation;

	[[nodiscard]] auto color_format_of(
		gpu::color_format format
	) -> WGPUTextureFormat;

	[[nodiscard]] auto shader_stage_of(
		gpu::stage_flag stage
	) -> WGPUShaderStage;

	[[nodiscard]] auto is_unfilterable(
		gpu::image_format format
	) -> bool;

	auto on_uncaptured_error(
		WGPUDevice const* device,
		WGPUErrorType type,
		WGPUStringView message,
		void* userdata1,
		void* userdata2
	) -> void;

	auto on_device_lost(
		WGPUDevice const* device,
		WGPUDeviceLostReason reason,
		WGPUStringView message,
		void* userdata1,
		void* userdata2
	) -> void;

	auto on_scope_popped(
		WGPUPopErrorScopeStatus status,
		WGPUErrorType type,
		WGPUStringView message,
		void* userdata1,
		void* userdata2
	) -> void;

	auto pop_validation_scope(
		WGPUDevice device,
		std::string_view label
	) -> void;

	auto on_work_done(
		WGPUQueueWorkDoneStatus status,
		WGPUStringView message,
		void* userdata1,
		void* userdata2
	) -> void;

	auto on_readback_mapped(
		WGPUMapAsyncStatus status,
		WGPUStringView message,
		void* userdata1,
		void* userdata2
	) -> void;

	[[nodiscard]] auto mapped_size(
		const buffer_record& buffer
	) -> gpu::device_size;
}

auto gse::webgpu::string_of(const WGPUStringView view) -> std::string_view {
	if (!view.data) {
		return {};
	}
	if (view.length == wgpu_strlen) {
		return std::string_view(view.data);
	}
	return std::string_view(view.data, view.length);
}

auto gse::webgpu::label_of(const std::string_view text) -> WGPUStringView {
	return { .data = text.data(), .length = text.size() };
}

auto gse::webgpu::repair_slang_wgsl(const std::string_view wgsl) -> std::string {
	std::string code(wgsl);

	constexpr std::string_view precise_qualifier = "precise ";
	for (auto at = code.find("precise var "); at != std::string::npos; at = code.find("precise var ", at)) {
		code.erase(at, precise_qualifier.size());
	}

	code.insert(0, "diagnostic(off, derivative_uniformity);\n");

	constexpr std::string_view interpolate = "@interpolate(";
	for (auto at = code.find(interpolate); at != std::string::npos; at = code.find(interpolate, at)) {
		const auto close = code.find(')', at);
		const auto next = code.find_first_not_of(' ', close + 1);
		if (next != std::string::npos && code[next] != '@') {
			code.erase(at, next - at);
		}
		else {
			at = close;
		}
	}

	constexpr std::string_view position = "@builtin(position)";
	constexpr std::string_view invariant = "@invariant ";
	for (auto at = code.find(position); at != std::string::npos; at = code.find(position, at + position.size())) {
		if (!std::string_view(code).substr(0, at).ends_with(invariant)) {
			code.insert(at, invariant);
			at += invariant.size();
		}
	}

	constexpr std::string_view vertex_entry = "@vertex";
	constexpr std::string_view returns = "-> ";
	constexpr std::string_view return_keyword = "return ";
	for (auto at = code.find(vertex_entry); at != std::string::npos; at = code.find(vertex_entry, at + vertex_entry.size())) {
		const auto arrow = code.find(returns, at);
		const auto open = code.find('{', arrow);
		assert(arrow != std::string::npos && open != std::string::npos, "webgpu: vertex entry has no struct return type to flip clip-space y in");
		const auto output_type = std::string_view(code).substr(arrow + returns.size(), open - arrow - returns.size());
		const auto type_name = output_type.substr(0, output_type.find_last_not_of(" \r\n") + 1);
		const auto declaration = code.find(std::format("struct {}\n", type_name));
		assert(declaration != std::string::npos, "webgpu: vertex output {} is not a struct", type_name);
		const auto marker = code.find(position, declaration);
		const auto name_begin = code.find_first_not_of(' ', marker + position.size());
		const auto field = code.substr(name_begin, code.find_first_of(" :", name_begin) - name_begin);

		std::size_t close = open + 1;
		for (int depth = 1; depth > 0; ++close) {
			depth += code[close] == '{' ? 1 : code[close] == '}' ? -1 : 0;
		}
		for (auto ret = code.find(return_keyword, open); ret < close; ret = code.find(return_keyword, ret)) {
			const auto end = code.find(';', ret);
			const auto value = code.substr(ret + return_keyword.size(), end - ret - return_keyword.size());
			const auto flipped = std::format("{{ var gse_flipped = {}; gse_flipped.{}.y = -gse_flipped.{}.y; return gse_flipped; }}", value, field, field);
			code.replace(ret, end + 1 - ret, flipped);
			close += flipped.size() - (end + 1 - ret);
			ret += flipped.size();
		}
	}
	return code;
}

auto gse::webgpu::backend_name(const WGPUBackendType type) -> std::string_view {
	switch (type) {
		case WGPUBackendType_Null: return "null";
		case WGPUBackendType_WebGPU: return "webgpu";
		case WGPUBackendType_D3D11: return "d3d11";
		case WGPUBackendType_D3D12: return "d3d12";
		case WGPUBackendType_Metal: return "metal";
		case WGPUBackendType_Vulkan: return "vulkan";
		case WGPUBackendType_OpenGL: return "opengl";
		case WGPUBackendType_OpenGLES: return "opengles";
		default: return "undefined";
	}
}

auto gse::webgpu::error_name(const WGPUErrorType type) -> std::string_view {
	switch (type) {
		case WGPUErrorType_Validation: return "validation";
		case WGPUErrorType_OutOfMemory: return "out of memory";
		case WGPUErrorType_Internal: return "internal";
		default: return "unknown";
	}
}

auto gse::webgpu::present_mode_of(const gpu::present_mode mode) -> WGPUPresentMode {
	switch (mode) {
		case gpu::present_mode::fifo: return WGPUPresentMode_Fifo;
		case gpu::present_mode::fifo_relaxed: return WGPUPresentMode_FifoRelaxed;
		case gpu::present_mode::mailbox: return WGPUPresentMode_Mailbox;
		case gpu::present_mode::immediate: return WGPUPresentMode_Immediate;
	}
	return WGPUPresentMode_Fifo;
}

auto gse::webgpu::texture_format_of(const gpu::image_format format) -> WGPUTextureFormat {
	switch (format) {
		case gpu::image_format::d32_sfloat: return WGPUTextureFormat_Depth32Float;
		case gpu::image_format::r8g8b8a8_srgb:
		case gpu::image_format::r8g8b8_srgb: return WGPUTextureFormat_RGBA8UnormSrgb;
		case gpu::image_format::r8g8b8a8_unorm:
		case gpu::image_format::r8g8b8_unorm: return WGPUTextureFormat_RGBA8Unorm;
		case gpu::image_format::b8g8r8a8_srgb: return WGPUTextureFormat_BGRA8UnormSrgb;
		case gpu::image_format::b8g8r8a8_unorm: return WGPUTextureFormat_BGRA8Unorm;
		case gpu::image_format::r8_unorm: return WGPUTextureFormat_R8Unorm;
		case gpu::image_format::b10g11r11_ufloat: return WGPUTextureFormat_RG11B10Ufloat;
		case gpu::image_format::r8g8_snorm: return WGPUTextureFormat_RG8Snorm;
		case gpu::image_format::r8g8_unorm: return WGPUTextureFormat_RG8Unorm;
		case gpu::image_format::r16g16b16a16_sfloat: return WGPUTextureFormat_RGBA16Float;
		case gpu::image_format::r16g16_sfloat: return WGPUTextureFormat_RG16Float;
		case gpu::image_format::undefined: return WGPUTextureFormat_Undefined;
	}
	return WGPUTextureFormat_Undefined;
}

auto gse::webgpu::list_of(const gpu::command_buffer_handle cmd) -> command_list* {
	return record_of<command_list>(cmd.value);
}

auto gse::webgpu::compare_function_of(const gpu::compare_op op) -> WGPUCompareFunction {
	switch (op) {
		case gpu::compare_op::never: return WGPUCompareFunction_Never;
		case gpu::compare_op::less: return WGPUCompareFunction_Less;
		case gpu::compare_op::equal: return WGPUCompareFunction_Equal;
		case gpu::compare_op::less_or_equal: return WGPUCompareFunction_LessEqual;
		case gpu::compare_op::greater: return WGPUCompareFunction_Greater;
		case gpu::compare_op::not_equal: return WGPUCompareFunction_NotEqual;
		case gpu::compare_op::greater_or_equal: return WGPUCompareFunction_GreaterEqual;
		case gpu::compare_op::always: return WGPUCompareFunction_Always;
	}
	return WGPUCompareFunction_Always;
}

auto gse::webgpu::blend_factor_of(const gpu::blend_factor factor) -> WGPUBlendFactor {
	switch (factor) {
		case gpu::blend_factor::zero: return WGPUBlendFactor_Zero;
		case gpu::blend_factor::one: return WGPUBlendFactor_One;
		case gpu::blend_factor::src_color: return WGPUBlendFactor_Src;
		case gpu::blend_factor::one_minus_src_color: return WGPUBlendFactor_OneMinusSrc;
		case gpu::blend_factor::dst_color: return WGPUBlendFactor_Dst;
		case gpu::blend_factor::one_minus_dst_color: return WGPUBlendFactor_OneMinusDst;
		case gpu::blend_factor::src_alpha: return WGPUBlendFactor_SrcAlpha;
		case gpu::blend_factor::one_minus_src_alpha: return WGPUBlendFactor_OneMinusSrcAlpha;
		case gpu::blend_factor::dst_alpha: return WGPUBlendFactor_DstAlpha;
		case gpu::blend_factor::one_minus_dst_alpha: return WGPUBlendFactor_OneMinusDstAlpha;
	}
	return WGPUBlendFactor_One;
}

auto gse::webgpu::blend_operation_of(const gpu::blend_op op) -> WGPUBlendOperation {
	switch (op) {
		case gpu::blend_op::add: return WGPUBlendOperation_Add;
		case gpu::blend_op::subtract: return WGPUBlendOperation_Subtract;
		case gpu::blend_op::reverse_subtract: return WGPUBlendOperation_ReverseSubtract;
		case gpu::blend_op::min: return WGPUBlendOperation_Min;
		case gpu::blend_op::max: return WGPUBlendOperation_Max;
	}
	return WGPUBlendOperation_Add;
}

auto gse::webgpu::color_format_of(const gpu::color_format format) -> WGPUTextureFormat {
	switch (format) {
		case gpu::color_format::swapchain: return WGPUTextureFormat_BGRA8Unorm;
		case gpu::color_format::hdr: return WGPUTextureFormat_RGBA16Float;
		case gpu::color_format::none: return WGPUTextureFormat_Undefined;
	}
	return WGPUTextureFormat_Undefined;
}

auto gse::webgpu::shader_stage_of(const gpu::stage_flag stage) -> WGPUShaderStage {
	switch (stage) {
		case gpu::stage_flag::vertex: return WGPUShaderStage_Vertex;
		case gpu::stage_flag::fragment: return WGPUShaderStage_Fragment;
		case gpu::stage_flag::compute: return WGPUShaderStage_Compute;
		default: return WGPUShaderStage_Vertex;
	}
}

auto gse::webgpu::is_unfilterable(const gpu::image_format format) -> bool {
	return format == gpu::image_format::d32_sfloat;
}

auto gse::webgpu::on_uncaptured_error(WGPUDevice const*, const WGPUErrorType type, const WGPUStringView message, void*, void*) -> void {
	log::println(log::level::error, log::category::render, "webgpu {} error: {}", error_name(type), string_of(message));
	log::flush();
}

auto gse::webgpu::on_scope_popped(const WGPUPopErrorScopeStatus status, const WGPUErrorType type, const WGPUStringView message, void* userdata1, void*) -> void {
	const std::unique_ptr<std::string> label(static_cast<std::string*>(userdata1));
	if (status != WGPUPopErrorScopeStatus_Success || type == WGPUErrorType_NoError) {
		return;
	}
	log::println(log::level::error, log::category::render, "webgpu: {} failed {}: {}", *label, error_name(type), string_of(message));
	log::flush();
}

auto gse::webgpu::pop_validation_scope(const WGPUDevice device, const std::string_view label) -> void {
	wgpuDevicePopErrorScope(device, WGPUPopErrorScopeCallbackInfo{
		.nextInChain = nullptr,
		.mode = WGPUCallbackMode_AllowSpontaneous,
		.callback = &on_scope_popped,
		.userdata1 = new std::string(label),
		.userdata2 = nullptr,
	});
}

auto gse::webgpu::on_device_lost(WGPUDevice const*, const WGPUDeviceLostReason reason, const WGPUStringView message, void* userdata1, void*) -> void {
	if (reason == WGPUDeviceLostReason_Destroyed || reason == WGPUDeviceLostReason_CallbackCancelled) {
		return;
	}
	static_cast<std::atomic<bool>*>(userdata1)->store(true, std::memory_order_release);
	log::println(log::level::error, log::category::render, "webgpu device lost (reason {}): {}", static_cast<std::uint32_t>(reason), string_of(message));
	log::flush();
}

auto gse::webgpu::on_work_done(WGPUQueueWorkDoneStatus, WGPUStringView, void* userdata1, void* userdata2) -> void {
	auto* completed = static_cast<std::atomic<std::uint64_t>*>(userdata1);
	const auto serial = std::bit_cast<std::uintptr_t>(userdata2);
	auto current = completed->load(std::memory_order_relaxed);
	while (current < serial && !completed->compare_exchange_weak(current, serial, std::memory_order_release, std::memory_order_relaxed)) {
	}
}

auto gse::webgpu::on_readback_mapped(const WGPUMapAsyncStatus status, const WGPUStringView message, void* userdata1, void* userdata2) -> void {
	if (status != WGPUMapAsyncStatus_Success) {
		log::println(log::level::error, log::category::render, "webgpu: readback map failed (status {}): {}", static_cast<std::uint32_t>(status), string_of(message));
	}
	static_cast<device*>(userdata1)->finish_readback(*static_cast<buffer_record*>(userdata2), status == WGPUMapAsyncStatus_Success);
}

auto gse::webgpu::mapped_size(const buffer_record& buffer) -> gpu::device_size {
	return std::max<gpu::device_size>((buffer.size + 3) & ~gpu::device_size{ 3 }, 4);
}

gse::webgpu::device::device(const std::optional<shared_view<window::data>> win) {
	install_platform_logging();
	m_instance = wgpuCreateInstance(nullptr);
	assert(m_instance != nullptr, "wgpuCreateInstance failed");

	if (win) {
		m_boot_surface = record_of<surface_record>(create_surface(win->primary.handle).value);
	}

	struct adapter_result {
		WGPUAdapter adapter = nullptr;
		bool done = false;
		std::string message;
	} adapter_out;

	const WGPURequestAdapterOptions adapter_options{
		.nextInChain = nullptr,
		.featureLevel = WGPUFeatureLevel_Core,
		.powerPreference = WGPUPowerPreference_HighPerformance,
		.forceFallbackAdapter = false,
		.backendType = WGPUBackendType_Undefined,
		.compatibleSurface = m_boot_surface ? m_boot_surface->surface : nullptr,
	};
	wgpuInstanceRequestAdapter(m_instance, &adapter_options, WGPURequestAdapterCallbackInfo{
		.nextInChain = nullptr,
		.mode = WGPUCallbackMode_AllowProcessEvents,
		.callback = [](const WGPURequestAdapterStatus status, WGPUAdapter adapter, const WGPUStringView message, void* userdata1, void*) {
			auto* out = static_cast<adapter_result*>(userdata1);
			if (status == WGPURequestAdapterStatus_Success) {
				out->adapter = adapter;
			}
			out->message = std::string(string_of(message));
			out->done = true;
		},
		.userdata1 = &adapter_out,
		.userdata2 = nullptr,
	});
	while (!adapter_out.done) {
		wgpuInstanceProcessEvents(m_instance);
	}
	assert(adapter_out.adapter != nullptr, "webgpu: no adapter available: {}", adapter_out.message);
	m_adapter = adapter_out.adapter;

	WGPUAdapterInfo info{};
	if (wgpuAdapterGetInfo(m_adapter, &info) == WGPUStatus_Success) {
		log::println(
			log::category::render,
			"webgpu adapter: vendor='{}' architecture='{}' device='{}' description='{}' backend={} vendor_id=0x{:04x} device_id=0x{:04x}",
			string_of(info.vendor),
			string_of(info.architecture),
			string_of(info.device),
			string_of(info.description),
			backend_name(info.backendType),
			info.vendorID,
			info.deviceID
		);
		wgpuAdapterInfoFreeMembers(info);
	}

	struct device_result {
		WGPUDevice device = nullptr;
		bool done = false;
		std::string message;
	} device_out;

	constexpr std::string_view device_label = "gse.webgpu.device";
	constexpr std::string_view queue_label = "gse.webgpu.queue";
	constexpr std::array optional_features{
		WGPUFeatureName_Float32Filterable,
		WGPUFeatureName_IndirectFirstInstance,
		WGPUFeatureName_RG11B10UfloatRenderable,
		WGPUFeatureName_TextureFormatsTier1,
		WGPUFeatureName_TextureFormatsTier2,
	};
	std::vector<WGPUFeatureName> features;
	for (const auto feature : optional_features) {
		if (wgpuAdapterHasFeature(m_adapter, feature)) {
			features.push_back(feature);
		}
	}
	WGPULimits adapter_limits{};
	const bool have_adapter_limits = wgpuAdapterGetLimits(m_adapter, &adapter_limits) == WGPUStatus_Success;
	WGPUDeviceDescriptor device_desc{};
	device_desc.label = label_of(device_label);
	device_desc.requiredFeatureCount = features.size();
	device_desc.requiredFeatures = features.data();
	device_desc.requiredLimits = have_adapter_limits ? &adapter_limits : nullptr;
	device_desc.defaultQueue.label = label_of(queue_label);
	device_desc.deviceLostCallbackInfo = {
		.nextInChain = nullptr,
		.mode = WGPUCallbackMode_AllowProcessEvents,
		.callback = &on_device_lost,
		.userdata1 = &m_lost,
		.userdata2 = nullptr,
	};
	device_desc.uncapturedErrorCallbackInfo = {
		.nextInChain = nullptr,
		.callback = &on_uncaptured_error,
		.userdata1 = nullptr,
		.userdata2 = nullptr,
	};
	wgpuAdapterRequestDevice(m_adapter, &device_desc, WGPURequestDeviceCallbackInfo{
		.nextInChain = nullptr,
		.mode = WGPUCallbackMode_AllowProcessEvents,
		.callback = [](const WGPURequestDeviceStatus status, WGPUDevice device, const WGPUStringView message, void* userdata1, void*) {
			auto* out = static_cast<device_result*>(userdata1);
			if (status == WGPURequestDeviceStatus_Success) {
				out->device = device;
			}
			out->message = std::string(string_of(message));
			out->done = true;
		},
		.userdata1 = &device_out,
		.userdata2 = nullptr,
	});
	while (!device_out.done) {
		wgpuInstanceProcessEvents(m_instance);
	}
	assert(device_out.device != nullptr, "webgpu: device request failed: {}", device_out.message);
	m_device = device_out.device;
	m_queue = wgpuDeviceGetQueue(m_device);

	WGPULimits limits{};
	if (wgpuDeviceGetLimits(m_device, &limits) == WGPUStatus_Success) {
		log::println(
			log::category::render,
			"webgpu device: max_texture_2d={} max_bind_groups={} max_storage_buffer_binding={} max_buffer={} storage_buffers_per_stage={} storage_textures_per_stage={} optional_features={} ray_queries=unsupported",
			limits.maxTextureDimension2D,
			limits.maxBindGroups,
			limits.maxStorageBufferBindingSize,
			limits.maxBufferSize,
			limits.maxStorageBuffersPerShaderStage,
			limits.maxStorageTexturesPerShaderStage,
			features.size()
		);
		m_storage_offset_alignment = limits.minStorageBufferOffsetAlignment;
	}

	m_slots.resize(texture_capacity + image_capacity + buffer_capacity);
	m_samplers.resize(sampler_capacity);
	m_nearest_samplers.resize(sampler_capacity);
	const WGPUBufferDescriptor ring_desc{
		.nextInChain = nullptr,
		.label = label_of("gse.webgpu.push_ring"),
		.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst,
		.size = push_ring_size,
		.mappedAtCreation = false,
	};
	m_push_ring = wgpuDeviceCreateBuffer(m_device, &ring_desc);
	create_dummies();

	m_image_pool.base_index = texture_capacity;
	m_image_pool.reset(image_capacity);
	m_buffer_pool.base_index = texture_capacity + image_capacity;
	m_buffer_pool.reset(buffer_capacity);
	m_texture_pool.reset(texture_capacity);
	m_sampler_pool.base_index = texture_capacity;
	m_sampler_pool.reset(sampler_capacity);

	for (auto& queue_lists : m_frame_lists) {
		for (auto& list : queue_lists) {
			list = new_list();
		}
	}
}

gse::webgpu::device::~device() {
	log::println(log::category::render, "webgpu bindless high water: buffer {}/{} image {}/{} texture {}/{} sampler {}/{}", m_buffer_pool.high_water, m_buffer_pool.capacity, m_image_pool.high_water, m_image_pool.capacity, m_texture_pool.high_water, m_texture_pool.capacity, m_sampler_pool.high_water, m_sampler_pool.capacity);
	log::println(log::category::render, "webgpu push ring high water: {}/{}", m_push_high_water, push_ring_size);
	wait_idle();
	for (auto& list : m_lists) {
		if (list.pass) {
			wgpuRenderPassEncoderRelease(list.pass);
		}
		if (list.encoder) {
			wgpuCommandEncoderRelease(list.encoder);
		}
		if (list.finished) {
			wgpuCommandBufferRelease(list.finished);
		}
	}
	for (auto& view : m_views) {
		if (view.view) {
			wgpuTextureViewRelease(view.view);
		}
	}
	for (auto& texture : m_textures) {
		if (texture.default_view) {
			wgpuTextureViewRelease(texture.default_view);
		}
		if (texture.texture) {
			wgpuTextureRelease(texture.texture);
		}
	}
	for (auto& buffer : m_buffers) {
		if (buffer.buffer) {
			wgpuBufferRelease(buffer.buffer);
		}
	}
	for (auto& surface : m_surfaces) {
		for (std::uint32_t i = 0; i < swapchain_image_count; ++i) {
			release_surface_image(surface, i);
		}
		if (surface.surface) {
			if (surface.configured) {
				wgpuSurfaceUnconfigure(surface.surface);
			}
			wgpuSurfaceRelease(surface.surface);
		}
	}
	if (m_queue) {
		wgpuQueueRelease(m_queue);
	}
	if (m_device) {
		wgpuDeviceRelease(m_device);
	}
	if (m_adapter) {
		wgpuAdapterRelease(m_adapter);
	}
	if (m_instance) {
		wgpuInstanceRelease(m_instance);
	}
}

auto gse::webgpu::device::raw() const -> WGPUDevice {
	return m_device;
}

auto gse::webgpu::device::lost() const -> bool {
	return m_lost.load(std::memory_order_acquire);
}

auto gse::webgpu::device::process_events() -> void {
	const std::lock_guard _(m_events_mutex);
	wgpuInstanceProcessEvents(m_instance);
}

auto gse::webgpu::device::serial_reached(const std::uint64_t serial) const -> bool {
	return m_completed.load(std::memory_order_acquire) >= serial;
}

auto gse::webgpu::device::readbacks_settled(const std::uint64_t serial) -> bool {
	const std::lock_guard _(m_mutex);
	return readbacks_settled_locked(serial);
}

auto gse::webgpu::device::readbacks_settled_locked(const std::uint64_t serial) const -> bool {
	return std::ranges::none_of(m_mapping, [serial](const buffer_record* buffer) {
		return buffer->mapping_serial <= serial;
	});
}

auto gse::webgpu::device::finish_readback(buffer_record& buffer, const bool mapped) -> void {
	if (mapped) {
		const auto size = mapped_size(buffer);
		const auto* data = static_cast<const std::byte*>(wgpuBufferGetConstMappedRange(buffer.buffer, 0, size));
		std::memcpy(buffer.shadow.get(), data, size);
		wgpuBufferUnmap(buffer.buffer);
	}
	const std::lock_guard _(m_mutex);
	buffer.mapping_serial = 0;
	std::erase(m_mapping, &buffer);
}

auto gse::webgpu::device::wait_serial(const std::uint64_t serial) -> void {
	while ((!serial_reached(serial) || !readbacks_settled(serial)) && !lost()) {
		process_events();
		std::this_thread::yield();
	}
}

auto gse::webgpu::device::track_submission(const std::uint64_t serial) -> void {
	wgpuQueueOnSubmittedWorkDone(m_queue, WGPUQueueWorkDoneCallbackInfo{
		.nextInChain = nullptr,
		.mode = WGPUCallbackMode_AllowProcessEvents,
		.callback = &on_work_done,
		.userdata1 = &m_completed,
		.userdata2 = std::bit_cast<void*>(static_cast<std::uintptr_t>(serial)),
	});
}

auto gse::webgpu::device::wait_idle() -> void {
	wait_serial(m_submitted.load(std::memory_order_acquire));
}

auto gse::webgpu::device::new_list() -> command_list* {
	auto& list = m_lists.emplace_back();
	list.owner = this;
	return &list;
}

auto gse::webgpu::device::frame_command_buffer(const gpu::queue_type queue, const std::uint32_t frame_index) -> gpu::command_buffer_handle {
	return { address_of(m_frame_lists[static_cast<std::size_t>(queue)][frame_index % gpu::max_frames_in_flight]) };
}

auto gse::webgpu::device::reset_worker_command_pools(const std::uint32_t frame_index) -> void {
	const std::lock_guard _(m_mutex);
	auto& pool = m_worker_pools[frame_index % gpu::max_frames_in_flight];
	for (command_list* list : pool.live) {
		pool.free.push_back(list);
	}
	pool.live.clear();
}

auto gse::webgpu::device::acquire_worker_command_buffer(const std::uint32_t frame_index) -> gpu::command_buffer_handle {
	const std::lock_guard _(m_mutex);
	auto& pool = m_worker_pools[frame_index % gpu::max_frames_in_flight];
	command_list* list = nullptr;
	if (pool.free.empty()) {
		list = new_list();
	}
	else {
		list = pool.free.back();
		pool.free.pop_back();
	}
	pool.live.push_back(list);
	return { address_of(list) };
}

auto gse::webgpu::device::create_transient_command_pool() -> gpu::transient_pool_handle {
	const std::lock_guard _(m_mutex);
	m_transient_pools.emplace_back();
	return { .index = static_cast<std::uint32_t>(m_transient_pools.size() - 1) };
}

auto gse::webgpu::device::allocate_transient_primary(const gpu::transient_pool_handle pool_handle) -> gpu::command_buffer_handle {
	const std::lock_guard _(m_mutex);
	auto* pool = &m_transient_pools[pool_handle.index];
	command_list* list = nullptr;
	if (pool->free.empty()) {
		list = new_list();
	}
	else {
		list = pool->free.back();
		pool->free.pop_back();
	}
	list->marked = false;
	pool->live.push_back(list);
	return { address_of(list) };
}

auto gse::webgpu::device::transient_pool_try_reset(const gpu::transient_pool_handle pool_handle, const std::uint64_t queue_progress) -> void {
	const std::lock_guard _(m_mutex);
	auto* pool = &m_transient_pools[pool_handle.index];
	std::erase_if(pool->live, [&](command_list* list) {
		if (!list->marked || list->in_use_value > queue_progress) {
			return false;
		}
		pool->free.push_back(list);
		return true;
	});
}

auto gse::webgpu::device::transient_pool_mark_in_use(const gpu::transient_pool_handle pool_handle, const std::uint64_t value) -> void {
	const std::lock_guard _(m_mutex);
	auto* pool = &m_transient_pools[pool_handle.index];
	for (command_list* list : pool->live) {
		if (!list->marked) {
			list->marked = true;
			list->in_use_value = value;
		}
	}
}

auto gse::webgpu::device::transient_pool_reset_all(const gpu::transient_pool_handle pool_handle) -> void {
	const std::lock_guard _(m_mutex);
	auto* pool = &m_transient_pools[pool_handle.index];
	for (command_list* list : pool->live) {
		pool->free.push_back(list);
	}
	pool->live.clear();
}

auto gse::webgpu::device::cmd_reset(const gpu::command_buffer_handle cmd) -> void {
	auto* list = list_of(cmd);
	if (list->pass) {
		wgpuRenderPassEncoderRelease(list->pass);
		list->pass = nullptr;
	}
	if (list->encoder) {
		wgpuCommandEncoderRelease(list->encoder);
		list->encoder = nullptr;
	}
	if (list->finished) {
		wgpuCommandBufferRelease(list->finished);
		list->finished = nullptr;
	}
}

auto gse::webgpu::device::cmd_begin(const gpu::command_buffer_handle cmd) -> void {
	cmd_reset(cmd);
	list_of(cmd)->encoder = wgpuDeviceCreateCommandEncoder(m_device, nullptr);
}

auto gse::webgpu::device::cmd_end(const gpu::command_buffer_handle cmd) -> void {
	auto* list = list_of(cmd);
	assert(list->encoder != nullptr, "webgpu: cmd_end on a command buffer that was never begun");
	assert(list->pass == nullptr, "webgpu: cmd_end inside an open render pass");
	list->finished = wgpuCommandEncoderFinish(list->encoder, nullptr);
	wgpuCommandEncoderRelease(list->encoder);
	list->encoder = nullptr;
}

auto gse::webgpu::device::begin_debug_event(const gpu::command_buffer_handle cmd, const std::string_view label) -> void {
	auto* list = list_of(cmd);
	if (list->pass) {
		wgpuRenderPassEncoderPushDebugGroup(list->pass, label_of(label));
	}
	else if (list->encoder) {
		wgpuCommandEncoderPushDebugGroup(list->encoder, label_of(label));
	}
}

auto gse::webgpu::device::end_debug_event(const gpu::command_buffer_handle cmd) -> void {
	auto* list = list_of(cmd);
	if (list->pass) {
		wgpuRenderPassEncoderPopDebugGroup(list->pass);
	}
	else if (list->encoder) {
		wgpuCommandEncoderPopDebugGroup(list->encoder);
	}
}

auto gse::webgpu::device::submit(const gpu::submit_info& info, const gpu::handle<gpu::fence> signal_fence) -> gpu::result {
	if (lost()) {
		return gpu::result::error_device_lost;
	}

	std::vector<WGPUCommandBuffer> buffers;
	buffers.reserve(info.command_buffers.size());
	for (const auto& entry : info.command_buffers) {
		auto* list = list_of(entry.command_buffer);
		assert(list->finished != nullptr, "webgpu: submitted a command buffer that was not ended");
		buffers.push_back(list->finished);
		list->finished = nullptr;
	}

	const std::lock_guard _(m_mutex);
	const auto push_cursor = m_push_cursor.load(std::memory_order_acquire);
	m_push_high_water = std::max(m_push_high_water, push_cursor - m_push_submitted_cursor.load(std::memory_order_relaxed));
	m_push_submitted_cursor.store(push_cursor, std::memory_order_release);
	upload_host_writes();
	wgpuQueueSubmit(m_queue, buffers.size(), buffers.data());
	for (const auto buffer : buffers) {
		wgpuCommandBufferRelease(buffer);
	}

	const auto serial = m_submitted.fetch_add(1, std::memory_order_acq_rel) + 1;
	track_submission(serial);

	for (const auto& entry : info.command_buffers) {
		auto* list = list_of(entry.command_buffer);
		for (auto* readback : list->readbacks) {
			readback->mapping_serial = serial;
			m_mapping.push_back(readback);
			wgpuBufferMapAsync(readback->buffer, WGPUMapMode_Read, 0, mapped_size(*readback), WGPUBufferMapCallbackInfo{
				.nextInChain = nullptr,
				.mode = WGPUCallbackMode_AllowProcessEvents,
				.callback = &on_readback_mapped,
				.userdata1 = this,
				.userdata2 = readback,
			});
		}
		list->readbacks.clear();
	}

	for (const auto& signal : info.signal_semaphores) {
		if (auto* sem = record_of<semaphore_record>(signal.semaphore.value); sem && signal.value > 0) {
			sem->signals.push_back({ .value = signal.value, .serial = serial });
		}
	}
	if (auto* fence = record_of<fence_record>(signal_fence.value)) {
		fence->serial = serial;
		fence->pending = true;
	}
	return gpu::result::success;
}

auto gse::webgpu::device::wait_for_fence(const gpu::handle<gpu::fence> f, const std::uint64_t timeout_ns) -> gpu::result {
	auto* fence = record_of<fence_record>(f.value);
	if (!fence) {
		return gpu::result::success;
	}
	if (lost()) {
		return gpu::result::error_device_lost;
	}
	if (!fence->pending) {
		return fence->serial == 0 ? gpu::result::success : gpu::result::timeout;
	}
	if (timeout_ns == 0) {
		process_events();
		return serial_reached(fence->serial) && readbacks_settled(fence->serial) ? gpu::result::success : gpu::result::timeout;
	}
	wait_serial(fence->serial);
	return lost() ? gpu::result::error_device_lost : gpu::result::success;
}

auto gse::webgpu::device::reset_fence(const gpu::handle<gpu::fence> f) -> void {
	if (auto* fence = record_of<fence_record>(f.value)) {
		fence->pending = false;
		fence->serial = std::numeric_limits<std::uint64_t>::max();
	}
}

auto gse::webgpu::device::create_fence(const bool signaled) -> gpu::handle<gpu::fence> {
	const std::lock_guard _(m_mutex);
	auto& fence = m_fences.emplace_back();
	fence.serial = signaled ? 0 : std::numeric_limits<std::uint64_t>::max();
	return { address_of(&fence) };
}

auto gse::webgpu::device::create_semaphore(const std::uint64_t initial_value) -> gpu::handle<gpu::semaphore> {
	const std::lock_guard _(m_mutex);
	auto& sem = m_semaphores.emplace_back();
	sem.reached = initial_value;
	return { address_of(&sem) };
}

auto gse::webgpu::device::retire_fence(gpu::handle<gpu::fence>) -> void {
}

auto gse::webgpu::device::retire_semaphore(gpu::handle<gpu::semaphore>) -> void {
}

auto gse::webgpu::device::semaphore_counter_value(const gpu::handle<gpu::semaphore> s) -> std::uint64_t {
	process_events();
	const std::lock_guard _(m_mutex);
	auto* sem = record_of<semaphore_record>(s.value);
	std::erase_if(sem->signals, [&](const timeline_signal& signal) {
		if (!serial_reached(signal.serial) || !readbacks_settled_locked(signal.serial)) {
			return false;
		}
		sem->reached = std::max(sem->reached, signal.value);
		return true;
	});
	return sem->reached;
}

auto gse::webgpu::device::wait_semaphore_for(const gpu::handle<gpu::semaphore> s, const std::uint64_t value, const std::optional<time> timeout) -> bool {
	const clock timer;
	while (semaphore_counter_value(s) < value) {
		if (lost() || (timeout && timer.elapsed() > *timeout)) {
			return false;
		}
		std::this_thread::yield();
	}
	return true;
}

auto gse::webgpu::device::boot_surface() const -> gpu::surface {
	return { address_of(m_boot_surface) };
}

auto gse::webgpu::device::create_surface(const native_window_handle handle) -> gpu::surface {
	const std::lock_guard _(m_mutex);
	auto& surface = m_surfaces.emplace_back();
	surface.surface = create_platform_surface(m_instance, handle, label_of("gse.webgpu.surface"));
	assert(surface.surface != nullptr, "webgpu: surface creation failed for window {}", handle.value);
	for (std::uint32_t i = 0; i < swapchain_image_count; ++i) {
		surface.views[i].texture = &surface.images[i];
	}
	return { address_of(&surface) };
}

auto gse::webgpu::device::destroy_surface(const gpu::surface handle) -> void {
	auto* surface = record_of<surface_record>(handle.value);
	if (!surface || !surface->surface) {
		return;
	}
	wait_idle();
	for (std::uint32_t i = 0; i < swapchain_image_count; ++i) {
		release_surface_image(*surface, i);
	}
	if (surface->configured) {
		wgpuSurfaceUnconfigure(surface->surface);
	}
	wgpuSurfaceRelease(surface->surface);
	surface->surface = nullptr;
	surface->configured = false;
}

auto gse::webgpu::device::configure_surface(const gpu::surface handle, const vec2i framebuffer_size, const gpu::present_mode mode) -> gpu::expected<gpu::swap_chain_info> {
	auto* surface = record_of<surface_record>(handle.value);
	assert(surface != nullptr && surface->surface != nullptr, "webgpu: configure_surface on a null surface");
	wait_idle();
	for (std::uint32_t i = 0; i < swapchain_image_count; ++i) {
		release_surface_image(*surface, i);
	}

	const vec2u extent{
		static_cast<std::uint32_t>(std::max(framebuffer_size.x(), 1)),
		static_cast<std::uint32_t>(std::max(framebuffer_size.y(), 1)),
	};

	WGPUSurfaceCapabilities caps{};
	auto present_mode = present_mode_of(mode);
	WGPUTextureUsage usage = WGPUTextureUsage_RenderAttachment;
	if (wgpuSurfaceGetCapabilities(surface->surface, m_adapter, &caps) == WGPUStatus_Success) {
		usage |= caps.usages & (WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding);
		const std::span modes(caps.presentModes, caps.presentModeCount);
		if (std::ranges::find(modes, present_mode) == modes.end()) {
			log::println(log::level::warning, log::category::render, "webgpu: present mode {} unsupported by the surface, using fifo", mode);
			present_mode = WGPUPresentMode_Fifo;
		}
		wgpuSurfaceCapabilitiesFreeMembers(caps);
	}

	const WGPUSurfaceConfiguration config{
		.nextInChain = nullptr,
		.device = m_device,
		.format = WGPUTextureFormat_BGRA8Unorm,
		.usage = usage,
		.width = extent.x(),
		.height = extent.y(),
		.viewFormatCount = 0,
		.viewFormats = nullptr,
		.alphaMode = WGPUCompositeAlphaMode_Auto,
		.presentMode = present_mode,
	};
	wgpuSurfaceConfigure(surface->surface, &config);
	surface->configured = true;
	surface->extent = extent;
	surface->mode = mode;

	log::println(log::category::render, "webgpu surface configured {}x{} format=bgra8unorm requested_mode={} fifo_fallback={}", extent.x(), extent.y(), mode, present_mode != present_mode_of(mode));

	gpu::swap_chain_info out{
		.handle = handle_of<gpu::swap_chain_handle>(surface),
		.extent = extent,
		.format = gpu::image_format::b8g8r8a8_unorm,
		.present_mode = mode,
	};
	for (std::uint32_t i = 0; i < swapchain_image_count; ++i) {
		surface->images[i].format = gpu::image_format::b8g8r8a8_unorm;
		surface->images[i].extent = vec3u{ extent.x(), extent.y(), 1u };
		out.images.push_back(handle_of<gpu::handle<gpu::image>>(&surface->images[i]));
		out.image_views.push_back(handle_of<gpu::handle<gpu::image_view>>(&surface->views[i]));
	}
	return out;
}

auto gse::webgpu::device::release_surface_image(surface_record& surface, const std::uint32_t index) -> void {
	if (surface.views[index].view) {
		wgpuTextureViewRelease(surface.views[index].view);
		surface.views[index].view = nullptr;
	}
	if (surface.images[index].texture) {
		wgpuTextureRelease(surface.images[index].texture);
		surface.images[index].texture = nullptr;
	}
}

auto gse::webgpu::device::acquire_surface_image(const gpu::swap_chain_handle swapchain) -> gpu::acquire_next_image_result {
	auto* surface = record_of<surface_record>(swapchain);
	if (lost()) {
		return { .result = gpu::result::error_device_lost };
	}
	const auto index = surface->next_image;
	surface->next_image = (index + 1) % swapchain_image_count;
	release_surface_image(*surface, index);

	WGPUSurfaceTexture current{};
	wgpuSurfaceGetCurrentTexture(surface->surface, &current);
	switch (current.status) {
		case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
		case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
			break;
		case WGPUSurfaceGetCurrentTextureStatus_Outdated:
			return { .result = gpu::result::error_out_of_date_khr };
		case WGPUSurfaceGetCurrentTextureStatus_Lost:
			return { .result = gpu::result::error_surface_lost_khr };
		case WGPUSurfaceGetCurrentTextureStatus_Timeout:
			return { .result = gpu::result::timeout };
		default:
			return { .result = gpu::result::error_unknown };
	}

	surface->images[index].texture = current.texture;
	surface->views[index].view = wgpuTextureCreateView(current.texture, nullptr);
	return {
		.result = gpu::result::success,
		.image_index = index,
	};
}

auto gse::webgpu::device::present(const gpu::present_info& info) -> gpu::result {
	if (lost()) {
		return gpu::result::error_device_lost;
	}
	for (std::size_t i = 0; i < info.swapchains.size(); ++i) {
		auto* surface = record_of<surface_record>(info.swapchains[i]);
		const auto status = wgpuSurfacePresent(surface->surface);
		release_surface_image(*surface, info.image_indices[i]);
		if (status != WGPUStatus_Success) {
			return gpu::result::error_out_of_date_khr;
		}
	}
	return gpu::result::success;
}

auto gse::webgpu::device::create_buffer(const gpu::buffer_desc& desc, const std::string_view tag) -> gpu::buffer {
	const auto size = std::max<gpu::device_size>((desc.size + 3) & ~gpu::device_size{ 3 }, 4);

	WGPUBufferUsage usage = desc.readback ? WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst : WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc | WGPUBufferUsage_Storage;
	if (desc.usage.test(gpu::buffer_flag::index)) {
		usage |= WGPUBufferUsage_Index;
	}
	if (desc.usage.test(gpu::buffer_flag::indirect)) {
		usage |= WGPUBufferUsage_Indirect;
	}
	const WGPUBufferDescriptor buffer_desc{
		.nextInChain = nullptr,
		.label = label_of(tag),
		.usage = usage,
		.size = size,
		.mappedAtCreation = false,
	};

	const std::lock_guard _(m_mutex);
	auto& record = m_buffers.emplace_back();
	record.buffer = wgpuDeviceCreateBuffer(m_device, &buffer_desc);
	record.size = desc.size;
	if (!desc.device_local || desc.readback) {
		record.shadow = std::make_unique<std::byte[]>(size);
	}
	if (!desc.device_local && !desc.readback) {
		record.uploaded = std::make_unique<std::byte[]>(size);
	}
	if (desc.data) {
		auto staging = std::make_unique<std::byte[]>(size);
		std::memcpy(staging.get(), desc.data, desc.size);
		wgpuQueueWriteBuffer(m_queue, record.buffer, 0, staging.get(), size);
		if (record.shadow) {
			std::memcpy(record.shadow.get(), staging.get(), size);
		}
		if (record.uploaded) {
			std::memcpy(record.uploaded.get(), staging.get(), size);
		}
	}

	record.address = m_next_address;
	m_next_address += (size + 0xFFFF) & ~gpu::device_size{ 0xFFFF };
	m_addresses.emplace(record.address, &record);

	record.slot = gpu::takes_slot(desc) ? m_buffer_pool.allocate() : gpu::bindless_slot{};
	if (record.slot.valid()) {
		m_slots[record.slot.index] = { .buffer = record.buffer, .offset = 0, .size = size };
	}
	return gpu::buffer(handle_of<gpu::handle<gpu::buffer>>(&record), desc.size, record.address, record.shadow.get(), record.slot);
}

auto gse::webgpu::device::create_image(const gpu::image_desc& desc, const std::string_view tag) -> gpu::image {
	const bool cube = desc.view == gpu::image_view_type::cube;
	const bool volume = desc.view == gpu::image_view_type::e3d;
	const auto format = texture_format_of(desc.format);
	assert(format != WGPUTextureFormat_Undefined, "webgpu: image format {} has no WebGPU equivalent", desc.format);

	WGPUTextureUsage usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc;
	if (desc.usage.test(gpu::image_flag::sampled)) {
		usage |= WGPUTextureUsage_TextureBinding;
	}
	if (desc.usage.test(gpu::image_flag::storage)) {
		usage |= WGPUTextureUsage_StorageBinding;
	}
	if (desc.usage.test(gpu::image_flag::color_attachment) || desc.usage.test(gpu::image_flag::depth_attachment)) {
		usage |= WGPUTextureUsage_RenderAttachment;
	}

	const auto layers = cube ? 6u : volume ? 1u : std::max(desc.depth, 1u);
	const WGPUTextureDescriptor texture_desc{
		.nextInChain = nullptr,
		.label = label_of(tag),
		.usage = usage,
		.dimension = volume ? WGPUTextureDimension_3D : WGPUTextureDimension_2D,
		.size = {
			.width = desc.size.x(),
			.height = desc.size.y(),
			.depthOrArrayLayers = volume ? desc.depth : layers,
		},
		.format = format,
		.mipLevelCount = 1,
		.sampleCount = 1,
		.viewFormatCount = 0,
		.viewFormats = nullptr,
	};
	const WGPUTextureViewDescriptor view_desc{
		.nextInChain = nullptr,
		.label = label_of(tag),
		.format = format,
		.dimension = cube ? WGPUTextureViewDimension_Cube : volume ? WGPUTextureViewDimension_3D : layers > 1 ? WGPUTextureViewDimension_2DArray : WGPUTextureViewDimension_2D,
		.baseMipLevel = 0,
		.mipLevelCount = 1,
		.baseArrayLayer = 0,
		.arrayLayerCount = layers,
		.aspect = WGPUTextureAspect_All,
		.usage = WGPUTextureUsage_None,
	};

	const std::lock_guard _(m_mutex);
	auto& record = m_textures.emplace_back();
	record.texture = wgpuDeviceCreateTexture(m_device, &texture_desc);
	record.default_view = wgpuTextureCreateView(record.texture, &view_desc);
	record.format = desc.format;
	record.extent = vec3u{ desc.size.x(), desc.size.y(), desc.depth };

	auto& view = m_views.emplace_back();
	view.view = record.default_view;
	view.texture = &record;
	record.default_record = &view;

	const gpu::image_view_create_info view_info{
		.format = desc.format,
		.view_type = desc.view,
		.aspects = gpu::image_aspect_for(desc.format),
		.layer_count = layers,
	};
	record.storage_slot = desc.usage.test(gpu::image_flag::storage) ? m_image_pool.allocate() : gpu::bindless_slot{};
	record.sampled_slot = desc.usage.test(gpu::image_flag::sampled) ? m_texture_pool.allocate() : gpu::bindless_slot{};
	for (const auto slot : { record.storage_slot, record.sampled_slot }) {
		if (slot.valid()) {
			m_slots[slot.index] = { .view = record.default_view, .format = desc.format };
		}
	}
	return gpu::image(
		handle_of<gpu::handle<gpu::image>>(&record),
		handle_of<gpu::handle<gpu::image_view>>(&view),
		desc.format,
		record.extent,
		view_info,
		record.storage_slot,
		record.sampled_slot
	);
}

auto gse::webgpu::device::create_image_view(const gpu::handle<gpu::image> img, const gpu::image_view_create_info& info) -> gpu::handle<gpu::image_view> {
	auto* texture = record_of<texture_record>(img);
	const WGPUTextureViewDescriptor view_desc{
		.nextInChain = nullptr,
		.label = label_of("gse.webgpu.view"),
		.format = texture_format_of(info.format),
		.dimension = info.view_type == gpu::image_view_type::cube ? WGPUTextureViewDimension_Cube : info.view_type == gpu::image_view_type::e3d ? WGPUTextureViewDimension_3D : info.layer_count > 1 ? WGPUTextureViewDimension_2DArray : WGPUTextureViewDimension_2D,
		.baseMipLevel = info.base_mip_level,
		.mipLevelCount = info.level_count,
		.baseArrayLayer = info.base_array_layer,
		.arrayLayerCount = info.layer_count,
		.aspect = WGPUTextureAspect_All,
		.usage = WGPUTextureUsage_None,
	};
	const std::lock_guard _(m_mutex);
	auto& view = m_views.emplace_back();
	view.view = wgpuTextureCreateView(texture->texture, &view_desc);
	view.texture = texture;
	return handle_of<gpu::handle<gpu::image_view>>(&view);
}

auto gse::webgpu::device::host_upload_image_layers(const gpu::handle<gpu::image> img, const std::span<const void* const> layer_pointers, const vec2u extent) const -> void {
	auto* texture = record_of<texture_record>(img);
	const auto source_bytes = gpu::bytes_per_pixel(texture->format);
	const bool widen = source_bytes == 3;
	const auto bytes_per_row = extent.x() * (widen ? 4u : source_bytes);
	const auto texel_count = static_cast<std::size_t>(extent.x()) * extent.y();
	std::vector<std::byte> widened(widen ? texel_count * 4 : 0);
	for (std::uint32_t layer = 0; layer < layer_pointers.size(); ++layer) {
		const void* pixels = layer_pointers[layer];
		if (widen) {
			const auto* source = static_cast<const std::byte*>(pixels);
			for (std::size_t i = 0; i < texel_count; ++i) {
				widened[i * 4 + 0] = source[i * 3 + 0];
				widened[i * 4 + 1] = source[i * 3 + 1];
				widened[i * 4 + 2] = source[i * 3 + 2];
				widened[i * 4 + 3] = std::byte{ 0xFF };
			}
			pixels = widened.data();
		}
		const WGPUTexelCopyTextureInfo destination{
			.texture = texture->texture,
			.mipLevel = 0,
			.origin = { .x = 0, .y = 0, .z = layer },
			.aspect = WGPUTextureAspect_All,
		};
		const WGPUTexelCopyBufferLayout layout{
			.offset = 0,
			.bytesPerRow = bytes_per_row,
			.rowsPerImage = extent.y(),
		};
		const WGPUExtent3D size{
			.width = extent.x(),
			.height = extent.y(),
			.depthOrArrayLayers = 1,
		};
		wgpuQueueWriteTexture(m_queue, &destination, pixels,static_cast<std::size_t>(bytes_per_row) * extent.y(), &layout, &size);
	}
}

auto gse::webgpu::device::allocate_buffer_slot() -> gpu::bindless_handle {
	return gpu::bindless_handle(&m_buffer_pool, m_buffer_pool.allocate());
}

auto gse::webgpu::device::allocate_image_slot() -> gpu::bindless_handle {
	return gpu::bindless_handle(&m_image_pool, m_image_pool.allocate());
}

auto gse::webgpu::device::register_sampler(const gpu::sampler_desc& desc) -> gpu::bindless_handle {
	const auto address_of = [](const gpu::sampler_address_mode mode) {
		switch (mode) {
			case gpu::sampler_address_mode::repeat: return WGPUAddressMode_Repeat;
			case gpu::sampler_address_mode::mirrored_repeat: return WGPUAddressMode_MirrorRepeat;
			case gpu::sampler_address_mode::clamp_to_edge:
			case gpu::sampler_address_mode::clamp_to_border: return WGPUAddressMode_ClampToEdge;
		}
		return WGPUAddressMode_Repeat;
	};
	const auto filter_of = [](const gpu::sampler_filter filter) {
		return filter == gpu::sampler_filter::linear ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
	};
	const WGPUSamplerDescriptor sampler_desc{
		.nextInChain = nullptr,
		.label = label_of("gse.webgpu.sampler"),
		.addressModeU = address_of(desc.address_u),
		.addressModeV = address_of(desc.address_v),
		.addressModeW = address_of(desc.address_w),
		.magFilter = filter_of(desc.mag),
		.minFilter = filter_of(desc.min),
		.mipmapFilter = desc.min == gpu::sampler_filter::linear ? WGPUMipmapFilterMode_Linear : WGPUMipmapFilterMode_Nearest,
		.lodMinClamp = desc.min_lod,
		.lodMaxClamp = std::max(desc.max_lod, desc.min_lod),
		.compare = desc.compare_enable ? compare_function_of(desc.compare) : WGPUCompareFunction_Undefined,
		.maxAnisotropy = static_cast<std::uint16_t>(std::clamp(desc.max_anisotropy, 1.f, 16.f)),
	};
	const std::lock_guard _(m_mutex);
	auto handle = gpu::bindless_handle(&m_sampler_pool, m_sampler_pool.allocate());
	const auto sampler_index = handle.slot().index - m_sampler_pool.base_index;
	m_samplers[sampler_index] = wgpuDeviceCreateSampler(m_device, &sampler_desc);
	m_nearest_samplers[sampler_index] = desc.min == gpu::sampler_filter::nearest && desc.mag == gpu::sampler_filter::nearest;
	return handle;
}

auto gse::webgpu::device::register_texture(const gpu::image& img) -> gpu::bindless_handle {
	const std::lock_guard _(m_mutex);
	auto handle = gpu::bindless_handle(&m_texture_pool, m_texture_pool.allocate());
	const auto* view = record_of<view_record>(img.view());
	m_slots[handle.slot().index] = { .view = view->view, .format = view->texture->format };
	return handle;
}

auto gse::webgpu::device::write_storage_buffer(const gpu::bindless_slot slot, const gpu::device_address address, const gpu::device_size size) -> void {
	const std::lock_guard _(m_mutex);
	const auto it = m_addresses.upper_bound(address);
	assert(it != m_addresses.begin(), "webgpu: write_storage_buffer at address 0x{:x}, which no buffer owns", address);
	const auto* record = std::prev(it)->second;
	const auto offset = address - record->address;
	assert(offset + size <= ((record->size + 3) & ~gpu::device_size{ 3 }), "webgpu: write_storage_buffer range [0x{:x}, +{}) runs past its buffer", address, size);
	m_slots[slot.index] = { .buffer = record->buffer, .offset = offset, .size = size };
}

auto gse::webgpu::device::write_sampled_image(const gpu::bindless_slot slot, const gpu::image& img) -> void {
	const std::lock_guard _(m_mutex);
	const auto* view = record_of<view_record>(img.view());
	m_slots[slot.index] = { .view = view->view, .format = view->texture->format };
}

auto gse::webgpu::device::upload_host_writes() -> void {
	constexpr gpu::device_size chunk = 256;
	for (auto& buffer : m_buffers) {
		if (!buffer.uploaded) {
			continue;
		}
		const auto size = std::max<gpu::device_size>((buffer.size + 3) & ~gpu::device_size{ 3 }, 4);
		gpu::device_size run_begin = size;
		for (gpu::device_size at = 0; at < size + chunk; at += chunk) {
			const auto length = std::min(chunk, size - std::min(at, size));
			const bool changed = length > 0 && std::memcmp(buffer.shadow.get() + at, buffer.uploaded.get() + at, length) != 0;
			if (changed && run_begin == size) {
				run_begin = at;
			}
			if (!changed && run_begin != size) {
				const auto run_end = std::min(at, size);
				std::memcpy(buffer.uploaded.get() + run_begin, buffer.shadow.get() + run_begin, run_end - run_begin);
				wgpuQueueWriteBuffer(m_queue, buffer.buffer, run_begin, buffer.uploaded.get() + run_begin, run_end - run_begin);
				run_begin = size;
			}
		}
	}
}

auto gse::webgpu::device::write_texture_region(const texture_record& texture, const std::byte* data, const gpu::buffer_image_copy_region& region) const -> void {
	const auto row_texels = region.buffer_row_length != 0 ? region.buffer_row_length : region.image_extent.x();
	const auto image_rows = region.buffer_image_height != 0 ? region.buffer_image_height : region.image_extent.y();
	const auto bytes_per_row = row_texels * gpu::bytes_per_pixel(texture.format);
	const auto layers = std::max(region.image_extent.z(), region.image_subresource.layer_count);
	const WGPUTexelCopyTextureInfo destination{
		.texture = texture.texture,
		.mipLevel = region.image_subresource.mip_level,
		.origin = { .x = static_cast<std::uint32_t>(region.image_offset.x()), .y = static_cast<std::uint32_t>(region.image_offset.y()), .z = static_cast<std::uint32_t>(region.image_offset.z()) + region.image_subresource.base_array_layer },
		.aspect = WGPUTextureAspect_All,
	};
	const WGPUTexelCopyBufferLayout layout{
		.offset = 0,
		.bytesPerRow = bytes_per_row,
		.rowsPerImage = image_rows,
	};
	const WGPUExtent3D size{
		.width = region.image_extent.x(),
		.height = region.image_extent.y(),
		.depthOrArrayLayers = layers,
	};
	wgpuQueueWriteTexture(m_queue, &destination, data, static_cast<std::size_t>(bytes_per_row) * image_rows * layers, &layout, &size);
}

auto gse::webgpu::device::collect_garbage() -> void {
	process_events();
}

auto gse::webgpu::device::create_dummies() -> void {
	const WGPUBufferDescriptor buffer_desc{
		.nextInChain = nullptr,
		.label = label_of("gse.webgpu.dummy_buffer"),
		.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst,
		.size = dummy_buffer_size,
		.mappedAtCreation = false,
	};
	m_dummy_buffer = wgpuDeviceCreateBuffer(m_device, &buffer_desc);

	const auto make_texture = [&](const WGPUTextureDimension dimension, WGPUTexture& texture, WGPUTextureView& view) {
		const WGPUTextureDescriptor desc{
			.nextInChain = nullptr,
			.label = label_of("gse.webgpu.dummy_texture"),
			.usage = WGPUTextureUsage_TextureBinding,
			.dimension = dimension,
			.size = { .width = 1, .height = 1, .depthOrArrayLayers = 1 },
			.format = WGPUTextureFormat_RGBA8Unorm,
			.mipLevelCount = 1,
			.sampleCount = 1,
			.viewFormatCount = 0,
			.viewFormats = nullptr,
		};
		texture = wgpuDeviceCreateTexture(m_device, &desc);
		view = wgpuTextureCreateView(texture, nullptr);
	};
	make_texture(WGPUTextureDimension_2D, m_dummy_texture_2d, m_dummy_view_2d);
	make_texture(WGPUTextureDimension_3D, m_dummy_texture_3d, m_dummy_view_3d);

	const WGPUSamplerDescriptor sampler_desc{
		.nextInChain = nullptr,
		.label = label_of("gse.webgpu.dummy_sampler"),
		.addressModeU = WGPUAddressMode_ClampToEdge,
		.addressModeV = WGPUAddressMode_ClampToEdge,
		.addressModeW = WGPUAddressMode_ClampToEdge,
		.magFilter = WGPUFilterMode_Linear,
		.minFilter = WGPUFilterMode_Linear,
		.mipmapFilter = WGPUMipmapFilterMode_Nearest,
		.lodMinClamp = 0.f,
		.lodMaxClamp = 32.f,
		.compare = WGPUCompareFunction_Undefined,
		.maxAnisotropy = 1,
	};
	m_dummy_sampler = wgpuDeviceCreateSampler(m_device, &sampler_desc);
}

auto gse::webgpu::device::dummy_storage_view(const gpu::binding_kind kind, const gpu::image_format format) -> WGPUTextureView {
	const auto key = std::pair(kind, format);
	if (const auto it = m_dummy_storage.find(key); it != m_dummy_storage.end()) {
		return it->second.second;
	}
	const WGPUTextureDescriptor desc{
		.nextInChain = nullptr,
		.label = label_of("gse.webgpu.dummy_storage"),
		.usage = WGPUTextureUsage_StorageBinding,
		.dimension = kind == gpu::binding_kind::storage_texture_3d ? WGPUTextureDimension_3D : WGPUTextureDimension_2D,
		.size = { .width = 1, .height = 1, .depthOrArrayLayers = 1 },
		.format = texture_format_of(format),
		.mipLevelCount = 1,
		.sampleCount = 1,
		.viewFormatCount = 0,
		.viewFormats = nullptr,
	};
	auto* texture = wgpuDeviceCreateTexture(m_device, &desc);
	auto* view = wgpuTextureCreateView(texture, nullptr);
	m_dummy_storage.emplace(key, std::pair(texture, view));
	return view;
}

auto gse::webgpu::device::create_program(const gpu::shader_program_create_info& info) -> gpu::shader_program {
	program_record* program = nullptr;
	{
		const std::lock_guard _(m_mutex);
		program = &m_programs.emplace_back();
	}
	program->compute = info.is_compute;
	program->bindings.assign(info.bindings.begin(), info.bindings.end());
	program->push_size = info.push_offset_start;
	program->binding_offset = info.push_offset_start;
	assert(program->push_size <= max_push_bytes, "webgpu: {} B of push constants exceeds the {} B push slice", program->push_size, max_push_bytes);
	program->state = info.state;
	program->color_target_count = info.color_target_count;
	program->label = std::string(info.label);

	std::vector<gpu::stage_flag> stages;
	{
		const std::lock_guard _(m_scope_mutex);
		wgpuDevicePushErrorScope(m_device, WGPUErrorFilter_Validation);
		for (const auto& stage : info.stages) {
			const std::string_view words(reinterpret_cast<const char*>(stage.spirv.data()), stage.spirv.size_bytes());
			const auto code = repair_slang_wgsl(words.substr(0, words.find('\0')));
			WGPUShaderSourceWGSL source{
				.chain = { .next = nullptr, .sType = WGPUSType_ShaderSourceWGSL },
				.code = label_of(code),
			};
			const WGPUShaderModuleDescriptor module_desc{
				.nextInChain = &source.chain,
				.label = label_of(program->label),
			};
			program->stages.push_back({
				.stage = stage.stage,
				.module = wgpuDeviceCreateShaderModule(m_device, &module_desc),
				.entry_point = std::string(stage.entry_point),
			});
			stages.push_back(stage.stage);
		}
		pop_validation_scope(m_device, program->label);
	}

	if (program->push_size > 0) {
		WGPUBindGroupLayoutEntry entry{};
		entry.binding = 0;
		entry.visibility = program->compute ? WGPUShaderStage_Compute : WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
		entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
		entry.buffer.hasDynamicOffset = true;
		const WGPUBindGroupLayoutDescriptor layout_desc{
			.nextInChain = nullptr,
			.label = label_of("gse.webgpu.push_layout"),
			.entryCount = 1,
			.entries = &entry,
		};
		program->push_layout = wgpuDeviceCreateBindGroupLayout(m_device, &layout_desc);
		const WGPUBindGroupEntry group_entry{
			.nextInChain = nullptr,
			.binding = 0,
			.buffer = m_push_ring,
			.offset = 0,
			.size = (program->push_size + 3) & ~std::uint64_t{ 3 },
			.sampler = nullptr,
			.textureView = nullptr,
		};
		const WGPUBindGroupDescriptor group_desc{
			.nextInChain = nullptr,
			.label = label_of("gse.webgpu.push_group"),
			.layout = program->push_layout,
			.entryCount = 1,
			.entries = &group_entry,
		};
		program->push_group = wgpuDeviceCreateBindGroup(m_device, &group_desc);
	}

	attachment_formats prewarm;
	if (!program->compute) {
		for (std::uint32_t i = 0; i < info.color_target_count && i < max_color_attachments; ++i) {
			prewarm.colors[prewarm.color_count++] = color_format_of(info.color_targets[i]);
		}
		prewarm.depth = info.depth_target == gpu::depth_format::d32_sfloat ? WGPUTextureFormat_Depth32Float : WGPUTextureFormat_Undefined;
	}
	{
		const std::lock_guard _(m_mutex);
		(void)variant_for(*program, prewarm, 0);
	}

	const auto handle = handle_of<gpu::handle<gpu::shader_object>>(program);
	return gpu::shader_program(
		handle_of<gpu::handle<gpu::pipeline_layout>>(program),
		stages,
		std::vector(stages.size(), handle),
		info.state,
		info.is_compute,
		info.is_mesh
	);
}

auto gse::webgpu::device::variant_for(program_record& program, const attachment_formats& formats, const std::uint64_t unfilterable) -> program_variant& {
	for (auto& variant : program.variants) {
		if (variant.unfilterable == unfilterable && (program.compute || variant.formats == formats)) {
			return variant;
		}
	}
	auto& variant = program.variants.emplace_back();
	variant.formats = formats;
	variant.unfilterable = unfilterable;
	build_variant(program, variant);
	return variant;
}

auto gse::webgpu::device::build_variant(const program_record& program, program_variant& variant) -> void {
	WGPUShaderStage visibility = WGPUShaderStage_None;
	for (const auto& stage : program.stages) {
		visibility |= shader_stage_of(stage.stage);
	}
	const WGPUShaderStage writable_visibility = program.compute ? visibility : visibility & ~WGPUShaderStage_Vertex;
	const std::lock_guard _(m_scope_mutex);
	wgpuDevicePushErrorScope(m_device, WGPUErrorFilter_Validation);

	std::vector<WGPUBindGroupLayoutEntry> entries(program.bindings.size());
	for (std::uint32_t i = 0; i < program.bindings.size(); ++i) {
		const auto& binding = program.bindings[i];
		auto& entry = entries[i];
		entry.binding = i;
		entry.visibility = visibility;
		switch (binding.kind) {
			case gpu::binding_kind::read_only_storage_buffer:
				entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
				break;
			case gpu::binding_kind::storage_buffer:
				entry.visibility = writable_visibility;
				entry.buffer.type = WGPUBufferBindingType_Storage;
				break;
			case gpu::binding_kind::sampled_texture_2d:
			case gpu::binding_kind::sampled_texture_3d:
				entry.texture.sampleType = (i < 64 && (variant.unfilterable >> i) & 1) ? WGPUTextureSampleType_UnfilterableFloat : WGPUTextureSampleType_Float;
				entry.texture.viewDimension = binding.kind == gpu::binding_kind::sampled_texture_3d ? WGPUTextureViewDimension_3D : WGPUTextureViewDimension_2D;
				break;
			case gpu::binding_kind::storage_texture_2d:
			case gpu::binding_kind::storage_texture_3d:
				entry.visibility = writable_visibility;
				entry.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
				entry.storageTexture.format = texture_format_of(binding.storage_format);
				entry.storageTexture.viewDimension = binding.kind == gpu::binding_kind::storage_texture_3d ? WGPUTextureViewDimension_3D : WGPUTextureViewDimension_2D;
				break;
			case gpu::binding_kind::sampler:
				entry.sampler.type = (i < 64 && (variant.unfilterable >> i) & 1) ? WGPUSamplerBindingType_NonFiltering : WGPUSamplerBindingType_Filtering;
				break;
			case gpu::binding_kind::acceleration_structure:
				assert(false, "webgpu: {} binds an acceleration structure, which this backend cannot express", program.label);
				break;
		}
	}
	const WGPUBindGroupLayoutDescriptor layout_desc{
		.nextInChain = nullptr,
		.label = label_of(program.label),
		.entryCount = entries.size(),
		.entries = entries.data(),
	};
	variant.group_layout = wgpuDeviceCreateBindGroupLayout(m_device, &layout_desc);

	std::array<WGPUBindGroupLayout, 2> layouts{ variant.group_layout, program.push_layout };
	const WGPUPipelineLayoutDescriptor pipeline_layout_desc{
		.nextInChain = nullptr,
		.label = label_of(program.label),
		.bindGroupLayoutCount = program.push_layout ? 2u : 1u,
		.bindGroupLayouts = layouts.data(),
		.immediateSize = 0,
	};
	variant.pipeline_layout = wgpuDeviceCreatePipelineLayout(m_device, &pipeline_layout_desc);

	if (program.compute) {
		const auto& stage = program.stages.front();
		const WGPUComputePipelineDescriptor desc{
			.nextInChain = nullptr,
			.label = label_of(program.label),
			.layout = variant.pipeline_layout,
			.compute = {
				.nextInChain = nullptr,
				.module = stage.module,
				.entryPoint = label_of(stage.entry_point),
				.constantCount = 0,
				.constants = nullptr,
			},
		};
		variant.compute = wgpuDeviceCreateComputePipeline(m_device, &desc);
		pop_validation_scope(m_device, program.label);
		return;
	}

	const program_stage* vertex = nullptr;
	const program_stage* fragment = nullptr;
	for (const auto& stage : program.stages) {
		if (stage.stage == gpu::stage_flag::vertex) {
			vertex = &stage;
		}
		else if (stage.stage == gpu::stage_flag::fragment) {
			fragment = &stage;
		}
	}
	assert(vertex != nullptr, "webgpu: graphics program {} has no vertex stage", program.label);

	const auto& state = program.state;
	std::array<WGPUBlendState, max_color_attachments> blends{};
	std::array<WGPUColorTargetState, max_color_attachments> targets{};
	for (std::uint32_t i = 0; i < variant.formats.color_count; ++i) {
		auto& target = targets[i];
		target.format = variant.formats.colors[i];
		target.writeMask = WGPUColorWriteMask_None;
		if (i >= program.color_target_count) {
			continue;
		}
		const auto mask = i < state.color_write_masks.size() ? state.color_write_masks[i] : gpu::color_component_flags{};
		target.writeMask = (mask.test(gpu::color_component_flag::r) ? WGPUColorWriteMask_Red : WGPUColorWriteMask_None)
			| (mask.test(gpu::color_component_flag::g) ? WGPUColorWriteMask_Green : WGPUColorWriteMask_None)
			| (mask.test(gpu::color_component_flag::b) ? WGPUColorWriteMask_Blue : WGPUColorWriteMask_None)
			| (mask.test(gpu::color_component_flag::a) ? WGPUColorWriteMask_Alpha : WGPUColorWriteMask_None);
		if (i < state.blend_enables.size() && state.blend_enables[i] && i < state.blend_equations.size()) {
			const auto& eq = state.blend_equations[i];
			blends[i] = {
				.color = { .operation = blend_operation_of(eq.color_op), .srcFactor = blend_factor_of(eq.src_color), .dstFactor = blend_factor_of(eq.dst_color) },
				.alpha = { .operation = blend_operation_of(eq.alpha_op), .srcFactor = blend_factor_of(eq.src_alpha), .dstFactor = blend_factor_of(eq.dst_alpha) },
			};
			target.blend = &blends[i];
		}
	}

	WGPUDepthStencilState depth{};
	if (variant.formats.depth != WGPUTextureFormat_Undefined) {
		depth.format = variant.formats.depth;
		depth.depthWriteEnabled = state.depth.test && state.depth.write ? WGPUOptionalBool_True : WGPUOptionalBool_False;
		depth.depthCompare = state.depth.test ? compare_function_of(state.depth.compare) : WGPUCompareFunction_Always;
		depth.stencilReadMask = 0;
		depth.stencilWriteMask = 0;
		if (state.depth_bias_enable) {
			depth.depthBias = static_cast<std::int32_t>(state.depth_bias_constant);
			depth.depthBiasSlopeScale = state.depth_bias_slope;
			depth.depthBiasClamp = state.depth_bias_clamp;
		}
	}

	const WGPUFragmentState fragment_state{
		.nextInChain = nullptr,
		.module = fragment ? fragment->module : nullptr,
		.entryPoint = fragment ? label_of(fragment->entry_point) : WGPUStringView{},
		.constantCount = 0,
		.constants = nullptr,
		.targetCount = variant.formats.color_count,
		.targets = targets.data(),
	};

	const auto topology_of = [](const gpu::topology t) {
		switch (t) {
			case gpu::topology::line_list: return WGPUPrimitiveTopology_LineList;
			case gpu::topology::point_list: return WGPUPrimitiveTopology_PointList;
			case gpu::topology::triangle_list: return WGPUPrimitiveTopology_TriangleList;
		}
		return WGPUPrimitiveTopology_TriangleList;
	};
	const auto cull_of = [](const gpu::cull_mode c) {
		switch (c) {
			case gpu::cull_mode::none: return WGPUCullMode_None;
			case gpu::cull_mode::front: return WGPUCullMode_Front;
			case gpu::cull_mode::back: return WGPUCullMode_Back;
		}
		return WGPUCullMode_None;
	};

	const WGPURenderPipelineDescriptor desc{
		.nextInChain = nullptr,
		.label = label_of(program.label),
		.layout = variant.pipeline_layout,
		.vertex = {
			.nextInChain = nullptr,
			.module = vertex->module,
			.entryPoint = label_of(vertex->entry_point),
			.constantCount = 0,
			.constants = nullptr,
			.bufferCount = 0,
			.buffers = nullptr,
		},
		.primitive = {
			.nextInChain = nullptr,
			.topology = topology_of(state.topology),
			.stripIndexFormat = WGPUIndexFormat_Undefined,
			.frontFace = state.front == gpu::front_face::counter_clockwise ? WGPUFrontFace_CCW : WGPUFrontFace_CW,
			.cullMode = cull_of(state.cull),
			.unclippedDepth = false,
		},
		.depthStencil = variant.formats.depth != WGPUTextureFormat_Undefined ? &depth : nullptr,
		.multisample = {
			.nextInChain = nullptr,
			.count = 1,
			.mask = 0xFFFFFFFFu,
			.alphaToCoverageEnabled = false,
		},
		.fragment = fragment ? &fragment_state : nullptr,
	};
	variant.render = wgpuDeviceCreateRenderPipeline(m_device, &desc);
	pop_validation_scope(m_device, program.label);
}

auto gse::webgpu::device::unfilterable_mask(const program_record& program, const command_list& list) -> std::uint64_t {
	std::uint64_t mask = 0;
	for (std::uint32_t i = 0; i < program.bindings.size() && i < 64; ++i) {
		const auto& binding = program.bindings[i];
		const auto index = pushed_slot(program, binding, list);
		if (binding.kind == gpu::binding_kind::sampler) {
			const auto sampler_index = index - m_sampler_pool.base_index;
			if (index >= m_sampler_pool.base_index && sampler_index < m_samplers.size() && m_samplers[sampler_index] && m_nearest_samplers[sampler_index]) {
				mask |= std::uint64_t{ 1 } << i;
			}
			continue;
		}
		if (binding.kind != gpu::binding_kind::sampled_texture_2d && binding.kind != gpu::binding_kind::sampled_texture_3d) {
			continue;
		}
		if (index < m_slots.size() && m_slots[index].view && is_unfilterable(m_slots[index].format)) {
			mask |= std::uint64_t{ 1 } << i;
		}
	}
	return mask;
}

auto gse::webgpu::device::make_group(const program_record& program, const program_variant& variant, const command_list& list) -> WGPUBindGroup {
	std::vector<WGPUBindGroupEntry> entries(program.bindings.size());
	for (std::uint32_t i = 0; i < program.bindings.size(); ++i) {
		const auto& binding = program.bindings[i];
		auto& entry = entries[i];
		entry.binding = i;
		const auto index = pushed_slot(program, binding, list);
		const slot_entry* slot = index < m_slots.size() ? &m_slots[index] : nullptr;
		switch (binding.kind) {
			case gpu::binding_kind::read_only_storage_buffer:
			case gpu::binding_kind::storage_buffer:
				if (slot && slot->buffer) {
					entry.buffer = slot->buffer;
					entry.offset = slot->offset;
					entry.size = (slot->size + 3) & ~gpu::device_size{ 3 };
				}
				else {
					entry.buffer = m_dummy_buffer;
					entry.size = dummy_buffer_size;
				}
				break;
			case gpu::binding_kind::sampled_texture_2d:
				entry.textureView = slot && slot->view ? slot->view : m_dummy_view_2d;
				break;
			case gpu::binding_kind::sampled_texture_3d:
				entry.textureView = slot && slot->view ? slot->view : m_dummy_view_3d;
				break;
			case gpu::binding_kind::storage_texture_2d:
			case gpu::binding_kind::storage_texture_3d:
				entry.textureView = slot && slot->view ? slot->view : dummy_storage_view(binding.kind, binding.storage_format);
				break;
			case gpu::binding_kind::sampler: {
				const auto sampler_index = index - m_sampler_pool.base_index;
				entry.sampler = index >= m_sampler_pool.base_index && sampler_index < m_samplers.size() && m_samplers[sampler_index] ? m_samplers[sampler_index] : m_dummy_sampler;
				break;
			}
			case gpu::binding_kind::acceleration_structure:
				break;
		}
	}
	const WGPUBindGroupDescriptor desc{
		.nextInChain = nullptr,
		.label = label_of(program.label),
		.layout = variant.group_layout,
		.entryCount = entries.size(),
		.entries = entries.data(),
	};
	return wgpuDeviceCreateBindGroup(m_device, &desc);
}

auto gse::webgpu::device::push_slice(const program_record& program, const command_list& list) -> std::uint32_t {
	const auto bytes = (static_cast<std::uint64_t>(program.push_size) + 3) & ~std::uint64_t{ 3 };
	const auto stride = (bytes + m_storage_offset_alignment - 1) / m_storage_offset_alignment * m_storage_offset_alignment;
	const auto cursor = m_push_cursor.fetch_add(stride, std::memory_order_acq_rel);
	assert(cursor + stride * 2 - m_push_submitted_cursor.load(std::memory_order_acquire) <= push_ring_size, "webgpu: push ring overrun within one submission");
	auto offset = cursor % push_ring_size;
	if (offset + stride > push_ring_size) {
		offset = 0;
	}
	std::array<std::byte, max_push_bytes> staging{};
	std::memcpy(staging.data(), list.push.data(), std::min<std::size_t>(list.push.size(), program.push_size));
	wgpuQueueWriteBuffer(m_queue, m_push_ring, offset, staging.data(), bytes);
	return static_cast<std::uint32_t>(offset);
}

auto gse::webgpu::device::bind_for_draw(command_list& list) -> void {
	auto* program = list.program;
	assert(program != nullptr && !program->compute, "webgpu: draw with no graphics program bound");
	program_variant* variant = nullptr;
	WGPUBindGroup group = nullptr;
	{
		const std::lock_guard _(m_mutex);
		variant = &variant_for(*program, list.formats, unfilterable_mask(*program, list));
		group = make_group(*program, *variant, list);
	}
	wgpuRenderPassEncoderSetPipeline(list.pass, variant->render);
	wgpuRenderPassEncoderSetBindGroup(list.pass, 0, group, 0, nullptr);
	wgpuBindGroupRelease(group);
	if (program->push_group) {
		const auto offset = push_slice(*program, list);
		wgpuRenderPassEncoderSetBindGroup(list.pass, push_group_index, program->push_group, 1, &offset);
	}

	const auto extent = vec2f{ static_cast<float>(list.attachment_extent.x()), static_cast<float>(list.attachment_extent.y()) };
	const auto viewport = list.viewport.value_or(gpu::viewport{ .width = extent.x(), .height = extent.y() });
	const float x = std::clamp(viewport.x, 0.f, extent.x());
	const float y = std::clamp(viewport.y, 0.f, extent.y());
	wgpuRenderPassEncoderSetViewport(list.pass, x, y, std::clamp(viewport.width, 0.f, extent.x() - x), std::clamp(viewport.height, 0.f, extent.y() - y), viewport.min_depth, viewport.max_depth);

	const auto width = static_cast<std::int32_t>(list.attachment_extent.x());
	const auto height = static_cast<std::int32_t>(list.attachment_extent.y());
	auto scissor_min = vec2i{ 0, 0 };
	auto scissor_max = vec2i{ width, height };
	if (list.scissor) {
		scissor_min = list.scissor->min();
		scissor_max = scissor_min + list.scissor->size();
	}
	const auto left = std::clamp(scissor_min.x(), 0, width);
	const auto top = std::clamp(scissor_min.y(), 0, height);
	const auto right = std::clamp(scissor_max.x(), left, width);
	const auto bottom = std::clamp(scissor_max.y(), top, height);
	wgpuRenderPassEncoderSetScissorRect(list.pass, static_cast<std::uint32_t>(left), static_cast<std::uint32_t>(top), static_cast<std::uint32_t>(right - left), static_cast<std::uint32_t>(bottom - top));

	if (list.index_buffer) {
		wgpuRenderPassEncoderSetIndexBuffer(list.pass, list.index_buffer, list.index_format, list.index_offset, whole_buffer_size);
	}
}

auto gse::webgpu::device::bind_for_dispatch(command_list& list, const WGPUComputePassEncoder pass) -> void {
	auto* program = list.program;
	assert(program != nullptr && program->compute, "webgpu: dispatch with no compute program bound");
	program_variant* variant = nullptr;
	WGPUBindGroup group = nullptr;
	{
		const std::lock_guard _(m_mutex);
		variant = &variant_for(*program, {}, unfilterable_mask(*program, list));
		group = make_group(*program, *variant, list);
	}
	wgpuComputePassEncoderSetPipeline(pass, variant->compute);
	wgpuComputePassEncoderSetBindGroup(pass, 0, group, 0, nullptr);
	wgpuBindGroupRelease(group);
	if (program->push_group) {
		const auto offset = push_slice(*program, list);
		wgpuComputePassEncoderSetBindGroup(pass, push_group_index, program->push_group, 1, &offset);
	}
}

auto gse::webgpu::pushed_slot(const program_record& program, const gpu::binding_layout_entry& binding, const command_list& list) -> std::uint32_t {
	const auto offset = static_cast<std::size_t>(program.binding_offset) + binding.args_offset;
	std::uint32_t index = gpu::bindless_slot::invalid_index;
	if (offset + sizeof(index) <= list.push.size()) {
		std::memcpy(&index, list.push.data() + offset, sizeof(index));
	}
	return index;
}
