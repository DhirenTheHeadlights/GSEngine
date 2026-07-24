export module gse.ide.search:query_driver;

import std;
import gse;

import :types;
import :index;
import :engine;

export namespace gse::ide::search {
	class query_driver : public non_copyable {
	public:
		query_driver() = default;
		~query_driver();
		query_driver(query_driver&&) noexcept = default;
		auto operator=(query_driver&&) noexcept -> query_driver& = default;

		std::string query;
		std::vector<result> results;
		int selected = -1;

		auto update(
			time now,
			const index_state* index,
			const options& opts
		) -> bool;

		auto accept() -> void;

	private:
		auto cancel_pending() -> void;

		std::string m_last_query;
		options m_last_options;
		time m_changed_at{};
		std::shared_ptr<query_buffer> m_pending;
		std::uint64_t m_last_index_generation = 0;
		std::uint64_t m_active_request_id = 0;
		bool m_dirty = false;
		bool m_debounce = false;
	};
}

namespace gse::ide::search {
	auto same_options(const options& lhs, const options& rhs) -> bool;
}

auto gse::ide::search::same_options(const options& lhs, const options& rhs) -> bool {
	return lhs.max_results == rhs.max_results
		&& lhs.include_content == rhs.include_content
		&& lhs.include_symbols == rhs.include_symbols
		&& lhs.include_files == rhs.include_files;
}

gse::ide::search::query_driver::~query_driver() {
	cancel_pending();
}

auto gse::ide::search::query_driver::cancel_pending() -> void {
	if (m_pending) {
		m_pending->cancelled.store(true, std::memory_order_release);
		m_pending.reset();
	}
}

auto gse::ide::search::query_driver::update(const time now, const index_state* index, const options& opts) -> bool {
	const std::shared_ptr<const search_snapshot> snapshot = index ? index->query_snapshot() : nullptr;
	const std::uint64_t index_generation = snapshot ? snapshot->generation : 0;
	const bool query_changed = query != m_last_query;
	const bool options_changed = !same_options(opts, m_last_options);
	const bool index_changed = index_generation != m_last_index_generation;
	bool visible_changed = false;

	if (query_changed || options_changed || index_changed) {
		cancel_pending();
		m_last_query = query;
		m_last_options = opts;
		m_last_index_generation = index_generation;
		m_changed_at = now;
		m_dirty = !query.empty() && snapshot != nullptr;
		m_debounce = query_changed;
		selected = -1;
		if (!results.empty()) {
			results.clear();
			visible_changed = true;
		}
	}

	if (query.empty() || !snapshot) {
		m_dirty = false;
		return visible_changed;
	}

	if (m_dirty && (!m_debounce || now - m_changed_at > milliseconds(120))) {
		m_dirty = false;
		m_debounce = false;
		++m_active_request_id;
		m_pending = std::make_shared<query_buffer>();
		m_pending->request_id = m_active_request_id;
		m_pending->index_generation = snapshot->generation;
		engine::submit(m_pending, snapshot, query, opts);
	}

	if (!m_pending || !m_pending->done.load(std::memory_order_acquire)) {
		return visible_changed;
	}

	std::shared_ptr<query_buffer> completed = std::move(m_pending);
	const std::shared_ptr<const search_snapshot> current = index ? index->query_snapshot() : nullptr;
	if (completed->cancelled.load(std::memory_order_acquire) || completed->request_id != m_active_request_id || !current || completed->index_generation != current->generation) {
		if (current && completed->index_generation != current->generation) {
			m_last_index_generation = current->generation;
			m_dirty = true;
			m_debounce = false;
		}
		return visible_changed;
	}

	results = std::move(completed->results);
	selected = -1;
	return true;
}

auto gse::ide::search::query_driver::accept() -> void {
	cancel_pending();
	query.clear();
	m_last_query.clear();
	results.clear();
	selected = -1;
	m_dirty = false;
	m_debounce = false;
}
