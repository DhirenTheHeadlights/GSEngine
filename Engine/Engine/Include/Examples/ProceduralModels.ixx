export module gse.examples:procedural_models;

import std;

import gse.graphics;
import gse.physics;

export namespace gse::procedural_model {
    auto box(
        const resource::handle<material>& mat
    ) -> resource::handle<model>;

    auto sphere(
        const resource::handle<material>& mat, 
        std::uint32_t sectors,
        std::uint32_t stacks
    ) -> resource::handle<model>;
}

auto gse::procedural_model::box(const resource::handle<material>& mat) -> resource::handle<model> {
    static std::once_flag once;
    static resource::handle<model> handle;

    std::vector<vertex> v;
    v.reserve(24);

    constexpr float h = 0.5f;

    auto push_face = [&](
        const raw3f a,
        const raw3f b,
        const raw3f c,
        const raw3f d,
        const raw3f n
        ) {
            v.push_back(vertex{ a, n, {0.0f, 0.0f} });
            v.push_back(vertex{ b, n, {1.0f, 0.0f} });
            v.push_back(vertex{ c, n, {1.0f, 1.0f} });
            v.push_back(vertex{ d, n, {0.0f, 1.0f} });
        };

    push_face({  h, -h, -h }, { -h, -h, -h }, { -h,  h, -h }, {  h,  h, -h }, {  0,  0, -1 });
    push_face({ -h, -h,  h }, {  h, -h,  h }, {  h,  h,  h }, { -h,  h,  h }, {  0,  0,  1 });
    push_face({ -h, -h, -h }, { -h, -h,  h }, { -h,  h,  h }, { -h,  h, -h }, { -1,  0,  0 });
    push_face({  h, -h,  h }, {  h, -h, -h }, {  h,  h, -h }, {  h,  h,  h }, {  1,  0,  0 });
    push_face({ -h,  h,  h }, {  h,  h,  h }, {  h,  h, -h }, { -h,  h, -h }, {  0,  1,  0 });
    push_face({ -h, -h, -h }, {  h, -h, -h }, {  h, -h,  h }, { -h, -h,  h }, {  0, -1,  0 });

    std::vector<std::uint32_t> idx;
    idx.reserve(36);
    for (std::uint32_t f = 0; f < 6; ++f) {
        const std::uint32_t o = f * 4;
        idx.push_back(o + 0); idx.push_back(o + 1); idx.push_back(o + 2);
        idx.push_back(o + 2); idx.push_back(o + 3); idx.push_back(o + 0);
    }

    std::vector data{ mesh_data{ std::move(v), std::move(idx), mat } };

    std::call_once(
        once,
        [&] {
            handle = queue<model>("proc/unit_box", std::move(data));
        }
    );

    return handle;
}

auto gse::procedural_model::sphere(const resource::handle<material>& mat, std::uint32_t sectors, std::uint32_t stacks) -> resource::handle<model> {
    sectors = std::max(3u, sectors);
    stacks = std::max(2u, stacks);

    const std::string key = std::format("proc/unit_sphere:{}x{}", sectors, stacks);

    static std::mutex cache_mutex;
    static std::unordered_map<std::string, resource::handle<model>> cache;

    {
        std::lock_guard lk(cache_mutex);
        if (const auto it = cache.find(key); it != cache.end()) {
            return it->second;
        }
    }

    std::vector<vertex> vertices;
    vertices.reserve((stacks + 1) * (sectors + 1));

    for (std::uint32_t i = 0; i <= stacks; ++i) {
        const float v = static_cast<float>(i) / static_cast<float>(stacks);
        const float phi = std::numbers::pi_v<float> *v;
        const float sp = std::sin(phi);
        const float cp = std::cos(phi);

        for (std::uint32_t j = 0; j <= sectors; ++j) {
            constexpr float r = 0.5f;
            const float u = static_cast<float>(j) / static_cast<float>(sectors);
            const float theta = 2.f * std::numbers::pi_v<float> *u;
            const float st = std::sin(theta);
            const float ct = std::cos(theta);

            const raw3f p{ r * sp * ct, r * cp, r * sp * st };
            const raw3f n{ sp * ct, cp, sp * st };
            const raw2f t{ u, v };

            vertices.push_back(vertex{ p, n, t });
        }
    }

    std::vector<std::uint32_t> indices;
    indices.reserve(stacks * sectors * 6);

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < sectors; ++j) {
            const int cur = i * (sectors + 1) + j;
            const int nxt = cur + (sectors + 1);

            indices.push_back(cur);
            indices.push_back(nxt);
            indices.push_back(cur + 1);

            indices.push_back(cur + 1);
            indices.push_back(nxt);
            indices.push_back(nxt + 1);
        }
    }

    std::vector data{ mesh_data{ std::move(vertices), std::move(indices), mat } };

    resource::handle<model> handle;
    {
        std::lock_guard lk(cache_mutex);
        if (const auto it = cache.find(key); it != cache.end()) return it->second;

        handle = queue<model>(key, std::move(data));
        cache.emplace(key, handle);
    }

    return handle;
}

