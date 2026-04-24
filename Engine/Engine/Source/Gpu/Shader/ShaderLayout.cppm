export module gse.gpu:shader_layout;

import std;

import :vulkan_allocator;

import gse.core;
import gse.containers;
import gse.config;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.assert;

export namespace gse {
    struct shader_layout_binding {
        std::string name;
        vk::DescriptorSetLayoutBinding layout_binding;
    };

    struct shader_layout_set {
        std::uint32_t set_index = 0;
        std::vector<shader_layout_binding> bindings;
    };

    class shader_layout final : public identifiable {
    public:
        explicit shader_layout(const std::filesystem::path& path);

        auto load(
            const vk::raii::Device& device
        ) -> void;

        auto unload(
        ) -> void;

        [[nodiscard]] auto name(
        ) const -> const std::string&;

        [[nodiscard]] auto sets(
        ) const -> std::span<const shader_layout_set>;

        [[nodiscard]] auto vk_layout(
            std::uint32_t set_index
        ) const -> vk::DescriptorSetLayout;

        [[nodiscard]] auto vk_layouts(
        ) const -> std::vector<vk::DescriptorSetLayout>;

        [[nodiscard]] auto layout_size(
            std::uint32_t set_index
        ) const -> vk::DeviceSize;

        [[nodiscard]] auto binding_offset(
            std::uint32_t set_index,
            std::uint32_t binding
        ) const -> vk::DeviceSize;

    private:
        std::string m_name;
        std::filesystem::path m_path;
        std::vector<shader_layout_set> m_sets;
        std::vector<std::optional<vk::raii::DescriptorSetLayout>> m_vk_layouts;
    };
}

export namespace gse {
    template<archive Ar>
    auto serialize(Ar& ar, shader_layout_binding& b) -> void {
        ar & b.name & b.layout_binding;
    }

    template<archive Ar>
    auto serialize(Ar& ar, shader_layout_set& s) -> void {
        ar & s.set_index & s.bindings;
    }
}

gse::shader_layout::shader_layout(const std::filesystem::path& path)
    : identifiable(path, config::baked_resource_path)
    , m_name(path.stem().string())
    , m_path(path) {
}

auto gse::shader_layout::load(const vk::raii::Device& device) -> void {
    std::ifstream in(m_path, std::ios::binary);
    assert(in.is_open(), std::source_location::current(), "Failed to open layout file: {}", m_path.string());
    if (!in.is_open()) return;

    binary_reader ar(in, 0x474C4159, 1, m_path.string());
    if (!ar.valid()) return;

    ar & m_name;
    ar & m_sets;

    std::uint32_t max_set = 0;
    for (const auto& [set_index, bindings] : m_sets) {
        max_set = std::max(max_set, set_index);
    }

    m_vk_layouts.resize(max_set + 1);

    for (const auto& [set_index, bindings] : m_sets) {
        std::vector<vk::DescriptorSetLayoutBinding> raw_bindings;
        raw_bindings.reserve(bindings.size());

        for (const auto& [name, layout_binding] : bindings) {
            auto lb = layout_binding;
            lb.pImmutableSamplers = nullptr;
            raw_bindings.push_back(lb);
        }

        vk::DescriptorSetLayoutCreateInfo ci{
            .flags = vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT,
            .bindingCount = static_cast<std::uint32_t>(raw_bindings.size()),
            .pBindings = raw_bindings.data()
        };

        m_vk_layouts[set_index].emplace(device, ci);
    }

    for (std::uint32_t i = 0; i <= max_set; ++i) {
        if (!m_vk_layouts[i]) {
            vk::DescriptorSetLayoutCreateInfo ci{
                .flags = vk::DescriptorSetLayoutCreateFlagBits::eDescriptorBufferEXT,
                .bindingCount = 0,
                .pBindings = nullptr
            };
            m_vk_layouts[i].emplace(device, ci);
        }
    }
}

auto gse::shader_layout::unload() -> void {
    m_vk_layouts.clear();
    m_sets.clear();
}

auto gse::shader_layout::name() const -> const std::string& {
    return m_name;
}

auto gse::shader_layout::sets() const -> std::span<const shader_layout_set> {
    return m_sets;
}

auto gse::shader_layout::vk_layout(const std::uint32_t set_index) const -> vk::DescriptorSetLayout {
    assert(
        set_index < m_vk_layouts.size() && m_vk_layouts[set_index],
        std::source_location::current(), "Layout set {} not found", set_index
    );
    return **m_vk_layouts[set_index];
}

auto gse::shader_layout::vk_layouts() const -> std::vector<vk::DescriptorSetLayout> {
    std::vector<vk::DescriptorSetLayout> result;
    result.reserve(m_vk_layouts.size());
    for (const auto& layout : m_vk_layouts) {
        result.push_back(layout ? **layout : vk::DescriptorSetLayout{});
    }
    return result;
}

auto gse::shader_layout::layout_size(std::uint32_t set_index) const -> vk::DeviceSize {
    assert(
        set_index < m_vk_layouts.size() && m_vk_layouts[set_index],
        std::source_location::current(), "Layout set {} not found", set_index
    );
    return m_vk_layouts[set_index]->getSizeEXT();
}

auto gse::shader_layout::binding_offset(std::uint32_t set_index, const std::uint32_t binding) const -> vk::DeviceSize {
    assert(
        set_index < m_vk_layouts.size() && m_vk_layouts[set_index],
        std::source_location::current(), "Layout set {} not found", set_index
    );
    return m_vk_layouts[set_index]->getBindingOffsetEXT(binding);
}
