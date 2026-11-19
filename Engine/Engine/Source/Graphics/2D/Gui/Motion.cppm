export module gse.graphics:motion;

import std;

import gse.core;
import gse.math;
import gse.meta;
import gse.time;

export namespace gse::gui {
	enum class motion_channel : std::uint8_t {
		color,
		presence,
		rect,
		value,
	};

	struct motion_spec {
		time duration = milliseconds(140.f);
		ease_kind curve = ease_kind::cubic_out;
	};

	struct motion_stats {
		std::size_t live = 0;
		std::size_t animating = 0;
	};

	struct motion_store {
		auto sample(
			const id& widget,
			motion_channel channel,
			vec4f target,
			vec4f initial,
			const motion_spec& spec
		) -> vec4f;

		auto sweep() -> void;

		[[nodiscard]] auto stats() const -> motion_stats;

	private:
		struct entry {
			vec4f from;
			vec4f to;
			time_t<double> start;
			time duration;
			ease_kind curve = ease_kind::linear;
			bool seen = true;
		};

		[[nodiscard]] auto value_of(
			const entry& e,
			time_t<double> now
		) const -> vec4f;

		std::unordered_map<std::uint64_t, entry> m_entries;
		motion_stats m_stats;
	};
}

auto gse::gui::motion_store::value_of(const entry& e, const time_t<double> now) const -> vec4f {
	const auto fraction = static_cast<float>((now - e.start) / e.duration);
	return lerp(e.from, e.to, ease(e.curve, fraction));
}

auto gse::gui::motion_store::sample(const id& widget, const motion_channel channel, const vec4f target, const vec4f initial, const motion_spec& spec) -> vec4f {
	const std::uint64_t key = hash_of(widget.number(), static_cast<std::uint64_t>(channel));
	const auto now = system_clock::now<time_t<double>>();

	const auto [it, inserted] = m_entries.try_emplace(key, entry{
		.from = initial,
		.to = target,
		.start = now,
		.duration = spec.duration,
		.curve = spec.curve,
	});

	entry& e = it->second;
	e.seen = true;

	if (!inserted && target != e.to) {
		e.from = value_of(e, now);
		e.to = target;
		e.start = now;
		e.duration = spec.duration;
		e.curve = spec.curve;
	}

	if (now - e.start >= e.duration) {
		e.from = e.to;
		return e.to;
	}

	frame_demand::request_redraw();
	return value_of(e, now);
}

auto gse::gui::motion_store::sweep() -> void {
	const auto now = system_clock::now<time_t<double>>();

	std::erase_if(m_entries, [](const auto& item) {
		return !item.second.seen;
	});

	m_stats = {};
	for (entry& e : std::views::values(m_entries)) {
		e.seen = false;
		++m_stats.live;
		if (now - e.start < e.duration) {
			++m_stats.animating;
		}
	}
}

auto gse::gui::motion_store::stats() const -> motion_stats {
	return m_stats;
}
