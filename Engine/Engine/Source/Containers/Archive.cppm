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

	consteval auto archive_schema_hash_mix(
		std::uint64_t hash,
		std::string_view text
	) -> std::uint64_t;

	template <typename T>
	struct archive_schema_fingerprint_impl {
		static consteval auto accumulate(
			std::uint64_t hash
		) -> std::uint64_t;
	};

	template <>
	struct archive_schema_fingerprint_impl<std::string> {
		static consteval auto accumulate(
			std::uint64_t hash
		) -> std::uint64_t;
	};

	template <typename T>
	struct archive_schema_fingerprint_impl<std::vector<T>> {
		static consteval auto accumulate(
			std::uint64_t hash
		) -> std::uint64_t;
	};

	template <typename T, std::size_t N>
	struct archive_schema_fingerprint_impl<std::inplace_vector<T, N>> {
		static consteval auto accumulate(
			std::uint64_t hash
		) -> std::uint64_t;
	};

	template <typename T>
	struct archive_schema_fingerprint_impl<std::optional<T>> {
		static consteval auto accumulate(
			std::uint64_t hash
		) -> std::uint64_t;
	};

	template <typename... Ts>
	struct archive_schema_fingerprint_impl<std::variant<Ts...>> {
		static consteval auto accumulate(
			std::uint64_t hash
		) -> std::uint64_t;
	};

	template <typename K, typename V>
	struct archive_schema_fingerprint_impl<std::unordered_map<K, V>> {
		static consteval auto accumulate(
			std::uint64_t hash
		) -> std::uint64_t;
	};

	template <typename T>
	consteval auto schema_fingerprint() -> std::uint64_t;

	consteval auto is_archive_skipped(
		std::meta::info member
	) -> bool;

	struct binary_writer {
		static constexpr bool is_writing = true;

		explicit binary_writer(
			std::ostream& stream
		);

		binary_writer(
			std::ostream& stream,
			std::uint32_t magic,
			std::uint32_t version
		);

		[[nodiscard]] auto valid() const -> bool;

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

		std::ostream& m_stream;
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

		[[nodiscard]] auto skipped_fields() const -> std::span<const std::string>;

	private:
		auto read_bytes(
			void* data,
			std::uint64_t size
		) -> bool;

		auto skip_bytes(
			std::uint64_t size
		) -> bool;

		template <typename T>
		auto read_schema() -> const std::vector<archive_field>&;

		auto skip_field(
			const archive_field& field
		) -> void;

		auto note_skipped_field(
			std::string_view type_name,
			const archive_field& field
		) -> void;

		std::ifstream& m_stream;
		std::unordered_map<std::uint64_t, std::vector<archive_field>> m_schemas;
		std::vector<std::string> m_skipped_fields;
		std::uint64_t m_remaining = 0;
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

	auto serialize(
		binary_writer& ar,
		std::filesystem::path& v
	) -> void;

	auto serialize(
		binary_reader& ar,
		std::filesystem::path& v
	) -> void;
}

consteval auto gse::is_archive_raw(const std::meta::info type) -> bool {
	return has_annotation<archive_raw>(type);
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
	return has_annotation<archive_skip>(member);
}

gse::binary_writer::binary_writer(std::ostream& stream) : m_stream(stream) {
}

gse::binary_writer::binary_writer(std::ostream& stream, const std::uint32_t magic, const std::uint32_t version)
	: m_stream(stream) {
	*this & magic & version & archive_format_epoch;
}

auto gse::binary_writer::valid() const -> bool {
	return m_stream.good();
}

template <typename T>
requires(std::is_trivially_copyable_v<T> && !gse::archive_schema_type<T>)
auto gse::binary_writer::operator&(const T& value) -> binary_writer& {
	if constexpr (std::same_as<T, bool>) {
		const std::uint8_t encoded = value ? 1 : 0;
		m_stream.write(reinterpret_cast<const char*>(&encoded), sizeof(encoded));
	}
	else {
		m_stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}
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
	const auto current = m_stream.tellg();
	m_stream.seekg(0, std::ios::end);
	const auto end = m_stream.tellg();
	m_stream.seekg(current);
	if (current < 0 || end < current) {
		m_valid = false;
		return;
	}
	m_remaining = static_cast<std::uint64_t>(end - current);
}

gse::binary_reader::binary_reader(std::ifstream& stream, const std::uint32_t expected_magic, const std::uint32_t expected_version, std::string_view path, const std::source_location& loc)
	: binary_reader(stream) {
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	std::uint32_t epoch = 0;
	*this & magic & version & epoch;
	m_valid = m_valid && magic == expected_magic && version == expected_version && epoch == archive_format_epoch;
	gse::assert(m_valid, loc, "Invalid or outdated baked file: {}", path);
}

auto gse::binary_reader::open(std::ifstream& stream, const std::uint32_t expected_magic, const std::uint32_t expected_version) -> std::expected<binary_reader, archive_mismatch> {
	binary_reader reader(stream);
	std::uint32_t magic = 0;
	std::uint32_t version = 0;
	std::uint32_t epoch = 0;
	reader & magic & version & epoch;

	if (!reader.valid()) {
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

auto gse::binary_reader::read_bytes(void* const data, const std::uint64_t size) -> bool {
	if (!m_valid || size > m_remaining || size > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
		m_valid = false;
		return false;
	}
	if (size != 0) {
		m_stream.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
	}
	if (!m_stream.good()) {
		m_valid = false;
		return false;
	}
	m_remaining -= size;
	return true;
}

auto gse::binary_reader::skip_bytes(const std::uint64_t size) -> bool {
	if (!m_valid || size > m_remaining || size > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
		m_valid = false;
		return false;
	}
	m_stream.seekg(static_cast<std::streamoff>(size), std::ios::cur);
	if (!m_stream.good()) {
		m_valid = false;
		return false;
	}
	m_remaining -= size;
	return true;
}

template <typename T>
requires(std::is_trivially_copyable_v<T> && !gse::archive_schema_type<T>)
auto gse::binary_reader::operator&(T& value) -> binary_reader& {
	if constexpr (std::same_as<T, bool>) {
		std::uint8_t encoded = 0;
		read_bytes(&encoded, sizeof(encoded));
		if (encoded > 1) {
			m_valid = false;
			value = false;
		}
		else {
			value = encoded != 0;
		}
	}
	else {
		read_bytes(&value, sizeof(T));
	}
	return *this;
}

auto gse::binary_reader::skip_field(const archive_field& field) -> void {
	if (field.size != 0) {
		skip_bytes(field.size);
		return;
	}
	std::uint32_t length = 0;
	read_bytes(&length, sizeof(length));
	skip_bytes(length);
}

template <typename T>
auto gse::binary_reader::read_schema() -> const std::vector<archive_field>& {
	const std::uint64_t key = archive_type_id<T>();
	if (const auto found = m_schemas.find(key); found != m_schemas.end()) {
		return found->second;
	}

	std::uint32_t count = 0;
	read_bytes(&count, sizeof(count));
	if (!m_valid || count > m_remaining) {
		m_valid = false;
		count = 0;
	}

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
							read_bytes(&length, sizeof(length));
						}
						*this & value.[:m:];
					}
				}
			}
			if (!matched) {
				note_skipped_field(meta::qualified_name<T>(), field);
				skip_field(field);
			}
		}
		return *this;
	}
}

auto gse::binary_reader::operator&(std::string& str) -> binary_reader& {
	std::uint32_t size = 0;
	read_bytes(&size, sizeof(size));
	if (!m_valid || size > m_remaining) {
		m_valid = false;
		str.clear();
		return *this;
	}
	str.resize(size);
	read_bytes(str.data(), size);
	return *this;
}

template <typename T>
auto gse::binary_reader::operator&(std::vector<T>& vec) -> binary_reader& {
	std::uint32_t count = 0;
	*this& count;
	if (!m_valid || count > m_remaining + 1) {
		m_valid = false;
		vec.clear();
		return *this;
	}
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
	if (!m_valid || count > N || count > m_remaining + 1) {
		m_valid = false;
		vec.clear();
		return *this;
	}
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
	if (!m_valid || index >= sizeof...(Ts)) {
		m_valid = false;
		return *this;
	}

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
	if (!m_valid || count > m_remaining + 1) {
		m_valid = false;
		map.clear();
		return *this;
	}
	map.clear();
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
	read_bytes(&byte_size, sizeof(byte_size));
	if (!m_valid || byte_size > m_remaining || byte_size % sizeof(T) != 0) {
		m_valid = false;
		blob.data.clear();
		return *this;
	}
	blob.data.resize(byte_size / sizeof(T));
	read_bytes(blob.data.data(), byte_size);
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

auto gse::serialize(binary_writer& ar, std::filesystem::path& v) -> void {
	std::string text = v.generic_native_encoded_string();
	ar & text;
}

auto gse::serialize(binary_reader& ar, std::filesystem::path& v) -> void {
	std::string text;
	ar & text;
	v = std::move(text);
}

consteval auto gse::archive_schema_hash_mix(std::uint64_t hash, const std::string_view text) -> std::uint64_t {
	for (const char c : text) {
		hash = (hash ^ static_cast<std::uint8_t>(c)) * 1099511628211ull;
	}
	return hash;
}

template <typename T>
consteval auto gse::archive_schema_fingerprint_impl<T>::accumulate(std::uint64_t hash) -> std::uint64_t {
	if constexpr (has_user_serialize<binary_reader, T>) {
		return hash;
	}
	else if constexpr (archive_schema_type<T> || (std::is_class_v<T> && !std::is_trivially_copyable_v<T>)) {
		template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
			if constexpr (!is_archive_skipped(m)) {
				hash = archive_schema_hash_mix(hash, std::meta::identifier_of(m));
				hash = archive_schema_hash_mix(hash, meta::qualified_name<typename [:std::meta::type_of(m):]>());
				hash = archive_schema_fingerprint_impl<typename [:std::meta::type_of(m):]>::accumulate(hash);
			}
		}
		return hash;
	}
	else {
		return hash;
	}
}

consteval auto gse::archive_schema_fingerprint_impl<std::string>::accumulate(const std::uint64_t hash) -> std::uint64_t {
	return hash;
}

template <typename T>
consteval auto gse::archive_schema_fingerprint_impl<std::vector<T>>::accumulate(const std::uint64_t hash) -> std::uint64_t {
	return archive_schema_fingerprint_impl<T>::accumulate(hash);
}

template <typename T, std::size_t N>
consteval auto gse::archive_schema_fingerprint_impl<std::inplace_vector<T, N>>::accumulate(const std::uint64_t hash) -> std::uint64_t {
	return archive_schema_fingerprint_impl<T>::accumulate(hash);
}

template <typename T>
consteval auto gse::archive_schema_fingerprint_impl<std::optional<T>>::accumulate(const std::uint64_t hash) -> std::uint64_t {
	return archive_schema_fingerprint_impl<T>::accumulate(hash);
}

template <typename... Ts>
consteval auto gse::archive_schema_fingerprint_impl<std::variant<Ts...>>::accumulate(std::uint64_t hash) -> std::uint64_t {
	((hash = archive_schema_fingerprint_impl<Ts>::accumulate(hash)), ...);
	return hash;
}

template <typename K, typename V>
consteval auto gse::archive_schema_fingerprint_impl<std::unordered_map<K, V>>::accumulate(const std::uint64_t hash) -> std::uint64_t {
	return archive_schema_fingerprint_impl<V>::accumulate(archive_schema_fingerprint_impl<K>::accumulate(hash));
}

template <typename T>
consteval auto gse::schema_fingerprint() -> std::uint64_t {
	return archive_schema_fingerprint_impl<T>::accumulate(archive_schema_hash_mix(14695981039346656037ull, meta::qualified_name<T>()));
}

auto gse::binary_reader::skipped_fields() const -> std::span<const std::string> {
	return m_skipped_fields;
}

auto gse::binary_reader::note_skipped_field(const std::string_view type_name, const archive_field& field) -> void {
	auto entry = std::format("{}.{} stored as {} (size {})", type_name, field.name, field.type, field.size);
	if (std::ranges::find(m_skipped_fields, entry) == m_skipped_fields.end()) {
		m_skipped_fields.push_back(std::move(entry));
	}
}
