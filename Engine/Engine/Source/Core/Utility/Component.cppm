export module gse.utility:component;

import std;

import :id;

export namespace gse {
	struct no_data {};
	class registry;

	template <typename Data = no_data, typename NetworkedData = no_data>
	struct component : identifiable_owned, Data {
		using params = Data;
		using network_data_t = NetworkedData;

		component() = default;

		explicit component(
			const id owner_id,
			const params& p = {}
		) : identifiable_owned(owner_id), Data(p) {
		}

		explicit component(
			const id owner_id,
			const network_data_t& nd
		) requires (!std::same_as<params, network_data_t>)
			: identifiable_owned(owner_id) {
			static_assert(std::is_trivially_copyable_v<network_data_t>);
			*reinterpret_cast<network_data_t*>(static_cast<Data*>(this)) = nd;
		}

		explicit component(
			const network_data_t& nd
		) {
			static_assert(std::is_trivially_copyable_v<network_data_t>);
			*reinterpret_cast<network_data_t*>(static_cast<Data*>(this)) = nd;
		}

		virtual ~component() = default;

		virtual auto on_registry(
			registry* reg
		) -> void {
			(void)reg;
		}

		auto networked_data(
		) -> network_data_t& {
			return *reinterpret_cast<network_data_t*>(static_cast<Data*>(this));
		}

		auto networked_data(
		) const -> const network_data_t& {
			return *reinterpret_cast<const network_data_t*>(static_cast<const Data*>(this));
		}
	};
}
