export module gse.containers:archive;

import std;
import gse.assert;
import gse.meta;

export namespace gse {

	struct archive_skip {};

	struct archive_raw {};

	constexpr std::uint32_t archive_format_epoch = 1;

	struct archive_mismatch {
		std::uint32_t magic = 0;
		std::uint32_t version = 0;
		std::uint32_t epoch = 0;
		bool readable = false;
	};

	struct archive_field {
		std::string name;
		std::string type;
		std::uint32_t size = 0;
	};

	consteval auto is_archive_raw(
		std::meta::info type
	) -> bool;

	template <typename T>
	consteval auto is_archive_opaque() -> bool;

	template <typename T>
	consteval auto has_reflected_members() -> bool;

	template <typename T>
	concept archive_schema_type =
		std::is_class_v<T>
		&& !is_archive_opaque<T>()
		&& !is_archive_raw(^^T)
		&& has_reflected_members<T>();

	template <typename T>
	consteval auto archive_field_size() -> std::uint32_t;

	auto next_archive_type_id() -> std::uint64_t;

	template <typename T>
	auto archive_type_id() -> std::uint64_t;

	template <typename T>
	struct raw_blob {
		std::vector<T>& data;
	};

	template <typename T>
	struct raw_blob_owned {
		std::vector<T> storage;
	};

	template <typename A, typename T>
	concept has_user_serialize = requires(A& a, T& v) { serialize(a, v); };

	consteval auto is_archive_skipped(
		std::meta::info member
	) -> bool;

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

		template <typename T>
		requires(std::is_trivially_copyable_v<T> && !archive_schema_type<T>)
		auto operator&(
			const T& value
		) -> binary_writer&;

		template <archive_schema_type T>
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
			const std::inplace_vector<T, N>& vec
		) -> binary_writer&;

		template <typename T>
		auto operator&(
			const std::optional<T>& opt
		) -> binary_writer&;

		template <typename... Ts>
		auto operator&(
			const std::variant<Ts...>& var
		) -> binary_writer&;

		template <typename K, typename V>
		auto operator&(
			const std::unordered_map<K, V>& map
		) -> binary_writer&;

		template <typename T>
		auto operator&(
			const raw_blob<T>& blob
		) -> binary_writer&;

		template <typename T>
		requires(!std::is_trivially_copyable_v<T> && !archive_schema_type<T>)
		auto operator&(
			const T& value
		) -> binary_writer&;

	private:
		template <typename T>
		auto emit_schema() -> void;

		std::ofstream& m_stream;
		std::unordered_set<std::uint64_t> m_emitted;
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

		[[nodiscard]] static auto open(
			std::ifstream& stream,
			std::uint32_t expected_magic,
			std::uint32_t expected_version
		) -> std::expected<binary_reader, archive_mismatch>;

		[[nodiscard]] auto valid() const -> bool;

		template <typename T>
		requires(std::is_trivially_copyable_v<T> && !archive_schema_type<T>)
		auto operator&(
			T& value
		) -> binary_reader&;

		template <archive_schema_type T>
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
			std::inplace_vector<T, N>& vec
		) -> binary_reader&;

		template <typename T>
		auto operator&(
			std::optional<T>& opt
		) -> binary_reader&;

		template <typename... Ts>
		auto operator&(
			std::variant<Ts...>& var
		) -> binary_reader&;

		template <typename K, typename V>
		auto operator&(
			std::unordered_map<K, V>& map
		) -> binary_reader&;

		template <typename T>
		auto operator&(
			const raw_blob<T>& blob
		) -> binary_reader&;

		template <typename T>
		requires(!std::is_trivially_copyable_v<T> && !archive_schema_type<T>)
		auto operator&(
			T& value
		) -> binary_reader&;

	private:
		template <typename T>
		auto read_schema() -> const std::vector<archive_field>&;

		auto skip_field(
			const archive_field& field
		) -> void;

		std::ifstream& m_stream;
		std::unordered_map<std::uint64_t, std::vector<archive_field>> m_schemas;
		bool m_valid = true;
	};

	template <typename T>
	concept archive = std::same_as<T, binary_writer> || std::same_as<T, binary_reader>;

	template <typename T>
	auto serialize(
		binary_writer& ar,
		raw_blob_owned<T>& v
	) -> void;

	template <typename T>
	auto serialize(
		binary_reader& ar,
		raw_blob_owned<T>& v
	) -> void;
}

consteval auto gse::is_archive_raw(const std::meta::info type) -> bool {
	return std::ranges::any_of(
		std::define_static_array(std::meta::annotations_of(type)),
		[](std::meta::info ann) {
			return std::meta::type_of(ann) == ^^archive_raw;
		}
	);
}

template <typename T>
consteval auto gse::is_archive_opaque() -> bool {
	std::meta::info entity = std::meta::dealias(^^T);
	if (std::meta::has_template_arguments(entity)) {
		entity = std::meta::template_of(entity);
	}

	const std::meta::info parent = std::meta::parent_of(entity);
	if (!std::meta::has_identifier(parent)) {
		return false;
	}

	const std::string_view scope = std::meta::identifier_of(parent);
	return scope == "std" || scope == "internal";
}

template <typename T>
consteval auto gse::has_reflected_members() -> bool {
	return !std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())).empty();
}

template <typename T>
consteval auto gse::archive_field_size() -> std::uint32_t {
	if constexpr (std::is_trivially_copyable_v<T> && !archive_schema_type<T>) {
		return static_cast<std::uint32_t>(sizeof(T));
	}
	else {
		return 0;
	}
}

auto gse::next_archive_type_id() -> std::uint64_t {
	static std::atomic<std::uint64_t> counter{ 0 };
	return counter.fetch_add(1, std::memory_order_relaxed);
}

template <typename T>
auto gse::archive_type_id() -> std::uint64_t {
	static const std::uint64_t value = next_archive_type_id();
	return value;
}

consteval auto gse::is_archive_skipped(const std::meta::info member) -> bool {
	return std::ranges::any_of(
		std::define_static_array(std::meta::annotations_of(member)),
		[](std::meta::info ann) {
			return std::meta::type_of(ann) == ^^archive_skip;
		}
	);
}

gse::binary_writer::binary_writer(std::ofstream& stream) : m_stream(stream) {
}

gse::binary_writer::binary_writer(std::ofstream& stream, const std::uint32_t magic, const std::uint32_t version)
	: m_stream(stream) {
	*this & magic & version & archive_format_epoch;
}

template <typename T>
requires(std::is_trivially_copyable_v<T> && !gse::archive_schema_type<T>)
auto gse::binary_writer::operator&(const T& value) -> binary_writer& {
	m_stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
	return *this;
}

template <typename T>
auto gse::binary_writer::emit_schema() -> void {
	if (!m_emitted.insert(archive_type_id<T>()).second) {
		return;
	}

	std::vector<archive_field> fields;
	template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
		if constexpr (!is_archive_skipped(m)) {
			fields.push_back({
				.name = std::string(std::meta::identifier_of(m)),
				.type = std::string(meta::qualified_name<typename [:std::meta::type_of(m):]>()),
				.size = archive_field_size<typename [:std::meta::type_of(m):]>(),
			});
		}
	}

	const auto count = static_cast<std::uint32_t>(fields.size());
	m_stream.write(reinterpret_cast<const char*>(&count), sizeof(count));
	for (const archive_field& field : fields) {
		*this & field.name & field.type & field.size;
	}
}

template <gse::archive_schema_type T>
auto gse::binary_writer::operator&(const T& value) -> binary_writer& {
	if constexpr (has_user_serialize<binary_writer, T>) {
		serialize(*this, const_cast<T&>(value));
		return *this;
	}
	else {
		emit_schema<T>();

		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
			if constexpr (!is_archive_skipped(m)) {
				if constexpr (archive_field_size<typename [:std::meta::type_of(m):]>() != 0) {
					*this & value.[:m:];
				}
				else {
					const std::streampos length_at = m_stream.tellp();
					std::uint32_t length = 0;
					m_stream.write(reinterpret_cast<const char*>(&length), sizeof(length));
					const std::streampos body_at = m_stream.tellp();
					*this & value.[:m:];
					const std::streampos end_at = m_stream.tellp();
					length = static_cast<std::uint32_t>(end_at - body_at);
					m_stream.seekp(length_at);
					m_stream.write(reinterpret_cast<const char*>(&length), sizeof(length));
					m_stream.seekp(end_at);
				}
			}
		}
		return *this;
	}
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
	*this& count;
	for (const auto& item : vec) {
		*this& item;
	}
	return *this;
}

template <typename T, std::size_t N>
auto gse::binary_writer::operator&(const std::inplace_vector<T, N>& vec) -> binary_writer& {
	const auto count = static_cast<std::uint32_t>(vec.size());
	*this& count;
	for (const auto& item : vec) {
		*this& item;
	}
	return *this;
}

template <typename... Ts>
auto gse::binary_writer::operator&(const std::variant<Ts...>& var) -> binary_writer& {
	assert(!var.valueless_by_exception(), "cannot serialize a valueless variant");
	auto index = static_cast<std::uint32_t>(var.index());
	*this& index;
	std::visit(
		[this](const auto& value) {
			*this& value;
		},
		var
	);
	return *this;
}

template <typename T>
auto gse::binary_writer::operator&(const std::optional<T>& opt) -> binary_writer& {
	const bool has = opt.has_value();
	*this& has;
	if (has) {
		*this&* opt;
	}
	return *this;
}

template <typename K, typename V>
auto gse::binary_writer::operator&(const std::unordered_map<K, V>& map) -> binary_writer& {
	const auto count = static_cast<std::uint32_t>(map.size());
	*this& count;
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

template <typename T>
requires(!std::is_trivially_copyable_v<T> && !gse::archive_schema_type<T>)
auto gse::binary_writer::operator&(const T& value) -> binary_writer& {
	if constexpr (has_user_serialize<binary_writer, T>) {
		serialize(*this, const_cast<T&>(value));
	}
	else {
		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
			if constexpr (!is_archive_skipped(m)) {
				*this& value.[:m:];
			}
		}
	}
	return *this;
}

gse::binary_reader::binary_reader(std::ifstream& stream) : m_stream(stream) {
}

gse::binary_reader::binary_reader(std::ifstream& stream, const std::uint32_t expected_magic, const std::uint32_t expected_version, std::string_view path, const std::source_location& loc)
	: m_stream(stream) {
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	std::uint32_t epoch = 0;
	*this & magic & version & epoch;
	m_valid = (magic == expected_magic && version == expected_version && epoch == archive_format_epoch);
	gse::assert(m_valid, loc, "Invalid or outdated baked file: {}", path);
}

auto gse::binary_reader::open(std::ifstream& stream, const std::uint32_t expected_magic, const std::uint32_t expected_version) -> std::expected<binary_reader, archive_mismatch> {
	binary_reader reader(stream);
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	std::uint32_t epoch = 0;
	reader & magic & version & epoch;

	if (!stream.good()) {
		return std::unexpected(archive_mismatch{});
	}
	if (magic != expected_magic || version != expected_version || epoch != archive_format_epoch) {
		return std::unexpected(archive_mismatch{
			.magic = magic,
			.version = version,
			.epoch = epoch,
			.readable = true,
		});
	}
	return reader;
}

auto gse::binary_reader::valid() const -> bool {
	return m_valid;
}

template <typename T>
requires(std::is_trivially_copyable_v<T> && !gse::archive_schema_type<T>)
auto gse::binary_reader::operator&(T& value) -> binary_reader& {
	m_stream.read(reinterpret_cast<char*>(&value), sizeof(T));
	return *this;
}

auto gse::binary_reader::skip_field(const archive_field& field) -> void {
	if (field.size != 0) {
		m_stream.seekg(field.size, std::ios::cur);
		return;
	}
	std::uint32_t length = 0;
	m_stream.read(reinterpret_cast<char*>(&length), sizeof(length));
	m_stream.seekg(length, std::ios::cur);
}

template <typename T>
auto gse::binary_reader::read_schema() -> const std::vector<archive_field>& {
	const std::uint64_t key = archive_type_id<T>();
	if (const auto found = m_schemas.find(key); found != m_schemas.end()) {
		return found->second;
	}

	std::uint32_t count = 0;
	m_stream.read(reinterpret_cast<char*>(&count), sizeof(count));

	std::vector<archive_field> fields(count);
	for (archive_field& field : fields) {
		*this & field.name & field.type & field.size;
	}
	return m_schemas.emplace(key, std::move(fields)).first->second;
}

template <gse::archive_schema_type T>
auto gse::binary_reader::operator&(T& value) -> binary_reader& {
	if constexpr (has_user_serialize<binary_reader, T>) {
		serialize(*this, value);
		return *this;
	}
	else {
		const std::vector<archive_field> schema = read_schema<T>();

		for (const archive_field& field : schema) {
			bool matched = false;
			template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
				if constexpr (!is_archive_skipped(m)) {
					if (!matched
						&& field.name == std::meta::identifier_of(m)
						&& field.type == meta::qualified_name<typename [:std::meta::type_of(m):]>()
						&& field.size == archive_field_size<typename [:std::meta::type_of(m):]>()) {
						matched = true;
						if constexpr (archive_field_size<typename [:std::meta::type_of(m):]>() == 0) {
							std::uint32_t length = 0;
							m_stream.read(reinterpret_cast<char*>(&length), sizeof(length));
						}
						*this & value.[:m:];
					}
				}
			}
			if (!matched) {
				skip_field(field);
			}
		}
		return *this;
	}
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
	*this& count;
	vec.resize(count);
	for (auto& item : vec) {
		*this& item;
	}
	return *this;
}

template <typename T, std::size_t N>
auto gse::binary_reader::operator&(std::inplace_vector<T, N>& vec) -> binary_reader& {
	std::uint32_t count = 0;
	*this& count;
	gse::assert(count <= N, "inplace_vector deserialization: count {} exceeds capacity {}", count, N);
	vec.clear();
	for (std::uint32_t i = 0; i < count; ++i) {
		T val{};
		*this& val;
		vec.push_back(std::move(val));
	}
	return *this;
}

template <typename... Ts>
auto gse::binary_reader::operator&(std::variant<Ts...>& var) -> binary_reader& {
	std::uint32_t index = 0;
	*this& index;

	[&]<std::size_t... Is>(std::index_sequence<Is...>) {
		(
			[&] {
				if (Is != index) {
					return;
				}
				std::variant_alternative_t<Is, std::variant<Ts...>> value{};
				*this& value;
				var = std::move(value);
			}(),
			...
		);
	}(std::index_sequence_for<Ts...>{});

	return *this;
}

template <typename T>
auto gse::binary_reader::operator&(std::optional<T>& opt) -> binary_reader& {
	bool has = false;
	*this& has;
	if (has) {
		T val{};
		*this& val;
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
	*this& count;
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

template <typename T>
requires(!std::is_trivially_copyable_v<T> && !gse::archive_schema_type<T>)
auto gse::binary_reader::operator&(T& value) -> binary_reader& {
	if constexpr (has_user_serialize<binary_reader, T>) {
		serialize(*this, value);
	}
	else {
		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
			if constexpr (!is_archive_skipped(m)) {
				*this& value.[:m:];
			}
		}
	}
	return *this;
}

template <typename T>
auto gse::serialize(binary_writer& ar, raw_blob_owned<T>& v) -> void {
	ar& raw_blob<T>{ v.storage };
}

template <typename T>
auto gse::serialize(binary_reader& ar, raw_blob_owned<T>& v) -> void {
	ar& raw_blob<T>{ v.storage };
}
