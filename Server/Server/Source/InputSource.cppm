export module gse.server:input_source;

import std;
import gse;

import :server;

export namespace gse {
	class server_input_source {
	public:
		explicit server_input_source(
			const std::unordered_map<network::address, client_data>* clients
		) : m_clients(clients) {}

		template <typename Fn>
		auto for_each_context(
			world& w,
			Fn&& fn
		) const -> void {
			if (!m_clients) {
				return;
			}

			auto* sc = w.current_scene();
			if (!sc) {
				return;
			}

			auto* reg = &sc->registry();

			for (const auto& cd : *m_clients | std::views::values) {
				actions::state const& s = cd.latest_input;

				evaluation_context ctx{
					.client_id = cd.entity_id,
					.input = &s,
					.registry = reg
				};

				fn(ctx);
			}
		}

		auto state_for(
			id owner
		) const -> const actions::state& {
			static actions::state empty;

			if (!m_clients) {
				return empty;
			}

			for (const auto& cd : *m_clients | std::views::values) {
				if (cd.entity_id == owner) {
					return cd.latest_input;
				}
			}

			return empty;
		}
	private:
		const std::unordered_map<network::address, client_data>* m_clients{};
	};
}
