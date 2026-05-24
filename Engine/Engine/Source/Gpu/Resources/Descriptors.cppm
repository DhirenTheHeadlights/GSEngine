export module gse.gpu:descriptors;

import std;

import :aliases;
import :types;
import :vulkan_buffer;
import :vulkan_device;
import :vulkan_acceleration_structure;
import :descriptor_heap;
import :shader_codegen;
import :shader_registry;

import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;

export namespace gse::gpu {
	auto allocate_descriptors(
		shader_registry& registry,
		descriptor_heap& heap,
		std::string_view layout_name,
		const std::source_location& loc = std::source_location::current()
	) -> descriptor_region;

	class descriptor_writer final : public non_copyable {
	public:
		descriptor_writer(
			handle<vulkan::device> dev,
			descriptor_region& region
		);

		descriptor_writer(
			shader_registry& registry,
			handle<vulkan::device> dev,
			descriptor_heap& heap,
			std::string_view layout_name
		);

		~descriptor_writer() override = default;

		descriptor_writer(
			descriptor_writer&&
		) noexcept = default;

		auto operator=(
			descriptor_writer&&
		) noexcept -> descriptor_writer& = default;

		template <shaders::is_shader_binding T>
		auto buffer(
			const vulkan::basic_buffer<vulkan::device>& buf
		) -> descriptor_writer&;

		template <shaders::is_shader_binding T>
		auto buffer(
			const vulkan::basic_buffer<vulkan::device>& buf,
			std::size_t offset,
			std::size_t range
		) -> descriptor_writer&;

		template <shaders::is_shader_binding T>
		auto storage_image(
			const vulkan::basic_image<vulkan::device>& img,
			image_layout layout = image_layout::general
		) -> descriptor_writer&;

		template <shaders::is_shader_binding T>
		auto combined_image_sampler(
			const vulkan::basic_image<vulkan::device>& img,
			const vulkan::sampler& sampler,
			image_layout layout = image_layout::general
		) -> descriptor_writer&;

		template <shaders::is_shader_binding T>
		auto acceleration_structure(
			acceleration_structure_handle as
		) -> descriptor_writer&;

		auto commit() -> void;

		auto begin(
			std::uint32_t frame_index
		) -> void;

		[[nodiscard]] auto native_writer(
			this auto&& self
		) -> auto&;

		[[nodiscard]] auto touched_resources() const -> std::span<const resource_slot>;

	private:
		enum class mode : std::uint8_t {
			persistent,
			push,
		};

		struct stored_buffer_info {
			handle<vulkan::buffer> buf;
			std::size_t offset = 0;
			std::size_t range = 0;
		};

		auto buffer_impl(
			std::uint32_t slot,
			const vulkan::basic_buffer<vulkan::device>& buf,
			std::size_t offset,
			std::size_t range
		) -> descriptor_writer&;

		auto storage_image_impl(
			std::uint32_t slot,
			const vulkan::basic_image<vulkan::device>& img,
			image_layout layout
		) -> descriptor_writer&;

		auto combined_image_sampler_impl(
			std::uint32_t slot,
			const vulkan::basic_image<vulkan::device>& img,
			const vulkan::sampler& sampler,
			image_layout layout
		) -> descriptor_writer&;

		auto acceleration_structure_impl(
			std::uint32_t slot,
			acceleration_structure_handle as
		) -> descriptor_writer&;

		const family_layout* m_family = nullptr;
		handle<vulkan::device> m_device;
		descriptor_region* m_region = nullptr;
		mode m_mode = mode::persistent;

		std::unordered_map<std::uint32_t, stored_buffer_info> m_buffer_infos;
		std::unordered_map<std::uint32_t, descriptor_image_info> m_storage_image_infos;
		std::unordered_map<std::uint32_t, descriptor_image_info> m_combined_sampler_infos;
		std::unordered_map<std::uint32_t, acceleration_structure_handle> m_as_infos;

		std::vector<resource_slot> m_touched;

		descriptor_set_writer m_push_writer;
	};
}

namespace gse::gpu {
	auto build_push_writer_from_family(
		descriptor_heap& heap,
		const family_layout& family
	) -> descriptor_set_writer;
}

auto gse::gpu::allocate_descriptors(shader_registry& registry, descriptor_heap& heap, const std::string_view layout_name, const std::source_location& loc) -> descriptor_region {
	const auto* family = registry.find_family(layout_name);
	assert(family, "Shader family layout not registered: {}", layout_name);
	constexpr auto persistent_idx = static_cast<std::uint32_t>(descriptor_set_type::persistent);
	assert(
		persistent_idx < family->layout_handles.size(),
		"Family has no persistent descriptor set to allocate"
	);

	const auto set_layout = family->layout_handles[persistent_idx];
	const auto size = heap.layout_size(set_layout);
	auto region = heap.allocate(size, loc);
	region.family = family;
	return region;
}

auto gse::gpu::build_push_writer_from_family(descriptor_heap& heap, const family_layout& family) -> descriptor_set_writer {
	constexpr auto push_idx = static_cast<std::uint32_t>(descriptor_set_type::push);

	auto set_it = std::ranges::find_if(family.sets, [](const auto& s) {
		return s.type == descriptor_set_type::push;
	});
	if (set_it == family.sets.end() || family.layout_handles.size() <= push_idx) {
		return {};
	}

	const auto set_layout = family.layout_handles[push_idx];
	const auto total_size = heap.layout_size(set_layout);

	std::uint32_t max_binding = 0;
	for (const auto& b : set_it->bindings) {
		max_binding = std::max(max_binding, b.desc.binding);
	}

	std::vector<descriptor_binding_info> bindings(max_binding + 1);
	for (const auto& b : set_it->bindings) {
		bindings[b.desc.binding] = {
			.offset = heap.binding_offset(
				set_layout,
				b.desc.binding
			),
			.descriptor_size = heap.props().descriptor_size_for(b.desc.type),
			.type = b.desc.type,
		};
	}

	return descriptor_set_writer(heap, set_layout, total_size, std::move(bindings));
}

gse::gpu::descriptor_writer::descriptor_writer(const handle<vulkan::device> dev, descriptor_region& region)
	: m_family(region.family), m_device(dev), m_region(&region) {
	assert(m_family, "descriptor_region was not allocated against a registered family");
}

gse::gpu::descriptor_writer::descriptor_writer(shader_registry& registry, const handle<vulkan::device> dev, descriptor_heap& heap, const std::string_view layout_name)
	: m_family(registry.find_family(layout_name)), m_device(dev), m_mode(mode::push) {
	assert(m_family, "Shader family layout not registered: {}", layout_name);
	m_push_writer = build_push_writer_from_family(heap, *m_family);
}

template <gse::shaders::is_shader_binding T>
auto gse::gpu::descriptor_writer::buffer(const vulkan::basic_buffer<vulkan::device>& buf) -> descriptor_writer& {
	return buffer<T>(buf, 0, buf.size_bytes());
}

template <gse::shaders::is_shader_binding T>
auto gse::gpu::descriptor_writer::buffer(const vulkan::basic_buffer<vulkan::device>& buf, const std::size_t offset, const std::size_t range) -> descriptor_writer& {
	using binding_t = [:shaders::find_binding_type(^^T):];
	return buffer_impl(binding_t::slot, buf, offset, range);
}

template <gse::shaders::is_shader_binding T>
auto gse::gpu::descriptor_writer::storage_image(const vulkan::basic_image<vulkan::device>& img, const image_layout layout) -> descriptor_writer& {
	using binding_t = [:shaders::find_binding_type(^^T):];
	return storage_image_impl(binding_t::slot, img, layout);
}

template <gse::shaders::is_shader_binding T>
auto gse::gpu::descriptor_writer::combined_image_sampler(const vulkan::basic_image<vulkan::device>& img, const vulkan::sampler& sampler, const image_layout layout) -> descriptor_writer& {
	using binding_t = [:shaders::find_binding_type(^^T):];
	return combined_image_sampler_impl(binding_t::slot, img, sampler, layout);
}

template <gse::shaders::is_shader_binding T>
auto gse::gpu::descriptor_writer::acceleration_structure(const acceleration_structure_handle as) -> descriptor_writer& {
	using binding_t = [:shaders::find_binding_type(^^T):];
	return acceleration_structure_impl(binding_t::slot, as);
}

auto gse::gpu::descriptor_writer::buffer_impl(const std::uint32_t slot, const vulkan::basic_buffer<vulkan::device>& buf, const std::size_t offset, const std::size_t range) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_buffer_infos[slot] = stored_buffer_info{
			.buf = buf.handle(),
			.offset = offset,
			.range = range,
		};
	}
	else {
		m_push_writer.buffer(slot, buf.handle(), offset, range);
	}
	m_touched.push_back({
		.slot = slot,
		.ref = {
			.ptr = std::addressof(buf),
			.type = resource_type::buffer,
		},
	});
	return *this;
}

auto gse::gpu::descriptor_writer::storage_image_impl(const std::uint32_t slot, const vulkan::basic_image<vulkan::device>& img, const image_layout layout) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_storage_image_infos[slot] = descriptor_image_info{
			.sampler = {},
			.image_view = img.view(),
			.layout = layout,
		};
	}
	else {
		m_push_writer.storage_image(slot, img.view(), layout);
	}
	m_touched.push_back({
		.slot = slot,
		.ref = {
			.ptr = std::addressof(img),
			.type = resource_type::image,
		},
	});
	return *this;
}

auto gse::gpu::descriptor_writer::combined_image_sampler_impl(const std::uint32_t slot, const vulkan::basic_image<vulkan::device>& img, const vulkan::sampler& sampler, const image_layout layout) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_combined_sampler_infos[slot] = descriptor_image_info{
			.sampler = sampler.native(),
			.image_view = img.view(),
			.layout = layout,
		};
	}
	else {
		m_push_writer.combined_image_sampler(slot, img.view(), sampler.native(), layout);
	}
	m_touched.push_back({
		.slot = slot,
		.ref = {
			.ptr = std::addressof(img),
			.type = resource_type::image,
		},
	});
	return *this;
}

auto gse::gpu::descriptor_writer::acceleration_structure_impl(const std::uint32_t slot, const acceleration_structure_handle as) -> descriptor_writer& {
	m_as_infos[slot] = as;
	m_touched.push_back({
		.slot = slot,
		.ref = {
			.ptr = std::bit_cast<const void*>(as.value),
			.type = resource_type::acceleration_structure,
		},
	});
	return *this;
}

auto gse::gpu::descriptor_writer::commit() -> void {
	assert(m_region && m_region->valid(), "Cannot commit to null descriptor region");

	const auto& heap = *m_region->heap;

	constexpr auto persistent_idx = static_cast<std::uint32_t>(descriptor_set_type::persistent);
	const auto set_layout = m_family->layout_handles[persistent_idx];
	const auto& region = *m_region;

	std::unordered_map<std::uint32_t, device_size> binding_sizes;
	for (const auto& fs : m_family->sets) {
		if (fs.type != descriptor_set_type::persistent) {
			continue;
		}
		binding_sizes.reserve(fs.bindings.size());
		for (const auto& b : fs.bindings) {
			binding_sizes[b.desc.binding] = heap.props().descriptor_size_for(b.desc.type);
		}
		break;
	}

	auto write_binding = [&](const std::uint32_t binding, const bool is_uniform) {
		const auto boff = heap.binding_offset(set_layout, binding);
		const auto size = binding_sizes[binding];

		if (auto it = m_buffer_infos.find(binding); it != m_buffer_infos.end()) {
			const auto& [buf, offset, range] = it->second;
			const auto buf_addr = heap.buffer_address(buf);

			const descriptor_get_info get_info{
				.type = is_uniform ? descriptor_type::uniform_buffer : descriptor_type::storage_buffer,
				.buffer = {
					.address = buf_addr + offset,
					.range = range,
				},
			};
			heap.write_descriptor(region, boff, get_info, size);
			return;
		}

		if (auto it_si = m_storage_image_infos.find(binding); it_si != m_storage_image_infos.end()) {
			const descriptor_get_info get_info{
				.type = descriptor_type::storage_image,
				.image = it_si->second,
			};
			heap.write_descriptor(region, boff, get_info, size);
			return;
		}

		if (auto it_cis = m_combined_sampler_infos.find(binding); it_cis != m_combined_sampler_infos.end()) {
			const descriptor_get_info get_info{
				.type = descriptor_type::combined_image_sampler,
				.image = it_cis->second,
			};
			heap.write_descriptor(region, boff, get_info, size);
			return;
		}

		if (auto it_as = m_as_infos.find(binding); it_as != m_as_infos.end()) {
			const auto as_addr = vulkan::acceleration_structure_address_from_handle(m_device, it_as->second);
			if (as_addr == 0) {
				log::println(
					log::level::warning,
					log::category::vulkan,
					"Descriptor AS write produced address 0 for binding={} handle={:#x}",
					binding,
					it_as->second.value
				);
			}
			const descriptor_get_info get_info{
				.type = descriptor_type::acceleration_structure,
				.acceleration_structure = as_addr,
			};
			heap.write_descriptor(region, boff, get_info, size);
		}
	};

	for (const auto& fs : m_family->sets) {
		if (fs.type == descriptor_set_type::persistent) {
			for (const auto& b : fs.bindings) {
				write_binding(b.desc.binding, b.desc.type == descriptor_type::uniform_buffer);
			}
			break;
		}
	}

	m_buffer_infos.clear();
	m_storage_image_infos.clear();
	m_combined_sampler_infos.clear();
	m_as_infos.clear();

	for (const auto& touched : m_touched) {
		const auto it = std::ranges::find_if(
			m_region->resources,
			[&](const resource_slot& existing) {
				return existing.slot == touched.slot;
			}
		);
		if (it == m_region->resources.end()) {
			m_region->resources.push_back(touched);
		}
		else {
			*it = touched;
		}
	}
	m_touched.clear();
}

auto gse::gpu::descriptor_writer::begin(const std::uint32_t frame_index) -> void {
	m_push_writer.begin(frame_index);
	m_touched.clear();
}

auto gse::gpu::descriptor_writer::native_writer(this auto&& self) -> auto& {
	return self.m_push_writer;
}

auto gse::gpu::descriptor_writer::touched_resources() const -> std::span<const resource_slot> {
	return m_touched;
}
