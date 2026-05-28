export module ide:workspace;

import std;

export namespace ide {
	struct open_document {
		std::filesystem::path path;
		std::uint32_t buffer_index = 0;
		bool dirty = false;
	};

	struct workspace {
		struct data {
			std::filesystem::path root;
			std::vector<open_document> documents;
			std::optional<std::size_t> active_document;
		};
	};
}
