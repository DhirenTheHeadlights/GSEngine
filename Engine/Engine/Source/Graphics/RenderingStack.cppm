export module gse.graphics:rendering_stack;

import std;

import :base_renderer;
import :rendering_context;

export namespace gse::renderer {
    template <typename T, typename... Args>
    auto add_renderer(
        Args&&... args
    ) -> T&;

    template <typename T>
    auto remove_renderer(
    ) -> void;

    template <typename T>
    auto has_renderer(
    ) -> bool;

    template <typename T>
    auto renderer(
    ) -> T&;

    auto renderers(
    ) -> std::span<std::unique_ptr<base_renderer>>;
}

namespace gse::renderer {
    std::vector<std::unique_ptr<base_renderer>> base_renderers;
}

template <typename T, typename... Args>
auto gse::renderer::add_renderer(Args&&... args) -> T& {
    for (const auto& r : base_renderers) {
        if (dynamic_cast<T*>(r.get()) != nullptr) {
            throw std::runtime_error("Renderer already exists");
        }
    }

    auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
    auto& ref = *ptr;
    base_renderers.push_back(std::move(ptr));
    return ref;
}

template <typename T>
auto gse::renderer::remove_renderer() -> void {
    auto it = std::ranges::find_if(
        base_renderers,
        [](const std::unique_ptr<base_renderer>& r) {
        return dynamic_cast<T*>(r.get()) != nullptr;
    }
    );

    if (it != base_renderers.end()) {
        base_renderers.erase(it);
    }
}

template <typename T>
auto gse::renderer::has_renderer() -> bool {
    for (const auto& r : base_renderers) {
        if (dynamic_cast<T*>(r.get()) != nullptr) {
            return true;
        }
    }
    return false;
}

template <typename T>
auto gse::renderer::renderer() -> T& {
    for (const auto& r : base_renderers) {
        if (auto* casted = dynamic_cast<T*>(r.get())) {
            return *casted;
        }
    }

    assert(
        false,
        std::source_location::current(),
        "Trying to find non-existent renderer {}", typeid(T).name()
    );

    std::unreachable();
}

auto gse::renderer::renderers() -> std::span<std::unique_ptr<base_renderer>> {
    return base_renderers;
}
