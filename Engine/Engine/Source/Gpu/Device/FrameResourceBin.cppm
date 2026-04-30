export module gse.gpu:frame_resource_bin;

import std;

import :handles;
import gse.core;

export namespace gse::gpu {
    enum class queue_id : std::uint8_t {
        graphics = 0,
        compute = 1,
    };

    constexpr std::size_t queue_id_count = 2;

    struct queue_progress {
        queue_id queue;
        std::uint64_t reached_value;
    };

    class frame_resource_bin final : public non_copyable {
    public:
        frame_resource_bin(
        ) = default;

        ~frame_resource_bin(
        );

        frame_resource_bin(
            frame_resource_bin&&
        ) noexcept = default;

        auto operator=(
            frame_resource_bin&&
        ) noexcept -> frame_resource_bin& = default;

        template <typename T>
        auto retain(
            queue_id queue,
            std::uint64_t until_value,
            T resource
        ) -> void;

        auto drain(
            std::span<const queue_progress> progress
        ) -> void;

        auto wait_idle_clear(
        ) -> void;

        [[nodiscard]] auto pending_count(
        ) const -> std::size_t;

    private:
        struct retained_base {
            virtual ~retained_base() = default;
        };

        template <typename T>
        struct retained_holder final : retained_base {
            T m_value;

            explicit retained_holder(
                T&& v
            ) : m_value(std::move(v)) {}
        };

        struct slot {
            queue_id m_queue;
            std::uint64_t m_until_value;
            std::unique_ptr<retained_base> m_holder;
        };

        std::vector<slot> m_slots;
    };
}

gse::gpu::frame_resource_bin::~frame_resource_bin() = default;

template <typename T>
auto gse::gpu::frame_resource_bin::retain(const queue_id queue, const std::uint64_t until_value, T resource) -> void {
    m_slots.push_back({
        .m_queue = queue,
        .m_until_value = until_value,
        .m_holder = std::make_unique<retained_holder<T>>(std::move(resource)),
    });
}

auto gse::gpu::frame_resource_bin::drain(std::span<const queue_progress> progress) -> void {
    auto reached = [progress](const queue_id q) -> std::uint64_t {
        for (const auto& p : progress) {
            if (p.queue == q) {
                return p.reached_value;
            }
        }
        return 0;
    };

    std::erase_if(m_slots, [&](const slot& s) {
        return s.m_until_value <= reached(s.m_queue);
    });
}

auto gse::gpu::frame_resource_bin::wait_idle_clear() -> void {
    m_slots.clear();
}

auto gse::gpu::frame_resource_bin::pending_count() const -> std::size_t {
    return m_slots.size();
}
