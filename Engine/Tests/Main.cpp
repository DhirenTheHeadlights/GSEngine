import std;

import gse.meta;
import gse.test;
import gse.tests;

namespace gse::tests {
	struct options {
		test::config test;
	};
}

auto main(int argc, char** argv) -> int {
	const auto parsed = gse::parse_args<gse::tests::options>(argc, argv);
	const std::array tables = { gse::test::registry<^^gse::tests>() };
	return gse::test::run({
		.tables = tables,
		.options = parsed.test,
	});
}
