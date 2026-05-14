export module gse.gpu:shader_registry;

import std;

import :aliases;
import :handles;
import :types;
import :device;
import :shader_codegen;
import :vulkan_descriptor_set_layout;

import gse.log;
import gse.core;
import gse.containers;

export namespace gse::gpu {
	struct family_layout {
		std::vector<shaders::family_set> sets;
		std::vector<descriptor_set_layout> owned_layouts;
		std::vector<handle<descriptor_set_layout>> layout_handles;
	};

	class shader_registry final : public non_copyable {
	public:
		explicit shader_registry(
			device& dev
		);

		~shader_registry() override;

		auto register_family(
			std::string name,
			std::vector<shaders::family_set> sets
		) -> void;

		[[nodiscard]] auto find_family(
			std::string_view name
		) -> const family_layout*;

	private:
		device* m_device;
		std::mutex m_mutex;
		std::unordered_map<std::string, std::unique_ptr<family_layout>> m_families;
	};
}

gse::gpu::shader_registry::shader_registry(device& dev) : m_device(&dev) {}

gse::gpu::shader_registry::~shader_registry() {}

auto gse::gpu::shader_registry::register_family(std::string name, std::vector<shaders::family_set> sets) -> void {
	std::lock_guard lock(m_mutex);

	if (m_families.contains(name)) {
		return;
	}

	auto family = std::make_unique<family_layout>();
	family->sets = std::move(sets);

	std::uint32_t max_set_index = static_cast<std::uint32_t>(descriptor_set_type::bind_less);
	for (const auto& s : family->sets) {
		max_set_index = std::max(max_set_index, static_cast<std::uint32_t>(s.type));
	}

	family->owned_layouts.resize(max_set_index + 1);

	for (const auto& s : family->sets) {
		const auto set_idx = static_cast<std::uint32_t>(s.type);
		std::vector<descriptor_binding_desc> descs;
		descs.reserve(s.bindings.size());
		for (const auto& b : s.bindings) {
			descs.push_back(b.desc);
		}
		family->owned_layouts[set_idx] = descriptor_set_layout::create(m_device->vulkan_device(), descs);
	}

	for (std::uint32_t i = 0; i <= max_set_index; ++i) {
		if (!family->owned_layouts[i]) {
			family->owned_layouts[i] = descriptor_set_layout::create(m_device->vulkan_device(), {});
		}
	}

	family->layout_handles.reserve(family->owned_layouts.size());
	for (const auto& l : family->owned_layouts) {
		family->layout_handles.push_back(l.handle());
	}

	log::println(log::category::vulkan, "Shader family layout registered: {}", name);
	m_families[std::move(name)] = std::move(family);
}

auto gse::gpu::shader_registry::find_family(const std::string_view name) -> const family_layout* {
	std::lock_guard lock(m_mutex);

	const auto it = m_families.find(std::string(name));
	if (it == m_families.end()) {
		return nullptr;
	}
	return it->second.get();
}
