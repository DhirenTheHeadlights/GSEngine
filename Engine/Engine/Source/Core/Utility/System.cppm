export module gse.utility:system;

import std;

import :concepts;
import :registry;
import :id;

export namespace gse {
	class system_provider;

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

	template <
		system_stage_kind Stage,
		typename ReadSet,
		typename WriteSet
	>
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

	class system_access {
	public:
		template <scheduled_system S>
		auto system_of(
		) -> S&;

		template <scheduled_system S>
		auto system_of(
		) const -> const S&;
	protected:
		static auto set_system_provider(
			system_provider* p
		) -> void;
	private:
		inline static system_provider* s_provider = nullptr;
	};

	class system_provider {
	public:
		virtual ~system_provider(
		) = default;

		virtual auto system(
			std::type_index idx
		) -> void* = 0;

		virtual auto system(
			std::type_index idx
		) const -> const void* = 0;
	};

	template <typename T, typename ReadSet, typename WriteSet>
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
			requires (set_contains<ReadSet, U>::value || set_contains<WriteSet, U>::value)
		auto other_read(const id owner_id) const -> const U* {
			return reg->try_linked_object_read<U>(owner_id);
		}

		template <is_component U>
			requires (!std::is_const_v<T> && set_contains<WriteSet, U>::value)
		auto other_write(const id owner_id) const -> U* {
			return reg->try_linked_object_write<U>(owner_id);
		}

		template <is_component U>
			requires (set_contains<ReadSet, U>::value || set_contains<WriteSet, U>::value)
		auto other_read_from(const T& component) const -> const U* {
			return other_read<U>(component.owner_id());
		}

		template <is_component U>
			requires (!std::is_const_v<T> && set_contains<WriteSet, U>::value)
		auto other_write_from(T& component) const -> U* {
			return other_write<U>(component.owner_id());
		}
	};

	class basic_system : public system_access {
	public:
		using schedule = system_schedule<
			system_stage<
				system_stage_kind::initialize,
				read_set<>,
				write_set<>
			>,
			system_stage<
				system_stage_kind::update,
				read_set<>,
				write_set<>
			>,
			system_stage<
				system_stage_kind::render,
				read_set<>,
				write_set<>
			>,
			system_stage<
				system_stage_kind::shutdown,
				read_set<>,
				write_set<>
			>
		>;

		virtual ~basic_system(
		) = default;

		virtual auto initialize(
		) -> void {}

		virtual auto update(
		) -> void {}

		virtual auto render(
		) -> void {}

		virtual auto shutdown(
		) -> void {}
	};

	template <typename ReadSet, typename WriteSet>
	class ecs_system : public basic_system {
	public:
		using read_set = ReadSet;
		using write_set = WriteSet;

		ecs_system() = default;

		explicit ecs_system(
			std::vector<std::reference_wrapper<registry>> registries
		) : m_registries(std::move(registries)) {
		}

		template <typename... Ts>
			requires readable_components<ReadSet, Ts...>
		auto read(
		);

		template <typename... Ts>
			requires writable_components<WriteSet, Ts...>
		auto write(
		);

		template <typename T, typename F>
			requires readable_components<ReadSet, T>
		auto for_each_read_chunk(
			F&& f
		);

		template <typename T, typename F>
			requires writable_components<WriteSet, T>
		auto for_each_write_chunk(
			F&& f
		);
	private:
		std::vector<std::reference_wrapper<registry>> m_registries;
	};
}

auto gse::system_access::set_system_provider(system_provider* p) -> void {
	s_provider = p;
}

template <gse::scheduled_system S>
auto gse::system_access::system_of() -> S& {
	auto* p = static_cast<S*>(s_provider->system(std::type_index(typeid(S))));
	return *p;
}

template <gse::scheduled_system S>
auto gse::system_access::system_of() const -> const S& {
	auto* p = static_cast<const S*>(s_provider->system(std::type_index(typeid(S))));
	return *p;
}

template <typename ReadSet, typename WriteSet>
template <typename... Ts>
	requires gse::readable_components<ReadSet, Ts...>
auto gse::ecs_system<ReadSet, WriteSet>::read() {
	std::tuple<std::vector<component_chunk<const Ts, ReadSet, WriteSet>>...> out;

	auto self = this;

	[&]<std::size_t... I>(std::index_sequence<I...>) {
		(
			[&] {
				using c = std::tuple_element_t<I, std::tuple<Ts...>>;
				using chunk_type = component_chunk<const c, ReadSet, WriteSet>;
				auto& vec = std::get<I>(out);
				vec.clear();
				vec.reserve(self->m_registries.size());
				for (auto& reg_ref : self->m_registries) {
					auto& reg = reg_ref.get();
					std::span<const c> s = reg.template linked_objects_read<c>();
					vec.push_back(chunk_type{ std::addressof(reg), std::span<const c>(s) });
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
auto gse::ecs_system<ReadSet, WriteSet>::write() {
	std::tuple<std::vector<component_chunk<Ts, ReadSet, WriteSet>>...> out;

	auto self = this;

	[&]<std::size_t... I>(std::index_sequence<I...>) {
		(
			[&] {
				using c = std::tuple_element_t<I, std::tuple<Ts...>>;
				using chunk_type = component_chunk<c, ReadSet, WriteSet>;
				auto& vec = std::get<I>(out);
				vec.clear();
				vec.reserve(self->m_registries.size());
				for (auto& reg_ref : self->m_registries) {
					auto& reg = reg_ref.get();
					std::span<c> s = reg.template linked_objects_write<c>();
					vec.push_back(chunk_type{ std::addressof(reg), std::span<c>(s) });
				}
			}(),
			...
		);
	}(std::index_sequence_for<Ts...>{});

	return out;
}

template <typename ReadSet, typename WriteSet>
template <typename T, typename F>
	requires gse::readable_components<ReadSet, T>
auto gse::ecs_system<ReadSet, WriteSet>::for_each_read_chunk(F&& f) {
	for (auto [chunks] = this->read<T>(); auto& chunk : chunks) {
		f(chunk);
	}
}

template <typename ReadSet, typename WriteSet>
template <typename T, typename F>
	requires gse::writable_components<WriteSet, T>
auto gse::ecs_system<ReadSet, WriteSet>::for_each_write_chunk(F&& f) {
	for (auto [chunks] = this->write<T>(); auto& chunk : chunks) {
		f(chunk);
	}
}
