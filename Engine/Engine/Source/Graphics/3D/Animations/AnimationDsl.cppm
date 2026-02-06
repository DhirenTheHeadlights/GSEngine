export module gse.graphics:animation_dsl;

import std;

import gse.utility;
import gse.physics.math;

export namespace gse::animation {
	enum class condition_type {
		bool_equals,
		float_greater,
		float_less,
		trigger,
		exit_time
	};

	struct condition {
		std::string param_name;
		condition_type type = condition_type::bool_equals;
		float float_value = 0.f;
		bool bool_value = true;
	};

	struct condition_list {
		std::vector<condition> conditions;

		auto operator&&(const condition& other) const -> condition_list {
			auto result = *this;
			result.conditions.push_back(other);
			return result;
		}
	};

	auto operator&&(const condition& a, const condition& b) -> condition_list {
		return { { a, b } };
	}

	template <typename T>
	struct param_handle {
		std::string name;
	};

	template <typename T>
	auto param(const std::string_view name) -> param_handle<T> {
		return param_handle<T>{ std::string(name) };
	}

	auto operator>(const param_handle<float>& p, const float value) -> condition {
		return { p.name, condition_type::float_greater, value, false };
	}

	auto operator<(const param_handle<float>& p, const float value) -> condition {
		return { p.name, condition_type::float_less, value, false };
	}

	auto operator!(const param_handle<bool>& p) -> condition {
		return { p.name, condition_type::bool_equals, 0.f, false };
	}

	auto to_condition(const param_handle<bool>& p) -> condition {
		return { p.name, condition_type::bool_equals, 0.f, true };
	}

	struct trigger_handle {
		std::string name;
	};

	auto trigger(const std::string_view name) -> trigger_handle {
		return trigger_handle{ std::string(name) };
	}

	auto to_condition(const trigger_handle& t) -> condition {
		return { t.name, condition_type::trigger, 0.f, true };
	}

	auto after(const float normalized_time) -> condition {
		return { "", condition_type::exit_time, normalized_time, false };
	}

	struct state_handle {
		std::string name;
		id clip = {};
		float speed = 1.f;
		bool loop = false;
	};

	struct transition_def {
		state_handle from_state;
		state_handle to_state;
		bool from_any = false;
		std::vector<condition> conditions;
		float blend_duration = 0.2f;

		auto operator&&(const condition& c) const -> transition_def {
			auto result = *this;
			result.conditions.push_back(c);
			return result;
		}

		auto operator&&(const condition_list& c) const -> transition_def {
			auto result = *this;
			for (const auto& cond : c.conditions) {
				result.conditions.push_back(cond);
			}
			return result;
		}

		auto operator&&(const param_handle<bool>& p) const -> transition_def {
			return *this && to_condition(p);
		}

		auto operator&&(const trigger_handle& t) const -> transition_def {
			return *this && to_condition(t);
		}

		auto blend(const float duration) const -> transition_def {
			auto result = *this;
			result.blend_duration = duration;
			return result;
		}
	};

	struct state_pair {
		state_handle from;
		state_handle to;
		bool from_any = false;

		auto operator&&(const condition& c) const -> transition_def {
			return { from, to, from_any, { c }, 0.2f };
		}

		auto operator&&(const condition_list& c) const -> transition_def {
			return { from, to, from_any, c.conditions, 0.2f };
		}

		auto operator&&(const param_handle<bool>& p) const -> transition_def {
			return *this && to_condition(p);
		}

		auto operator&&(const trigger_handle& t) const -> transition_def {
			return *this && to_condition(t);
		}
	};

	auto operator>>(const state_handle& from, const state_handle& to) -> state_pair {
		return { from, to, false };
	}

	struct any_state_t {
		auto operator>>(const state_handle& to) const -> state_pair {
			return { {}, to, true };
		}
	};

	constexpr any_state_t any;

	class graph_builder {
	public:
		graph_builder(const std::string_view name, const state_handle& default_state)
			: m_name(name), m_default_state(default_state) {}

		template <typename... Transitions>
			requires (std::convertible_to<Transitions, transition_def> && ...)
		auto transitions(Transitions&&... t) -> graph_builder& {
			(m_transitions.push_back(std::forward<Transitions>(t)), ...);
			return *this;
		}

		auto name() const -> const std::string& { return m_name; }
		auto default_state() const -> const state_handle& { return m_default_state; }
		auto transitions() const -> const std::vector<transition_def>& { return m_transitions; }
	private:
		std::string m_name;
		state_handle m_default_state;
		std::vector<transition_def> m_transitions;
	};

	auto graph(std::string_view name, const state_handle& default_state) -> graph_builder {
		return { name, default_state };
	}

	using float_param = param_handle<float>;
	using bool_param = param_handle<bool>;
}
