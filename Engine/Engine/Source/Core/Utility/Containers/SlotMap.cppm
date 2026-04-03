export module gse.utility:slot_map;

import std;
import gse.assert;

export namespace gse {
    template <typename T>
    class slot_map {
    public:
        struct handle {
            std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
            std::uint32_t generation = 0;

            [[nodiscard]] auto valid(
            ) const -> bool {
                return index != std::numeric_limits<std::uint32_t>::max();
            }

            auto operator==(const handle&) const -> bool = default;
        };

        static constexpr handle invalid_handle{};

        auto insert(
            T value
        ) -> handle;

        auto remove(
            handle h
        ) -> bool;

        template <typename Self>
        [[nodiscard]] auto get(
            this Self& self,
            handle h
        ) -> decltype(auto);

        template <typename Self>
        auto operator[](
            this Self& self,
            handle h
        ) -> decltype(auto);

        [[nodiscard]] auto is_valid(
            handle h
        ) const -> bool;

        [[nodiscard]] auto size(
        ) const -> std::size_t;

        [[nodiscard]] auto empty(
        ) const -> bool;

        template <typename Self>
        auto begin(
            this Self& self
        ) -> decltype(auto);

        template <typename Self>
        auto end(
            this Self& self
        ) -> decltype(auto);

        auto clear(
        ) -> void;

    private:
        struct slot_entry {
            std::uint32_t data_index = 0;
            std::uint32_t generation = 0;
            bool occupied = false;
        };

        std::vector<T> m_data;
        std::vector<std::uint32_t> m_data_to_slot;
        std::vector<slot_entry> m_indices;
        std::vector<std::uint32_t> m_free_list;
    };
}

template <typename T>
auto gse::slot_map<T>::insert(T value) -> handle {
    std::uint32_t slot_index;
    if (!m_free_list.empty()) {
        slot_index = m_free_list.back();
        m_free_list.pop_back();
    }
    else {
        slot_index = static_cast<std::uint32_t>(m_indices.size());
        m_indices.push_back({});
    }

    auto& s = m_indices[slot_index];
    s.data_index = static_cast<std::uint32_t>(m_data.size());
    s.occupied = true;

    m_data.push_back(std::move(value));
    m_data_to_slot.push_back(slot_index);

    return { slot_index, s.generation };
}

template <typename T>
auto gse::slot_map<T>::remove(const handle h) -> bool {
    if (!is_valid(h)) {
        return false;
    }

    auto& s = m_indices[h.index];
    const std::uint32_t data_index = s.data_index;
    const std::uint32_t last = static_cast<std::uint32_t>(m_data.size()) - 1;

    if (data_index != last) {
        m_data[data_index] = std::move(m_data[last]);
        m_data_to_slot[data_index] = m_data_to_slot[last];
        m_indices[m_data_to_slot[data_index]].data_index = data_index;
    }

    m_data.pop_back();
    m_data_to_slot.pop_back();

    s.occupied = false;
    ++s.generation;
    m_free_list.push_back(h.index);
    return true;
}

template <typename T>
template <typename Self>
auto gse::slot_map<T>::get(this Self& self, const handle h) -> decltype(auto) {
    auto* result = self.is_valid(h) ? &self.m_data[self.m_indices[h.index].data_index] : nullptr;
    return result;
}

template <typename T>
template <typename Self>
auto gse::slot_map<T>::operator[](this Self& self, const handle h) -> decltype(auto) {
    gse::assert(self.is_valid(h), std::source_location::current(), "slot_map: invalid or stale handle");
    return self.m_data[self.m_indices[h.index].data_index];
}

template <typename T>
auto gse::slot_map<T>::is_valid(const handle h) const -> bool {
    if (!h.valid()) {
        return false;
    }
    if (h.index >= m_indices.size()) {
        return false;
    }
    const auto& s = m_indices[h.index];
    return s.occupied && s.generation == h.generation;
}

template <typename T>
auto gse::slot_map<T>::size() const -> std::size_t {
    return m_data.size();
}

template <typename T>
auto gse::slot_map<T>::empty() const -> bool {
    return m_data.empty();
}

template <typename T>
template <typename Self>
auto gse::slot_map<T>::begin(this Self& self) -> decltype(auto) {
    return self.m_data.begin();
}

template <typename T>
template <typename Self>
auto gse::slot_map<T>::end(this Self& self) -> decltype(auto) {
    return self.m_data.end();
}

template <typename T>
auto gse::slot_map<T>::clear() -> void {
    m_data.clear();
    m_data_to_slot.clear();
    m_indices.clear();
    m_free_list.clear();
}
