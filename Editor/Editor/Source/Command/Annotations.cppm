export module gse.ide.command:annotations;

import std;
import gse;

export namespace gse::ide {
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
