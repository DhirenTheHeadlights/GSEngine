export module gse.graphics:menu_stack;

import std;

import :builder;

export namespace gse::gui {
	struct screen {
		virtual ~screen() = default;
		virtual auto build(builder& ui) -> void = 0;
		virtual auto on_push() -> void {}
		virtual auto on_pop() -> void {}
		virtual auto captures_input() const -> bool { return true; }
	};

	struct menu_stack_state {
		std::vector<std::unique_ptr<screen>> stack;

		template <typename T, typename... Args>
		auto push(Args&&... args) -> void {
			auto s = std::make_unique<T>(std::forward<Args>(args)...);
			s->on_push();
			stack.push_back(std::move(s));
		}

		auto pop() -> void {
			if (!stack.empty()) {
				stack.back()->on_pop();
				stack.pop_back();
			}
		}

		auto clear() -> void {
			while (!stack.empty()) {
				pop();
			}
		}

		auto empty() const -> bool {
			return stack.empty();
		}

		auto top() -> screen* {
			return stack.empty() ? nullptr : stack.back().get();
		}

		auto captures_input() const -> bool {
			return !stack.empty() && stack.back()->captures_input();
		}
	};
}
