export module gse.gpu_record:recording_context;

import std;

import gse.gpu;
import :pipeline_builder;

import gse.gpu_backend;
import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.log;
import gse.math;
import gse.meta;

export namespace gse::gpu {
	class recording_context {
	public:
		auto set_viewport(
			float x,
			float y,
			float width,
			float height,
			float min_depth = 0.0f,
			float max_depth = 1.0f
		) const -> void;

		auto set_scissor(
			std::int32_t x,
			std::int32_t y,
			std::uint32_t width,
			std::uint32_t height
		) const -> void;

		auto set_color_blend_enable(
			std::uint32_t first_attachment,
			std::span<const std::uint8_t> enables
		) const -> void;

		auto set_color_blend_equation(
			std::uint32_t first_attachment,
			std::span<const gpu::color_blend_equation> equations
		) const -> void;

		auto draw(
			std::uint32_t vertex_count,
			std::uint32_t instance_count = 1,
			std::uint32_t first_vertex = 0,
			std::uint32_t first_instance = 0
		) -> void;

		auto draw_indexed(
			std::uint32_t index_count,
			std::uint32_t instance_count = 1,
			std::uint32_t first_index = 0,
			std::int32_t vertex_offset = 0,
			std::uint32_t first_instance = 0
		) -> void;

		auto draw_mesh_tasks(
			std::uint32_t x,
			std::uint32_t y = 1,
			std::uint32_t z = 1
		) -> void;

		auto dispatch(
			std::uint32_t x,
			std::uint32_t y = 1,
			std::uint32_t z = 1
		) -> void;

		template <typename Entry>
		auto dispatch(
			const entry_push_constants_t<Entry>& pc,
			const binding_args<entry_bindings_pack_t<Entry>>& args,
			vec3u groups
		) -> void;

		template <typename Entry>
		auto dispatch(
			const binding_args<entry_bindings_pack_t<Entry>>& args,
			vec3u groups
		) -> void;

		template <typename Entry>
		auto push_bindings(
			const entry_push_constants_t<Entry>& pc,
			const binding_args<entry_bindings_pack_t<Entry>>& args
		) -> void;

		template <typename Entry>
		auto push_bindings(
			const binding_args<entry_bindings_pack_t<Entry>>& args
		) -> void;

		auto dispatch_indirect(
			const buffer& buf,
			std::size_t offset = 0
		) -> void;

		template <typename T>
		auto push_data(
			const T& value,
			std::uint32_t offset = 0
		) const -> void;

		auto draw_indirect(
			const buffer& buf,
			std::size_t offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) -> void;

		auto draw_mesh_tasks_indirect(
			const buffer& buf,
			std::size_t offset,
			std::uint32_t draw_count,
			std::uint32_t stride
		) -> void;

		auto bind(
			const gpu::shader_program& p
		) -> void;

		auto bind_index(
			const buffer& buf,
			gpu::index_type type = gpu::index_type::uint32,
			std::size_t offset = 0
		) -> void;

		auto set_viewport(
			vec2u extent
		) const -> void;

		auto set_scissor(
			vec2u extent
		) const -> void;

		auto copy_buffer(
			const buffer& src,
			const buffer& dst,
			std::size_t size,
			std::size_t src_offset = 0,
			std::size_t dst_offset = 0
		) -> void;

		auto fill_buffer(
			const buffer& dst,
			std::size_t offset,
			std::size_t size,
			std::uint32_t data = 0
		) -> void;

		auto sample_image(
			const image& img,
			gpu::pipeline_stage_flags stages
		) -> void;

		auto build_acceleration_structure(
			const gpu::acceleration_structure_build_geometry_info& build_info,
			std::span<const gpu::acceleration_structure_build_range_info* const> range_infos
		) -> void;

		auto pipeline_barrier(
			const gpu::dependency_info& dep
		) -> void;

		auto capture_swapchain(
			const gpu::swap_chain& swapchain,
			const gpu::frame& frame,
			const buffer& dst
		) -> void;

		auto blit_swapchain_to_image(
			const gpu::swap_chain& swapchain,
			const gpu::frame& frame,
			const image& dst,
			vec2u dst_extent
		) -> void;

		[[nodiscard]] auto resolve(
			transient_image_handle h
		) const -> const image&;

		[[nodiscard]] auto resolve(
			transient_buffer_handle h
		) const -> const buffer&;

		recording_context(
			recording_context&& other
		) noexcept;

		auto operator=(
			recording_context&& other
		) noexcept -> recording_context&;

		recording_context(
			const recording_context&
		) = delete;

		auto operator=(
			const recording_context&
		) -> recording_context& = delete;

		~recording_context();

		static auto finalize_active_on_current_thread() noexcept -> void;

	private:
		friend class request_pass_awaitable;

		struct touched_resource {
			resource_ref ref;
			gpu::pipeline_stage_flags stages = {};
			gpu::access_flags access = {};
		};

		struct image_state_track {
			gpu::image_aspect_flags aspects = {};
			gpu::resource_state first = gpu::resource_state::undefined;
			gpu::resource_state current = gpu::resource_state::undefined;
		};

		struct access_track {
			gpu::pipeline_stage_flags stages = {};
			gpu::access_flags access = {};
		};

		gpu::pass_recorder m_recorder;
		render_pass_data* m_pass = nullptr;
		const gpu::transient_pool* m_transient_pool = nullptr;
		gpu::device* m_device = nullptr;
		std::vector<touched_resource> m_touched;
		std::unordered_map<const void*, access_track> m_last_access;
		std::unordered_map<const void*, image_state_track> m_image_states;
		std::vector<gpu::memory_barrier> m_pending_memory_barriers;
		std::vector<gpu::buffer_barrier> m_pending_buffer_barriers;
		std::vector<gpu::image_barrier> m_pending_image_barriers;
		std::thread::id m_origin_thread;
		gpu::pipeline_state_cache m_state_cache;
		bool m_bindless_heaps_valid = false;
		bool m_bound_is_compute = false;

		static constexpr std::size_t binding_cache_capacity = 512;
		std::array<std::byte, binding_cache_capacity> m_last_binding_bytes{};
		const void* m_last_binding_pack = nullptr;
		gpu::pipeline_stage_flags m_last_binding_stages{};
		gpu::pipeline_stage_flags m_companion_stages{};
		gpu::access_flags m_companion_access{};
		bool m_binding_repeat_valid = false;
		bool m_binding_companion_armed = false;

		recording_context(
			pass_recorder rec,
			render_pass_data* pass,
			const gpu::transient_pool* transient_pool,
			gpu::device* device
		);

		explicit recording_context(
			recording_context_init&& init
		);

		auto check_active() const -> void;

		auto ensure_descriptor_heaps() -> void;

		auto note_touched(
			resource_ref ref,
			gpu::pipeline_stage_flags stages,
			gpu::access_flags access
		) -> void;

		auto emit_intra_pass_barrier(
			const resource_ref& ref,
			gpu::pipeline_stage_flags stages,
			gpu::access_flags access
		) -> bool;

		auto note_bindings_repeat(
			gpu::pipeline_stage_flags stages,
			gpu::access_flags access
		) -> void;

		auto invalidate_binding_repeat() -> void;

		auto note_binding_mutation(
			gpu::pipeline_stage_flags stages,
			gpu::access_flags access,
			bool known_resource
		) -> void;

		auto flush_pending_barriers() -> void;

		[[nodiscard]] auto bound_shader_stages() const -> gpu::pipeline_stage_flags;

		auto transition_image_for_binding(
			const resource_ref& ref,
			gpu::resource_state target,
			gpu::pipeline_stage_flags stages,
			gpu::access_flags access
		) -> void;

		template <typename Entry>
		auto register_bindless_usage(
			const binding_args<entry_bindings_pack_t<Entry>>& args,
			gpu::pipeline_stage_flags stages
		) -> void;

		template <typename T, typename Args>
		auto register_one_bindless(
			const Args& args,
			gpu::pipeline_stage_flags stages
		) -> void;

		template <typename Args, typename T>
		static consteval auto bindless_member_for() -> std::meta::info;

		auto apply_dynamic_state(
			const gpu::dynamic_pipeline_state& s
		) -> void;

		auto finalize_pass() -> void;
	};
}

template <typename Args, typename T>
consteval auto gse::gpu::recording_context::bindless_member_for() -> std::meta::info {
	for (const auto m : std::meta::nonstatic_data_members_of(^^Args, std::meta::access_context::unchecked())) {
		if (std::meta::identifier_of(m) == std::meta::identifier_of(^^T)) {
			return m;
		}
	}
	return std::meta::info{};
}

namespace gse::gpu {
	template <typename Pack>
	constexpr char binding_pack_tag_v = 0;

	template <typename T>
	consteval auto binding_access_contribution() -> gpu::access_flags;

	template <typename... Ts>
	consteval auto binding_union_access(
		type_pack<Ts...>
	) -> gpu::access_flags;
}

template <typename T>
consteval auto gse::gpu::binding_access_contribution() -> gpu::access_flags {
	constexpr auto dtype = gpu::descriptor_type_v<T>;
	constexpr bool is_image = dtype == gpu::descriptor_type::sampled_image
		|| dtype == gpu::descriptor_type::storage_image
		|| dtype == gpu::descriptor_type::combined_image_sampler;
	constexpr bool is_buffer = dtype == gpu::descriptor_type::storage_buffer;
	if constexpr ((is_image || is_buffer) && gpu::descriptor_count_v<T> == 1) {
		if constexpr (gpu::descriptor_access_v<T> == gpu::descriptor_access::read_write) {
			return gpu::access_flags{ gpu::access_flag::shader_storage_read, gpu::access_flag::shader_storage_write };
		}
		else if constexpr (is_image) {
			return gpu::access_flags{ gpu::access_flag::shader_sampled_read };
		}
		else {
			return gpu::access_flags{ gpu::access_flag::shader_storage_read };
		}
	}
	else {
		return gpu::access_flags{};
	}
}

template <typename... Ts>
consteval auto gse::gpu::binding_union_access(type_pack<Ts...>) -> gpu::access_flags {
	gpu::access_flags acc{};
	((acc |= binding_access_contribution<Ts>()), ...);
	return acc;
}

template <typename T, typename Args>
auto gse::gpu::recording_context::register_one_bindless(const Args& args, const gpu::pipeline_stage_flags stages) -> void {
	constexpr auto dtype = gpu::descriptor_type_v<T>;
	constexpr bool is_image = dtype == gpu::descriptor_type::sampled_image
		|| dtype == gpu::descriptor_type::storage_image
		|| dtype == gpu::descriptor_type::combined_image_sampler;
	constexpr bool is_buffer = dtype == gpu::descriptor_type::storage_buffer;
	if constexpr ((is_image || is_buffer) && gpu::descriptor_count_v<T> == 1) {
		constexpr std::meta::info member = bindless_member_for<Args, T>();
		std::uint32_t index;
		if constexpr (dtype == gpu::descriptor_type::combined_image_sampler) {
			index = args.[:member:].image.index;
		}
		else {
			index = args.[:member:].index;
		}
		const resource_ref ref = m_device->resource_for_slot(index);
		if (ref.ptr) {
			const auto access = (gpu::descriptor_access_v<T> == gpu::descriptor_access::read_write)
				? gpu::access_flags{ gpu::access_flag::shader_storage_read, gpu::access_flag::shader_storage_write }
				: gpu::access_flags{ is_image ? gpu::access_flag::shader_sampled_read : gpu::access_flag::shader_storage_read };
			note_touched(ref, stages, access);
			if constexpr (is_image) {
				constexpr auto target = dtype == gpu::descriptor_type::storage_image
					? gpu::resource_state::storage_read_write
					: gpu::resource_state::sampled;
				transition_image_for_binding(ref, target, stages, access);
			}
		}
	}
}

template <typename Entry>
auto gse::gpu::recording_context::register_bindless_usage(const binding_args<entry_bindings_pack_t<Entry>>& args, const gpu::pipeline_stage_flags stages) -> void {
	constexpr auto union_access = binding_union_access(entry_bindings_pack_t<Entry>{});
	const void* pack_tag = &binding_pack_tag_v<entry_bindings_pack_t<Entry>>;

	if constexpr (sizeof(args) <= binding_cache_capacity) {
		if (m_binding_repeat_valid
			&& m_last_binding_pack == pack_tag
			&& m_last_binding_stages.bits() == stages.bits()
			&& std::memcmp(m_last_binding_bytes.data(), &args, sizeof(args)) == 0) {
			note_bindings_repeat(stages, union_access);
			return;
		}
	}

	[&]<typename... Ts>(type_pack<Ts...>) {
		(register_one_bindless<Ts>(args, stages), ...);
	}(entry_bindings_pack_t<Entry>{});

	if constexpr (sizeof(args) <= binding_cache_capacity) {
		std::memcpy(m_last_binding_bytes.data(), &args, sizeof(args));
		m_last_binding_pack = pack_tag;
		m_last_binding_stages = stages;
		m_companion_stages = {};
		m_companion_access = {};
		m_binding_companion_armed = false;
		m_binding_repeat_valid = true;
	}
}

template <typename Entry>
auto gse::gpu::recording_context::dispatch(const entry_push_constants_t<Entry>& pc, const binding_args<entry_bindings_pack_t<Entry>>& args, const vec3u groups) -> void {
	push_data(pc, 0);
	push_data(args, sizeof(entry_push_constants_t<Entry>));
	register_bindless_usage<Entry>(args, gpu::pipeline_stage_flag::compute_shader);
	dispatch(groups.x(), groups.y(), groups.z());
}

template <typename Entry>
auto gse::gpu::recording_context::dispatch(const binding_args<entry_bindings_pack_t<Entry>>& args, const vec3u groups) -> void {
	push_data(args, 0);
	register_bindless_usage<Entry>(args, gpu::pipeline_stage_flag::compute_shader);
	dispatch(groups.x(), groups.y(), groups.z());
}

template <typename Entry>
auto gse::gpu::recording_context::push_bindings(const entry_push_constants_t<Entry>& pc, const binding_args<entry_bindings_pack_t<Entry>>& args) -> void {
	push_data(pc, 0);
	push_data(args, sizeof(entry_push_constants_t<Entry>));
	register_bindless_usage<Entry>(args, bound_shader_stages());
}

template <typename Entry>
auto gse::gpu::recording_context::push_bindings(const binding_args<entry_bindings_pack_t<Entry>>& args) -> void {
	push_data(args, 0);
	register_bindless_usage<Entry>(args, bound_shader_stages());
}

template <typename T>
auto gse::gpu::recording_context::push_data(const T& value, const std::uint32_t offset) const -> void {
	check_active();
	const auto bytes = std::span(reinterpret_cast<const std::byte*>(std::addressof(value)), sizeof(T));
	m_recorder.push_data(offset, bytes);
}
