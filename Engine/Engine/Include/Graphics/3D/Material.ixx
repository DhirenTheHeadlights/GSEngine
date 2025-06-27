export module gse.graphics:material;

import std;

import :texture;
import :shader;

import gse.physics.math;
import gse.utility;
import gse.platform;

export namespace gse {
    struct material {
        unitless::vec3 ambient = unitless::vec3(1.0f);
        unitless::vec3 diffuse = unitless::vec3(1.0f);
        unitless::vec3 specular = unitless::vec3(1.0f);
        unitless::vec3 emission = unitless::vec3(0.0f);

        float shininess = 0.0f;
        float optical_density = 1.0f;
        float transparency = 1.0f;
        int illumination_model = 0;

        id diffuse_texture;
        id normal_texture;
        id specular_texture;
    };

    class material_handle {
    public:
	    material_handle(material* m = nullptr) : m_material(m) {}
        auto operator->() const -> const material* { return m_material; }
        auto data() const -> material* { return m_material; }
        auto exists() const -> bool { return m_material != nullptr; }

    private:
        material* m_material = nullptr;
    };

    struct material_properties {
        unitless::vec3 ambient, diffuse, specular, emission;
        float shininess, optical_density, transparency;
        int illumination_model;
        id diffuse_texture, normal_texture, specular_texture;

        auto operator==(material_properties const& o) const noexcept -> bool {
            return ambient == o.ambient
                && diffuse == o.diffuse
                && specular == o.specular
                && emission == o.emission
                && shininess == o.shininess
                && optical_density == o.optical_density
                && transparency == o.transparency
                && illumination_model == o.illumination_model
                && diffuse_texture.tag() == o.diffuse_texture.tag()
                && normal_texture.tag() == o.normal_texture.tag()
                && specular_texture.tag() == o.specular_texture.tag();
        }
    };

    struct material_properties_hash {
        auto operator()(material_properties const& p) const noexcept -> size_t {
            auto comb = [](size_t& h, const size_t v) {
                h ^= v + 0x9e3779b97f4a7c15 + (h << 6) + (h >> 2);
                };

            size_t h = 0;
            const auto hf = std::hash<float>{};
            const auto hi = std::hash<int>{};
            const auto hs = std::hash<std::string>{};

            comb(h, hf(p.ambient.x)); comb(h, hf(p.ambient.y)); comb(h, hf(p.ambient.z));
            comb(h, hf(p.diffuse.x)); comb(h, hf(p.diffuse.y)); comb(h, hf(p.diffuse.z));
            comb(h, hf(p.specular.x)); comb(h, hf(p.specular.y)); comb(h, hf(p.specular.z));
            comb(h, hf(p.emission.x)); comb(h, hf(p.emission.y)); comb(h, hf(p.emission.z));

            comb(h, hf(p.shininess));
            comb(h, hf(p.optical_density));
            comb(h, hf(p.transparency));
            comb(h, hi(p.illumination_model));

            comb(h, hs(p.diffuse_texture.tag()));
            comb(h, hs(p.normal_texture.tag()));
            comb(h, hs(p.specular_texture.tag()));

            return h;
        }
    };

    auto get_material(const std::string& name) -> material_handle;
    auto does_material_exist(const std::string& name) -> bool;

    auto generate_material(
        unitless::vec3 const& ambient,
        unitless::vec3 const& diffuse,
        unitless::vec3 const& specular,
        unitless::vec3 const& emission,
        float shininess,
        float optical_density,
        float transparency,
        int illumination_model,
        std::string name = ""
    ) -> material_handle;

    auto generate_material(
        id const& diffuse_texture,
        id const& normal_texture,
        id const& specular_texture,
        std::string name = ""
    ) -> material_handle;

    auto get_materials() -> std::unordered_map<std::string, material>&;
}

std::unordered_map<std::string, gse::material> g_materials;
std::unordered_map<gse::material_properties, std::string, gse::material_properties_hash> g_material_cache;
std::size_t g_material_counter = 0;

auto gse::get_material(const std::string& name) -> material_handle {
    assert(g_materials.contains(name), "Material not found.");
    return &g_materials[name];
}

auto gse::does_material_exist(const std::string& name) -> bool {
    return g_materials.contains(name);
}

auto gse::generate_material(
    unitless::vec3 const& ambient,
    unitless::vec3 const& diffuse,
    unitless::vec3 const& specular,
    unitless::vec3 const& emission,
    const float shininess,
    const float optical_density,
    const float transparency,
    const int illumination_model,
    std::string name
) -> material_handle {
    material_properties key{
    	ambient, diffuse, specular, emission,
    	shininess, optical_density, transparency,
    	illumination_model,
        {}, {}, {}
    };

    if (const auto it = g_material_cache.find(key); it != g_material_cache.end()) {
        return &g_materials[it->second];
    }

    if (name.empty()) {
        name = "material_" + std::to_string(g_material_counter++);
    }
    assert(!g_materials.contains(name), "Material name collision.");

    auto [it, _] = g_materials.emplace(
        name,
        material{
        	ambient, diffuse, specular, emission,
        	shininess, optical_density, transparency, illumination_model,
			{}, {}, {}
        }
    );
    g_material_cache.emplace(key, name);
    return &it->second;
}

auto gse::generate_material(
    id const& diffuse_texture,
    id const& normal_texture,
    id const& specular_texture,
    std::string name
) -> material_handle {
    material_properties key{
        {}, {}, {}, {},
        0, 1, 1, 0,
        diffuse_texture, normal_texture, specular_texture
    };

    if (const auto it = g_material_cache.find(key); it != g_material_cache.end()) {
        return &g_materials[it->second];
    }

    if (name.empty()) {
        name = diffuse_texture.tag();
    }
    assert(!g_materials.contains(name), "Material name collision.");

    auto [it, _] = g_materials.emplace(
        name,
        material{
            unitless::vec3(1.0f),
            unitless::vec3(1.0f),
            unitless::vec3(1.0f),
            unitless::vec3(0.0f),
            0.0f, 1.0f, 1.0f, 0,
            diffuse_texture, normal_texture, specular_texture
        }
    );
    g_material_cache.emplace(key, name);
    return &it->second;
}

auto gse::get_materials() -> std::unordered_map<std::string, material>& {
    return g_materials;
}

