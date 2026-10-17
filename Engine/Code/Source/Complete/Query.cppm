export module gse.complete:query;

import std;

import gse.core;
import gse.syntax;

import :context;
import :fuzzy;
import :model;

export namespace gse::complete {
	struct usage_table {
		std::unordered_map<std::string, std::uint32_t, transparent_hash, transparent_equal> words;
		std::unordered_map<std::string, std::uint32_t, transparent_hash, transparent_equal> members;

		[[nodiscard]] auto word_count(
			std::string_view name
		) const -> std::uint32_t;

		[[nodiscard]] auto member_count(
			std::string_view name
		) const -> std::uint32_t;
	};

	auto count_words(
		std::span<const std::string_view> lines,
		usage_table& into
	) -> void;

	struct sources {
		std::span<const model* const> models;
		std::span<const std::string_view> lines;
		const usage_table& usage;
	};

	struct candidate {
		std::string label;
		std::string detail;
		entity_kind kind = entity_kind::word;
		float score = 0.f;
		std::uint64_t matched = 0;
	};

	struct completion {
		context where;
		std::vector<candidate> items;
	};

	auto complete_at(
		const sources& from,
		position caret
	) -> completion;

	struct parameter_span {
		std::uint32_t start = 0;
		std::uint32_t length = 0;
	};

	struct signature {
		std::string label;
		std::vector<parameter_span> parameters;
	};

	struct signature_help {
		std::vector<signature> overloads;
		std::uint32_t active = 0;
		std::uint32_t argument = 0;
		position open;
	};

	auto signature_at(
		const sources& from,
		position caret
	) -> std::optional<signature_help>;

	auto signature_of(
		const entity& callable
	) -> signature;
}

namespace gse::complete {
	constexpr std::size_t candidate_cap = 200;
	constexpr std::uint32_t base_depth_limit = 8;
	constexpr float proximity_local = 30.f;
	constexpr float proximity_member = 24.f;
	constexpr float proximity_namespace = 10.f;
	constexpr float proximity_keyword = 4.f;
	constexpr float proximity_word = 0.f;
	constexpr float expected_type_bonus = 40.f;
	constexpr float usage_weight = 4.f;
	constexpr float prefix_bonus = 50.f;
	constexpr float case_bonus = 40.f;
	constexpr float exact_bonus = 1000.f;

	struct environment {
		const sources& from;
		position caret;
		std::vector<std::string> namespaces;
		std::string owner;
		std::vector<const entity*> locals;
	};

	struct target {
		std::optional<std::string> name_space;
		std::optional<type_ref> type;
		bool names_type = false;
		std::vector<const entity*> candidates;
	};

	auto environment_at(
		const sources& from,
		std::span<const syntax::token> tokens,
		position caret
	) -> environment;

	auto lexical_usings(
		std::span<const syntax::token> tokens
	) -> std::vector<std::string>;

	auto qualify(
		std::string_view name_space,
		std::string_view name
	) -> std::string;

	auto find_type(
		const environment& env,
		std::string_view key
	) -> const type_entry*;

	auto collect_members(
		const environment& env,
		std::string_view key,
		std::vector<const member*>& out,
		std::uint32_t depth = 0
	) -> void;

	auto members_named(
		const environment& env,
		std::string_view key,
		std::string_view name
	) -> std::vector<const member*>;

	auto member_type(
		const environment& env,
		std::string_view key,
		std::string_view name
	) -> std::optional<type_ref>;

	auto from_entities(
		std::span<const entity* const> entities
	) -> target;

	auto lookup_namespace(
		const environment& env,
		std::string_view name_space,
		std::string_view name
	) -> target;

	auto lookup_unqualified(
		const environment& env,
		std::string_view name
	) -> target;

	auto lookup_in(
		const environment& env,
		const target& outer,
		access_op via,
		std::string_view name
	) -> target;

	auto apply_postfix(
		const environment& env,
		target t,
		postfix p
	) -> target;

	auto resolve_chain(
		const environment& env,
		std::span<const link> links
	) -> target;

	struct name_lookup {
		target owner;
		target found;
	};

	auto lookup_last(
		const environment& env,
		std::span<const link> links
	) -> name_lookup;

	auto dereference(
		const environment& env,
		type_ref type
	) -> type_ref;

	auto normalize_type(
		std::string_view display
	) -> std::string;

	auto unqualified_name(
		std::string_view key
	) -> std::string_view;

	auto is_hidden_name(
		std::string_view name
	) -> bool;

	auto detail_of(
		const entity& e
	) -> std::string;

	auto expected_types(
		const environment& env,
		const context& where
	) -> std::vector<std::string>;
}

auto gse::complete::usage_table::word_count(const std::string_view name) const -> std::uint32_t {
	const auto found = words.find(name);
	return found == words.end() ? 0 : found->second;
}

auto gse::complete::usage_table::member_count(const std::string_view name) const -> std::uint32_t {
	const auto found = members.find(name);
	return found == members.end() ? 0 : found->second;
}

auto gse::complete::count_words(const std::span<const std::string_view> lines, usage_table& into) -> void {
	const syntax::lex_result lexed = syntax::tokenize(lines);
	bool after_access = false;
	for (const syntax::token& t : lexed.tokens) {
		if (t.type == syntax::token_type::identifier) {
			auto& table = after_access ? into.members : into.words;
			if (const auto found = table.find(t.text); found != table.end()) {
				++found->second;
			}
			else {
				table.emplace(std::string(t.text), 1u);
			}
		}
		after_access = t.type == syntax::token_type::punctuation && (t.text.ends_with(".") || t.text.ends_with("->"));
	}
}

auto gse::complete::qualify(const std::string_view name_space, const std::string_view name) -> std::string {
	if (name_space.empty()) {
		return std::string(name);
	}
	std::string out(name_space);
	out += "::";
	out += name;
	return out;
}

auto gse::complete::lexical_usings(const std::span<const syntax::token> tokens) -> std::vector<std::string> {
	std::vector<std::string> out;
	for (std::size_t i = 0; i + 2 < tokens.size(); ++i) {
		if (tokens[i].text != "using" || tokens[i + 1].text != "namespace" || tokens[i + 2].type != syntax::token_type::identifier) {
			continue;
		}
		std::string name(tokens[i + 2].text);
		std::size_t j = i + 3;
		while (j + 1 < tokens.size() && tokens[j].text == "::" && tokens[j + 1].type == syntax::token_type::identifier) {
			name += "::";
			name += tokens[j + 1].text;
			j += 2;
		}
		out.push_back(std::move(name));
	}
	return out;
}

auto gse::complete::environment_at(const sources& from, const std::span<const syntax::token> tokens, const position caret) -> environment {
	environment env{
		.from = from,
		.caret = caret,
	};
	env.namespaces.emplace_back();

	auto add_namespace = [&](const std::string_view name) {
		if (std::ranges::find(env.namespaces, name) == env.namespaces.end()) {
			env.namespaces.emplace_back(name);
		}
	};

	for (const model* m : from.models) {
		for (std::optional<std::uint32_t> at = m->innermost_scope(caret); at; at = m->scopes[*at].parent) {
			const scope& s = m->scopes[*at];
			const bool ordered = s.kind == scope_kind::block || s.kind == scope_kind::function;
			for (const std::uint32_t index : m->declarations_by_scope[*at]) {
				const declaration& d = m->declarations[index];
				if (!ordered || d.at < caret) {
					env.locals.push_back(&d.what);
				}
			}
			for (const std::string& name : s.usings) {
				add_namespace(name);
			}
			if (s.kind == scope_kind::name_space) {
				for (std::size_t split = s.owner.find("::"); split != std::string::npos; split = s.owner.find("::", split + 2)) {
					add_namespace(std::string_view(s.owner).substr(0, split));
				}
				add_namespace(s.owner);
			}
			else if (env.owner.empty() && !s.owner.empty()) {
				env.owner = s.owner;
			}
		}
	}

	for (const std::string& name : lexical_usings(tokens)) {
		add_namespace(name);
	}
	return env;
}

auto gse::complete::find_type(const environment& env, const std::string_view key) -> const type_entry* {
	for (const model* m : env.from.models) {
		if (const type_entry* found = m->find_type(key)) {
			return found;
		}
	}
	return nullptr;
}

auto gse::complete::collect_members(const environment& env, const std::string_view key, std::vector<const member*>& out, const std::uint32_t depth) -> void {
	if (depth > base_depth_limit) {
		return;
	}
	const type_entry* type = find_type(env, key);
	if (!type) {
		return;
	}
	for (const member& m : type->members) {
		out.push_back(&m);
	}
	for (const std::string& base : type->bases) {
		collect_members(env, base, out, depth + 1);
	}
}

auto gse::complete::members_named(const environment& env, const std::string_view key, const std::string_view name) -> std::vector<const member*> {
	std::vector<const member*> all;
	collect_members(env, key, all);
	std::erase_if(all, [&](const member* m) {
		return m->what.name != name;
	});
	return all;
}

auto gse::complete::member_type(const environment& env, const std::string_view key, const std::string_view name) -> std::optional<type_ref> {
	const std::vector<const member*> found = members_named(env, key, name);
	if (found.empty()) {
		return std::nullopt;
	}
	return found.front()->what.type;
}

auto gse::complete::from_entities(const std::span<const entity* const> entities) -> target {
	target out;
	if (entities.empty()) {
		return out;
	}
	const entity& first = *entities.front();
	out.candidates.assign(entities.begin(), entities.end());
	out.type = first.type;
	out.names_type = first.kind == entity_kind::type;
	return out;
}

auto gse::complete::lookup_namespace(const environment& env, const std::string_view name_space, const std::string_view name) -> target {
	std::vector<const entity*> found;
	for (const model* m : env.from.models) {
		for (const std::uint32_t index : m->namespace_members(name_space)) {
			const entity& e = m->namespace_entries[index].what;
			if (e.name != name) {
				continue;
			}
			if (e.kind == entity_kind::name_space) {
				return { .name_space = qualify(name_space, name) };
			}
			found.push_back(&e);
		}
	}
	if (!found.empty()) {
		return from_entities(found);
	}
	const std::string key = qualify(name_space, name);
	if (find_type(env, key)) {
		return {
			.type = type_ref{ .key = key, .display = key },
			.names_type = true,
		};
	}
	return {};
}

auto gse::complete::lookup_unqualified(const environment& env, const std::string_view name) -> target {
	if (name == "this" && !env.owner.empty()) {
		return { .type = type_ref{ .key = env.owner, .indirection = 1, .display = env.owner } };
	}

	std::vector<const entity*> locals;
	for (const entity* e : env.locals) {
		if (e->name == name) {
			locals.push_back(e);
		}
	}
	if (!locals.empty()) {
		return from_entities(locals);
	}

	if (!env.owner.empty()) {
		std::vector<const entity*> members;
		for (const member* m : members_named(env, env.owner, name)) {
			members.push_back(&m->what);
		}
		if (!members.empty()) {
			return from_entities(members);
		}
	}

	for (const std::string& name_space : env.namespaces | std::views::reverse) {
		target found = lookup_namespace(env, name_space, name);
		if (found.name_space || found.type || !found.candidates.empty()) {
			return found;
		}
	}
	return {};
}

auto gse::complete::dereference(const environment& env, type_ref type) -> type_ref {
	if (type.indirection > 0) {
		--type.indirection;
		return type;
	}
	if (std::optional<type_ref> arrow = member_type(env, type.key, "operator->")) {
		if (arrow->indirection > 0) {
			--arrow->indirection;
		}
		return *arrow;
	}
	return type;
}

auto gse::complete::lookup_in(const environment& env, const target& outer, const access_op via, const std::string_view name) -> target {
	if (via == access_op::scope && outer.name_space) {
		return lookup_namespace(env, *outer.name_space, name);
	}
	if (!outer.type) {
		return {};
	}
	const type_ref owner = via == access_op::arrow ? dereference(env, *outer.type) : *outer.type;
	std::vector<const entity*> found;
	for (const member* m : members_named(env, owner.key, name)) {
		found.push_back(&m->what);
	}
	if (!found.empty()) {
		return from_entities(found);
	}
	if (via == access_op::scope) {
		const std::string key = qualify(owner.key, name);
		if (find_type(env, key)) {
			return {
				.type = type_ref{ .key = key, .display = key },
				.names_type = true,
			};
		}
	}
	return {};
}

auto gse::complete::apply_postfix(const environment& env, target t, const postfix p) -> target {
	if (!t.type) {
		return {};
	}
	if (p == postfix::call) {
		const bool callable = !t.candidates.empty() && info_of(t.candidates.front()->kind).callable;
		if (!callable && !t.names_type) {
			if (std::optional<type_ref> result = member_type(env, t.type->key, "operator()")) {
				return { .type = std::move(result) };
			}
			return {};
		}
		return { .type = t.type };
	}
	if (t.type->indirection > 0) {
		type_ref element = *t.type;
		--element.indirection;
		return { .type = std::move(element) };
	}
	if (std::optional<type_ref> element = member_type(env, t.type->key, "operator[]")) {
		return { .type = std::move(element) };
	}
	return {};
}

auto gse::complete::resolve_chain(const environment& env, const std::span<const link> links) -> target {
	target t;
	for (std::size_t i = 0; i < links.size(); ++i) {
		const link& l = links[i];
		if (i == 0) {
			t = l.via == access_op::scope ? lookup_namespace(env, "", l.name) : lookup_unqualified(env, l.name);
		}
		else {
			t = lookup_in(env, t, l.via, l.name);
		}
		for (const postfix p : l.postfixes) {
			t = apply_postfix(env, std::move(t), p);
		}
		if (!t.name_space && !t.type) {
			return {};
		}
	}
	return t;
}

auto gse::complete::lookup_last(const environment& env, const std::span<const link> links) -> name_lookup {
	const link& last = links.back();
	if (links.size() == 1) {
		return {
			.found = last.via == access_op::scope ? lookup_namespace(env, "", last.name) : lookup_unqualified(env, last.name),
		};
	}
	name_lookup out{
		.owner = resolve_chain(env, links.first(links.size() - 1)),
	};
	out.found = lookup_in(env, out.owner, last.via, last.name);
	return out;
}

auto gse::complete::normalize_type(const std::string_view display) -> std::string {
	std::string out;
	out.reserve(display.size());
	std::string_view rest = display;
	while (!rest.empty()) {
		if (rest.starts_with("const ")) {
			rest.remove_prefix(6);
			continue;
		}
		if (rest.starts_with("volatile ")) {
			rest.remove_prefix(9);
			continue;
		}
		const char c = rest.front();
		rest.remove_prefix(1);
		if (c != '&' && c != ' ') {
			out.push_back(c);
		}
	}
	while (out.ends_with("const")) {
		out.resize(out.size() - 5);
	}
	return out;
}

auto gse::complete::unqualified_name(const std::string_view key) -> std::string_view {
	std::string_view base = key.substr(0, key.find('<'));
	const std::size_t split = base.rfind("::");
	return split == std::string_view::npos ? base : base.substr(split + 2);
}

auto gse::complete::is_hidden_name(const std::string_view name) -> bool {
	return name.empty() || name.starts_with("__") || name.starts_with("_M_") || name.starts_with("operator") || name.contains('<') || name.contains('~') || name.contains(' ');
}

auto gse::complete::signature_of(const entity& callable) -> signature {
	signature out;
	out.label = callable.name;
	out.label += '(';
	for (std::size_t i = 0; i < callable.params.size(); ++i) {
		if (i > 0) {
			out.label += ", ";
		}
		const parameter& p = callable.params[i];
		const auto start = static_cast<std::uint32_t>(out.label.size());
		out.label += p.type;
		if (!p.name.empty()) {
			out.label += ' ';
			out.label += p.name;
		}
		out.parameters.push_back({
			.start = start,
			.length = static_cast<std::uint32_t>(out.label.size()) - start,
		});
	}
	out.label += ')';
	if (!callable.type.display.empty()) {
		out.label += " -> ";
		out.label += callable.type.display;
	}
	return out;
}

auto gse::complete::detail_of(const entity& e) -> std::string {
	if (info_of(e.kind).callable) {
		std::string out = "(";
		for (std::size_t i = 0; i < e.params.size(); ++i) {
			if (i > 0) {
				out += ", ";
			}
			out += e.params[i].type;
		}
		out += ")";
		if (!e.type.display.empty()) {
			out += " -> ";
			out += e.type.display;
		}
		return out;
	}
	if (e.kind == entity_kind::keyword) {
		return "keyword";
	}
	return e.type.display;
}

auto gse::complete::expected_types(const environment& env, const context& where) -> std::vector<std::string> {
	std::vector<std::string> out;
	auto add = [&](const std::string_view display) {
		std::string type = normalize_type(display);
		if (!type.empty() && std::ranges::find(out, type) == out.end()) {
			out.push_back(std::move(type));
		}
	};

	if (!where.assigned.empty()) {
		if (const target t = resolve_chain(env, where.assigned); t.type) {
			add(t.type->display);
		}
	}
	if (where.after_return) {
		for (const model* m : env.from.models) {
			for (std::optional<std::uint32_t> at = m->innermost_scope(env.caret); at; at = m->scopes[*at].parent) {
				if (m->scopes[*at].kind == scope_kind::function) {
					add(m->scopes[*at].result.display);
					break;
				}
			}
		}
	}
	if (!where.call) {
		return out;
	}
	for (const entity* e : lookup_last(env, where.call->callee).found.candidates) {
		if (where.call->argument < e->params.size()) {
			add(e->params[where.call->argument].type);
		}
	}
	return out;
}

auto gse::complete::complete_at(const sources& from, const position caret) -> completion {
	completion out{
		.where = context_at(from.lines, caret),
	};
	const context& where = out.where;
	if (where.kind == trigger::none) {
		return out;
	}

	const syntax::lex_result lexed = syntax::tokenize(from.lines);
	const environment env = environment_at(from, lexed.tokens, caret);
	const std::string query = to_lower(where.prefix);
	const std::vector<std::string> expected = expected_types(env, where);

	std::unordered_map<std::string, std::size_t, transparent_hash, transparent_equal> by_label;
	std::unordered_map<std::string, std::uint32_t, transparent_hash, transparent_equal> overloads;

	auto offer = [&](const std::string_view label, const entity_kind kind, const float proximity, const std::string_view detail, const std::string_view type_display, const std::uint32_t usage) {
		if (is_hidden_name(label)) {
			return;
		}
		const std::string lower = to_lower(label);
		const score_result fuzzy = fuzzy_match(query, label, lower);
		if (!fuzzy.matched) {
			return;
		}
		float score = fuzzy.score * 100.f + proximity + std::log2(1.f + static_cast<float>(usage)) * usage_weight;
		if (!query.empty() && lower.starts_with(query)) {
			score += label == where.prefix ? exact_bonus : prefix_bonus;
		}
		if (!type_display.empty() && std::ranges::find(expected, normalize_type(type_display)) != expected.end()) {
			score += expected_type_bonus;
		}
		std::uint64_t matched = 0;
		std::size_t typed = 0;
		std::size_t same_case = 0;
		for (const match_range& r : fuzzy.ranges) {
			for (std::uint32_t i = r.start; i < r.start + r.length; ++i) {
				if (i < 64) {
					matched |= std::uint64_t{ 1 } << i;
				}
				if (label[i] == where.prefix[typed++]) {
					++same_case;
				}
			}
		}
		if (!where.prefix.empty()) {
			score += case_bonus * static_cast<float>(same_case) / static_cast<float>(where.prefix.size());
		}
		if (const auto found = by_label.find(label); found != by_label.end()) {
			candidate& existing = out.items[found->second];
			if (kind == existing.kind && info_of(kind).callable) {
				++overloads[existing.label];
			}
			const bool demotes = kind == entity_kind::word && existing.kind != entity_kind::word;
			if (!demotes && (score > existing.score || existing.kind == entity_kind::word)) {
				existing.score = score;
				existing.kind = kind;
				existing.detail = std::string(detail);
			}
			return;
		}
		by_label.emplace(std::string(label), out.items.size());
		out.items.push_back({
			.label = std::string(label),
			.detail = std::string(detail),
			.kind = kind,
			.score = score,
			.matched = matched,
		});
	};

	auto offer_entity = [&](const entity& e, const float proximity, const std::uint32_t usage) {
		offer(e.name, e.kind, proximity, detail_of(e), e.type.display, usage);
	};

	auto offer_members = [&](const std::string_view key, const bool statics_only) {
		std::vector<const member*> members;
		collect_members(env, key, members);
		const bool inside = env.owner == key;
		const std::string_view constructor = unqualified_name(key);
		for (const member* m : members) {
			if (m->access != member_access::everyone && !inside) {
				continue;
			}
			if (m->what.name == constructor) {
				continue;
			}
			if (statics_only && !m->is_static && m->what.kind != entity_kind::enumerator && m->what.kind != entity_kind::type) {
				continue;
			}
			offer_entity(m->what, proximity_member, from.usage.member_count(m->what.name));
		}
	};

	auto offer_namespace = [&](const std::string_view name_space, const float proximity) {
		for (const model* m : from.models) {
			for (const std::uint32_t index : m->namespace_members(name_space)) {
				const entity& e = m->namespace_entries[index].what;
				offer_entity(e, proximity, from.usage.word_count(e.name));
			}
		}
	};

	auto offer_words = [&](const bool members_only) {
		std::unordered_map<std::string_view, std::uint32_t> counts;
		bool after_access = false;
		for (const syntax::token& t : lexed.tokens) {
			if (t.type == syntax::token_type::identifier && (!members_only || after_access)) {
				const bool is_prefix = t.line == where.replace_from.line && t.start == where.replace_from.column;
				if (!is_prefix) {
					++counts[t.text];
				}
			}
			after_access = t.type == syntax::token_type::punctuation && (t.text.ends_with(".") || t.text.ends_with("->"));
		}
		for (const auto& [word, count] : counts) {
			if (word == where.prefix) {
				continue;
			}
			const std::uint32_t usage = count + (members_only ? from.usage.member_count(word) : from.usage.word_count(word));
			offer(word, entity_kind::word, proximity_word, {}, {}, usage);
		}
	};

	if (where.kind == trigger::member) {
		bool resolved = false;
		if (where.receiver_resolvable && !where.receiver.empty()) {
			const target t = resolve_chain(env, where.receiver);
			if (t.type) {
				const type_ref owner = where.op == access_op::arrow ? dereference(env, *t.type) : *t.type;
				resolved = find_type(env, owner.key) != nullptr;
				offer_members(owner.key, false);
			}
		}
		if (!resolved) {
			offer_words(true);
		}
	}
	else if (where.kind == trigger::scope) {
		if (where.receiver.empty()) {
			offer_namespace("", proximity_namespace);
		}
		else {
			const target t = resolve_chain(env, where.receiver);
			if (t.name_space) {
				offer_namespace(*t.name_space, proximity_namespace);
			}
			else if (t.type) {
				offer_members(t.type->key, true);
			}
		}
	}
	else {
		for (const entity* e : env.locals) {
			offer_entity(*e, proximity_local, from.usage.word_count(e->name));
		}
		if (!env.owner.empty()) {
			offer_members(env.owner, false);
		}
		for (const std::string& name_space : env.namespaces) {
			offer_namespace(name_space, proximity_namespace);
		}
		for (const std::string_view word : syntax::reserved_words()) {
			offer(word, entity_kind::keyword, proximity_keyword, "keyword", {}, from.usage.word_count(word));
		}
		offer_words(false);
	}

	for (candidate& c : out.items) {
		if (const auto found = overloads.find(c.label); found != overloads.end()) {
			c.detail += std::format("  +{}", found->second);
		}
	}

	std::ranges::sort(out.items, [](const candidate& a, const candidate& b) {
		if (a.score != b.score) {
			return a.score > b.score;
		}
		return a.label < b.label;
	});
	if (out.items.size() > candidate_cap) {
		out.items.resize(candidate_cap);
	}
	return out;
}

auto gse::complete::signature_at(const sources& from, const position caret) -> std::optional<signature_help> {
	const context where = context_at(from.lines, caret);
	if (!where.call) {
		return std::nullopt;
	}
	const syntax::lex_result lexed = syntax::tokenize(from.lines);
	const environment env = environment_at(from, lexed.tokens, caret);
	const target t = lookup_last(env, where.call->callee).found;

	std::vector<const entity*> callables;
	if (t.names_type && t.type) {
		for (const member* m : members_named(env, t.type->key, unqualified_name(t.type->key))) {
			callables.push_back(&m->what);
		}
	}
	else {
		for (const entity* e : t.candidates) {
			if (info_of(e->kind).callable) {
				callables.push_back(e);
			}
		}
	}
	if (callables.empty()) {
		return std::nullopt;
	}

	signature_help out{
		.argument = where.call->argument,
		.open = where.call->open,
	};
	std::optional<std::uint32_t> active;
	for (const entity* e : callables) {
		if (!active && where.call->argument < e->params.size()) {
			active = static_cast<std::uint32_t>(out.overloads.size());
		}
		out.overloads.push_back(signature_of(*e));
	}
	out.active = active.value_or(0);
	return out;
}
