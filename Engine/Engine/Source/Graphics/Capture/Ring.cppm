export module gse.graphics:capture_ring;

import std;

import gse.math;
import gse.platform;

export namespace gse::renderer::capture {
    class ring {
    public:
        auto push(
            gpu::encoded_unit unit
        ) -> void;

        [[nodiscard]] auto snapshot_from_earliest_keyframe(
        ) const -> std::vector<gpu::encoded_unit>;

        auto set_budget(
            time budget
        ) -> void;

        [[nodiscard]] auto budget(
        ) const -> time;

        [[nodiscard]] auto bytes_stored(
        ) const -> std::size_t;

        [[nodiscard]] auto frame_count(
        ) const -> std::size_t;

        auto clear(
        ) -> void;
    private:
        auto evict_to_budget(
        ) -> void;

        std::deque<gpu::encoded_unit> m_units;
        std::size_t m_bytes = 0;
        time m_budget = seconds(30.f);
        time m_newest_pts{};
    };
}

auto gse::renderer::capture::ring::push(gpu::encoded_unit unit) -> void {
    m_newest_pts = unit.pts;
    m_bytes += unit.bytes.size();
    m_units.push_back(std::move(unit));
    evict_to_budget();
}

auto gse::renderer::capture::ring::snapshot_from_earliest_keyframe() const -> std::vector<gpu::encoded_unit> {
    const auto first_keyframe = std::ranges::find_if(m_units, [](const gpu::encoded_unit& u) {
        return u.keyframe;
    });

    if (first_keyframe == m_units.end()) {
        return {};
    }

    std::vector<gpu::encoded_unit> snapshot;
    snapshot.reserve(static_cast<std::size_t>(std::ranges::distance(first_keyframe, m_units.end())));

    for (auto it = first_keyframe; it != m_units.end(); ++it) {
        snapshot.push_back({
            .bytes = it->bytes,
            .pts = it->pts,
            .keyframe = it->keyframe
        });
    }

    return snapshot;
}

auto gse::renderer::capture::ring::set_budget(const time budget) -> void {
    m_budget = budget;
    evict_to_budget();
}

auto gse::renderer::capture::ring::budget() const -> time {
    return m_budget;
}

auto gse::renderer::capture::ring::bytes_stored() const -> std::size_t {
    return m_bytes;
}

auto gse::renderer::capture::ring::frame_count() const -> std::size_t {
    return m_units.size();
}

auto gse::renderer::capture::ring::clear() -> void {
    m_units.clear();
    m_bytes = 0;
    m_newest_pts = {};
}

auto gse::renderer::capture::ring::evict_to_budget() -> void {
    while (!m_units.empty() && (m_newest_pts - m_units.front().pts) > m_budget) {
        m_bytes -= m_units.front().bytes.size();
        m_units.pop_front();
    }

    while (!m_units.empty() && !m_units.front().keyframe) {
        m_bytes -= m_units.front().bytes.size();
        m_units.pop_front();
    }
}
