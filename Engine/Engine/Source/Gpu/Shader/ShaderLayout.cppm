export module gse.gpu:shader_layout;

import std;

import :handles;
import :types;
import :descriptor_heap;
import :device;

import gse.assert;
import gse.assets;
import gse.config;
import gse.core;

export namespace gse {
	struct shader_layout_binding {
		std::string name;
		gpu::descriptor_binding_desc desc;
	};

	struct shader_layout_set {
		std::uint32_t set_index = 0;
		std::vector<shader_layout_binding> bindings;
	};

	class shader_layout final : public identifiable {
	public:
		explicit shader_layout(
			const std::filesystem::path& path
		);

		auto load(
			gpu::device& device
		) -> void;

		auto unload(
		) -> void;

		[[nodiscard]] auto name(
		) const -> const std::string&;

		[[nodiscard]] auto sets(
		) const -> std::span<const shader_layout_set>;

		[[nodiscard]] auto layout_handle(
			std::uint32_t set_index
		) const -> gpu::handle<descriptor_set_layout>;

		[[nodiscard]] auto layout_handles(
		) const -> std::vector<gpu::handle<descriptor_set_layout>>;

		[[nodiscard]] auto layout_size(
			std::uint32_t set_index
		) const -> gpu::device_size;

		[[nodiscard]] auto binding_offset(
			std::uint32_t set_index,
			std::uint32_t binding
		) const -> gpu::device_size;

	private:
		std::string m_name;
		std::filesystem::path m_path;
		std::vector<shader_layout_set> m_sets;
		std::vector<gpu::descriptor_set_layout> m_layouts;
		gpu::device* m_device = nullptr;
	};
}

gse::shader_layout::shader_layout(const std::filesystem::path& path)
	: identifiable(path, config::baked_resource_path),
	  m_name(path.stem().string()),
	  m_path(path) {}

auto gse::shader_layout::load(gpu::device& device) -> void {
	m_device = &device;

	std::ifstream in(m_path, std::ios::binary);
	assert(in.is_open(), std::source_location::current(), "Failed to open layout file: {}", m_path.string());
	if (!in.is_open()) {
		return;
	}

	binary_reader ar(in, 0x474C4159, 2, m_path.string());
	if (!ar.valid()) {
		return;
	}

	ar & m_name;
	ar & m_sets;

	std::uint32_t max_set = 0;
	for (const auto& [set_index, bindings] : m_sets) {
		max_set = std::max(max_set, set_index);
	}

	m_layouts.resize(max_set + 1);

	for (const auto& [set_index, bindings] : m_sets) {
		std::vector<gpu::descriptor_binding_desc> descs;
		descs.reserve(bindings.size());
		for (const auto& b : bindings) {
			descs.push_back(b.desc);
		}
		m_layouts[set_index] = gpu::descriptor_set_layout::create(device.vulkan_device(), descs);
	}

	for (std::uint32_t i = 0; i <= max_set; ++i) {
		if (!m_layouts[i]) {
			m_layouts[i] = gpu::descriptor_set_layout::create(device.vulkan_device(), {});
		}
	}
}

auto gse::shader_layout::unload() -> void {
	m_layouts.clear();
	m_sets.clear();
	m_device = nullptr;
}

auto gse::shader_layout::name() const -> const std::string& {
	return m_name;
}

auto gse::shader_layout::sets() const -> std::span<const shader_layout_set> {
	return m_sets;
}

auto gse::shader_layout::layout_handle(const std::uint32_t set_index) const -> gpu::handle<descriptor_set_layout> {
	assert(
		set_index < m_layouts.size() && m_layouts[set_index],
		std::source_location::current(), "Layout set {} not found", set_index
	);
	return m_layouts[set_index].handle();
}

auto gse::shader_layout::layout_handles() const -> std::vector<gpu::handle<descriptor_set_layout>> {
	std::vector<gpu::handle<descriptor_set_layout>> result;
	result.reserve(m_layouts.size());
	for (const auto& layout : m_layouts) {
		result.push_back(layout ? layout.handle() : gpu::handle<descriptor_set_layout>{ 0 });
	}
	return result;
}

auto gse::shader_layout::layout_size(const std::uint32_t set_index) const -> gpu::device_size {
	assert(
		m_device != nullptr && set_index < m_layouts.size() && m_layouts[set_index],
		std::source_location::current(), "Layout set {} not found", set_index
	);
	return m_device->descriptor_heap().layout_size(m_layouts[set_index].handle());
}

auto gse::shader_layout::binding_offset(const std::uint32_t set_index, const std::uint32_t binding) const -> gpu::device_size {
	assert(
		m_device != nullptr && set_index < m_layouts.size() && m_layouts[set_index],
		std::source_location::current(), "Layout set {} not found", set_index
	);
	return m_device->descriptor_heap().binding_offset(m_layouts[set_index].handle(), binding);
}
