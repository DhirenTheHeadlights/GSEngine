export module gse.concurrency:pending;

import gse.core;
import std;

import :task;

export namespace gse::task {
	template <typename T>
	class pending : non_copyable {
	public:
		pending() = default;

		~pending();

		pending(
			pending&&
		) noexcept = default;

		auto operator=(
			pending&&
		) noexcept -> pending& = default;

		template <typename F>
		auto start(
			F&& fn,
			id label,
			lane target
		) -> void;

		[[nodiscard]] auto active() const -> bool;

		[[nodiscard]] auto take() -> std::optional<T>;

		auto cancel() -> void;

	private:
		struct control {
			std::atomic<bool> done = false;
			std::stop_source stop;
			T value;
		};

		std::shared_ptr<control> m_control;
	};
}

template <typename T>
gse::task::pending<T>::~pending() {
	cancel();
}

template <typename T>
template <typename F>
auto gse::task::pending<T>::start(F&& fn, const id label, const lane target) -> void {
	cancel();
	m_control = std::make_shared<control>();
	job j = [block = m_control, body = std::forward<F>(fn)]() mutable {
		if constexpr (std::invocable<std::decay_t<F>&, std::stop_token>) {
			block->value = body(block->stop.get_token());
		}
		else {
			block->value = body();
		}
		block->done.store(true, std::memory_order_release);
	};
	switch (target) {
		case lane::worker:
			post(std::move(j), label);
			break;
		case lane::io:
			post_io(std::move(j), label);
			break;
		case lane::background:
			post_background(std::move(j), label);
			break;
	}
}

template <typename T>
auto gse::task::pending<T>::active() const -> bool {
	return m_control != nullptr;
}

template <typename T>
auto gse::task::pending<T>::take() -> std::optional<T> {
	if (!m_control || !m_control->done.load(std::memory_order_acquire)) {
		return std::nullopt;
	}
	std::optional<T> value(std::move(m_control->value));
	m_control.reset();
	return value;
}

template <typename T>
auto gse::task::pending<T>::cancel() -> void {
	if (m_control) {
		m_control->stop.request_stop();
		m_control.reset();
	}
}