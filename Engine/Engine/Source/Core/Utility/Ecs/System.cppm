export module gse.utility:system;

import std;

import :concepts;
import :registry;
import :id;
import :task;
import :n_buffer;
import :lambda_traits;
import :frame_sync;

export namespace gse {
	class scheduler;

	enum class active_stage { none, update, render };
	thread_local auto t_current_stage = active_stage::none;

	template <is_component T>
	class component_view {
	public:
		component_view(
			registry* reg,
			std::span<const T> span
		);

		auto begin(
		) const -> std::span<const T>::iterator;

		auto end(
		) const -> std::span<const T>::iterator;

		auto size(
		) const -> std::size_t;

		auto empty(
		) const -> bool;

		auto operator[](
			std::size_t i
		) const -> const T&;

		template <is_component U>
		auto get(
			id owner
		) const -> const U*;

		template <is_component U>
		auto associated(
			const T& component
		) const -> const U*;

	private:
		registry* m_reg;
		std::span<const T> m_span;
	};

	template <is_component T>
	class component_chunk {
	public:
		component_chunk(
			registry* reg,
			std::span<T> span
		);

		auto begin(
		) const -> std::span<T>::iterator;

		auto end(
		) const -> std::span<T>::iterator;

		auto size(
		) const -> std::size_t;

		auto empty(
		) const -> bool;

		auto data(
		) const -> T*;

		auto operator[](
			std::size_t i
		) const -> T&;

		auto read(
			id owner
		) const -> const T*;

		auto read_from(
			const T& component
		) const -> const T*;

		auto write(
			id owner
		) const -> T*;

		auto write_from(
			T& component
		) const -> T*;

		template <is_component U>
		auto read(
			id owner
		) const -> const U*;

		template <is_component U>
		auto read_from(
			const T& component
		) const -> const U*;

		template <is_component U>
		auto write(
			id owner
		) const -> U*;

		template <is_component U>
		auto write_from(
			T& component
		) const -> U*;

		template <typename F>
		auto parallel_for_each(
			F&& f
		) const -> void;

	private:
		registry* m_reg;
		std::span<T> m_span;
	};

	template <typename T>
	class channel {
	public:
		class reader {
		public:
			explicit reader(
				const std::vector<T>* data
			);

			auto begin(
			) const -> std::vector<T>::const_iterator;

			auto end(
			) const -> std::vector<T>::const_iterator;

			auto size(
			) const -> std::size_t;

			auto empty(
			) const -> bool;

			auto operator[](
				std::size_t i
			) const -> const T&;

		private:
			const std::vector<T>* m_data;
		};

		using value_type = T;

		channel();

		auto read(
		) const -> reader;

		auto push(
			T item
		) -> void;

		template <typename... Args>
		auto emplace(
			Args&&... args
		) -> T&;

		auto flip(
		) -> void;

	private:
		double_buffer<std::vector<T>> m_buffer;
		mutable std::mutex m_write_mutex;
	};

	struct channel_base {
		virtual ~channel_base(
		) = default;
	};

	template <typename T>
	struct typed_channel final : channel_base {
		channel<T> data;
	};

	using channel_factory_fn = std::unique_ptr<channel_base>(*)();

	class system_provider {
	public:
		virtual ~system_provider(
		) = default;

		virtual auto system_ptr(
			std::type_index idx
		) -> void* = 0;

		virtual auto system_ptr(
			std::type_index idx
		) const -> const void* = 0;

		virtual auto ensure_channel(
			std::type_index idx,
			channel_factory_fn factory
		) -> channel_base& = 0;
	};

	template <typename T>
	struct extract_channel_type;

	template <typename T>
	struct extract_channel_type<channel<T>> {
		using type = T;
	};

	template <typename T>
	struct extract_channel_type<channel<T>&> {
		using type = T;
	};

	template <typename T>
	struct extract_channel_type<const channel<T>&> {
		using type = T;
	};

	template <typename T>
	using extract_channel_type_t = extract_channel_type<T>::type;

	struct pending_work {
		id name;
		std::vector<std::type_index> component_writes;
		std::type_index channel_write = typeid(void);
		std::move_only_function<void()> execute;
	};

	template <typename T>
	struct extract_component;

	template <is_component T>
	struct extract_component<component_chunk<T>> {
		using type = T;
	};

	template <is_component T>
	struct extract_component<component_chunk<T>&> {
		using type = T;
	};

	template <is_component T>
	struct extract_component<const component_chunk<T>&> {
		using type = T;
	};

	template <typename F>
	struct callable_traits;

	template <typename R, typename C, typename... Args>
	struct callable_traits<R(C::*)(Args...) const> {
		using component_types = std::tuple<std::type_identity<typename extract_component<std::decay_t<Args>>::type>...>;
	};

	template <typename R, typename C, typename... Args>
	struct callable_traits<R(C::*)(Args...)> {
		using component_types = std::tuple<std::type_identity<typename extract_component<std::decay_t<Args>>::type>...>;
	};

	template <typename F>
		requires requires { &F::operator(); }
	struct callable_traits<F> : callable_traits<decltype(&F::operator())> {};

	/**
	 * Base class for all engine systems.
	 *
	 * IMPORTANT: Per-Frame Resource Double-Buffering
	 * ================================================
	 *
	 * Systems with separate update() and render() threads must double-buffer
	 * resources that are modified in update() and read in render().
	 *
	 * The engine runs with 2 frames in flight:
	 *   - Frame N rendering on GPU
	 *   - Frame N+1 updating on CPU
	 *
	 * Without per-frame resources, update() for frame N+1 would overwrite
	 * buffers/descriptors while the GPU is still reading them for frame N,
	 * causing race conditions and visual corruption.
	 *
	 * Use gse::per_frame_resource<T> for type-safe per-frame resource management:
	 *
	 * Example (Renderer System):
	 *
	 *   class my_renderer : public system {
	 *   private:
	 *       // ✅ CORRECT: Per-frame resources
	 *       per_frame_resource<vulkan::buffer_resource> m_camera_ubo;
	 *       per_frame_resource<vk::raii::DescriptorSet> m_descriptor_sets;
	 *
	 *       // ❌ WRONG: Shared resource (will cause race conditions!)
	 *       vulkan::buffer_resource m_shared_ubo;
	 *
	 *       auto update() -> void override {
	 *           const auto frame = m_context.config().current_frame();
	 *
	 *           // Update the correct frame's buffer
	 *           m_shader->set_uniform("view", ..., m_camera_ubo[frame].allocation);
	 *
	 *           // m_camera_ubo.allocation; // ❌ Compile error - must index by frame!
	 *       }
	 *
	 *       auto render() -> void override {
	 *           const auto frame = m_context.config().current_frame();
	 *
	 *           // Bind the correct frame's descriptor set
	 *           command.bindDescriptorSets(..., m_descriptor_sets[frame], ...);
	 *       }
	 *   };
	 *
	 * Resources that DO NOT need per-frame duplication:
	 *   - Push constants (written directly to command buffer)
	 *   - Read-only resources (textures, shaders, models)
	 *   - Single-use resources (immediate mode rendering)
	 *
	 * Resources that DO need per-frame duplication:
	 *   - UBO/SSBO buffers updated every frame
	 *   - Persistent descriptor sets
	 *   - Dynamic vertex/index buffers
	 *
	 * See gse::per_frame_resource for implementation details.
	 */
	class system {
	public:
		virtual ~system(
		) = default;

		virtual auto initialize(
		) -> void;

		virtual auto update(
		) -> void;

		virtual auto render(
		) -> void;

		virtual auto shutdown(
		) -> void;

		virtual auto begin_frame(
		) -> bool;

		virtual auto end_frame(
		) -> void;

	protected:
		friend class scheduler;

		template <is_component T>
		auto read(
		) const -> component_view<T>;

		template <is_component... Ts>
			requires (sizeof...(Ts) > 1)
		auto read(
		) const -> std::tuple<component_view<Ts>...>;

		template <std::derived_from<gse::system> S>
		auto system_of(
		) const -> const S&;

		template <typename T>
		auto channel_of(
		) const -> channel<T>::reader;

		template <typename F>
		auto write(
			F&& f,
			std::source_location loc = std::source_location::current()
		) -> void;

		template <typename F>
		auto publish(
			F&& f,
			std::source_location loc = std::source_location::current()
		) -> void;

		template <is_component T>
		auto defer_add(
			id entity,
			T::network_data_t data
		) -> void;

		template <is_component T>
		auto defer_remove(
			id entity
		) -> void;

	private:
		registry* m_registry = nullptr;
		system_provider* m_scheduler = nullptr;
		std::vector<pending_work> m_pending_update;
		std::vector<pending_work> m_pending_render;
		mutable std::mutex m_pending_update_mutex;
		mutable std::mutex m_pending_render_mutex;
		std::vector<std::move_only_function<void(registry&)>> m_deferred;

		auto push_pending(
			pending_work work
		) -> void;

		template <typename F, is_component... Ts>
		auto write_impl(
			F&& f,
			std::tuple<std::type_identity<Ts>...>,
			std::source_location loc = std::source_location::current()
		) -> void;

		auto take_pending(
			active_stage stage
		) -> std::vector<pending_work>;

		auto flush_deferred(
		) -> void;
	};
}

template <gse::is_component T>
gse::component_view<T>::component_view(registry* reg, std::span<const T> span)
	: m_reg(reg), m_span(span) {}

template <gse::is_component T>
auto gse::component_view<T>::begin() const -> std::span<const T>::iterator {
	return m_span.begin();
}

template <gse::is_component T>
auto gse::component_view<T>::end() const -> std::span<const T>::iterator {
	return m_span.end();
}

template <gse::is_component T>
auto gse::component_view<T>::size() const -> std::size_t {
	return m_span.size();
}

template <gse::is_component T>
auto gse::component_view<T>::empty() const -> bool {
	return m_span.empty();
}

template <gse::is_component T>
auto gse::component_view<T>::operator[](std::size_t i) const -> const T& {
	return m_span[i];
}

template <gse::is_component T>
template <gse::is_component U>
auto gse::component_view<T>::get(const id owner) const -> const U* {
	return m_reg->try_linked_object_read<U>(owner);
}

template <gse::is_component T>
template <gse::is_component U>
auto gse::component_view<T>::associated(const T& component) const -> const U* {
	return get<U>(component.owner_id());
}

template <gse::is_component T>
gse::component_chunk<T>::component_chunk(registry* reg, std::span<T> span)
	: m_reg(reg), m_span(span) {}

template <gse::is_component T>
auto gse::component_chunk<T>::begin() const -> std::span<T>::iterator {
	return m_span.begin();
}

template <gse::is_component T>
auto gse::component_chunk<T>::end() const -> std::span<T>::iterator {
	return m_span.end();
}

template <gse::is_component T>
auto gse::component_chunk<T>::size() const -> std::size_t {
	return m_span.size();
}

template <gse::is_component T>
auto gse::component_chunk<T>::empty() const -> bool {
	return m_span.empty();
}

template <gse::is_component T>
auto gse::component_chunk<T>::data() const -> T* {
	return m_span.data();
}

template <gse::is_component T>
auto gse::component_chunk<T>::operator[](std::size_t i) const -> T& {
	return m_span[i];
}

template <gse::is_component T>
auto gse::component_chunk<T>::read(const id owner) const -> const T* {
	return m_reg->try_linked_object_read<T>(owner);
}

template <gse::is_component T>
auto gse::component_chunk<T>::read_from(const T& component) const -> const T* {
	return read(component.owner_id());
}

template <gse::is_component T>
auto gse::component_chunk<T>::write(const id owner) const -> T* {
	return m_reg->try_linked_object_write<T>(owner);
}

template <gse::is_component T>
auto gse::component_chunk<T>::write_from(T& component) const -> T* {
	return write(component.owner_id());
}

template <gse::is_component T>
template <gse::is_component U>
auto gse::component_chunk<T>::read(const id owner) const -> const U* {
	return m_reg->try_linked_object_read<U>(owner);
}

template <gse::is_component T>
template <gse::is_component U>
auto gse::component_chunk<T>::read_from(const T& component) const -> const U* {
	return read<U>(component.owner_id());
}

template <gse::is_component T>
template <gse::is_component U>
auto gse::component_chunk<T>::write(const id owner) const -> U* {
	return m_reg->try_linked_object_write<U>(owner);
}

template <gse::is_component T>
template <gse::is_component U>
auto gse::component_chunk<T>::write_from(T& component) const -> U* {
	return write<U>(component.owner_id());
}

template <gse::is_component T>
template <typename F>
auto gse::component_chunk<T>::parallel_for_each(F&& f) const -> void {
	if (m_span.empty()) {
		return;
	}

	task::parallel_for(0uz, m_span.size(), [&](std::size_t i) {
		f(m_span[i]);
	});
}

template <typename T>
gse::channel<T>::reader::reader(const std::vector<T>* data)
	: m_data(data) {}

template <typename T>
auto gse::channel<T>::reader::begin() const -> std::vector<T>::const_iterator {
	return m_data->begin();
}

template <typename T>
auto gse::channel<T>::reader::end() const -> std::vector<T>::const_iterator {
	return m_data->end();
}

template <typename T>
auto gse::channel<T>::reader::size() const -> std::size_t {
	return m_data->size();
}

template <typename T>
auto gse::channel<T>::reader::empty() const -> bool {
	return m_data->empty();
}

template <typename T>
auto gse::channel<T>::reader::operator[](std::size_t i) const -> const T& {
	return (*m_data)[i];
}

template <typename T>
gse::channel<T>::channel() {
	frame_sync::on_end([this] {
		flip();
	});
}

template <typename T>
auto gse::channel<T>::read() const -> reader {
	return reader(&m_buffer.read());
}

template <typename T>
auto gse::channel<T>::push(T item) -> void {
	std::lock_guard lock(m_write_mutex);
	m_buffer.write().push_back(std::move(item));
}

template <typename T>
template <typename... Args>
auto gse::channel<T>::emplace(Args&&... args) -> T& {
	std::lock_guard lock(m_write_mutex);
	return m_buffer.write().emplace_back(std::forward<Args>(args)...);
}

template <typename T>
auto gse::channel<T>::flip() -> void {
	std::lock_guard lock(m_write_mutex);
	m_buffer.flip();
	m_buffer.write().clear();
}

auto gse::system::initialize() -> void {}

auto gse::system::update() -> void {}

auto gse::system::render() -> void {}

auto gse::system::shutdown() -> void {}

auto gse::system::begin_frame() -> bool {
	return true;
}

auto gse::system::end_frame() -> void {}

auto gse::system::push_pending(pending_work work) -> void {
    switch (t_current_stage) {
        case active_stage::update: {
            std::lock_guard lock(m_pending_update_mutex);
            m_pending_update.push_back(std::move(work));
            break;
        }
        case active_stage::render: {
            std::lock_guard lock(m_pending_render_mutex);
            m_pending_render.push_back(std::move(work));
            break;
        }
        default: {
            std::lock_guard lock(m_pending_update_mutex);
            m_pending_update.push_back(std::move(work));
            break;
        }
    }
}

auto gse::system::take_pending(const active_stage stage) -> std::vector<pending_work> {
	switch (stage) {
		case active_stage::update: {
			std::lock_guard lock(m_pending_update_mutex);
			return std::exchange(m_pending_update, {});
		}
		case active_stage::render: {
			std::lock_guard lock(m_pending_render_mutex);
			return std::exchange(m_pending_render, {});
		}
		default:
			return {};
	}
}

template <gse::is_component T>
auto gse::system::read() const -> component_view<T> {
	return component_view<T>{ m_registry, m_registry->linked_objects_read<T>() };
}

template <gse::is_component... Ts>
	requires (sizeof...(Ts) > 1)
auto gse::system::read() const -> std::tuple<component_view<Ts>...> {
	return { read<Ts>()... };
}

template <std::derived_from<gse::system> S>
auto gse::system::system_of() const -> const S& {
	const auto* p = static_cast<const system_provider*>(m_scheduler)->system_ptr(std::type_index(typeid(S)));
	return *static_cast<const S*>(p);
}

template <typename T>
auto gse::system::channel_of() const -> channel<T>::reader {
	auto& base = m_scheduler->ensure_channel(std::type_index(typeid(T)), +[]() -> std::unique_ptr<channel_base> {
		return std::make_unique<typed_channel<T>>();
	});

	return static_cast<typed_channel<T>&>(base).data.read();
}

template <typename F>
auto gse::system::write(F&& f, std::source_location loc) -> void {
	using traits = callable_traits<std::decay_t<F>>;
	using components = typename traits::component_types;

	write_impl(std::forward<F>(f), components{}, loc);
}

template <typename F, gse::is_component... Ts>
auto gse::system::write_impl(F&& f, std::tuple<std::type_identity<Ts>...>, std::source_location loc) -> void {
    auto name = std::string(loc.function_name());
    if (const auto paren = name.find('('); paren != std::string::npos) {
        name = name.substr(0, paren);
    }
    if (const auto space = name.rfind(' '); space != std::string::npos) {
        name = name.substr(space + 1);
    }
    
    if constexpr (sizeof...(Ts) > 0) {
        name += "::write<";
        bool first = true;
        ((name += (first ? (first = false, "") : ", "), name += typeid(Ts).name()), ...);
        name += ">";
    } else {
        name += "::write";
    }

    push_pending(pending_work{
        .name = find_or_generate_id(name),
        .component_writes = { std::type_index(typeid(Ts))... },
        .channel_write = typeid(void),
        .execute = [this, func = std::forward<F>(f)]() mutable {
            if constexpr (sizeof...(Ts) == 0) {
                func();
            } else if constexpr (sizeof...(Ts) == 1) {
                using t = std::tuple_element_t<0, std::tuple<Ts...>>;
                component_chunk<t> chunk{
                    m_registry,
                    m_registry->linked_objects_write<t>()
                };
                func(chunk);
            } else {
                auto chunks = std::tuple{
                    component_chunk<Ts>{
                        m_registry,
                        m_registry->linked_objects_write<Ts>()
                    }...
                };
                std::apply(func, chunks);
            }
        }
    });
}

template <typename F>
auto gse::system::publish(F&& f, const std::source_location loc) -> void {
    using channel_type = std::remove_cvref_t<first_arg_t<F>>;
    using t = extract_channel_type_t<channel_type>;

    auto name = std::string(loc.function_name());
    if (const auto paren = name.find('('); paren != std::string::npos) {
        name = name.substr(0, paren);
    }
    if (const auto space = name.rfind(' '); space != std::string::npos) {
        name = name.substr(space + 1);
    }

    name += "::publish<";
    name += typeid(t).name();
    name += ">";

    push_pending(pending_work{
        .name = find_or_generate_id(name),
        .component_writes = {},
        .channel_write = std::type_index(typeid(t)),
        .execute = [this, func = std::forward<F>(f)]() mutable {
            auto& base = m_scheduler->ensure_channel(std::type_index(typeid(t)), +[]() -> std::unique_ptr<channel_base> {
                return std::make_unique<typed_channel<t>>();
            });

            func(static_cast<typed_channel<t>&>(base).data);
        }
    });
}

template <gse::is_component T>
auto gse::system::defer_add(id entity, typename T::network_data_t data) -> void {
	m_deferred.push_back([entity, data = std::move(data)](registry& r) {
		r.ensure_exists(entity);
		r.add_deferred_action(entity, [entity, data](registry& reg) -> bool {
			if (!reg.active(entity)) {
				reg.ensure_active(entity);
				return false;
			}
			if (auto* c = reg.try_linked_object_write<T>(entity)) {
				c->networked_data() = data;
				return true;
			}
			reg.add_component<T>(entity, data);
			return true;
		});
	});
}

template <gse::is_component T>
auto gse::system::defer_remove(id entity) -> void {
	if (!entity.exists()) {
		return;
	}

	m_deferred.push_back([entity](registry& r) {
		r.add_deferred_action(entity, [entity](registry& reg) -> bool {
			reg.remove_link<T>(entity);
			return true;
		});
	});
}

auto gse::system::flush_deferred() -> void {
	for (auto& d : m_deferred) {
		d(*m_registry);
	}
	m_deferred.clear();
}