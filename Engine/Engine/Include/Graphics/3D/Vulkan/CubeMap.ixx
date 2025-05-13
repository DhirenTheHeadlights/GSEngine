export module gse.graphics.cube_map;

import std;
import vulkan_hpp;
import gse.physics.math;
import gse.platform;

export namespace gse {
    class cube_map {
    public:
        cube_map() = default;
        ~cube_map();

        auto create(vulkan::config& config, const std::array<std::filesystem::path, 6>& face_paths) -> void;
        auto create(const vulkan::config& config, int resolution, bool depth_only = false) -> void;
        auto destroy(const vulkan::config& config) -> void;

        auto get_image_resource() const -> const vulkan::persistent_allocator::image_resource&;

    private:
        vulkan::persistent_allocator::image_resource m_image_resource;
        vk::Sampler m_sampler = nullptr;

        int m_resolution = 0;
        bool m_depth_only = false;
        bool m_initialized = false;
    };
}
