export module gse.utility:non_copyable;

import std;

export namespace gse {
	class non_copyable {
	public:
		non_copyable() = default;
		non_copyable(const non_copyable&) = delete;
		auto operator=(const non_copyable&) -> non_copyable& = delete;
		non_copyable(non_copyable&&) = default;
		auto operator=(non_copyable&&) -> non_copyable& = default;
		virtual ~non_copyable() = default;
	};

	class non_movable {
		public:
		non_movable() = default;
		non_movable(const non_movable&) = default;
		auto operator=(const non_movable&) -> non_movable& = default;
		non_movable(non_movable&&) = delete;
		auto operator=(non_movable&&) -> non_movable& = delete;
		virtual ~non_movable() = default;
	};
}