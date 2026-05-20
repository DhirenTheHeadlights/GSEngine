export module gse.network:actions;

import std;

import gse.math;
import gse.os;
import gse.time;

import :input_frame;

export namespace gse::network {
	auto extract_input_frame(
		const actions::state& state,
		std::span<const std::uint16_t> axes1_ids,
		std::span<const std::uint16_t> axes2_ids,
		std::uint32_t input_sequence,
		angle camera_yaw = {}
	) -> input_frame;

	auto apply_input_frame(actions::state& target, const input_frame& m) -> void;
}

auto gse::network::extract_input_frame(
	const actions::state& state,
	const std::span<const std::uint16_t> axes1_ids,
	const std::span<const std::uint16_t> axes2_ids,
	const std::uint32_t input_sequence,
	const angle camera_yaw
) -> input_frame {
	const auto& pm = state.pressed_mask();
	const auto& rm = state.released_mask();
	const auto& hm = state.held_mask();
	const auto wc = std::max({ pm.word_count(), rm.word_count(), hm.word_count() });

	const auto pad = [wc](std::span<const std::uint64_t> src) {
		std::vector<std::uint64_t> out(wc);
		std::ranges::copy_n(src.begin(), std::min<std::size_t>(src.size(), wc), out.begin());
		return out;
	};

	const auto build_axes1 = [&](auto id) {
		return axes1_pair{
			.id = id,
			.value = state.axis1(id),
		};
	};

	const auto axes1_active = [](const axes1_pair& p) {
		return p.value != 0.f;
	};

	const auto build_axes2 = [&](auto id) -> axes2_pair {
		const auto v = state.axis2_v(id);
		return {
			.id = id,
			.x = v.x(),
			.y = v.y(),
		};
	};

	const auto axes2_active = [](const axes2_pair& p) {
		return p.x != 0.f || p.y != 0.f;
	};

	return input_frame{
		.input_sequence = input_sequence,
		.client_time = system_clock::now<time_t<std::uint32_t, milliseconds>>(),
		.camera_yaw = static_cast<float>(camera_yaw),
		.pressed = pad(pm.words()),
		.released = pad(rm.words()),
		.held = pad(hm.words()),
		.axes1 = axes1_ids | std::views::transform(build_axes1) | std::views::filter(axes1_active) |
			std::ranges::to<std::vector>(),
		.axes2 = axes2_ids | std::views::transform(build_axes2) | std::views::filter(axes2_active) |
			std::ranges::to<std::vector>(),
	};
}

auto gse::network::apply_input_frame(actions::state& target, const input_frame& m) -> void {
	target.begin_frame();
	target.ensure_capacity(m.pressed.size() * 64);
	target.load_state(m.pressed, m.released, m.held);
	target.clear_all_axes();

	for (const auto& [id, value] : m.axes1) {
		target.set_axis1(id, value);
	}

	for (const auto& [id, x, y] : m.axes2) {
		target.set_axis2(id, actions::axis{ x, y });
	}

	target.set_camera_yaw(gse::radians(m.camera_yaw));
}
