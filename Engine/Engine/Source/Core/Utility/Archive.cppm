export module gse.utility:archive;

import std;
import gse.assert;

import :static_vector;

export namespace gse {

    template <typename T>
    struct raw_blob {
        std::vector<T>& data;
    };

    struct binary_writer {
        static constexpr bool is_writing = true;

        explicit binary_writer(
            std::ofstream& stream
        );

        binary_writer(
            std::ofstream& stream,
            std::uint32_t magic,
            std::uint32_t version
        );

        template <typename T> requires std::is_trivially_copyable_v<T>
        auto operator&(
            const T& value
        ) -> binary_writer&;

        auto operator&(
            const std::string& str
        ) -> binary_writer&;

        template <typename T>
        auto operator&(
            const std::vector<T>& vec
        ) -> binary_writer&;

        template <typename T, std::size_t N>
        auto operator&(
            const static_vector<T, N>& vec
        ) -> binary_writer&;

        template <typename T>
        auto operator&(
            const std::optional<T>& opt
        ) -> binary_writer&;

        template <typename K, typename V>
        auto operator&(
            const std::unordered_map<K, V>& map
        ) -> binary_writer&;

        template <typename T>
        auto operator&(
            const raw_blob<T>& blob
        ) -> binary_writer&;

        template <typename T> requires (!std::is_trivially_copyable_v<T>)
        auto operator&(
            const T& value
        ) -> binary_writer&;

    private:
        std::ofstream& m_stream;
    };

    struct binary_reader {
        static constexpr bool is_writing = false;

        explicit binary_reader(
            std::ifstream& stream
        );

        binary_reader(
            std::ifstream& stream,
            std::uint32_t expected_magic,
            std::uint32_t expected_version,
            std::string_view path,
            const std::source_location& loc = std::source_location::current()
        );

        auto valid(
        ) const -> bool;

        template <typename T> requires std::is_trivially_copyable_v<T>
        auto operator&(
            T& value
        ) -> binary_reader&;

        auto operator&(
            std::string& str
        ) -> binary_reader&;

        template <typename T>
        auto operator&(
            std::vector<T>& vec
        ) -> binary_reader&;

        template <typename T, std::size_t N>
        auto operator&(
            static_vector<T, N>& vec
        ) -> binary_reader&;

        template <typename T>
        auto operator&(
            std::optional<T>& opt
        ) -> binary_reader&;

        template <typename K, typename V>
        auto operator&(
            std::unordered_map<K, V>& map
        ) -> binary_reader&;

        template <typename T>
        auto operator&(
            const raw_blob<T>& blob
        ) -> binary_reader&;

        template <typename T> requires (!std::is_trivially_copyable_v<T>)
        auto operator&(
            T& value
        ) -> binary_reader&;

    private:
        std::ifstream& m_stream;
        bool m_valid = true;
    };

    template <typename T>
    concept archive = std::same_as<T, binary_writer> || std::same_as<T, binary_reader>;
}

gse::binary_writer::binary_writer(std::ofstream& stream) : m_stream(stream) {}

gse::binary_writer::binary_writer(std::ofstream& stream, const std::uint32_t magic, const std::uint32_t version) : m_stream(stream) {
    *this & magic & version;
}

template <typename T> requires std::is_trivially_copyable_v<T>
auto gse::binary_writer::operator&(const T& value) -> binary_writer& {
    m_stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return *this;
}

auto gse::binary_writer::operator&(const std::string& str) -> binary_writer& {
    const auto size = static_cast<std::uint32_t>(str.size());
    m_stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
    m_stream.write(str.c_str(), size);
    return *this;
}

template <typename T>
auto gse::binary_writer::operator&(const std::vector<T>& vec) -> binary_writer& {
    const auto count = static_cast<std::uint32_t>(vec.size());
    *this & count;
    for (const auto& item : vec) {
        *this & item;
    }
    return *this;
}

template <typename T, std::size_t N>
auto gse::binary_writer::operator&(const static_vector<T, N>& vec) -> binary_writer& {
    const auto count = static_cast<std::uint32_t>(vec.size());
    *this & count;
    for (const auto& item : vec) {
        *this & item;
    }
    return *this;
}

template <typename T>
auto gse::binary_writer::operator&(const std::optional<T>& opt) -> binary_writer& {
    const bool has = opt.has_value();
    *this & has;
    if (has) {
        *this & *opt;
    }
    return *this;
}

template <typename K, typename V>
auto gse::binary_writer::operator&(const std::unordered_map<K, V>& map) -> binary_writer& {
    const auto count = static_cast<std::uint32_t>(map.size());
    *this & count;
    for (const auto& [k, v] : map) {
        *this & k & v;
    }
    return *this;
}

template <typename T>
auto gse::binary_writer::operator&(const raw_blob<T>& blob) -> binary_writer& {
    const auto byte_size = static_cast<std::uint64_t>(blob.data.size() * sizeof(T));
    m_stream.write(reinterpret_cast<const char*>(&byte_size), sizeof(byte_size));
    if (byte_size > 0) {
        m_stream.write(reinterpret_cast<const char*>(blob.data.data()), static_cast<std::streamsize>(byte_size));
    }
    return *this;
}

template <typename T> requires (!std::is_trivially_copyable_v<T>)
auto gse::binary_writer::operator&(const T& value) -> binary_writer& {
    serialize(*this, const_cast<T&>(value));
    return *this;
}

gse::binary_reader::binary_reader(std::ifstream& stream) : m_stream(stream) {}

gse::binary_reader::binary_reader(std::ifstream& stream, const std::uint32_t expected_magic, const std::uint32_t expected_version, std::string_view path, const std::source_location& loc) : m_stream(stream) {
    std::uint32_t magic = 0, version = 0;
    *this & magic & version;
    m_valid = (magic == expected_magic && version == expected_version);
    gse::assert(m_valid, loc, "Invalid or outdated baked file: {}", path);
}

auto gse::binary_reader::valid() const -> bool {
    return m_valid;
}

template <typename T> requires std::is_trivially_copyable_v<T>
auto gse::binary_reader::operator&(T& value) -> binary_reader& {
    m_stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return *this;
}

auto gse::binary_reader::operator&(std::string& str) -> binary_reader& {
    std::uint32_t size = 0;
    m_stream.read(reinterpret_cast<char*>(&size), sizeof(size));
    str.resize(size);
    if (size > 0) {
        m_stream.read(str.data(), size);
    }
    return *this;
}

template <typename T>
auto gse::binary_reader::operator&(std::vector<T>& vec) -> binary_reader& {
    std::uint32_t count = 0;
    *this & count;
    vec.resize(count);
    for (auto& item : vec) {
        *this & item;
    }
    return *this;
}

template <typename T, std::size_t N>
auto gse::binary_reader::operator&(static_vector<T, N>& vec) -> binary_reader& {
    std::uint32_t count = 0;
    *this & count;
    gse::assert(count <= N, std::source_location::current(), "static_vector deserialization: count {} exceeds capacity {}", count, N);
    vec.clear();
    for (std::uint32_t i = 0; i < count; ++i) {
        T val{};
        *this & val;
        vec.push_back(std::move(val));
    }
    return *this;
}

template <typename T>
auto gse::binary_reader::operator&(std::optional<T>& opt) -> binary_reader& {
    bool has = false;
    *this & has;
    if (has) {
        T val{};
        *this & val;
        opt = std::move(val);
    }
    else {
        opt = std::nullopt;
    }
    return *this;
}

template <typename K, typename V>
auto gse::binary_reader::operator&(std::unordered_map<K, V>& map) -> binary_reader& {
    std::uint32_t count = 0;
    *this & count;
    for (std::uint32_t i = 0; i < count; ++i) {
        K k{};
        V v{};
        *this & k & v;
        map.emplace(std::move(k), std::move(v));
    }
    return *this;
}

template <typename T>
auto gse::binary_reader::operator&(const raw_blob<T>& blob) -> binary_reader& {
    std::uint64_t byte_size = 0;
    m_stream.read(reinterpret_cast<char*>(&byte_size), sizeof(byte_size));
    blob.data.resize(byte_size / sizeof(T));
    if (byte_size > 0) {
        m_stream.read(reinterpret_cast<char*>(blob.data.data()), static_cast<std::streamsize>(byte_size));
    }
    return *this;
}

template <typename T> requires (!std::is_trivially_copyable_v<T>)
auto gse::binary_reader::operator&(T& value) -> binary_reader& {
    serialize(*this, value);
    return *this;
}
