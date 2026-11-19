export module gse.tests.feed;

import std;

import gse.sdk;
import gse.test;

export namespace gse::tests::feed {
	[[= test::unit{ .tags = "feed" }]]
	auto release_versions_order_numerically(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "feed" }]]
	auto unorderable_versions_compare_by_difference(
		test::context& ctx
	) -> void;

	[[= test::unit{ .tags = "feed" }]]
	auto a_feed_survives_a_round_trip(
		test::context& ctx
	) -> void;
}

auto gse::tests::feed::release_versions_order_numerically(test::context& ctx) -> void {
	ctx.expect(sdk::is_newer("v2026.1008.10", "v2026.1008.2"));
	ctx.expect(!sdk::is_newer("v2026.1008.2", "v2026.1008.10"));
	ctx.expect(sdk::is_newer("v2026.1008.1", "v2026.1007.9"));
	ctx.expect(sdk::is_newer("v2027.0101.1", "v2026.1231.4"));
	ctx.expect(!sdk::is_newer("v2026.1008.3", "v2026.1008.3"));
}

auto gse::tests::feed::unorderable_versions_compare_by_difference(test::context& ctx) -> void {
	ctx.expect(sdk::is_newer("v2026.1008.1", "17594e55092f"));
	ctx.expect(sdk::is_newer("17594e55092f", "v2026.1008.1"));
	ctx.expect(!sdk::is_newer("17594e55092f", "17594e55092f"));
	ctx.expect(!sdk::parse_version("v2026.1008").has_value());
	ctx.expect(!sdk::parse_version("2026.1008.1").has_value());
	ctx.expect(!sdk::parse_version("v2026.1008.1.2").has_value());
	ctx.expect(!sdk::parse_version("v2026..1").has_value());
}

auto gse::tests::feed::a_feed_survives_a_round_trip(test::context& ctx) -> void {
	const sdk::feed written{
		.product = "Sandbox",
		.version = "v2026.1008.1",
		.preset = "x64-mingw-gcc-Release",
		.asset = "Sandbox-v2026.1008.1-x64-mingw-gcc-Release-setup.exe",
		.url = "https://example.invalid/releases/download/v2026.1008.1/Sandbox-setup.exe",
		.sha256 = "0f1e2d3c4b5a69788796a5b4c3d2e1f00f1e2d3c4b5a69788796a5b4c3d2e1f0",
		.size = 114133003,
		.server = "play.example.invalid:9000",
	};
	const auto read = sdk::parse_feed(sdk::write_feed(written));
	if (!ctx.expect_value(read)) {
		return;
	}
	ctx.expect_eq(read->version, written.version);
	ctx.expect_eq(read->asset, written.asset);
	ctx.expect_eq(read->url, written.url);
	ctx.expect_eq(read->sha256, written.sha256);
	ctx.expect_eq(read->size, written.size);
	ctx.expect_eq(read->server, written.server);

	ctx.expect_error(sdk::parse_feed("{}"));
	ctx.expect_error(sdk::parse_feed("not json"));
}
