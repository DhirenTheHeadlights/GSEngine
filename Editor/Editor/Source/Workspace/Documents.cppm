export module gse.ide.workspace:documents;

import std;
import gse;

import gse.ide.analysis;
import gse.ide.highlight;
import gse.ide.diagnostic;

export namespace gse::ide {
	enum class document_persistence {
		clean,
		dirty,
		conflicted,
	};

	enum class document_view {
		source,
		rendered,
	};

	struct rendered_view {
		markdown::rendered_document content;
		document_revision revision;
		float width = 0.f;
		float font_size = 0.f;
		std::uint64_t style_key = 0;
		gui::text_area_state view;
		bool built = false;
	};

	struct document {
		std::filesystem::path path;
		std::string tab_name;
		gui::text_buffer buffer;
		document_revision revision;
		gui::text_area_state view;
		document_persistence persistence = document_persistence::clean;
		std::string persistence_error;
		std::optional<std::filesystem::file_time_type> disk_write_time;
		syntax_producer::data syntax;
		bool highlight_dirty = true;
		document_language language = document_language::plain;
		document_view mode = document_view::rendered;
		rendered_view rendered;
		std::vector<diagnostic> diagnostics;
		std::vector<diagnostic> lint;
		analysis::diagnostics_status analysis_status = analysis::diagnostics_status::not_analyzed;
		std::string analysis_detail;
		time analysis_duration{};
		bool diag_dirty = true;
		std::optional<clock> edit_clock;
		std::optional<std::uint32_t> pending_center_line;
	};

	class open_documents {
	public:
		auto begin(
			this auto& self
		) -> decltype(auto);

		auto end(
			this auto& self
		) -> decltype(auto);

		auto find(
			this auto& self,
			id document_id
		) -> decltype(auto);

		auto at(
			this auto& self,
			id document_id
		) -> decltype(auto);

		[[nodiscard]] auto empty() const -> bool;

		[[nodiscard]] auto order() const -> std::span<const id>;

		[[nodiscard]] auto active_document_id() const -> std::optional<id>;

		[[nodiscard]] auto active_tab_id() const -> id;

		auto add(
			document doc
		) -> id;

		auto activate(
			id document_id
		) -> void;

		auto reorder(
			id document_id,
			std::size_t index
		) -> void;

		auto erase(
			id document_id
		) -> void;

	private:
		struct no_document {};

		struct document_view {
			id document_id;
		};

		using active_view = std::variant<no_document, document_view>;

		static constexpr id game_tab_id = id_of<"ide.workspace.game_tab">();

		std::unordered_map<id, document> m_documents;
		std::vector<id> m_order;
		active_view m_active = no_document{};
		std::uint64_t m_next_document_sequence = 1;
	};
}

auto gse::ide::open_documents::begin(this auto& self) -> decltype(auto) {
	return self.m_documents.begin();
}

auto gse::ide::open_documents::end(this auto& self) -> decltype(auto) {
	return self.m_documents.end();
}

auto gse::ide::open_documents::find(this auto& self, const id document_id) -> decltype(auto) {
	return self.m_documents.find(document_id);
}

auto gse::ide::open_documents::at(this auto& self, const id document_id) -> decltype(auto) {
	return self.m_documents.at(document_id);
}

auto gse::ide::open_documents::empty() const -> bool {
	return m_documents.empty();
}

auto gse::ide::open_documents::order() const -> std::span<const id> {
	return m_order;
}

auto gse::ide::open_documents::active_document_id() const -> std::optional<id> {
	if (const document_view* view = std::get_if<document_view>(&m_active)) {
		return view->document_id;
	}
	return std::nullopt;
}

auto gse::ide::open_documents::active_tab_id() const -> id {
	return active_document_id().value_or(game_tab_id);
}

auto gse::ide::open_documents::add(document doc) -> id {
	id document_id;
	do {
		document_id = generate_temp_id(hash_combine(stable_id("ide.workspace.document"), m_next_document_sequence++));
	} while (m_documents.contains(document_id));
	m_documents.emplace(document_id, std::move(doc));
	m_order.push_back(document_id);
	m_active = document_view{ .document_id = document_id };
	return document_id;
}

auto gse::ide::open_documents::activate(const id document_id) -> void {
	if (m_documents.contains(document_id)) {
		m_active = document_view{ .document_id = document_id };
	}
}

auto gse::ide::open_documents::reorder(const id document_id, const std::size_t index) -> void {
	const auto current = std::ranges::find(m_order, document_id);
	if (current == m_order.end()) {
		return;
	}
	m_order.erase(current);
	m_order.insert(m_order.begin() + static_cast<std::ptrdiff_t>(std::min(index, m_order.size())), document_id);
}

auto gse::ide::open_documents::erase(const id document_id) -> void {
	const auto order_it = std::ranges::find(m_order, document_id);
	if (order_it != m_order.end() && active_document_id() == document_id) {
		if (const auto next = std::next(order_it); next != m_order.end()) {
			m_active = document_view{ .document_id = *next };
		}
		else if (order_it != m_order.begin()) {
			m_active = document_view{ .document_id = *std::prev(order_it) };
		}
		else {
			m_active = no_document{};
		}
	}
	if (order_it != m_order.end()) {
		m_order.erase(order_it);
	}
	m_documents.erase(document_id);
}