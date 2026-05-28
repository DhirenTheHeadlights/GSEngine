export module ide:annotations;

import std;
import gse;

export namespace ide {
	template <gse::fixed_string V>
	struct name {
		static constexpr std::string_view value = V;
	};

	template <gse::fixed_string V>
	struct shortcut {
		static constexpr std::string_view value = V;
	};

	template <gse::fixed_string V>
	struct description {
		static constexpr std::string_view value = V;
	};

	template <gse::fixed_string V>
	struct when {
		static constexpr std::string_view value = V;
	};
}
