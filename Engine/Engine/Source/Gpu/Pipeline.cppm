export module gse.gpu:pipeline;

import std;

import :handles;
import :types;
import :vulkan_pipeline;
import :vulkan_commands;
import :vulkan_device;
import :descriptor_heap;
import :device;
import :shader;
import :shader_registry;
import :bindless;

import gse.assets;
import gse.assert;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;

export namespace gse::gpu {
	struct graphics_pipeline_desc {
		rasterization_state rasterization;
		depth_state depth;
		blend_preset blend = blend_preset::none;
		color_format color = color_format::swapchain;
		depth_format depth_fmt = depth_format::d32_sfloat;
		topology topology = topology::triangle_list;
		std::string push_constant_block;
	};

	struct push_constant_member {
		std::uint32_t offset = 0;
		std::uint32_t size = 0;
	};

	class cached_push_constants {
	public:
		std::unordered_map<std::string, push_constant_member> members;
		std::vector<std::byte> data;
		stage_flags stage_flags{};

		template <typename T>
		auto set(
			std::string_view name,
			const T& value
		) -> void;

		auto replay(
			handle<command_buffer> cmd,
			handle<pipeline_layout> layout
		) const -> void;
	};

	auto cache_push_block(
		const resource::handle<shader>& s,
		std::string_view block_name
	) -> cached_push_constants;

	[[nodiscard]] auto create_graphics_pipeline(
		gpu::device& dev,
		gpu::shader_registry& registry,
		gpu::bindless_texture_set& bindless,
		const resource::handle<shader>& s,
		const graphics_pipeline_desc& desc = {}
	) -> pipeline;

	[[nodiscard]] auto create_compute_pipeline(
		gpu::device& dev,
		gpu::shader_registry& registry,
		gpu::bindless_texture_set& bindless,
		const resource::handle<shader>& s,
		std::string_view push_constant_block = {}
	) -> pipeline;
}

template <typename T>
auto gse::gpu::cached_push_constants::set(const std::string_view name, const T& value) -> void {
	const auto it = members.find(std::string(name));
	assert(it != members.end(), std::source_location::current(), "Push constant member '{}' not found", name);
	assert(sizeof(T) <= it->second.size, std::source_location::current(), "Push constant member '{}' size mismatch", name);
	gse::memcpy(data.data() + it->second.offset, value);
}

auto gse::gpu::cached_push_constants::replay(const handle<command_buffer> cmd, const handle<pipeline_layout> layout) const -> void {
	vulkan::commands{ cmd }.push_constants(
		layout,
		stage_flags,
		0,
		static_cast<std::uint32_t>(data.size()),
		data.data()
	);
}

auto gse::gpu::cache_push_block(const resource::handle<shader>& s, const std::string_view block_name) -> cached_push_constants {
	const auto blocks = s->push_constants();
	const auto it = std::ranges::find_if(blocks, [&](const struct shader::uniform_block& b) {
		return b.name == block_name;
	});

	assert(it != blocks.end(), std::source_location::current(), "Push constant block '{}' not found", block_name);
	std::unordered_map<std::string, push_constant_member> members;
	for (const auto& [name, member] : it->members) {
		members[name] = {
			.offset = member.offset,
			.size = member.size
		};
	}
	return {
		.members = std::move(members),
		.data = std::vector(it->size, std::byte{ 0 }),
		.stage_flags = it->stage_flags,
	};
}

auto gse::gpu::create_graphics_pipeline(gpu::device& dev, gpu::shader_registry& registry, gpu::bindless_texture_set& bindless, const resource::handle<shader>& s, const graphics_pipeline_desc& desc) -> pipeline {
	const auto& cache = registry.cache(s);

	auto layouts = cache.layout_handles;
	constexpr auto bindless_idx = static_cast<std::uint32_t>(descriptor_set_type::bind_less);
	if (layouts.size() > bindless_idx) {
		layouts[bindless_idx] = bindless.layout_handle();
	}

	std::optional<push_constant_range> push_range;
	if (!desc.push_constant_block.empty()) {
		const auto pb = s->push_block(desc.push_constant_block);
		push_range = push_constant_range{
			.stages = pb.stage_flags,
			.offset = 0,
			.size = pb.size,
		};
	}

	image_format color_format_value = image_format::r8g8b8a8_unorm;
	if (desc.color == color_format::swapchain) {
		color_format_value = dev.surface_format();
	}

	const bool has_color = desc.color != color_format::none;
	const bool has_depth = desc.depth_fmt != depth_format::none;
	const bool is_mesh = s->is_mesh_shader();

	std::vector<shader_stage_create_info> stages;
	auto make_stage = [&](shader_stage stage, stage_flag flag) -> shader_stage_create_info {
		return {
			.stage = flag,
			.module_handle = cache.modules.at(stage).handle().value,
			.entry_point = "main",
		};
	};

	if (is_mesh) {
		stages.push_back(make_stage(shader_stage::task, stage_flag::task));
		stages.push_back(make_stage(shader_stage::mesh, stage_flag::mesh));
		stages.push_back(make_stage(shader_stage::fragment, stage_flag::fragment));
	} else {
		stages.push_back(make_stage(shader_stage::vertex, stage_flag::vertex));
		stages.push_back(make_stage(shader_stage::fragment, stage_flag::fragment));
	}

	std::span<const vertex_binding_desc> vertex_bindings;
	std::span<const vertex_attribute_desc> vertex_attributes;
	if (!is_mesh) {
		const auto& vi = s->vertex_input_data();
		vertex_bindings = vi.bindings;
		vertex_attributes = vi.attributes;
	}

	vulkan::graphics_pipeline_create_info info{
		.stages = stages,
		.vertex_bindings = vertex_bindings,
		.vertex_attributes = vertex_attributes,
		.set_layouts = layouts,
		.push_constant_range = push_range,
		.rasterization = desc.rasterization,
		.depth = desc.depth,
		.blend = desc.blend,
		.topology = desc.topology,
		.color_format = color_format_value,
		.depth_format = image_format::d32_sfloat,
		.has_color = has_color,
		.has_depth = has_depth,
		.is_mesh_pipeline = is_mesh,
	};

	return vulkan::pipeline::create_graphics(dev.vulkan_device(), info);
}

auto gse::gpu::create_compute_pipeline(gpu::device& dev, gpu::shader_registry& registry, gpu::bindless_texture_set& bindless, const resource::handle<shader>& s, const std::string_view push_constant_block) -> pipeline {
	const auto& cache = registry.cache(s);

	auto layouts = cache.layout_handles;
	constexpr auto bindless_idx = static_cast<std::uint32_t>(descriptor_set_type::bind_less);
	if (layouts.size() > bindless_idx) {
		layouts[bindless_idx] = bindless.layout_handle();
	}

	std::optional<push_constant_range> push_range;
	if (!push_constant_block.empty()) {
		const auto pb = s->push_block(push_constant_block);
		push_range = push_constant_range{
			.stages = pb.stage_flags,
			.offset = 0,
			.size = pb.size,
		};
	}

	const shader_stage_create_info compute_stage{
		.stage = stage_flag::compute,
		.module_handle = cache.modules.at(shader_stage::compute).handle().value,
		.entry_point = "main",
	};

	vulkan::compute_pipeline_create_info info{
		.stage = compute_stage,
		.set_layouts = layouts,
		.push_constant_range = push_range,
	};

	return vulkan::pipeline::create_compute(dev.vulkan_device(), info);
}
