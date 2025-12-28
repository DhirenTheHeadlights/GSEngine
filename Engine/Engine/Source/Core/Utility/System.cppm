export module gse.utility:system;

import std;

import :concepts;
import :registry;
import :id;

export namespace gse {
	class scheduler;
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
		friend class scheduler;

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
	class component_chunk {
	public:
		component_chunk(
			registry* reg,
			std::span<T> span
		) : m_reg(reg), m_span(span) {}

		auto begin() const {
			return m_span.begin();
		}

		auto end() const {
			return m_span.end();
		}

		auto size() const -> std::size_t {
			return m_span.size();
		}

		auto operator[](std::size_t i) const -> T& {
			return m_span[i];
		}

		template <is_component U>
			requires (set_contains<ReadSet, U>::value || set_contains<WriteSet, U>::value)
		auto other_read(const id owner_id) const -> const U* {
			return m_reg->try_linked_object_read<U>(owner_id);
		}

		template <is_component U>
			requires (!std::is_const_v<T> && set_contains<WriteSet, U>::value)
		auto other_write(const id owner_id) const -> U* {
			return m_reg->try_linked_object_write<U>(owner_id);
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
	private:
		registry* m_reg = nullptr;
		std::span<T> m_span{};
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

		virtual auto begin_frame(
		) -> bool {
			return true;
		}

		virtual auto render(
		) -> void {}

		virtual auto end_frame(
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
			registry& registry
		) : m_registry(&registry) {
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

		template <typename T>
			requires writable_components<WriteSet, T>
		auto upsert_component(
			id entity,
			const T::network_data_t& data
		) -> void;

		template <typename T>
			requires writable_components<WriteSet, T>
		auto remove_component(
			id entity
		) -> void;
	private:
		registry* m_registry = nullptr;
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
	return std::tuple<component_chunk<const Ts, ReadSet, WriteSet>...>(
		component_chunk<const Ts, ReadSet, WriteSet>{
			m_registry,
			m_registry->linked_objects_read<Ts>()
		}...
	);
}

template <typename ReadSet, typename WriteSet>
template <typename... Ts>
	requires gse::writable_components<WriteSet, Ts...>
auto gse::ecs_system<ReadSet, WriteSet>::write() {
	return std::tuple<component_chunk<Ts, ReadSet, WriteSet>...>(
		component_chunk<Ts, ReadSet, WriteSet>{
			m_registry,
			m_registry->linked_objects_write<Ts>()
		}...
	);
}

template <typename ReadSet, typename WriteSet>
template <typename T, typename F>
	requires gse::readable_components<ReadSet, T>
auto gse::ecs_system<ReadSet, WriteSet>::for_each_read_chunk(F&& f) {
	auto tuple = this->read<T>();
	f(std::get<0>(tuple));
}

template <typename ReadSet, typename WriteSet>
template <typename T, typename F>
	requires gse::writable_components<WriteSet, T>
auto gse::ecs_system<ReadSet, WriteSet>::for_each_write_chunk(F&& f) {
	auto tuple = this->write<T>();
	f(std::get<0>(tuple));
}

template <typename ReadSet, typename WriteSet>
template <typename T> requires gse::writable_components<WriteSet, T>
auto gse::ecs_system<ReadSet, WriteSet>::upsert_component(const id entity, const typename T::network_data_t& data) -> void {
	m_registry->ensure_exists(entity);

	m_registry->add_deferred_action(entity, [entity, data](registry& r) -> bool {
		if (!r.active(entity)) {
			r.ensure_active(entity);
			return false; 
		}

		if (auto* c = r.try_linked_object_write<T>(entity)) {
			c->networked_data() = data;
			return true;
		}

		r.add_component<T>(entity, data);
		return true;
	});
}

template <typename ReadSet, typename WriteSet>
template <typename T> requires gse::writable_components<WriteSet, T>
auto gse::ecs_system<ReadSet, WriteSet>::remove_component(const id entity) -> void {
	if (!entity.exists()) return;

	m_registry->add_deferred_action(entity, [entity](registry& r) -> bool {
		r.remove_link<T>(entity);
		return true;
	});
}
