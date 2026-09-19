export module gse.devtools:alloc_panel;

import std;

import gse.core;
import gse.concurrency;
import gse.containers;
import gse.diag;
import gse.ecs;
import gse.graphics;
import gse.meta;
import gse.os;
import gse.time;
import gse.math;

export namespace gse::alloc_panel {
	struct [[= system_state<"AllocPanel">{}]] data {
		[[= actions::bind<"Toggle Allocation Panel", key::f10>{}]]
		[[= actions::hidden{}]]
		actions::handle toggle;

		bool visible = false;
		int top_rows = 15;
		std::vector<alloc::site> sites;
		std::unordered_map<std::uint64_t, std::string> labels;
		alloc::address_space usage;
		interval_timer<> refresh_timer{ milliseconds(500.f) };
	};

	[[= system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		channel_write<gui::menu_content> ui_out,
		shared_view<actions::data> actions_d
	) -> async::task<>;
}

namespace gse::alloc_panel {
	auto refresh(
		data& d
	) -> void;

	auto build_panel(
		data& d,
		gui::builder& ui
	) -> void;
}

auto gse::alloc_panel::refresh(data& d) -> void {
	d.usage = alloc::address_space_usage();
	alloc::snapshot(d.sites);
	std::ranges::sort(d.sites, std::ranges::greater{}, &alloc::site::live);

	for (const auto& row : d.sites | std::views::take(d.top_rows)) {
		if (!d.labels.contains(row.pc)) {
			d.labels.emplace(row.pc, alloc::label_of(row.pc));
		}
	}
}

auto gse::alloc_panel::build_panel(data& d, gui::builder& ui) -> void {
	ui.draw<gui::quantity_value<byte_count, mebibytes>>({
		.name = "Process private",
		.val = d.usage.private_committed,
	});

	ui.draw<gui::quantity_value<byte_count, mebibytes>>({
		.name = "Process image",
		.val = d.usage.image,
	});

	ui.draw<gui::quantity_value<byte_count, mebibytes>>({
		.name = "Estimated live",
		.val = alloc::estimated_live(),
	});

	ui.draw<gui::value<std::int64_t>>({
		.name = "Live samples",
		.val = alloc::live_samples(),
	});

	if (const auto evicted = alloc::evicted_samples(); evicted != 0) {
		ui.draw<gui::value<std::int64_t>>({
			.name = "Evicted samples",
			.val = evicted,
		});
	}

	bool on = alloc::enabled();
	ui.draw<gui::toggle>({
		.name = "Sampling",
		.value = on,
	});
	alloc::set_enabled(on);

	ui.draw<gui::slider<int>>({
		.name = "Rows",
		.value = d.top_rows,
		.min = 1,
		.max = 40,
	});

	if (ui.draw<gui::button>({
		.text = "Reset baseline"
		})) {
		alloc::mark();
	}

	if (ui.draw<gui::button>({
		.text = "Write report to log"
		})) {
		alloc::log_report(d.top_rows);
	}

	ui.draw<gui::separator>();

	if (!on) {
		ui.draw<gui::text>({
			.content = "Sampling is paused.",
		});
		return;
	}

	if (d.sites.empty()) {
		ui.draw<gui::text>({
			.content = "No sampled allocations yet.",
		});
		return;
	}

	for (const auto& row : d.sites | std::views::take(d.top_rows)) {
		ui.draw<gui::text>({
			.content = std::format(
				"{:>9.2f:MiB} {:>+9.2f:MiB}  {}",
				row.live,
				row.since_mark,
				d.labels.at(row.pc)
			),
		});
	}
}

auto gse::alloc_panel::run(context& ctx, data& d, const channel_write<gui::menu_content> ui_out, const shared_view<actions::data> actions_d) -> async::task<> {
	if (actions::pressed(d.toggle, actions::current_state(actions_d), actions_d)) {
		d.visible = !d.visible;
		if (d.visible) {
			alloc::set_enabled(true);
		}
	}

	if (!d.visible) {
		return {};
	}

	if (d.sites.empty() || d.refresh_timer.tick()) {
		refresh(d);
	}

	ui_out.push<gui::menu_content>({
		.menu = "Allocations",
		.build = [&d](gui::builder& ui) {
			build_panel(d, ui);
		},
	});

	return {};
}