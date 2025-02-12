export module gse.core.timed_lock;

import std;

import gse.core.main_clock;
import gse.physics.math.units.duration;

export namespace gse {
	template <typename T>
	class timed_lock {
	public:
		timed_lock() = default;

		timed_lock(const T& value, const time duration) : m_value(value), m_duration(duration) {}

		template <typename... Args>
		timed_lock(const time duration, Args&&... args) : m_value(std::forward<Args>(args)...), m_duration(duration) {}

		// Updates m_value only if the cooldown has expired.
		// If the cooldown is still active, the assignment is silently ignored.
		auto operator=(const T& value) -> timed_lock&;

		// Returns a mutable pointer to the value if the cooldown has expired.
		// If still cooling down, returns nullptr.
		// Note: calling this function resets the cooldown timer upon a successful retrieval.
		auto try_get_mutable_value() -> T*;

		// Always returns the value (read-only).
		auto get_value() const -> const T*;

		// Returns true if the value is currently mutable (i.e. the cooldown period has expired).
		auto is_value_mutable() const -> bool;

	protected:
		T m_value;
		time m_duration;
		time m_start_time = main_clock::get_current_time();
	};

	struct unlockable_timed_lock : timed_lock<bool> {
		unlockable_timed_lock(const time duration) : timed_lock(duration, false) {}
		auto unlock() -> void {
			m_value = true;
		}
	};
}

template <typename T>
auto gse::timed_lock<T>::operator=(const T& value) -> timed_lock& {
	const auto now = main_clock::get_current_time();

	if (now - m_start_time < m_duration) {
		return *this;
	}

	m_value = value;
	m_start_time = now;

	return *this;
}

template <typename T>
auto gse::timed_lock<T>::try_get_mutable_value() -> T* {
	const auto now = main_clock::get_current_time();
	if (now - m_start_time < m_duration) {
		return nullptr;
	}

	m_start_time = now;
	return &m_value;
}

template <typename T>
auto gse::timed_lock<T>::get_value() const -> const T* {
	return &m_value;
}

template <typename T>
auto gse::timed_lock<T>::is_value_mutable() const -> bool {
	const auto now = main_clock::get_current_time();
	return now - m_start_time >= m_duration;
}