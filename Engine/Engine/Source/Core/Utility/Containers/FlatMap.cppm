export module gse.utility:flat_map;

import std;
import gse.assert;

export namespace gse {
    template <typename Key, typename Value, typename Compare = std::less<Key>>
    class flat_map {
    public:
        using key_type = Key;
        using mapped_type = Value;
        using value_type = std::pair<Key, Value>;
        using size_type = std::size_t;
        using iterator = typename std::vector<value_type>::iterator;
        using const_iterator = typename std::vector<value_type>::const_iterator;

        flat_map() = default;

        flat_map(
            std::initializer_list<value_type> init
        );

        template <typename Self>
        [[nodiscard]] auto at(
            this Self& self,
            const Key& key
        ) -> decltype(auto);

        auto operator[](
            const Key& key
        ) -> Value&;

        template <typename Self>
        [[nodiscard]] auto find(
            this Self& self,
            const Key& key
        ) -> decltype(auto);

        auto insert(
            const value_type& pair
        ) -> std::pair<iterator, bool>;

        auto insert(
            value_type&& pair
        ) -> std::pair<iterator, bool>;

        template <typename... Args>
        auto emplace(
            const Key& key,
            Args&&... args
        ) -> std::pair<iterator, bool>;

        auto erase(
            const Key& key
        ) -> size_type;

        auto erase(
            iterator pos
        ) -> iterator;

        auto clear(
        ) -> void;

        [[nodiscard]] auto size(
        ) const -> size_type;

        [[nodiscard]] auto empty(
        ) const -> bool;

        [[nodiscard]] auto contains(
            const Key& key
        ) const -> bool;

        template <typename Self>
        [[nodiscard]] auto begin(
            this Self& self
        ) -> decltype(auto);

        template <typename Self>
        [[nodiscard]] auto end(
            this Self& self
        ) -> decltype(auto);

        auto reserve(
            size_type capacity
        ) -> void;

        auto assign_unsorted(
            std::vector<value_type> values
        ) -> void;

    private:
        [[nodiscard]] auto lower(
            const Key& key
        ) const -> const_iterator;

        [[nodiscard]] auto lower(
            const Key& key
        ) -> iterator;

        std::vector<value_type> m_data;
        [[no_unique_address]] Compare m_compare;
    };
}

template <typename Key, typename Value, typename Compare>
gse::flat_map<Key, Value, Compare>::flat_map(std::initializer_list<value_type> init) : m_data(init) {
    std::ranges::sort(m_data, [this](const value_type& a, const value_type& b) {
        return m_compare(a.first, b.first);
    });
}

template <typename Key, typename Value, typename Compare>
template <typename Self>
auto gse::flat_map<Key, Value, Compare>::at(this Self& self, const Key& key) -> decltype(auto) {
    auto it = self.lower(key);
    assert(it != self.m_data.end() && it->first == key, std::source_location::current(), "flat_map::at: key not found");
    return (it->second);
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::operator[](const Key& key) -> Value& {
    auto it = lower(key);
    if (it != m_data.end() && it->first == key) {
        return it->second;
    }
    it = m_data.insert(it, { key, Value{} });
    return it->second;
}

template <typename Key, typename Value, typename Compare>
template <typename Self>
auto gse::flat_map<Key, Value, Compare>::find(this Self& self, const Key& key) -> decltype(auto) {
    auto it = self.lower(key);
    if (it != self.m_data.end() && it->first == key) {
        return it;
    }
    return self.m_data.end();
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::insert(const value_type& pair) -> std::pair<iterator, bool> {
    auto it = lower(pair.first);
    if (it != m_data.end() && it->first == pair.first) {
        return { it, false };
    }
    return { m_data.insert(it, pair), true };
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::insert(value_type&& pair) -> std::pair<iterator, bool> {
    auto it = lower(pair.first);
    if (it != m_data.end() && it->first == pair.first) {
        return { it, false };
    }
    return { m_data.insert(it, std::move(pair)), true };
}

template <typename Key, typename Value, typename Compare>
template <typename... Args>
auto gse::flat_map<Key, Value, Compare>::emplace(const Key& key, Args&&... args) -> std::pair<iterator, bool> {
    auto it = lower(key);
    if (it != m_data.end() && it->first == key) {
        return { it, false };
    }
    return { m_data.emplace(it, key, Value{ std::forward<Args>(args)... }), true };
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::erase(const Key& key) -> size_type {
    auto it = lower(key);
    if (it != m_data.end() && it->first == key) {
        m_data.erase(it);
        return 1;
    }
    return 0;
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::erase(iterator pos) -> iterator {
    return m_data.erase(pos);
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::clear() -> void {
    m_data.clear();
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::size() const -> size_type {
    return m_data.size();
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::empty() const -> bool {
    return m_data.empty();
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::contains(const Key& key) const -> bool {
    auto it = lower(key);
    return it != m_data.end() && it->first == key;
}

template <typename Key, typename Value, typename Compare>
template <typename Self>
auto gse::flat_map<Key, Value, Compare>::begin(this Self& self) -> decltype(auto) {
    return self.m_data.begin();
}

template <typename Key, typename Value, typename Compare>
template <typename Self>
auto gse::flat_map<Key, Value, Compare>::end(this Self& self) -> decltype(auto) {
    return self.m_data.end();
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::reserve(const size_type capacity) -> void {
    m_data.reserve(capacity);
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::assign_unsorted(std::vector<value_type> values) -> void {
    m_data = std::move(values);
    std::ranges::sort(m_data, m_compare, &value_type::first);
    const auto dup_begin = std::ranges::unique(m_data, {}, &value_type::first).begin();
    m_data.erase(dup_begin, m_data.end());
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::lower(const Key& key) const -> const_iterator {
    return std::ranges::lower_bound(m_data, key, m_compare, &value_type::first);
}

template <typename Key, typename Value, typename Compare>
auto gse::flat_map<Key, Value, Compare>::lower(const Key& key) -> iterator {
    return std::ranges::lower_bound(m_data, key, m_compare, &value_type::first);
}
