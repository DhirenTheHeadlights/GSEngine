export module gse.core.timed_lock;

import std;
import gse.core.main_clock;
import gse.physics.math.units.duration;

export namespace gse {

    template <typename T>
    class timed_lock {
    public:
        virtual ~timed_lock() = default;
        timed_lock() = default;

        timed_lock(const T& value, const time duration)
            : m_value(value), m_duration(duration) {
        }

        template <typename... Args>
        timed_lock(const time duration, Args&&... args)
            : m_value(std::forward<Args>(args)...), m_duration(duration) {
        }

        // If the cooldown has expired, assign the value and update the timer.
        virtual auto operator=(const T& value) -> timed_lock& {
	        if (const auto now = main_clock::get_current_time(); now - m_start_time >= m_duration) {
                m_value = value;
                m_start_time = now;
            }
            return *this;
        }

        // If the cooldown has expired, reset the timer and return a mutable pointer.
        virtual auto try_get_mutable_value() -> T* {
	        if (const auto now = main_clock::get_current_time(); now - m_start_time >= m_duration) {
                m_start_time = now;
                return &m_value;
            }
            return nullptr;
        }

        // Always returns the value (read-only).
        auto get_value() const -> const T& {
            return m_value;
        }

        // Returns true if the cooldown period has expired.
        auto is_value_mutable() const -> bool {
            return main_clock::get_current_time() - m_start_time >= m_duration;
        }

    protected:
        T m_value;
        time m_duration;
        time m_start_time = main_clock::get_current_time();
    };

    template <typename T, int N>
    class quota_timed_lock final : public timed_lock<T> {
        using timed_lock<T>::timed_lock;
    public:
        // Uses quota or, if quota is exhausted but the cooldown has expired,
        // resets the quota and then updates the value.
        auto operator=(const T& value) -> quota_timed_lock& override {
            const auto now = main_clock::get_current_time();
            if (m_quota > 0) {
                m_quota--;
                this->m_value = value;
                this->m_start_time = now;
            }
            else if (now - this->m_start_time >= this->m_duration) {
                m_quota = N - 1;  // reset quota (using one use for the current assignment)
                this->m_value = value;
                this->m_start_time = now;
            }
            return *this;
        }

        // Returns a mutable pointer if quota is available or, if quota is exhausted,
        // the cooldown has expired (in which case the quota resets).
        auto try_get_mutable_value() -> T* override {
            const auto now = main_clock::get_current_time();
            if (m_quota > 0) {
                m_quota--;
                this->m_start_time = now;
                return &this->m_value;
            }
            if (now - this->m_start_time >= this->m_duration) {
	            m_quota = N - 1;
	            this->m_start_time = now;
	            return &this->m_value;
            }
            return nullptr;
        }

    private:
        int m_quota = N;
    };
}
