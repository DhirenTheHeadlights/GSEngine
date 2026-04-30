export module gse.gpu:shader_registry;

import std;

import :handles;
import :types;
import :device;
import :shader;
import :shader_layout;

import gse.assets;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;

export namespace gse::gpu {
	struct shader_cache_entry {
		std::unordered_map<shader_stage, shader_module> modules;
		std::vector<descriptor_set_layout> owned_layouts;
		std::vector<handle<descriptor_set_layout>> layout_handles;
		const shader_layout* external_layout = nullptr;
	};

	class shader_registry final : public non_copyable {
	public:
		explicit shader_registry(
			device& dev
		);

		~shader_registry() override;

		auto load_layouts(
			const std::filesystem::path& layouts_dir
		) -> void;

		[[nodiscard]] auto cache(
			const resource::handle<shader>& s
		) -> const shader_cache_entry&;

	private:
		device* m_device;
		std::unordered_map<id, shader_cache_entry> m_cache;
		std::unordered_map<std::string, std::unique_ptr<shader_layout>> m_layouts;
	};
}

gse::gpu::shader_registry::shader_registry(device& dev) : m_device(&dev) {}

gse::gpu::shader_registry::~shader_registry() {}

auto gse::gpu::shader_registry::load_layouts(const std::filesystem::path& layouts_dir) -> void {
	if (!m_layouts.empty()) {
		return;
	}
	if (!std::filesystem::exists(layouts_dir)) {
		return;
	}

	for (const auto& entry : std::filesystem::directory_iterator(layouts_dir)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".glayout") {
			continue;
		}

		auto layout = std::make_unique<shader_layout>(entry.path());
		layout->load(*m_device);

		const auto& name = layout->name();
		log::println(log::category::assets, "Layout loaded: {}", name);
		m_layouts[name] = std::move(layout);
	}
}

auto gse::gpu::shader_registry::cache(const resource::handle<shader>& s) -> const shader_cache_entry& {
	if (const auto it = m_cache.find(s.id()); it != m_cache.end()) {
		return it->second;
	}

	shader_cache_entry entry;

	auto create_module = [&](const shader_stage stage) -> shader_module {
		const auto spirv = s->spirv(stage);
		if (spirv.empty()) {
			return {};
		}
		return shader_module::create(m_device->vulkan_device(), spirv);
	};

	if (s->is_compute()) {
		entry.modules.emplace(shader_stage::compute, create_module(shader_stage::compute));
	}
	else if (s->is_mesh_shader()) {
		entry.modules.emplace(shader_stage::task, create_module(shader_stage::task));
		entry.modules.emplace(shader_stage::mesh, create_module(shader_stage::mesh));
		entry.modules.emplace(shader_stage::fragment, create_module(shader_stage::fragment));
	}
	else {
		entry.modules.emplace(shader_stage::vertex, create_module(shader_stage::vertex));
		entry.modules.emplace(shader_stage::fragment, create_module(shader_stage::fragment));
	}

	const auto& layout_name = s->layout_name();
	const auto& [sets] = s->layout_data();

	const shader_layout* external_layout = nullptr;
	if (!layout_name.empty()) {
		if (const auto it = m_layouts.find(layout_name); it != m_layouts.end()) {
			external_layout = it->second.get();
		}
	}

	if (external_layout) {
		entry.layout_handles = external_layout->layout_handles();
		entry.external_layout = external_layout;
		auto [result_it, inserted] = m_cache.emplace(s.id(), std::move(entry));
		return result_it->second;
	}

	std::uint32_t max_set_index = static_cast<std::uint32_t>(descriptor_set_type::bind_less);
	for (const auto& type : std::views::keys(sets)) {
		max_set_index = std::max(max_set_index, static_cast<std::uint32_t>(type));
	}

	entry.owned_layouts.resize(max_set_index + 1);

	for (const auto& [type, set_data] : sets) {
		const auto set_idx = static_cast<std::uint32_t>(type);

		std::vector<descriptor_binding_desc> descs;
		descs.reserve(set_data.bindings.size());
		for (const auto& b : set_data.bindings) {
			descs.push_back(b.desc);
		}

		entry.owned_layouts[set_idx] = descriptor_set_layout::create(*m_device, descs);
	}

	for (std::uint32_t i = 0; i <= max_set_index; ++i) {
		if (!entry.owned_layouts[i]) {
			entry.owned_layouts[i] = descriptor_set_layout::create(*m_device, {});
		}
	}

	entry.layout_handles.reserve(entry.owned_layouts.size());
	for (const auto& l : entry.owned_layouts) {
		entry.layout_handles.push_back(l.handle());
	}

	auto [result_it, inserted] = m_cache.emplace(s.id(), std::move(entry));
	return result_it->second;
}
