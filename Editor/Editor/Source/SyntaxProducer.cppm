export module ide:syntax_producer;

import std;
import gse;

export namespace ide {
	struct syntax_producer {
		struct data {
			std::vector<gse::gui::text_span> spans;
		};

		static auto rebuild(
			data& d,
			const gse::gui::text_buffer& buffer
		) -> void;
	};
}

auto ide::syntax_producer::rebuild(data& d, const gse::gui::text_buffer& buffer) -> void {
	d.spans.clear();
	(void)buffer;
}
