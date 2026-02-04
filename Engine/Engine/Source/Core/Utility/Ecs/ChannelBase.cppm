export module gse.utility:channel_base;

import std;

import :n_buffer;
import :frame_sync;
import :per_frame_resource;
import :non_copyable;

export namespace gse {
	template <typename T>
	class channel {
	public:
		class reader {
		public:
			explicit reader(
				const std::vector<T>* data
			);

			auto begin(
			) const -> std::vector<T>::const_iterator;

			auto end(
			) const -> std::vector<T>::const_iterator;

			auto size(
			) const -> std::size_t;

			auto empty(
			) const -> bool;

			auto operator[](
				std::size_t i
			) const -> const T&;

		private:
			const std::vector<T>* m_data;
		};

		using value_type = T;

		channel();

		auto read(
		) const -> reader;

		auto read_raw(
		) const -> const std::vector<T>&;

		auto push(
			T item
		) -> void;

		template <typename... Args>
		auto emplace(
			Args&&... args
		) -> T&;

		auto flip(
		) -> void;

	private:
		double_buffer<std::vector<T>> m_buffer;
		mutable std::mutex m_write_mutex;
	};

	struct channel_base {
		virtual ~channel_base() = default;
		virtual auto take_snapshot() -> void = 0;
		virtual auto snapshot_data() const -> const void* = 0;
		virtual auto push_any(std::any item) -> void = 0;
	};

	template <typename T>
	struct typed_channel final : channel_base {
		channel<T> data;

		auto take_snapshot(
		) -> void override {}

		auto snapshot_data(
		) const -> const void* override {
			return &data.read_raw();
		}

		auto push_any(
			std::any item
		) -> void override {
			if (auto* ptr = std::any_cast<T>(&item)) {
				data.push(std::move(*ptr));
			}
		}
	};

	using channel_factory_fn = std::unique_ptr<channel_base>(*)();

	template <typename T>
	class channel_read_guard : non_copyable {
	public:
		explicit channel_read_guard(
			const std::vector<T>& data
		);

		~channel_read_guard(
		) override;

		channel_read_guard(
			channel_read_guard&& other
		) noexcept;

		auto operator=(
			channel_read_guard&&
		) -> channel_read_guard& = delete;

		auto operator[](
			std::size_t i
		) const -> const T&;

		auto empty(
		) const -> bool;

		auto size(
		) const -> std::size_t;

		auto begin(
		) const -> std::vector<T>::const_iterator;

		auto end(
		) const -> std::vector<T>::const_iterator;
	private:
		const std::vector<T>* m_data;
		const std::uint32_t* m_previous_expected = nullptr;
		bool m_owns_scope = false;
	};
}

template <typename T>
gse::channel<T>::reader::reader(const std::vector<T>* data)
	: m_data(data) {}

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
	frame_sync::on_end([this] {
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
gse::channel_read_guard<T>::channel_read_guard(const std::vector<T>& data) : m_data(&data) {
	if constexpr (requires { data[0].frame_index; }) {
		if (!data.empty()) {
			m_previous_expected = expected_frame_index;
			expected_frame_index = &data[0].frame_index;
			m_owns_scope = true;
		}
	}
}

template <typename T>
gse::channel_read_guard<T>::~channel_read_guard() {
	if (m_owns_scope) {
		expected_frame_index = m_previous_expected;
	}
}

template <typename T>
gse::channel_read_guard<T>::channel_read_guard(channel_read_guard&& other) noexcept : m_data(other.m_data), m_previous_expected(other.m_previous_expected), m_owns_scope(other.m_owns_scope) {
	other.m_owns_scope = false;
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
