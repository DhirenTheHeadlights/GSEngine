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
			std::span<const color_blend_equation> equations
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
			const entry_binding_args<Entry>& args,
			vec3u groups
		) -> void;

		template <typename Entry>
		auto dispatch(
			const entry_binding_args<Entry>& args,
			vec3u groups
		) -> void;

		template <typename Entry>
		auto push_bindings(
			const entry_push_constants_t<Entry>& pc,
			const entry_binding_args<Entry>& args
		) -> void;

		template <typename Entry>
		auto push_bindings(
			const entry_binding_args<Entry>& args
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
			const shader_program& p
		) -> void;

		auto bind_index(
			const buffer& buf,
			index_type type = index_type::uint32,
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
			pipeline_stage_flags stages
		) -> void;

		auto build_acceleration_structure(
			const acceleration_structure_build_geometry_info& build_info,
			std::span<const acceleration_structure_build_range_info* const> range_infos
		) -> void;

		auto pipeline_barrier(
			const dependency_info& dep
		) -> void;

		auto mark(
			id label
		) const -> void;

		auto copy_target_to_buffer(
			const image_ref& src,
			const buffer& dst
		) -> void;

		auto blit_target_to_image(
			const image_ref& src,
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
			pipeline_stage_flags stages = {};
			access_flags access = {};
		};

		struct access_track {
			pipeline_stage_flags stages = {};
			access_flags access = {};
			std::uint64_t generation = 0;
			bool covered = false;
		};

		pass_recorder m_recorder;
		render_pass_data* m_pass = nullptr;
		const transient_pool* m_transient_pool = nullptr;
		device* m_device = nullptr;
		std::vector<touched_resource> m_touched;
		std::unordered_map<std::uint64_t, access_track> m_last_access;
		std::vector<memory_barrier> m_pending_memory_barriers;
		std::uint64_t m_access_generation = 0;
		bool m_repeat_barrier_pending = false;
		std::thread::id m_origin_thread;
		pipeline_state_cache m_state_cache;
		bool m_bindless_heaps_valid = false;
		bool m_bound_is_compute = false;

		static constexpr std::size_t binding_cache_capacity = 512;
		std::array<std::byte, binding_cache_capacity> m_last_binding_bytes{};
		const void* m_last_binding_pack = nullptr;
		pipeline_stage_flags m_last_binding_stages{};
		pipeline_stage_flags m_companion_stages{};
		access_flags m_companion_access{};
		bool m_binding_repeat_valid = false;
		bool m_binding_companion_armed = false;
		pass_mark_cursor m_marks;

		recording_context(
			pass_recorder rec,
			render_pass_data* pass,
			const transient_pool* transient_pool,
			device* device
		);

		explicit recording_context(
			recording_context_init&& init
		);

		auto check_active() const -> void;

		auto ensure_descriptor_heaps() -> void;

		auto note_touched(
			resource_ref ref,
			pipeline_stage_flags stages,
			access_flags access
		) -> void;

		auto emit_intra_pass_barrier(
			const resource_ref& ref,
			pipeline_stage_flags stages,
			access_flags access
		) -> bool;

		auto note_bindings_repeat(
			pipeline_stage_flags stages,
			access_flags access
		) -> void;

		auto invalidate_binding_repeat() -> void;

		auto note_binding_mutation(
			pipeline_stage_flags stages,
			access_flags access,
			bool known_resource
		) -> void;

		auto flush_pending_barriers() -> void;

		auto cover_barrier_sources(
			std::uint64_t dispatch_generation
		) -> void;

		auto merge_pending_barrier(
			pipeline_stage_flags src_stages,
			access_flags src_access,
			pipeline_stage_flags dst_stages,
			access_flags dst_access
		) -> void;

		[[nodiscard]] auto bound_shader_stages() const -> pipeline_stage_flags;

		template <typename Entry>
		auto register_bindless_usage(
			const entry_binding_args<Entry>& args,
			pipeline_stage_flags stages
		) -> void;

		auto apply_dynamic_state(
			const dynamic_pipeline_state& s
		) -> void;

		auto finalize_pass() -> void;
	};
}

namespace gse::gpu {
	template <typename Args>
	constexpr char binding_args_tag_v = 0;

	template <shaders::is_shader_binding T>
	consteval auto binding_access_of() -> access_flags;

	template <typename Args>
	consteval auto binding_union_access() -> access_flags;
}

template <gse::shaders::is_shader_binding T>
consteval auto gse::gpu::binding_access_of() -> access_flags {
	constexpr auto info = shaders::binding_info_v<T>;
	return access_flags{ info.read, info.write };
}

template <typename Args>
consteval auto gse::gpu::binding_union_access() -> access_flags {
	access_flags acc{};
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^Args, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		acc |= binding_access_of<member_t>();
	}
	return acc;
}

template <typename Entry>
auto gse::gpu::recording_context::register_bindless_usage(const entry_binding_args<Entry>& args, const pipeline_stage_flags stages) -> void {
	using args_t = entry_binding_args<Entry>;
	constexpr auto union_access = binding_union_access<args_t>();
	const void* pack_tag = &binding_args_tag_v<args_t>;

	if constexpr (sizeof(args) <= binding_cache_capacity) {
		if (m_binding_repeat_valid
			&& m_last_binding_pack == pack_tag
			&& m_last_binding_stages.bits() == stages.bits()
			&& std::memcmp(m_last_binding_bytes.data(), &args, sizeof(args)) == 0) {
			note_bindings_repeat(stages, union_access);
			return;
		}
	}

	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^args_t, std::meta::access_context::unchecked()))) {
		using member_t = [:std::meta::type_of(m):];
		constexpr auto access = binding_access_of<member_t>();
		if constexpr (access) {
			note_touched(m_device->resource_for_slot(args.[:m:].index), stages, access);
		}
	}

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
auto gse::gpu::recording_context::dispatch(const entry_push_constants_t<Entry>& pc, const entry_binding_args<Entry>& args, const vec3u groups) -> void {
	push_data(pc, 0);
	push_data(args, sizeof(entry_push_constants_t<Entry>));
	register_bindless_usage<Entry>(args, pipeline_stage_flag::compute_shader);
	dispatch(groups.x(), groups.y(), groups.z());
}

template <typename Entry>
auto gse::gpu::recording_context::dispatch(const entry_binding_args<Entry>& args, const vec3u groups) -> void {
	push_data(args, 0);
	register_bindless_usage<Entry>(args, pipeline_stage_flag::compute_shader);
	dispatch(groups.x(), groups.y(), groups.z());
}

template <typename Entry>
auto gse::gpu::recording_context::push_bindings(const entry_push_constants_t<Entry>& pc, const entry_binding_args<Entry>& args) -> void {
	push_data(pc, 0);
	push_data(args, sizeof(entry_push_constants_t<Entry>));
	register_bindless_usage<Entry>(args, bound_shader_stages());
}

template <typename Entry>
auto gse::gpu::recording_context::push_bindings(const entry_binding_args<Entry>& args) -> void {
	push_data(args, 0);
	register_bindless_usage<Entry>(args, bound_shader_stages());
}

template <typename T>
auto gse::gpu::recording_context::push_data(const T& value, const std::uint32_t offset) const -> void {
	check_active();
	const auto bytes = std::span(reinterpret_cast<const std::byte*>(std::addressof(value)), sizeof(T));
	m_recorder.push_data(offset, bytes);
}