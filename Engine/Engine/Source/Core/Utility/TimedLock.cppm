export module gse.utility:timed_lock;

import std;

import :clock;

import gse.physics.math;

export namespace gse {
    template <typename T>
    class timed_lock {
    public:
        virtual ~timed_lock() = default;
        timed_lock() = default;

        timed_lock(
            const T& value, 
            time duration
        );

        template <typename... Args>
        timed_lock(
            time duration, 
            Args&&... args
        );

        // If the cooldown has expired, assign the value and reset the internal clock.
        virtual auto operator=(
            const T& value
        ) -> timed_lock&;

        // If the cooldown has expired, reset the internal clock and return a mutable pointer.
        virtual auto try_get_mutable_value(
        ) -> T*;

        // Always returns the value (read-only).
        auto value(
        ) const -> const T& ;

        // Returns true if the cooldown period has expired.
        auto value_mutable(
        ) const -> bool;
    protected:
        T m_value;
        time m_duration;
        clock m_clock;
    };
}

template <typename T>
gse::timed_lock<T>::timed_lock(const T& value, const time duration) : m_value(value), m_duration(duration) {}

template <typename T>
template <typename ... Args>
gse::timed_lock<T>::timed_lock(const time duration, Args&&... args): m_value(std::forward<Args>(args)), m_duration(duration) {}

template <typename T>
auto gse::timed_lock<T>::operator=(const T& value) -> timed_lock& {
	if (m_clock.elapsed() >= m_duration) {
		m_value = value;
		m_clock.reset();
	}
	return *this;
}

template <typename T>
auto gse::timed_lock<T>::try_get_mutable_value() -> T* {
	if (m_clock.elapsed() >= m_duration) {
		m_clock.reset();
		return &m_value;
	}
	return nullptr;
}

template <typename T>
auto gse::timed_lock<T>::value() const -> const T& {
	return m_value;
}

template <typename T>
auto gse::timed_lock<T>::value_mutable() const -> bool {
	return m_clock.elapsed() >= m_duration;
}

export namespace gse {
    template <typename T, int N>
    class quota_timed_lock final : public timed_lock<T> {
        using timed_lock<T>::timed_lock;
    public:
        // Uses quota or, if quota is exhausted but the cooldown has expired,
        // resets the quota and then updates the value.
        auto operator=(
            const T& value
        ) -> quota_timed_lock& override;

        // Returns a mutable pointer if quota is available or, if quota is exhausted,
        // the cooldown has expired (in which case the quota resets).
        auto try_get_mutable_value(
        ) -> T* override;
    private:
        int m_quota = N;
    };
}

template <typename T, int N>
auto gse::quota_timed_lock<T, N>::operator=(const T& value) -> gse::quota_timed_lock<T, N>& {
    if (m_quota > 0) {
        m_quota--;
        this->m_value = value;
        this->m_clock.reset();
    }
    else if (this->m_clock.elapsed() >= this->m_duration) {
        m_quota = N - 1;  // reset quota (using one use for the current assignment)
        this->m_value = value;
        this->m_clock.reset();
    }
    return *this;
}

template <typename T, int N>
auto gse::quota_timed_lock<T, N>::try_get_mutable_value() -> T* {
	if (m_quota > 0) {
		m_quota--;
		this->m_clock.reset();
		return &this->m_value;
	}

	if (this->m_clock.elapsed() >= this->m_duration) {
		m_quota = N - 1;
		this->m_clock.reset();
		return &this->m_value;
	}

	return nullptr;
}
