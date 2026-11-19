export module gse.concurrency:resume_slot;

import gse.assert;
import gse.core;
import gse.log;
import std;

import :async_task;

export namespace gse::async {
	struct continuation_base {};

	template <typename T>
	class resume_slot : public continuation_base {
	public:
		resume_slot() = default;

		resume_slot(
			std::coroutine_handle<> h,
			T*& slot
		);

		auto resume_with(
			T& value
		) const -> void;

	private:
		checked_handle m_handle;
		T** m_slot = nullptr;
	};
}

template <typename T>
gse::async::resume_slot<T>::resume_slot(const std::coroutine_handle<> h, T*& slot) : m_handle(track_frame(h)), m_slot(&slot) {
}

template <typename T>
auto gse::async::resume_slot<T>::resume_with(T& value) const -> void {
	assert(
		m_slot != nullptr,
		"a resume_slot<{}> was handed over without a continuation to resume",
		type_tag<T>()
	);

	struct pending_store {
		T** slot;
		T* value;
	};

	pending_store store{ m_slot, &value };

	const auto resumed = resume_checked_with(
		m_handle,
		[](void* context) -> void {
			const pending_store& pending = *static_cast<pending_store*>(context);
			*pending.slot = pending.value;
		},
		&store
	);

	if (!resumed) {
		log::println(
			log::level::error,
			log::category::task,
			"resume_slot: continuation was not resumed because its coroutine frame had already been destroyed"
		);
	}
}
