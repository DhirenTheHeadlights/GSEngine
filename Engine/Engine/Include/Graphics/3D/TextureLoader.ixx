export module gse.graphics:texture_loader;

import std;

import :texture;
import :resource_loader;
import :rendering_context;

import gse.assert;
import gse.utility;
import gse.platform;

export namespace gse {
    class texture_loader final : resource_loader<texture, texture::handle, renderer::context> {
    public:
        using resource_loader::resource_loader;

        auto load_blank(const vulkan::config& config) -> void {
            m_blank_texture.load(config);
		}

        auto blank() -> texture& {
            return m_blank_texture;
		}
    private:
		texture m_blank_texture{ { 1, 1, 1, 1 } };
    };
}