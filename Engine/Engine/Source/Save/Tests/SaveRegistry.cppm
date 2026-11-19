export module gse.tests.save_registry;

import std;

import gse.core;
import gse.ecs;
import gse.fs;
import gse.meta;
import gse.process;
import gse.save;
import gse.test;

export namespace gse::tests::save_registry {
	[[= test::unit{ .tags = "save_registry" }]]
	auto reverted_value_is_persisted(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "save_registry" }]]
	auto untouched_key_survives_a_foreign_write(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "save_registry" }]]
	auto unclaimed_key_is_left_alone(
		test::context& ctx
	) -> void;
}

namespace gse::tests::save_registry {
	struct knobs {
		[[= settings::describe<"first knob">{}]]
		std::string alpha = "alpha_default";

		[[= settings::describe<"second knob">{}]]
		std::string beta = "beta_default";
	};

	constexpr std::string_view test_category = "TestMerge";

	auto write_file(
		const std::filesystem::path& path,
		std::string_view content
	) -> void;

	auto value_on_disk(
		const std::filesystem::path& path,
		std::string_view key
	) -> std::string;

	auto record_for(
		knobs& k
	) -> settings::register_settings_type;
}

auto gse::tests::save_registry::write_file(const std::filesystem::path& path, const std::string_view content) -> void {
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out << content;
}

auto gse::tests::save_registry::value_on_disk(const std::filesystem::path& path, const std::string_view key) -> std::string {
	return save::registry::read_one<std::string>(path, test_category, key);
}

auto gse::tests::save_registry::record_for(knobs& k) -> settings::register_settings_type {
	return {
		.category = std::string(test_category),
		.type_id = id_of<knobs>(),
		.settings_ptr = &k,
		.keys = settings::collect_settings_keys<knobs>(),
		.write = &settings::write_settings_for<knobs>,
		.read = &settings::read_settings_for<knobs>,
	};
}

auto gse::tests::save_registry::reverted_value_is_persisted(test::context& ctx) -> void {
	const std::filesystem::path path = process::temporary_path("settings_revert", "ini");
	const auto _ = make_scope_exit([&path] {
		layout_store::flush();
		std::error_code ec;
		std::filesystem::remove(path, ec);
	});

	write_file(path, "[TestMerge]\nalpha = one\n");

	knobs k;
	save::registry reg;
	reg.set_paths({ .user = path });
	reg.load();
	reg.add(record_for(k));
	reg.set_auto_save(true);

	if (!ctx.expect_eq(k.alpha, std::string("one"))) {
		return;
	}

	k.alpha = "two";
	reg.save_now();
	layout_store::flush();
	ctx.expect_eq(value_on_disk(path, "alpha"), std::string("two"));

	k.alpha = "one";
	reg.save_now();
	layout_store::flush();
	ctx.expect_eq(value_on_disk(path, "alpha"), std::string("one"));
}

auto gse::tests::save_registry::untouched_key_survives_a_foreign_write(test::context& ctx) -> void {
	const std::filesystem::path path = process::temporary_path("settings_foreign", "ini");
	const auto _ = make_scope_exit([&path] {
		layout_store::flush();
		std::error_code ec;
		std::filesystem::remove(path, ec);
	});

	write_file(path, "[TestMerge]\nalpha = one\nbeta = two\n");

	knobs k;
	save::registry reg;
	reg.set_paths({ .user = path });
	reg.load();
	reg.add(record_for(k));
	reg.set_auto_save(true);

	if (!ctx.expect_eq(k.beta, std::string("two"))) {
		return;
	}

	write_file(path, "[TestMerge]\nalpha = one\nbeta = elsewhere\n");

	k.alpha = "changed";
	reg.save_now();
	layout_store::flush();

	ctx.expect_eq(value_on_disk(path, "alpha"), std::string("changed"));
	ctx.expect_eq(value_on_disk(path, "beta"), std::string("elsewhere"));
}

auto gse::tests::save_registry::unclaimed_key_is_left_alone(test::context& ctx) -> void {
	const std::filesystem::path path = process::temporary_path("settings_unclaimed", "ini");
	const auto _ = make_scope_exit([&path] {
		layout_store::flush();
		std::error_code ec;
		std::filesystem::remove(path, ec);
	});

	write_file(path, "[TestMerge]\nalpha = one\n");

	knobs k;
	save::registry reg;
	reg.set_paths({ .user = path });
	reg.load();
	reg.add(record_for(k));
	reg.set_auto_save(true);

	write_file(path, "[TestMerge]\nalpha = one\nbeta = elsewhere\n");

	reg.save_now();
	layout_store::flush();

	ctx.expect_eq(value_on_disk(path, "beta"), std::string("elsewhere"));
}
