import std;

import gse;
import packer;

auto main(int argc, char** argv) -> int {
	const auto args = gse::parse_args<packer::arguments>(argc, argv);
	packer::transcript out;

	const auto plan = packer::plan_for(args);
	if (!plan) {
		std::println("packaging failed: {}", plan.error());
		return 1;
	}

	const auto finish = [&out, &plan](const int code) -> int {
		const std::filesystem::path log = packer::write_transcript(*plan, out);
		if (!log.empty()) {
			std::println("transcript: {}", log.generic_display_string());
		}
		return code;
	};

	auto result = packer::stage_image(*plan, out);
	if (result && args.verify) {
		result = packer::verify_image(*plan, out);
	}
	if (!result) {
		packer::emit(out, "packaging failed: " + result.error());
		return finish(1);
	}
	if (!args.setup) {
		packer::emit(out, "image ready: " + plan->stage.generic_display_string());
		return finish(0);
	}

	const auto setup = packer::write_setup(*plan, out);
	if (!setup) {
		packer::emit(out, "packaging failed: " + setup.error());
		return finish(1);
	}
	packer::emit(out, "image ready: " + plan->stage.generic_display_string() + ", setup " + setup->generic_display_string());
	return finish(0);
}
