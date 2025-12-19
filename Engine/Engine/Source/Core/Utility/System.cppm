export module gse.utility:system;

import std;

import :concepts;
import :registry;
import :id;

export namespace gse {
	template <is_component... Ts>
	struct read_set {
	};

	template <is_component... Ts>
	struct write_set {
	};

	enum struct system_stage_kind {
		initialize,
		update,
		render,
		shutdown
	};

	template <system_stage_kind Stage, typename ReadSet, typename WriteSet>
	struct system_stage {
		static constexpr system_stage_kind stage = Stage;
		using read_set = ReadSet;
		using write_set = WriteSet;
	};

	template <typename... Stages>
	struct system_schedule {
		using stages = std::tuple<Stages...>;
	};

	template <typename S>
	concept scheduled_system = requires {
		typename S::schedule::stages;
	};

	template <typename Set, typename T>
	struct set_contains;

	template <template <typename...> typename Set, typename... Cs, typename T>
	struct set_contains<Set<Cs...>, T> : std::bool_constant<(std::same_as<Cs, T> || ...)> {
	};

	template <typename ReadSet, typename... Ts>
	concept readable_components = (set_contains<ReadSet, Ts>::value && ...);

	template <typename WriteSet, typename... Ts>
	concept writable_components = (set_contains<WriteSet, Ts>::value && ...);

	template <typename T>
	struct component_chunk {
		registry* reg = nullptr;
		std::span<T> span{};

		auto begin() const {
			return span.begin();
		}

		auto end() const {
			return span.end();
		}

		auto size() const -> std::size_t {
			return span.size();
		}

		auto operator[](std::size_t i) const -> T& {
			return span[i];
		}

		template <is_component U>
		auto other_read(const id owner_id) const -> const U* {
			return reg->try_linked_object_read<U>(owner_id);
		}

		template <is_component U>
		auto other_write(const id owner_id) const -> U* {
			return reg->try_linked_object_write<U>(owner_id);
		}

		template <is_component U>
		auto other_read_from(const T& component) const -> const U* {
			return other_read<U>(component.owner_id());
		}

		template <is_component U>
		auto other_write_from(T& component) const -> U* {
			return other_write<U>(component.owner_id());
		}
	};

	template <typename ReadSet, typename WriteSet>
	class system {
	public:
		using read_set = ReadSet;
		using write_set = WriteSet;

		system() = default;

		explicit system(
			std::vector<std::reference_wrapper<registry>> registries
		) : m_registries(std::move(registries)) {
		}

		auto registries(
		) const -> std::span<const std::reference_wrapper<registry>> {
			return m_registries;
		}

		template <typename... Ts>
			requires readable_components<ReadSet, Ts...>
		auto read();

		template <typename... Ts>
			requires writable_components<WriteSet, Ts...>
		auto write();
	protected:
		auto registries_mut(
		) -> std::vector<std::reference_wrapper<registry>>& {
			return m_registries;
		}
	private:
		std::vector<std::reference_wrapper<registry>> m_registries;
	};
}

template <typename ReadSet, typename WriteSet>
template <typename... Ts>
	requires gse::readable_components<ReadSet, Ts...>
auto gse::system<ReadSet, WriteSet>::read() {
	std::tuple<std::vector<component_chunk<const Ts>>...> out;

	auto self = this;

	[&]<std::size_t... I>(std::index_sequence<I...>) {
		(
			[&] {
				using c = std::tuple_element_t<I, std::tuple<Ts...>>;
				auto& vec = std::get<I>(out);
				vec.clear();
				vec.reserve(self->m_registries.size());
				for (auto& reg_ref : self->m_registries) {
					auto& reg = reg_ref.get();
					std::span<const c> s = reg.template linked_objects_read<c>();
					vec.push_back(component_chunk<const c>{ &reg, s });
				}
			}(),
			...
		);
	}(std::index_sequence_for<Ts...>{});

	return out;
}

template <typename ReadSet, typename WriteSet>
template <typename... Ts>
	requires gse::writable_components<WriteSet, Ts...>
auto gse::system<ReadSet, WriteSet>::write() {
	std::tuple<std::vector<component_chunk<Ts>>...> out;

	auto self = this;

	[&]<std::size_t... I>(std::index_sequence<I...>) {
		(
			[&] {
				using c = std::tuple_element_t<I, std::tuple<Ts...>>;
				auto& vec = std::get<I>(out);
				vec.clear();
				vec.reserve(self->m_registries.size());
				for (auto& reg_ref : self->m_registries) {
					auto& reg = reg_ref.get();
					std::span<c> s = reg.template linked_objects_write<c>();
					vec.push_back(component_chunk<c>{ &reg, s });
				}
			}(),
			...
		);
	}(std::index_sequence_for<Ts...>{});

	return out;
}
