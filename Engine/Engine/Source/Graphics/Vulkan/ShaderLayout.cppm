export module gse.graphics:shader_layout;

import std;

import gse.platform;
import gse.utility;
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
        ) const -> const std::vector<shader_layout_set>&;

        [[nodiscard]] auto vk_layout(
            std::uint32_t set_index
        ) const -> vk::DescriptorSetLayout;

        [[nodiscard]] auto vk_layouts(
        ) const -> std::vector<vk::DescriptorSetLayout>;

    private:
        std::string m_name;
        std::filesystem::path m_path;
        std::vector<shader_layout_set> m_sets;
        std::vector<std::shared_ptr<vk::raii::DescriptorSetLayout>> m_vk_layouts;
    };
}

gse::shader_layout::shader_layout(const std::filesystem::path& path)
    : identifiable(path)
    , m_name(path.stem().string())
    , m_path(path) {
}

auto gse::shader_layout::load(const vk::raii::Device& device) -> void {
    std::ifstream in(m_path, std::ios::binary);
    assert(in.is_open(), std::source_location::current(), "Failed to open layout file: {}", m_path.string());

    auto read_string = [](std::ifstream& stream) -> std::string {
        std::uint32_t size = 0;
        stream.read(reinterpret_cast<char*>(&size), sizeof(size));
        std::string str(size, '\0');
        stream.read(str.data(), size);
        return str;
    };

    auto read_data = [](std::ifstream& stream, auto& value) {
        stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    };

    constexpr std::uint32_t expected_magic = 0x474C4159; // "GLAY"
    std::uint32_t magic = 0;
    read_data(in, magic);
    assert(magic == expected_magic, std::source_location::current(), "Invalid layout file magic");

    std::uint32_t version = 0;
    read_data(in, version);

    m_name = read_string(in);

    std::uint32_t set_count = 0;
    read_data(in, set_count);

    m_sets.resize(set_count);
    for (auto& set : m_sets) {
        read_data(in, set.set_index);

        std::uint32_t binding_count = 0;
        read_data(in, binding_count);

        set.bindings.resize(binding_count);
        for (auto& binding : set.bindings) {
            binding.name = read_string(in);
            read_data(in, binding.layout_binding);
        }
    }

    // Create Vulkan descriptor set layouts
    std::uint32_t max_set = 0;
    for (const auto& set : m_sets) {
        max_set = std::max(max_set, set.set_index);
    }

    m_vk_layouts.resize(max_set + 1);

    for (const auto& set : m_sets) {
        std::vector<vk::DescriptorSetLayoutBinding> raw_bindings;
        raw_bindings.reserve(set.bindings.size());

        for (const auto& b : set.bindings) {
            auto lb = b.layout_binding;
            lb.pImmutableSamplers = nullptr;
            raw_bindings.push_back(lb);
        }

        const bool is_push_set = (set.set_index == 1);

        vk::DescriptorSetLayoutCreateInfo ci{
            .flags = is_push_set
                ? vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR
                : vk::DescriptorSetLayoutCreateFlags{},
            .bindingCount = static_cast<std::uint32_t>(raw_bindings.size()),
            .pBindings = raw_bindings.data()
        };

        m_vk_layouts[set.set_index] = std::make_shared<vk::raii::DescriptorSetLayout>(device, ci);
    }

    // Create empty layouts for gaps
    for (std::uint32_t i = 0; i <= max_set; ++i) {
        if (!m_vk_layouts[i]) {
            const bool is_push_set = (i == 1);
            vk::DescriptorSetLayoutCreateInfo ci{
                .flags = is_push_set
                    ? vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR
                    : vk::DescriptorSetLayoutCreateFlags{},
                .bindingCount = 0,
                .pBindings = nullptr
            };
            m_vk_layouts[i] = std::make_shared<vk::raii::DescriptorSetLayout>(device, ci);
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

auto gse::shader_layout::sets() const -> const std::vector<shader_layout_set>& {
    return m_sets;
}

auto gse::shader_layout::vk_layout(const std::uint32_t set_index) const -> vk::DescriptorSetLayout {
    assert(set_index < m_vk_layouts.size() && m_vk_layouts[set_index],
        std::source_location::current(), "Layout set {} not found", set_index);
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
