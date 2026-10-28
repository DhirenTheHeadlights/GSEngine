export module gse.concurrency:channel_base;

import std;

import gse.assert;
import gse.core;
import gse.containers;
import gse.meta;
import gse.time;

export namespace gse {
	struct same_frame_channel_tag {};
	constexpr same_frame_channel_tag same_frame_channel{};

	template <fixed_string Panel>
	struct reveals {
		static constexpr std::string_view value = Panel;
	};

	struct reveal_quiet_tag {};
	constexpr reveal_quiet_tag reveal_quiet{};

	template <typename T>
	consteval auto reveal_panel_of() -> std::string_view;

	template <typename T>
	consteval auto reveal_quiet_member() -> std::meta::info;

	template <typename T>
	auto reveal_quiet_of(
		const T& entry
	) -> bool;

	template <typename T>
	auto channel_pending_reveal(
		const std::vector<T>& entries
	) -> std::string_view;

	template <typename T>
	class channel {
	public:
		class reader {
		public:
			explicit reader(
				const std::vector<T>* data
			);

			auto begin() const -> std::vector<T>::const_iterator;

			auto end() const -> std::vector<T>::const_iterator;

			[[nodiscard]] auto size() const -> std::size_t;

			[[nodiscard]] auto empty() const -> bool;

			auto operator[](
				std::size_t i
			) const -> const T&;

		private:
			const std::vector<T>* m_data;
		};

		using value_type = T;

		channel();

		auto read() const -> reader;

		auto read_raw() const -> const std::vector<T>&;

		auto push(
			T item
		) -> void;

		template <typename... Args>
		auto emplace(
			Args&&... args
		) -> T&;

		auto flip() -> void;

	private:
		double_buffer<std::vector<T>> m_buffer;
		mutable std::mutex m_write_mutex;
	};

	struct channel_base {
		virtual ~channel_base() = default;

		virtual auto flip() -> void = 0;

		[[nodiscard]] virtual auto pending_reveal() const -> std::string_view {
			return {};
		}
	};

	template <typename T>
	struct typed_channel final : channel_base {
		channel<T> data;

		auto flip() -> void override {
			data.flip();
		}

		[[nodiscard]] auto pending_reveal() const -> std::string_view override {
			return channel_pending_reveal<T>(data.read_raw());
		}
	};

	template <typename T>
	constexpr bool is_same_frame_channel_v = has_annotation<same_frame_channel_tag>(^^T);

	template <typename T>
	struct same_frame_typed_channel final : channel_base {
		std::vector<T> data;
		mutable std::mutex mutex;

		auto push(
			T item
		) -> void;

		auto flip() -> void override;

		auto drain() -> std::vector<T>;

		[[nodiscard]] auto pending_reveal() const -> std::string_view override;
	};

	template <typename T>
	class channel_read_guard : non_copyable {
	public:
		explicit channel_read_guard(
			const std::vector<T>& data
		);

		auto operator[](
			std::size_t i
		) const -> const T&;

		[[nodiscard]] auto empty() const -> bool;

		[[nodiscard]] auto size() const -> std::size_t;

		auto begin() const -> std::vector<T>::const_iterator;

		auto end() const -> std::vector<T>::const_iterator;

		auto front() const -> const T&;

	private:
		const std::vector<T>* m_data;
	};
}

template <typename T>
consteval auto gse::reveal_panel_of() -> std::string_view {
	constexpr auto anno = meta::find_class_template_annotation(^^T, ^^reveals);
	if constexpr (anno != std::meta::info{}) {
		using anno_t = [:anno:];
		return anno_t::value;
	}
	else {
		return std::string_view{};
	}
}

template <typename T>
consteval auto gse::reveal_quiet_member() -> std::meta::info {
	for (const auto m : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())) {
		if (has_annotation<reveal_quiet_tag>(m)) {
			return m;
		}
	}
	return std::meta::info{};
}

template <typename T>
auto gse::reveal_quiet_of(const T& entry) -> bool {
	constexpr auto member = reveal_quiet_member<T>();
	if constexpr (member == std::meta::info{}) {
		return false;
	}
	else {
		using member_t = std::remove_cvref_t<decltype(entry.[:member:])>;
		static_assert(
			std::is_same_v<member_t, std::string> || std::is_same_v<member_t, bool>,
			"[[= gse::reveal_quiet]] marks the member that suppresses a reveal, and only a std::string (quiet when non-empty) or a bool (quiet when true) says that; give the request one of those or drop the annotation"
		);
		if constexpr (std::is_same_v<member_t, std::string>) {
			return !entry.[:member:].empty();
		}
		else {
			return entry.[:member:];
		}
	}
}

template <typename T>
auto gse::channel_pending_reveal(const std::vector<T>& entries) -> std::string_view {
	constexpr std::string_view panel = reveal_panel_of<T>();
	if constexpr (panel.empty()) {
		return {};
	}
	else {
		for (const T& entry : entries) {
			if (!reveal_quiet_of(entry)) {
				return panel;
			}
		}
		return {};
	}
}

template <typename T>
gse::channel<T>::reader::reader(const std::vector<T>* data) : m_data(data) {
}

template <typename T>
auto gse::channel<T>::reader::begin() const -> std::vector<T>::const_iterator {
	return m_data->begin();
}

template <typename T>
auto gse::channel<T>::reader::end() const -> std::vector<T>::const_iterator {
	return m_data->end();
}

template <typename T>
auto gse::channel<T>::reader::size() const -> std::size_t {
	return m_data->size();
}

template <typename T>
auto gse::channel<T>::reader::empty() const -> bool {
	return m_data->empty();
}

template <typename T>
auto gse::channel<T>::reader::operator[](std::size_t i) const -> const T& {
	return (*m_data)[i];
}

template <typename T>
gse::channel<T>::channel() {
	frame_sync::on_end([this] -> auto {
		flip();
	});
}

template <typename T>
auto gse::channel<T>::read() const -> reader {
	return reader(&m_buffer.read());
}

template <typename T>
auto gse::channel<T>::read_raw() const -> const std::vector<T>& {
	return m_buffer.read();
}

template <typename T>
auto gse::channel<T>::push(T item) -> void {
	std::lock_guard lock(m_write_mutex);
	m_buffer.write().push_back(std::move(item));
}

template <typename T>
template <typename... Args>
auto gse::channel<T>::emplace(Args&&... args) -> T& {
	std::lock_guard lock(m_write_mutex);
	return m_buffer.write().emplace_back(std::forward<Args>(args)...);
}

template <typename T>
auto gse::channel<T>::flip() -> void {
	std::lock_guard lock(m_write_mutex);
	m_buffer.flip();
	m_buffer.write().clear();
}

template <typename T>
auto gse::same_frame_typed_channel<T>::push(T item) -> void {
	std::lock_guard lock(mutex);
	data.push_back(std::move(item));
}

template <typename T>
auto gse::same_frame_typed_channel<T>::flip() -> void {
	std::lock_guard lock(mutex);
	data.clear();
}

template <typename T>
auto gse::same_frame_typed_channel<T>::drain() -> std::vector<T> {
	std::lock_guard lock(mutex);
	return std::exchange(
		data,
		{}
	);
}

template <typename T>
auto gse::same_frame_typed_channel<T>::pending_reveal() const -> std::string_view {
	std::lock_guard lock(mutex);
	return channel_pending_reveal<T>(data);
}

template <typename T>
gse::channel_read_guard<T>::channel_read_guard(const std::vector<T>& data) : m_data(&data) {
}

template <typename T>
auto gse::channel_read_guard<T>::operator[](const std::size_t i) const -> const T& {
	return (*m_data)[i];
}

template <typename T>
auto gse::channel_read_guard<T>::empty() const -> bool {
	return m_data->empty();
}

template <typename T>
auto gse::channel_read_guard<T>::size() const -> std::size_t {
	return m_data->size();
}

template <typename T>
auto gse::channel_read_guard<T>::begin() const -> std::vector<T>::const_iterator {
	return m_data->begin();
}

template <typename T>
auto gse::channel_read_guard<T>::end() const -> std::vector<T>::const_iterator {
	return m_data->end();
}

template <typename T>
auto gse::channel_read_guard<T>::front() const -> const T& {
	assert(!m_data->empty(), "Attempted to access front of empty channel read guard");
	return (*m_data)[0];
}
