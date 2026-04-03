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
		);

		explicit component(
			const id owner_id,
			const network_data_t& nd
		) requires (!std::same_as<params, network_data_t>);

		explicit component(
			const network_data_t& nd
		);

		virtual ~component() = default;

		virtual auto on_registry(
			registry* reg
		) -> void;

		template <typename Self>
		auto networked_data(
			this Self& self
		) -> decltype(auto);
	};
}

template <typename Data, typename NetworkedData>
gse::component<Data, NetworkedData>::component(const id owner_id, const params& p) : identifiable_owned(owner_id), Data(p) {}

template <typename Data, typename NetworkedData>
gse::component<Data, NetworkedData>::component(const id owner_id, const network_data_t& nd) requires (!std::same_as<params, network_data_t>) : identifiable_owned(owner_id) {
	static_assert(std::is_trivially_copyable_v<network_data_t>);
	*reinterpret_cast<network_data_t*>(static_cast<Data*>(this)) = nd;
}

template <typename Data, typename NetworkedData>
gse::component<Data, NetworkedData>::component(const network_data_t& nd) {
	static_assert(std::is_trivially_copyable_v<network_data_t>);
	*reinterpret_cast<network_data_t*>(static_cast<Data*>(this)) = nd;
}

template <typename Data, typename NetworkedData>
auto gse::component<Data, NetworkedData>::on_registry(registry* reg) -> void {
	(void)reg;
}

template <typename Data, typename NetworkedData>
template <typename Self>
auto gse::component<Data, NetworkedData>::networked_data(this Self& self) -> decltype(auto) {
	using data_t = std::conditional_t<std::is_const_v<Self>, const Data, Data>;
	using net_t = std::conditional_t<std::is_const_v<Self>, const network_data_t, network_data_t>;
	return reinterpret_cast<net_t&>(static_cast<data_t&>(self));
}
