#include "gcc-plugin.h"
#include "plugin-version.h"
#include "coretypes.h"
#include "tree.h"
#include "intl.h"
#include "stringpool.h"
#include "cp/cp-tree.h"
#include "cp/name-lookup.h"
#include "hash-set.h"
#include "hash-map.h"
#include <stdarg.h>

int plugin_is_GPL_compatible;

static FILE *g_out = nullptr;
static const char *g_roots[8] = {};
static size_t g_root_lens[8] = {};
static size_t g_root_count = 0;
static hash_set<tree> *g_walked = nullptr;
static hash_set<tree> *g_visited_ns = nullptr;
static hash_set<tree> *g_emitted = nullptr;
static hash_set<tree> *g_emitted_defs = nullptr;
static tree g_scope = nullptr;
static tree g_function = nullptr;
static tree g_class_context = nullptr;
static auto_vec<tree, 32> *g_local_decls = nullptr;
static location_t g_use_anchor = UNKNOWN_LOCATION;
static char *g_src = nullptr;
static long g_src_len = 0;
static auto_vec<long> *g_src_lines = nullptr;
static hash_map<tree, tree> *g_lexical_ns = nullptr;

/* Per-line fprintf was half the plugin's index-mode cost (stream lock +
   format parse per call).  All token output goes through this buffer instead;
   only %d and %s exist in the emission formats.  */
static char g_out_buffer[1 << 14];
static int g_out_length = 0;

static void out_flush() {
	if (g_out_length > 0) {
		fwrite(g_out_buffer, 1, (size_t)g_out_length, g_out ? g_out : stderr);
		g_out_length = 0;
	}
}

static void out_reserve(int needed) {
	if (g_out_length + needed > (int)sizeof(g_out_buffer)) {
		out_flush();
	}
}

static void out_char(char c) {
	out_reserve(1);
	g_out_buffer[g_out_length++] = c;
}

static void out_str(const char *s) {
	const size_t n = strlen(s);
	if (n >= sizeof(g_out_buffer)) {
		out_flush();
		fwrite(s, 1, n, g_out ? g_out : stderr);
		return;
	}
	out_reserve((int)n);
	memcpy(g_out_buffer + g_out_length, s, n);
	g_out_length += (int)n;
}

static void out_int(int v) {
	char digits[16];
	int count = 0;
	unsigned magnitude = v < 0 ? 0u - (unsigned)v : (unsigned)v;
	do {
		digits[count++] = (char)('0' + magnitude % 10u);
		magnitude /= 10u;
	} while (magnitude);
	out_reserve(count + 1);
	if (v < 0) {
		g_out_buffer[g_out_length++] = '-';
	}
	while (count > 0) {
		g_out_buffer[g_out_length++] = digits[--count];
	}
}

static void out_printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	for (const char *p = fmt; *p; ++p) {
		if (*p != '%') {
			out_char(*p);
			continue;
		}
		++p;
		if (*p == 'd') {
			out_int(va_arg(args, int));
		}
		else if (*p == 's') {
			out_str(va_arg(args, const char *));
		}
		else {
			gcc_unreachable();
		}
	}
	va_end(args);
}

static void record_lexical_namespace(tree decl) {
	if (!decl || !DECL_P(decl) || !scope_chain || !current_namespace) {
		return;
	}
	if (!g_lexical_ns) {
		g_lexical_ns = new hash_map<tree, tree>();
	}
	g_lexical_ns->put(decl, current_namespace);
}

static tree lexical_namespace_of(tree decl) {
	if (g_lexical_ns) {
		if (tree *slot = g_lexical_ns->get(decl)) {
			return *slot;
		}
	}
	return global_namespace;
}

static bool ensure_source();
static bool qual_ident_char(char c);

struct cached_file {
	char *path;
	char *data;
	long len;
	auto_vec<long> *line_starts;
};

static auto_vec<cached_file, 16> *g_files = nullptr;

static bool g_index_only = false;
static bool g_audit = false;

enum class source_token_kind {
	identifier,
	scope,
	left_brace,
	right_brace,
	semicolon,
	left_paren,
	right_paren,
	comma,
	less,
	greater,
	other
};

struct source_token {
	source_token_kind kind = source_token_kind::other;
	long start = 0;
	long finish = 0;
	int line = 0;
	int column = 0;
	int byte_column = 0;
	int pair = -1;
	bool binding_scope = false;
	tree ident = NULL_TREE;
};

/* Dedupe keys pack (line, column, length) injectively: line maps far past any
   real file and the line table caps columns well below 16 bits.  */
struct token_span_hash : int_hash<uint64_t, 0> {
	static hashval_t hash(uint64_t v) { return (hashval_t)((v ^ (v >> 32)) * 0x9E3779B9u); }
};

static uint64_t token_span_key(int line, int column, int length) {
	return ((uint64_t)(uint32_t)line << 32) | ((uint64_t)(uint16_t)column << 16) | (uint16_t)length;
}

static auto_vec<source_token, 256> *g_source_tokens = nullptr;
static hash_set<uint64_t, false, token_span_hash> *g_emitted_qualifiers = nullptr;

static tree walk_cb(tree *tp, int *walk_subtrees, void *);
static void walk_fn_body(tree fndecl);
static tree body_walk_identity(tree fndecl);
static void process_type_decl(tree d);
static bool is_coroutine_actor(tree fndecl);

/* walk_tree_without_duplicates allocates a fresh visited-set per call, and
   walk_cb re-roots a walk at every located expression node.  Pool the sets by
   recursion depth instead: each call still gets its own empty dedup domain,
   so traversal (and output) is unchanged, only the allocation churn goes.  */
static auto_vec<hash_set<tree> *, 8> *g_walk_psets = nullptr;
static unsigned g_walk_depth = 0;

static void walk_expr_tree(tree *tp) {
	if (!g_walk_psets) {
		g_walk_psets = new auto_vec<hash_set<tree> *, 8>();
	}
	if (g_walk_psets->length() <= g_walk_depth) {
		g_walk_psets->safe_push(new hash_set<tree>());
	}
	hash_set<tree> *pset = (*g_walk_psets)[g_walk_depth];
	pset->empty();
	++g_walk_depth;
	walk_tree(tp, walk_cb, nullptr, pset);
	--g_walk_depth;
}

static char ascii_lower(char c) {
	return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static bool same_file(const char *a, const char *b) {
	if (!a || !b) return false;
	for (;; ++a, ++b) {
		char ca = (*a == '\\') ? '/' : ascii_lower(*a);
		char cb = (*b == '\\') ? '/' : ascii_lower(*b);
		if (ca != cb) return false;
		if (!ca) return true;
	}
}

/* expand_location file names are interned in the line table, so a small
   pointer-keyed memo skips the char-by-char path comparison on the hot
   emission gates without changing any result.  */
static bool same_file_as_main(const char *file) {
	if (!file || !main_input_filename) return false;
	static const char *keys[4] = {};
	static bool vals[4] = {};
	static unsigned next = 0;
	for (unsigned i = 0; i < 4; ++i) {
		if (keys[i] == file) return vals[i];
	}
	const bool result = same_file(file, main_input_filename);
	keys[next] = file;
	vals[next] = result;
	next = (next + 1) & 3;
	return result;
}

static bool skipped_path_segment(const char *segment, size_t length) {
	static const char *names[] = {
		"out", ".git", ".vs", ".vscode", ".claude", "external", "vcpkg",
		".gcc-ci-build", "build", "node_modules", ".cache"
	};
	for (const char *name : names) {
		const size_t name_length = strlen(name);
		if (name_length != length) continue;
		bool equal = true;
		for (size_t i = 0; i < length; ++i) {
			if (ascii_lower(segment[i]) != name[i]) {
				equal = false;
				break;
			}
		}
		if (equal) return true;
	}
	return false;
}

static bool under_root(const char *file);

static bool under_root_cached(const char *file) {
	if (!file || !g_root_count) return false;
	static const char *keys[256] = {};
	static bool vals[256] = {};
	const unsigned slot = (unsigned)(((uintptr_t)file * 2654435761u) >> 20) & 255;
	if (keys[slot] == file) return vals[slot];
	const bool result = under_root(file);
	keys[slot] = file;
	vals[slot] = result;
	return result;
}

static bool main_file_under_root() {
	static int cached = -1;
	if (cached < 0) {
		cached = under_root(main_input_filename) ? 1 : 0;
	}
	return cached == 1;
}

static bool under_one_root(const char *file, const char *root, size_t root_len) {
	for (size_t i = 0; i < root_len; ++i) {
		char a = file[i];
		if (!a) return false;
		if (a == '\\') a = '/';
		char b = root[i];
		if (b == '\\') b = '/';
		if (ascii_lower(a) != ascii_lower(b)) return false;
	}
	if (root_len > 0 && root[root_len - 1] != '/' && root[root_len - 1] != '\\' && file[root_len] != '/' && file[root_len] != '\\' && file[root_len] != 0) {
		return false;
	}
	const char *segment = file + root_len;
	while (*segment) {
		while (*segment == '/' || *segment == '\\') ++segment;
		const char *end = segment;
		while (*end && *end != '/' && *end != '\\') ++end;
		if (skipped_path_segment(segment, (size_t)(end - segment))) return false;
		segment = end;
	}
	return true;
}

static bool under_root(const char *file) {
	if (!file) return false;
	for (size_t i = 0; i < g_root_count; ++i) {
		if (under_one_root(file, g_roots[i], g_root_lens[i])) return true;
	}
	return false;
}

static cached_file *get_cached_file(const char *path) {
	if (!path) return nullptr;
	static const char *keys[256] = {};
	static int vals[256] = {};
	if (!g_files) g_files = new auto_vec<cached_file, 16>();
	const unsigned slot = (unsigned)(((uintptr_t)path * 2654435761u) >> 20) & 255;
	if (keys[slot] == path) return &(*g_files)[vals[slot]];
	for (unsigned i = 0; i < g_files->length(); ++i) {
		if (same_file((*g_files)[i].path, path)) {
			keys[slot] = path;
			vals[slot] = (int)i;
			return &(*g_files)[i];
		}
	}
	cached_file entry{ xstrdup(path), nullptr, 0, nullptr };
	if (same_file_as_main(path) && ensure_source()) {
		entry.data = g_src;
		entry.len = g_src_len;
	}
	else if (FILE *f = fopen(path, "rb")) {
		fseek(f, 0, SEEK_END);
		const long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (n > 0) {
			entry.data = (char *)xmalloc(n + 1);
			entry.len = (long)fread(entry.data, 1, n, f);
			entry.data[entry.len] = 0;
		}
		fclose(f);
	}
	g_files->safe_push(entry);
	keys[slot] = path;
	vals[slot] = (int)g_files->length() - 1;
	return &g_files->last();
}

static const char *file_line(const char *path, int line, int *out_len) {
	*out_len = 0;
	if (line <= 0) return nullptr;
	cached_file *cf = get_cached_file(path);
	if (!cf || !cf->data) return nullptr;
	if (!cf->line_starts) {
		cf->line_starts = new auto_vec<long>();
		cf->line_starts->safe_push(0);
		for (long i = 0; i < cf->len; ++i)
			if (cf->data[i] == '\n') cf->line_starts->safe_push(i + 1);
	}
	if (line - 1 >= (int)cf->line_starts->length()) return nullptr;
	const long start = (*cf->line_starts)[line - 1];
	const long stop = line < (int)cf->line_starts->length() ? (*cf->line_starts)[line] : cf->len;
	const char *p = cf->data + start;
	int n = (int)(stop - start);
	while (n > 0 && (p[n - 1] == '\n' || p[n - 1] == '\r')) --n;
	*out_len = n;
	return p;
}

static int name_byte_column(const char *file, int line, int start_col, const char *name, int name_len) {
	if (!file || !name || name_len <= 0 || line <= 0 || start_col <= 0) return start_col;
	int n = 0;
	const char *p = file_line(file, line, &n);
	if (!p) return start_col;
	const int from = start_col - 1 <= n ? start_col - 1 : 0;
	for (int i = from; i + name_len <= n; ++i) {
		if (strncmp(p + i, name, name_len) != 0) continue;
		if (i > 0 && qual_ident_char(p[i - 1])) continue;
		if (i + name_len < n && qual_ident_char(p[i + name_len])) continue;
		if (i + name_len + 1 < n && p[i + name_len] == ':' && p[i + name_len + 1] == ':') continue;
		return i + 1;
	}
	return start_col;
}

static const char *kind_of(tree t) {
	switch (TREE_CODE(t)) {
		case VAR_DECL: {
			tree ctx = DECL_CONTEXT(t);
			if (!ctx || TREE_CODE(ctx) == NAMESPACE_DECL || TREE_CODE(ctx) == TRANSLATION_UNIT_DECL)
				return "global";
			return "variable";
		}
		case PARM_DECL: return DECL_TEMPLATE_PARM_P(t) ? "type" : "parameter";
		case FUNCTION_DECL: return "function";
		case FIELD_DECL: return "member";
		case TYPE_DECL: return "type";
		case CONST_DECL: return "enum_member";
		case NAMESPACE_DECL: return "name_space";
		default: return nullptr;
	}
}

static int ident_len(tree decl) {
	tree id = DECL_NAME(decl);
	if (id && TREE_CODE(id) == IDENTIFIER_NODE) return (int)IDENTIFIER_LENGTH(id);
	return 0;
}

/* Locations are append-only in the line table, so an expansion never changes
   once computed; a direct-mapped memo is exact.  Key 0 is the empty-slot
   marker, which is why UNKNOWN_LOCATION bypasses the cache.  */
static location_t g_expand_keys[2048];
static expanded_location g_expand_vals[2048];

static expanded_location expand_cached(location_t loc) {
	if (loc == UNKNOWN_LOCATION) {
		return expand_location(loc);
	}
	const unsigned slot = ((unsigned)loc * 2654435761u) >> 21;
	if (g_expand_keys[slot] == loc) {
		return g_expand_vals[slot];
	}
	const expanded_location xl = expand_location(loc);
	g_expand_keys[slot] = loc;
	g_expand_vals[slot] = xl;
	return xl;
}

static bool in_main_file(location_t loc) {
	if (loc == UNKNOWN_LOCATION || !main_input_filename) return false;
	expanded_location xl = expand_cached(loc);
	return xl.file && same_file_as_main(xl.file);
}

static bool expand_main(location_t loc, expanded_location *out) {
	if (loc == UNKNOWN_LOCATION || !main_input_filename) return false;
	*out = expand_cached(loc);
	return out->file && same_file_as_main(out->file);
}

static void emit(location_t loc, const char *kind, int len) {
	if (g_index_only) return;
	if (!kind || len <= 0) return;
	expanded_location xl;
	if (!expand_main(loc, &xl)) return;
	if (xl.line <= 0 || xl.column <= 0) return;
	out_printf("GSETOK\t%d\t%d\t%d\t%s\n", xl.line, xl.column, len, kind);
}

static void emit_at_name(location_t loc, const char *kind, tree id);
static void emit_ref(location_t use_loc, tree decl, int len);

static void emit_decl(tree d, location_t loc) {
	if (!d || !DECL_P(d) || DECL_ARTIFICIAL(d)) return;
	if (TREE_CODE(d) == FUNCTION_DECL && (DECL_CONSTRUCTOR_P(d) || DECL_DESTRUCTOR_P(d))) {
		tree ctx = DECL_CONTEXT(d);
		tree ctx_name = ctx && TYPE_P(ctx) && TYPE_NAME(ctx) ? DECL_NAME(TYPE_NAME(ctx)) : NULL_TREE;
		emit_at_name(loc, "type", ctx_name);
		return;
	}
	emit_at_name(loc, kind_of(d), DECL_NAME(d));
	emit_ref(loc, d, ident_len(d));
}

static bool source_spelled_name(tree id) {
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE || IDENTIFIER_LENGTH(id) <= 0) return false;
	const char *s = IDENTIFIER_POINTER(id);
	return s[0] != '_' && s[0] != '.' && s[0] != '<' && !strchr(s, ' ');
}

static void emit_type(tree d) {
	if (!d || TREE_CODE(d) != TYPE_DECL || DECL_SELF_REFERENCE_P(d)) return;
	tree type = TREE_TYPE(d);
	if (type && TYPE_P(type) && LAMBDA_TYPE_P(type)) return;
	tree id = DECL_NAME(d);
	if (!source_spelled_name(id)) return;
	emit_at_name(DECL_SOURCE_LOCATION(d), "type", id);
}

static void walk_fn_default_args(tree decl) {
	if (decl && TREE_CODE(decl) == TEMPLATE_DECL) decl = DECL_TEMPLATE_RESULT(decl);
	if (!decl || TREE_CODE(decl) != FUNCTION_DECL) return;
	tree fntype = TREE_TYPE(decl);
	if (!fntype || (TREE_CODE(fntype) != FUNCTION_TYPE && TREE_CODE(fntype) != METHOD_TYPE)) return;
	for (tree a = TYPE_ARG_TYPES(fntype); a && a != void_list_node; a = TREE_CHAIN(a)) {
		tree def = TREE_PURPOSE(a);
		if (def && !DECL_P(def) && TREE_CODE(def) != DEFERRED_PARSE) walk_expr_tree(&def);
	}
}

static void emit_template_params(tree tmpl) {
	if (g_index_only) return;
	if (!tmpl || TREE_CODE(tmpl) != TEMPLATE_DECL || !in_main_file(DECL_SOURCE_LOCATION(tmpl))) return;
	tree parms = DECL_TEMPLATE_PARMS(tmpl);
	if (!parms) return;
	tree vec = TREE_VALUE(parms);
	if (!vec || TREE_CODE(vec) != TREE_VEC) return;
	for (int i = 0; i < TREE_VEC_LENGTH(vec); ++i) {
		tree e = TREE_VEC_ELT(vec, i);
		tree p = e ? TREE_VALUE(e) : NULL_TREE;
		if (!p || !DECL_P(p)) continue;
		tree id = DECL_NAME(p);
		if (!id || TREE_CODE(id) != IDENTIFIER_NODE || IDENTIFIER_LENGTH(id) <= 0) continue;
		const char *s = IDENTIFIER_POINTER(id);
		if (s[0] != '_' && s[0] != '.' && !strchr(s, ' ')) {
			const char *kind = TREE_CODE(p) == TEMPLATE_DECL ? "type" : kind_of(p);
			emit_at_name(DECL_SOURCE_LOCATION(p), kind, id);
		}
	}
}

static void print_qualified_prefix(tree decl);

static bool function_definition_p(tree decl) {
	return decl && TREE_CODE(decl) == FUNCTION_DECL && DECL_INITIAL(decl);
}

static void emit_concept(tree cdecl) {
	if (!cdecl) return;
	tree id = DECL_NAME(cdecl);
	if (!source_spelled_name(id)) return;
	emit_at_name(DECL_SOURCE_LOCATION(cdecl), "type", id);
	const char *s = IDENTIFIER_POINTER(id);
	expanded_location xl;
	if (expand_main(DECL_SOURCE_LOCATION(cdecl), &xl) && xl.line > 0 && xl.column > 0) {
		FILE *out = g_out ? g_out : stderr;
		out_printf("GSESYM\t%s\tconcept_decl\t%s\t%d\t%d\t", s, xl.file, xl.line, xl.column);
		print_qualified_prefix(cdecl);
		out_str(s);
		out_str("\t\tdefinition\n");
	}
}

static const char *sym_kind_of(tree d) {
	switch (TREE_CODE(d)) {
		case FUNCTION_DECL: return "function";
		case NAMESPACE_DECL: return "name_space";
		case FIELD_DECL: return "member";
		case CONST_DECL: return "enum_member";
		case VAR_DECL: return "variable";
		case TYPE_DECL: {
			tree t = TREE_TYPE(d);
			if (t && TREE_CODE(t) == ENUMERAL_TYPE) return "enumeration";
			return "type";
		}
		default: return nullptr;
	}
}

static bool real_type_decl(tree decl) {
	if (TREE_CODE(decl) != TYPE_DECL || DECL_SELF_REFERENCE_P(decl)) return false;
	tree t = TREE_TYPE(decl);
	return t && TYPE_P(t) && TYPE_NAME(t) == decl && (RECORD_OR_UNION_TYPE_P(t) || TREE_CODE(t) == ENUMERAL_TYPE);
}

static void emit_sym(tree decl) {
	if (!decl || !DECL_P(decl)) return;
	if (DECL_ARTIFICIAL(decl) && !real_type_decl(decl)) return;
	const bool is_definition = function_definition_p(decl);
	if (is_definition) {
		if (g_emitted_defs && g_emitted_defs->add(decl)) return;
	}
	else if (g_emitted && g_emitted->add(decl)) return;
	const char *kind = sym_kind_of(decl);
	if (!kind) return;
	tree id = DECL_NAME(decl);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE || IDENTIFIER_LENGTH(id) <= 0) return;
	const char *name = IDENTIFIER_POINTER(id);
	if (name[0] == '_' || name[0] == '.' || strchr(name, ' ')) return;
	location_t loc = DECL_SOURCE_LOCATION(decl);
	expanded_location xl;
	if (!expand_main(loc, &xl)) return;
	if (xl.line <= 0 || xl.column <= 0) return;
	const int def_col = name_byte_column(xl.file, xl.line, xl.column, name, (int)IDENTIFIER_LENGTH(id));
	out_printf("GSESYM\t%s\t%s\t%s\t%d\t%d\t", name, kind, xl.file, xl.line, def_col);
	print_qualified_prefix(decl);
	out_str(name);
	out_str("\t\t");
	out_str(is_definition ? "definition" : "declaration");
	out_char('\n');
}

static void print_qualified_prefix(tree decl) {
	const char *stack[32];
	int n = 0;
	tree ctx = DECL_CONTEXT(decl);
	while (ctx && n < 32) {
		if (TREE_CODE(ctx) == NAMESPACE_DECL) {
			if (ctx == global_namespace) break;
			tree id = DECL_NAME(ctx);
			if (id && TREE_CODE(id) == IDENTIFIER_NODE) {
				const char *s = IDENTIFIER_POINTER(id);
				if (!(IDENTIFIER_LENGTH(id) >= 2 && s[0] == '_' && s[1] == '_'))
					stack[n++] = s;
			}
			ctx = DECL_CONTEXT(ctx);
		} else if (TYPE_P(ctx)) {
			tree tn = TYPE_NAME(ctx);
			if (!tn || TREE_CODE(tn) != TYPE_DECL) break;
			tree id = DECL_NAME(tn);
			if (id && TREE_CODE(id) == IDENTIFIER_NODE) {
				const char *s = IDENTIFIER_POINTER(id);
				if (!(IDENTIFIER_LENGTH(id) >= 2 && s[0] == '_' && s[1] == '_'))
					stack[n++] = s;
			}
			ctx = DECL_CONTEXT(tn);
		} else {
			break;
		}
	}
	for (int i = n - 1; i >= 0; --i) {
		out_str(stack[i]);
		out_str("::");
	}
}

static void out_str_sanitized(const char *s) {
	for (const char *p = s; *p; ++p) {
		if (*p == '@') {
			const char *q = p + 1;
			while (*q == '.' || *q == '_' || (*q >= 'a' && *q <= 'z') || (*q >= 'A' && *q <= 'Z') || (*q >= '0' && *q <= '9')) {
				++q;
			}
			p = q - 1;
			continue;
		}
		const char c = *p;
		out_char(c == '\t' || c == '\n' || c == '\r' ? ' ' : c);
	}
}

static void emit_decl_type(tree decl) {
	const enum tree_code code = TREE_CODE(decl);
	if (code != VAR_DECL && code != FIELD_DECL && code != PARM_DECL) return;
	tree type = TREE_TYPE(decl);
	if (!type || !TYPE_P(type)) return;
	const char *s = type_as_string(type, TFF_SCOPE);
	if (s) out_str_sanitized(s);
}

static tree collapsed_constant(tree value) {
	for (int guard = 0; value && TREE_CODE(value) == CONSTRUCTOR && CONSTRUCTOR_NELTS(value) == 1 && guard < 8; ++guard) {
		value = CONSTRUCTOR_ELT(value, 0)->value;
	}
	return value;
}

static void emit_decl_value(tree decl) {
	if (!decl || TREE_CODE(decl) != VAR_DECL || !decl_constant_var_p(decl)) return;
	tree value = collapsed_constant(DECL_INITIAL(decl));
	if (!value || !TREE_CONSTANT(value)) return;
	const char *s = expr_as_string(value, TFF_SCOPE);
	if (!s || !*s) return;
	char text[160];
	int length = 0;
	while (s[length] && length < (int)sizeof(text) - 4) {
		text[length] = s[length];
		++length;
	}
	if (s[length]) {
		text[length++] = '.';
		text[length++] = '.';
		text[length++] = '.';
	}
	text[length] = '\0';
	out_char('\t');
	out_str_sanitized(text);
}

static void emit_ref_at(const char *use_file, int use_line, int use_column, tree decl, int len) {
	if (!decl || !DECL_P(decl) || len <= 0) return;
	tree id = DECL_NAME(decl);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE) return;
	const char *name = IDENTIFIER_POINTER(id);
	if (name[0] == '_' || name[0] == '.' || strchr(name, ' ')) return;
	if (!use_file || use_line <= 0 || use_column <= 0 || !under_root_cached(use_file)) return;
	expanded_location dx = expand_cached(DECL_SOURCE_LOCATION(decl));
	if (!dx.file || dx.line <= 0 || dx.column <= 0) return;
	const int def_col = name_byte_column(dx.file, dx.line, dx.column, name, (int)IDENTIFIER_LENGTH(id));
	out_printf("GSEREF\t%s\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t", use_file, use_line, use_column, len, name, dx.file, dx.line, def_col);
	print_qualified_prefix(decl);
	out_str(name);
	out_str("\t\t");
	emit_decl_type(decl);
	emit_decl_value(decl);
	out_char('\n');
}

static void emit_ref(location_t use_loc, tree decl, int len) {
	if (!decl || !DECL_P(decl) || len <= 0 || use_loc == UNKNOWN_LOCATION) return;
	tree id = DECL_NAME(decl);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE) return;
	const char *name = IDENTIFIER_POINTER(id);
	expanded_location ux = expand_cached(use_loc);
	if (!ux.file || ux.line <= 0 || ux.column <= 0) return;
	const int use_column = name_byte_column(ux.file, ux.line, ux.column, name, len);
	emit_ref_at(ux.file, ux.line, use_column, decl, len);
}

static bool locate_name(location_t loc, const char *name, int len, int *out_line, int *out_col);

static void emit_call_ref(location_t caret, tree decl, int fallback_len) {
	if (!decl || !DECL_P(decl)) {
		return;
	}
	tree id = DECL_NAME(decl);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE || IDENTIFIER_LENGTH(id) <= 0) {
		return;
	}
	const char *name = IDENTIFIER_POINTER(id);
	const int len = (int)IDENTIFIER_LENGTH(id);
	int line = 0;
	int column = 0;
	if (locate_name(caret, name, len, &line, &column)) {
		emit_ref_at(main_input_filename, line, column, decl, len);
	}
	else {
		emit_ref(caret, decl, fallback_len);
	}
}

static tree enclosing_namespace(tree decl) {
	tree ctx = decl ? DECL_CONTEXT(decl) : NULL_TREE;
	while (ctx) {
		if (TREE_CODE(ctx) == NAMESPACE_DECL) return ctx;
		if (TREE_CODE(ctx) == TRANSLATION_UNIT_DECL) return global_namespace;
		if (TYPE_P(ctx)) {
			tree tn = TYPE_NAME(ctx);
			ctx = tn ? DECL_CONTEXT(tn) : NULL_TREE;
		} else if (DECL_P(ctx)) {
			ctx = DECL_CONTEXT(ctx);
		} else {
			return NULL_TREE;
		}
	}
	return NULL_TREE;
}

static tree declaration_template(tree decl) {
	if (!decl || !DECL_P(decl)) {
		return NULL_TREE;
	}
	if (TREE_CODE(decl) == TEMPLATE_DECL) {
		return decl;
	}
	if ((TREE_CODE(decl) == FUNCTION_DECL || TREE_CODE(decl) == VAR_DECL)
		&& DECL_LANG_SPECIFIC(decl) && DECL_TEMPLATE_INFO(decl)) {
		return DECL_TI_TEMPLATE(decl);
	}
	return NULL_TREE;
}

static bool declarations_match(tree left, tree right) {
	if (left == right) {
		return true;
	}
	tree left_template = declaration_template(left);
	tree right_template = declaration_template(right);
	if (left_template && left_template == right_template) {
		return true;
	}
	if (left_template && DECL_TEMPLATE_RESULT(left_template) == right) {
		return true;
	}
	if (right_template && DECL_TEMPLATE_RESULT(right_template) == left) {
		return true;
	}
	return false;
}

static bool overload_contains(tree binding, tree decl) {
	if (!binding) {
		return false;
	}
	if (declarations_match(binding, decl)) {
		return true;
	}
	if (TREE_CODE(binding) == OVERLOAD || TREE_CODE(binding) == FUNCTION_DECL) {
		for (ovl_iterator it(binding); it; ++it) {
			if (declarations_match(*it, decl)) {
				return true;
			}
		}
	}
	return false;
}

static tree class_binding(tree name_id) {
	tree context = g_function ? DECL_CONTEXT(g_function) : g_class_context;
	for (int depth = 0; context && depth < 32; ++depth) {
		if (TYPE_P(context)) {
			if (CLASS_TYPE_P(context)) {
				if (tree binding = lookup_member(context, name_id, 0, false, tf_none)) {
					return binding;
				}
			}
			tree name = TYPE_NAME(context);
			context = name && DECL_P(name) ? DECL_CONTEXT(name) : TYPE_CONTEXT(context);
		}
		else if (DECL_P(context)) {
			context = DECL_CONTEXT(context);
		}
		else {
			break;
		}
	}
	return NULL_TREE;
}

static bool qual_ident_char(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static bool ensure_source() {
	if (!g_src && main_input_filename) {
		FILE *f = fopen(main_input_filename, "rb");
		if (f) {
			fseek(f, 0, SEEK_END);
			g_src_len = ftell(f);
			fseek(f, 0, SEEK_SET);
			if (g_src_len > 0) {
				g_src = (char *)xmalloc(g_src_len + 1);
				g_src_len = (long)fread(g_src, 1, g_src_len, f);
				g_src[g_src_len] = 0;
			}
			fclose(f);
		}
	}
	return g_src != nullptr;
}

static bool source_identifier_start(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (unsigned char)c >= 0x80;
}

static bool source_identifier_continue(char c) {
	return source_identifier_start(c) || (c >= '0' && c <= '9');
}

static void advance_source_position(long *position, int *line, int *column) {
	const char c = g_src[*position];
	++*position;
	if (c == '\n') {
		++*line;
		*column = 1;
	}
	else if (c == '\t') {
		*column += 8 - (*column - 1) % 8;
	}
	else {
		++*column;
	}
}

static bool source_number_start(long position) {
	const char c = g_src[position];
	if (c >= '0' && c <= '9') {
		return true;
	}
	return c == '.' && position + 1 < g_src_len && g_src[position + 1] >= '0' && g_src[position + 1] <= '9';
}

static void skip_number_source(long *position, int *line, int *column) {
	advance_source_position(position, line, column);
	while (*position < g_src_len) {
		const char c = g_src[*position];
		if (c == '+' || c == '-') {
			const char previous = g_src[*position - 1];
			if (previous != 'e' && previous != 'E' && previous != 'p' && previous != 'P') {
				return;
			}
		}
		else if (!source_identifier_continue(c) && c != '.' && c != '\'') {
			return;
		}
		advance_source_position(position, line, column);
	}
}

static void skip_quoted_source(long *position, int *line, int *column, char quote) {
	advance_source_position(position, line, column);
	while (*position < g_src_len) {
		const char c = g_src[*position];
		advance_source_position(position, line, column);
		if (c == '\\' && *position < g_src_len) {
			advance_source_position(position, line, column);
		}
		else if (c == quote) {
			break;
		}
	}
}

static int raw_string_prefix_length(long position) {
	if (position + 1 < g_src_len && g_src[position] == 'R' && g_src[position + 1] == '"') {
		return 2;
	}
	if (position + 3 < g_src_len && g_src[position] == 'u' && g_src[position + 1] == '8'
		&& g_src[position + 2] == 'R' && g_src[position + 3] == '"') {
		return 4;
	}
	if (position + 2 < g_src_len && (g_src[position] == 'u' || g_src[position] == 'U' || g_src[position] == 'L')
		&& g_src[position + 1] == 'R' && g_src[position + 2] == '"') {
		return 3;
	}
	return 0;
}

static bool skip_raw_string_source(long *position, int *line, int *column) {
	const int prefix_length = raw_string_prefix_length(*position);
	if (!prefix_length) {
		return false;
	}
	const long delimiter_start = *position + prefix_length;
	long opening = delimiter_start;
	while (opening < g_src_len && opening - delimiter_start <= 16 && g_src[opening] != '(' && g_src[opening] != '\n') {
		++opening;
	}
	if (opening >= g_src_len || g_src[opening] != '(' || opening - delimiter_start > 16) {
		return false;
	}
	const long delimiter_length = opening - delimiter_start;
	long closing = opening + 1;
	while (closing + delimiter_length + 1 < g_src_len) {
		if (g_src[closing] == ')' && g_src[closing + delimiter_length + 1] == '"'
			&& strncmp(g_src + closing + 1, g_src + delimiter_start, delimiter_length) == 0) {
			const long finish = closing + delimiter_length + 2;
			while (*position < finish) {
				advance_source_position(position, line, column);
			}
			return true;
		}
		++closing;
	}
	return false;
}

static void skip_preprocessor_source(long *position, int *line, int *column) {
	while (*position < g_src_len) {
		if (g_src[*position] == '\\' && *position + 1 < g_src_len && g_src[*position + 1] == '\n') {
			advance_source_position(position, line, column);
			advance_source_position(position, line, column);
			continue;
		}
		if (g_src[*position] == '\\' && *position + 2 < g_src_len
			&& g_src[*position + 1] == '\r' && g_src[*position + 2] == '\n') {
			advance_source_position(position, line, column);
			advance_source_position(position, line, column);
			advance_source_position(position, line, column);
			continue;
		}
		if (g_src[*position] == '\n') {
			return;
		}
		advance_source_position(position, line, column);
	}
}

static source_token_kind punctuation_kind(char c) {
	switch (c) {
		case '{': return source_token_kind::left_brace;
		case '}': return source_token_kind::right_brace;
		case ';': return source_token_kind::semicolon;
		case '(': return source_token_kind::left_paren;
		case ')': return source_token_kind::right_paren;
		case ',': return source_token_kind::comma;
		case '<': return source_token_kind::less;
		case '>': return source_token_kind::greater;
		default: return source_token_kind::other;
	}
}

static int source_byte_column(long position) {
	long line_start = position;
	while (line_start > 0 && g_src[line_start - 1] != '\n') {
		--line_start;
	}
	return (int)(position - line_start + 1);
}

static bool ensure_source_tokens() {
	if (g_source_tokens) {
		return true;
	}
	if (!ensure_source()) {
		return false;
	}
	g_source_tokens = new auto_vec<source_token, 256>();
	auto_vec<int, 64> braces;
	long position = 0;
	int line = 1;
	int column = 1;
	bool line_has_content = false;
	while (position < g_src_len) {
		const char c = g_src[position];
		if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
			if (c == '\n') {
				line_has_content = false;
			}
			advance_source_position(&position, &line, &column);
			continue;
		}
		if (!line_has_content && c == '#') {
			skip_preprocessor_source(&position, &line, &column);
			continue;
		}
		if (c == '/' && position + 1 < g_src_len && g_src[position + 1] == '/') {
			while (position < g_src_len && g_src[position] != '\n') {
				advance_source_position(&position, &line, &column);
			}
			continue;
		}
		if (c == '/' && position + 1 < g_src_len && g_src[position + 1] == '*') {
			advance_source_position(&position, &line, &column);
			advance_source_position(&position, &line, &column);
			while (position < g_src_len) {
				if (g_src[position] == '*' && position + 1 < g_src_len && g_src[position + 1] == '/') {
					advance_source_position(&position, &line, &column);
					advance_source_position(&position, &line, &column);
					break;
				}
				if (g_src[position] == '\n') {
					line_has_content = false;
				}
				advance_source_position(&position, &line, &column);
			}
			continue;
		}
		if (skip_raw_string_source(&position, &line, &column)) {
			line_has_content = true;
			continue;
		}
		if (c == '"' || c == '\'') {
			line_has_content = true;
			skip_quoted_source(&position, &line, &column, c);
			continue;
		}
		const long start = position;
		const int token_line = line;
		const int token_column = column;
		source_token_kind kind = source_token_kind::other;
		if (source_number_start(position)) {
			skip_number_source(&position, &line, &column);
		}
		else if (source_identifier_start(c)) {
			kind = source_token_kind::identifier;
			do {
				advance_source_position(&position, &line, &column);
			} while (position < g_src_len && source_identifier_continue(g_src[position]));
		}
		else if (c == ':' && position + 1 < g_src_len && g_src[position + 1] == ':') {
			kind = source_token_kind::scope;
			advance_source_position(&position, &line, &column);
			advance_source_position(&position, &line, &column);
		}
		else {
			kind = punctuation_kind(c);
			advance_source_position(&position, &line, &column);
		}
		line_has_content = true;
		const int token_index = (int)g_source_tokens->length();
		g_source_tokens->safe_push({
			.kind = kind,
			.start = start,
			.finish = position,
			.line = token_line,
			.column = token_column,
			.byte_column = source_byte_column(start)
		});
		if (kind == source_token_kind::left_brace) {
			braces.safe_push(token_index);
		}
		else if (kind == source_token_kind::right_brace && !braces.is_empty()) {
			const int opening = braces.pop();
			(*g_source_tokens)[opening].pair = token_index;
			(*g_source_tokens)[token_index].pair = opening;
		}
	}
	return true;
}

static bool source_line(int line, const char **out_ptr, int *out_len) {
	if (!ensure_source()) return false;
	if (!g_src_lines) {
		g_src_lines = new auto_vec<long>();
		g_src_lines->safe_push(0);
		for (long i = 0; i < g_src_len; ++i) {
			if (g_src[i] == '\n') g_src_lines->safe_push(i + 1);
		}
	}
	if (line <= 0 || line - 1 >= (int)g_src_lines->length()) return false;
	const long start = (*g_src_lines)[line - 1];
	const long stop = line < (int)g_src_lines->length() ? (*g_src_lines)[line] : g_src_len;
	const char *p = g_src + start;
	int n = (int)(stop - start);
	if (n > 0 && p[n - 1] == '\n') --n;
	if (n > 0 && p[n - 1] == '\r') --n;
	*out_ptr = p;
	*out_len = n;
	return true;
}

static bool locate_name(location_t loc, const char *name, int len, int *out_line, int *out_col) {
	expanded_location xl;
	if (!expand_main(loc, &xl)) {
		return false;
	}
	if (xl.line <= 0 || xl.column <= 0) {
		return false;
	}
	const char *line = nullptr;
	int n = 0;
	if (!source_line(xl.line, &line, &n)) {
		return false;
	}
	int col = xl.column - 1;
	if (col > n) {
		col = n;
	}
	int best = -1;
	int best_distance = n + 1;
	for (int i = 0; i + len <= n; ++i) {
		if (strncmp(line + i, name, len) != 0) {
			continue;
		}
		if (i > 0 && source_identifier_continue(line[i - 1])) {
			continue;
		}
		if (i + len < n && source_identifier_continue(line[i + len])) {
			continue;
		}
		const int distance = i > col ? i - col : col - i;
		if (distance < best_distance) {
			best = i;
			best_distance = distance;
		}
	}
	if (best < 0) {
		return false;
	}
	*out_line = xl.line;
	*out_col = best + 1;
	return true;
}

static void emit_at_name(location_t loc, const char *kind, tree id) {
	if (g_index_only || !kind) {
		return;
	}
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE || IDENTIFIER_LENGTH(id) <= 0) {
		return;
	}
	const int len = (int)IDENTIFIER_LENGTH(id);
	int line = 0;
	int col = 0;
	if (locate_name(loc, IDENTIFIER_POINTER(id), len, &line, &col)) {
		out_printf("GSETOK\t%d\t%d\t%d\t%s\n", line, col, len, kind);
	}
}

static void emit_member_ref(location_t loc, tree field) {
	if (!field || TREE_CODE(field) != FIELD_DECL) {
		return;
	}
	tree id = DECL_NAME(field);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE || IDENTIFIER_LENGTH(id) <= 0) {
		return;
	}
	const int len = (int)IDENTIFIER_LENGTH(id);
	int line = 0;
	int col = 0;
	if (!locate_name(loc, IDENTIFIER_POINTER(id), len, &line, &col)) {
		return;
	}
	emit_ref_at(main_input_filename, line, col, field, len);
}

/* Inside a coroutine's actor function every parameter has been rewritten into a
   field of the generated frame, and those fields carry no source location.  The
   original parameter is still on the ramp function, so recover it by name;
   without this, every parameter use in a coroutine body resolves to nothing and
   colours as a member.  */
static tree coroutine_frame_source_decl(tree field) {
	if (!field || TREE_CODE(field) != FIELD_DECL || !g_function || !is_coroutine_actor(g_function)) {
		return NULL_TREE;
	}
	const expanded_location dx = expand_cached(DECL_SOURCE_LOCATION(field));
	if (dx.file && dx.line > 0 && dx.column > 0) {
		return NULL_TREE;
	}
	tree ramp = DECL_RAMP_FN(g_function);
	tree name = DECL_NAME(field);
	if (!ramp || !name) {
		return NULL_TREE;
	}
	for (tree parm = DECL_ARGUMENTS(ramp); parm; parm = DECL_CHAIN(parm)) {
		if (DECL_P(parm) && DECL_NAME(parm) == name) {
			return parm;
		}
	}
	return NULL_TREE;
}

static bool source_token_equals(int index, const char *text) {
	if (!g_source_tokens || index < 0 || index >= (int)g_source_tokens->length()) {
		return false;
	}
	const source_token& token = (*g_source_tokens)[index];
	const long length = token.finish - token.start;
	return length == (long)strlen(text) && strncmp(g_src + token.start, text, length) == 0;
}

static tree source_token_identifier(int index) {
	if (!g_source_tokens || index < 0 || index >= (int)g_source_tokens->length()) {
		return NULL_TREE;
	}
	source_token& token = (*g_source_tokens)[index];
	if (token.kind != source_token_kind::identifier || token.finish <= token.start) {
		return NULL_TREE;
	}
	if (!token.ident) {
		token.ident = get_identifier_with_length(g_src + token.start, token.finish - token.start);
	}
	return token.ident;
}

static bool declaration_visible_at_token(tree decl, const source_token& use) {
	if (!decl || !DECL_P(decl)) {
		return false;
	}
	const location_t decl_loc = DECL_SOURCE_LOCATION(decl);
	if (decl_loc == UNKNOWN_LOCATION) {
		return true;
	}
	const expanded_location value = expand_cached(decl_loc);
	if (!value.file || !same_file_as_main(value.file)) {
		return true;
	}
	if (value.line != use.line) {
		return value.line < use.line;
	}
	return value.column <= use.byte_column;
}

static tree local_binding_at(tree name_id, const source_token& use) {
	if (!g_local_decls) {
		return NULL_TREE;
	}
	for (unsigned index = g_local_decls->length(); index > 0; --index) {
		tree decl = (*g_local_decls)[index - 1];
		if (decl && DECL_P(decl) && DECL_NAME(decl) == name_id && declaration_visible_at_token(decl, use)) {
			return decl;
		}
	}
	return NULL_TREE;
}

/* Query a namespace's binding table without triggering lazy module CMI
   loads. lookup_qualified_name lazy-loads pending bindings for names that
   live in unimported CMIs, and that load path segfaults cc1plus for some
   entities (hit on `number`, `scale`, `offset`, `remove`, `find`, ...).
   Any name actually used in this TU is already loaded, so skipping lazy
   slots loses nothing the scanner should see.  */
static tree loaded_namespace_binding(tree ns, tree name_id) {
	if (!ns || TREE_CODE(ns) != NAMESPACE_DECL || !name_id || TREE_CODE(name_id) != IDENTIFIER_NODE) {
		return NULL_TREE;
	}
	tree found = NULL_TREE;
	if (hash_table<named_decl_hash> *table = DECL_NAMESPACE_BINDINGS(ns)) {
		if (tree bind = table->find_with_hash(name_id, IDENTIFIER_HASH_VALUE(name_id))) {
			if (TREE_CODE(bind) != BINDING_VECTOR) {
				return bind;
			}
			binding_cluster *clusters = BINDING_VECTOR_CLUSTER_BASE(bind);
			const unsigned count = BINDING_VECTOR_NUM_CLUSTERS(bind);
			for (unsigned i = 0; i < count; ++i) {
				for (unsigned j = 0; j < BINDING_VECTOR_SLOTS_PER_CLUSTER; ++j) {
					binding_slot &slot = clusters[i].slots[j];
					if (slot.is_lazy()) {
						continue;
					}
					tree value = slot;
					if (!value || value == error_mark_node) {
						continue;
					}
					if (MAYBE_STAT_TYPE(value)) {
						return value;
					}
					if (!found) {
						found = value;
					}
				}
			}
		}
	}
	if (found) {
		return found;
	}
	if (vec<tree, va_gc> *inlinees = DECL_NAMESPACE_INLINEES(ns)) {
		for (tree inlinee : *inlinees) {
			if (tree binding = loaded_namespace_binding(inlinee, name_id)) {
				return binding;
			}
		}
	}
	return NULL_TREE;
}

static tree namespace_binding_at(tree scope_ns, tree name_id, LOOK_want want = LOOK_want::TYPE_NAMESPACE) {
	(void)want;
	for (tree ns = scope_ns; ns && TREE_CODE(ns) == NAMESPACE_DECL; ) {
		if (tree binding = loaded_namespace_binding(ns, name_id)) {
			return binding;
		}
		if (ns == global_namespace) {
			break;
		}
		ns = CP_DECL_CONTEXT(ns);
	}
	return NULL_TREE;
}

static tree visible_binding_at(tree name_id, const source_token& use, LOOK_want want = LOOK_want::TYPE_NAMESPACE) {
	if (tree binding = local_binding_at(name_id, use)) {
		return binding;
	}
	if (tree binding = class_binding(name_id)) {
		return binding;
	}
	return namespace_binding_at(g_scope, name_id, want);
}

static tree preferred_type_binding(tree binding) {
	if (!binding || binding == error_mark_node) {
		return NULL_TREE;
	}
	if (tree type_binding = MAYBE_STAT_TYPE(binding)) {
		return type_binding;
	}
	return MAYBE_STAT_DECL(binding);
}

static tree type_decl_from_binding(tree binding);

static tree binding_scope_type(tree binding) {
	binding = preferred_type_binding(binding);
	if (!binding) {
		return NULL_TREE;
	}
	if (TYPE_P(binding)) {
		return binding;
	}
	if (tree td = type_decl_from_binding(binding); td && TREE_CODE(td) == TYPE_DECL) {
		return TREE_TYPE(td);
	}
	return NULL_TREE;
}

static tree resolve_scope_component(tree scope, tree name_id, LOOK_want want = LOOK_want::TYPE_NAMESPACE) {
	if (!scope || !name_id) {
		return NULL_TREE;
	}
	scope = preferred_type_binding(scope);
	if (!scope) {
		return NULL_TREE;
	}
	if (TREE_CODE(scope) == NAMESPACE_DECL) {
		return loaded_namespace_binding(scope, name_id);
	}
	if (tree type = binding_scope_type(scope); type) {
		if (CLASS_TYPE_P(type)) {
			tree binding = lookup_qualified_name(type, name_id, want, true);
			return binding == error_mark_node ? NULL_TREE : binding;
		}
		if (TREE_CODE(type) == ENUMERAL_TYPE) {
			for (tree v = TYPE_VALUES(type); v; v = TREE_CHAIN(v)) {
				if (TREE_PURPOSE(v) == name_id) {
					tree cst = TREE_VALUE(v);
					return (cst && TREE_CODE(cst) == CONST_DECL) ? cst : NULL_TREE;
				}
			}
			return NULL_TREE;
		}
	}
	return NULL_TREE;
}

static bool source_bindings_match(tree left, tree right) {
	if (left == right) {
		return true;
	}
	tree left_type = preferred_type_binding(left);
	tree right_type = preferred_type_binding(right);
	if (left_type == right_type || declarations_match(left_type, right_type)) {
		return true;
	}
	return overload_contains(left, right_type) || overload_contains(right, left_type);
}

static int parse_qualified_chain(int first, int limit, int *components, int capacity) {
	if (!g_source_tokens || first < 0 || first >= limit || capacity < 2) {
		return 0;
	}
	if ((*g_source_tokens)[first].kind != source_token_kind::identifier) {
		return 0;
	}
	if (first > 0 && (*g_source_tokens)[first - 1].kind == source_token_kind::scope) {
		return 0;
	}
	int count = 1;
	components[0] = first;
	int cursor = first + 1;
	while (cursor + 1 < limit && count < capacity && (*g_source_tokens)[cursor].kind == source_token_kind::scope) {
		++cursor;
		if (cursor < limit && source_token_equals(cursor, "template")) {
			++cursor;
		}
		if (cursor >= limit || (*g_source_tokens)[cursor].kind != source_token_kind::identifier) {
			break;
		}
		components[count++] = cursor;
		++cursor;
	}
	return count >= 2 ? count : 0;
}

static bool using_declaration_chain_p(int first) {
	for (int index = first - 1; index >= 0; --index) {
		const source_token_kind kind = (*g_source_tokens)[index].kind;
		if (kind == source_token_kind::semicolon || kind == source_token_kind::left_brace || kind == source_token_kind::right_brace) {
			return false;
		}
		if (source_token_equals(index, "=")) {
			return false;
		}
		if (source_token_equals(index, "using")) {
			return true;
		}
	}
	return false;
}

static int source_token_at(location_t location, source_token_kind kind) {
	if (!ensure_source_tokens() || location == UNKNOWN_LOCATION) {
		return -1;
	}
	expanded_location value;
	if (!expand_main(location, &value)) {
		return -1;
	}
	int low = 0;
	int high = (int)g_source_tokens->length();
	while (low < high) {
		const int middle = low + (high - low) / 2;
		const source_token& token = (*g_source_tokens)[middle];
		if (token.line < value.line || (token.line == value.line && token.byte_column < value.column)) {
			low = middle + 1;
		}
		else {
			high = middle;
		}
	}
	for (int index = low; index < (int)g_source_tokens->length(); ++index) {
		const source_token& token = (*g_source_tokens)[index];
		if (token.line != value.line || token.byte_column != value.column) {
			break;
		}
		if (token.kind == kind) {
			return index;
		}
	}
	return -1;
}

static bool qualifier_already_emitted(int line, int column, int length) {
	if (!g_emitted_qualifiers) {
		g_emitted_qualifiers = new hash_set<uint64_t, false, token_span_hash>();
	}
	return g_emitted_qualifiers->add(token_span_key(line, column, length));
}

static void emit_source_qualifier(const source_token& first, const source_token& final_scope) {
	if (first.line != final_scope.line || final_scope.finish <= final_scope.start || !main_input_filename) {
		return;
	}
	const int length = final_scope.column + (int)(final_scope.finish - final_scope.start) - first.column;
	if (qualifier_already_emitted(first.line, first.column, length)) {
		return;
	}
	out_printf("GSEQUAL\t%s\t%d\t%d\t%d\n", main_input_filename, first.line, first.column, length);
}

static hash_set<uint64_t, false, token_span_hash> *g_emitted_type_refs = nullptr;

static bool type_ref_already_emitted(int line, int column, int length) {
	if (!g_emitted_type_refs) {
		g_emitted_type_refs = new hash_set<uint64_t, false, token_span_hash>();
	}
	return g_emitted_type_refs->add(token_span_key(line, column, length));
}

static tree type_decl_from_binding(tree binding) {
	binding = preferred_type_binding(binding);
	if (!binding) {
		return NULL_TREE;
	}
	if (TREE_CODE(binding) == OVERLOAD) {
		for (ovl_iterator it(binding); it; ++it) {
			tree candidate = strip_using_decl(*it);
			if (candidate && TREE_CODE(candidate) == TEMPLATE_DECL) {
				candidate = DECL_TEMPLATE_RESULT(candidate);
			}
			if (candidate && (TREE_CODE(candidate) == TYPE_DECL || TREE_CODE(candidate) == CONCEPT_DECL)) {
				return candidate;
			}
		}
		return NULL_TREE;
	}
	binding = strip_using_decl(binding);
	if (binding && TREE_CODE(binding) == TEMPLATE_DECL) {
		binding = DECL_TEMPLATE_RESULT(binding);
	}
	if (binding && (TREE_CODE(binding) == TYPE_DECL || TREE_CODE(binding) == CONCEPT_DECL)) {
		return binding;
	}
	return NULL_TREE;
}

static void emit_type_use_ref(const source_token &use, tree decl) {
	if (!decl || !DECL_P(decl) || !main_input_filename) {
		return;
	}
	tree id = DECL_NAME(decl);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE || IDENTIFIER_LENGTH(id) <= 0) {
		return;
	}
	const char *name = IDENTIFIER_POINTER(id);
	if (name[0] == '.' || strchr(name, ' ')) {
		return;
	}
	const int use_len = (int)(use.finish - use.start);
	if (use_len <= 0) {
		return;
	}
	if (type_ref_already_emitted(use.line, use.byte_column, use_len)) {
		return;
	}
	if (!g_index_only) {
		out_printf("GSETOK\t%d\t%d\t%d\ttype\n", use.line, use.byte_column, use_len);
	}
	if (name[0] == '_' || !main_file_under_root()) {
		return;
	}
	const expanded_location dx = expand_cached(DECL_SOURCE_LOCATION(decl));
	if (!dx.file || dx.line <= 0 || dx.column <= 0) {
		return;
	}
	const int def_col = name_byte_column(dx.file, dx.line, dx.column, name, (int)IDENTIFIER_LENGTH(id));
	out_printf("GSEREF\t%s\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t", main_input_filename, use.line, use.byte_column, use_len, name, dx.file, dx.line, def_col);
	print_qualified_prefix(decl);
	out_str(name);
	out_char('\n');
}

static bool source_token_is_char(int index, char c) {
	if (!g_source_tokens || index < 0 || index >= (int)g_source_tokens->length()) {
		return false;
	}
	const source_token &token = (*g_source_tokens)[index];
	return token.finish - token.start == 1 && g_src[token.start] == c;
}

static bool trailing_return_arrow_at(int minus) {
	if (minus < 1) {
		return false;
	}
	if (source_token_equals(minus - 1, "const") || source_token_equals(minus - 1, "noexcept")
		|| source_token_equals(minus - 1, "override") || source_token_equals(minus - 1, "final")
		|| source_token_is_char(minus - 1, '&')
		|| (*g_source_tokens)[minus - 1].kind == source_token_kind::right_brace) {
		return true;
	}
	if ((*g_source_tokens)[minus - 1].kind != source_token_kind::right_paren) {
		return false;
	}
	int parentheses = 0;
	for (int index = minus - 1, guard = 0; index >= 0 && guard < 512; --index, ++guard) {
		const source_token_kind kind = (*g_source_tokens)[index].kind;
		if (kind == source_token_kind::right_paren) {
			++parentheses;
			continue;
		}
		if (kind != source_token_kind::left_paren || --parentheses > 0) {
			continue;
		}
		if (source_token_is_char(index - 1, ']')) {
			return true;
		}
		return index > 1 && (*g_source_tokens)[index - 1].kind == source_token_kind::identifier
			&& (source_token_equals(index - 2, "auto") || source_token_equals(index - 2, "operator"));
	}
	return false;
}

static bool source_token_follows_member_access(int index) {
	if (index <= 0) {
		return false;
	}
	int previous = index - 1;
	if (source_token_equals(previous, "template")) {
		--previous;
	}
	if (previous < 0) {
		return false;
	}
	if (source_token_is_char(previous, '.')) {
		return true;
	}
	return (*g_source_tokens)[previous].kind == source_token_kind::greater
		&& previous > 0
		&& source_token_is_char(previous - 1, '-')
		&& (*g_source_tokens)[previous - 1].finish == (*g_source_tokens)[previous].start
		&& !trailing_return_arrow_at(previous - 1);
}

static int source_token_lower_bound(location_t location) {
	const expanded_location value = expand_cached(location);
	int low = 0;
	int high = (int)g_source_tokens->length();
	while (low < high) {
		const int middle = low + (high - low) / 2;
		const source_token &token = (*g_source_tokens)[middle];
		if (token.line < value.line || (token.line == value.line && token.byte_column < value.column)) {
			low = middle + 1;
		}
		else {
			high = middle;
		}
	}
	return low < (int)g_source_tokens->length() ? low : (int)g_source_tokens->length() - 1;
}

static tree first_loc_cb(tree *tp, int *, void *data) {
	tree t = *tp;
	if (!t) {
		return NULL_TREE;
	}
	if (EXPR_P(t)) {
		const location_t loc = EXPR_LOCATION(t);
		if (loc != UNKNOWN_LOCATION && in_main_file(loc)) {
			*(location_t *)data = loc;
			return t;
		}
	}
	return NULL_TREE;
}

static tree record_field_named(tree type, int index) {
	tree id = source_token_identifier(index);
	if (!id) {
		return NULL_TREE;
	}
	for (tree f = TYPE_FIELDS(type); f; f = DECL_CHAIN(f)) {
		if (TREE_CODE(f) == FIELD_DECL && DECL_NAME(f) == id) {
			return f;
		}
	}
	return NULL_TREE;
}

static bool designator_list_p(int open_index, tree type) {
	const source_token &open = (*g_source_tokens)[open_index];
	return open.pair > open_index + 2
		&& (*g_source_tokens)[open_index + 1].kind == source_token_kind::other
		&& source_token_is_char(open_index + 1, '.')
		&& (*g_source_tokens)[open_index + 2].kind == source_token_kind::identifier
		&& record_field_named(type, open_index + 2) != NULL_TREE;
}

static void scan_designators(int open_index, tree type) {
	if (g_index_only || !type || !RECORD_OR_UNION_TYPE_P(type)) {
		return;
	}
	if (open_index < 0 || open_index >= (int)g_source_tokens->length()) {
		return;
	}
	const source_token &open = (*g_source_tokens)[open_index];
	if (open.kind != source_token_kind::left_brace || open.pair <= open_index || open.pair - open_index > 256) {
		return;
	}
	for (int index = open_index + 1; index < open.pair; ++index) {
		const source_token &token = (*g_source_tokens)[index];
		if (token.kind == source_token_kind::left_brace) {
			if (token.pair > index) {
				index = token.pair;
			}
			continue;
		}
		if (token.kind != source_token_kind::other || !source_token_is_char(index, '.')) {
			continue;
		}
		const source_token_kind prev = (*g_source_tokens)[index - 1].kind;
		if (prev != source_token_kind::left_brace && prev != source_token_kind::comma) {
			continue;
		}
		if (index + 1 >= open.pair) {
			break;
		}
		tree field = record_field_named(type, index + 1);
		if (!field) {
			continue;
		}
		const source_token &name = (*g_source_tokens)[index + 1];
		out_printf("GSETOK\t%d\t%d\t%d\tmember\n", name.line, name.byte_column, (int)(name.finish - name.start));
		emit_ref_at(main_input_filename, name.line, name.byte_column, field, (int)(name.finish - name.start));
		++index;
		if (index + 2 < open.pair && source_token_is_char(index + 1, '=') && (*g_source_tokens)[index + 2].kind == source_token_kind::left_brace) {
			tree field_type = TREE_TYPE(field);
			if (field_type && RECORD_OR_UNION_TYPE_P(field_type)) {
				scan_designators(index + 2, field_type);
			}
		}
	}
}

static void scan_decl_designators(tree decl, tree init) {
	if (g_index_only || !decl || !init || !ensure_source_tokens()) {
		return;
	}
	tree ctor = init;
	if (TREE_CODE(ctor) == TARGET_EXPR) {
		ctor = TARGET_EXPR_INITIAL(ctor);
	}
	if (!ctor || TREE_CODE(ctor) != CONSTRUCTOR || !TREE_TYPE(ctor) || !RECORD_OR_UNION_TYPE_P(TREE_TYPE(ctor))) {
		return;
	}
	const int anchor = source_token_at(DECL_SOURCE_LOCATION(decl), source_token_kind::identifier);
	if (anchor < 0) {
		return;
	}
	const int count = (int)g_source_tokens->length();
	for (int index = anchor + 1, guard = 0; index < count && guard < 64; ++index, ++guard) {
		const source_token_kind kind = (*g_source_tokens)[index].kind;
		if (kind == source_token_kind::semicolon) {
			return;
		}
		if (kind == source_token_kind::left_brace) {
			scan_designators(index, TREE_TYPE(ctor));
			return;
		}
	}
}

static void emit_directive_names() {
	if (g_index_only || !ensure_source_tokens()) {
		return;
	}
	const int count = (int)g_source_tokens->length();
	for (int i = 0; i < count; ++i) {
		const source_token &token = (*g_source_tokens)[i];
		if (token.kind != source_token_kind::identifier) {
			continue;
		}
		int cursor = -1;
		if (source_token_equals(i, "namespace")) {
			cursor = i + 1;
		}
		else if (i == 0 || (*g_source_tokens)[i - 1].line != token.line) {
			int head = i;
			if (source_token_equals(head, "export")) {
				++head;
			}
			if (source_token_equals(head, "import") || source_token_equals(head, "module")) {
				cursor = head + 1;
			}
		}
		if (cursor < 0) {
			continue;
		}
		int guard = 0;
		for (; cursor < count && guard < 48; ++cursor, ++guard) {
			const source_token &part = (*g_source_tokens)[cursor];
			if (part.kind == source_token_kind::semicolon || part.kind == source_token_kind::left_brace
				|| part.kind == source_token_kind::right_brace || part.kind == source_token_kind::left_paren
				|| part.kind == source_token_kind::less) {
				break;
			}
			if (part.kind != source_token_kind::identifier) {
				continue;
			}
			if (source_token_equals(cursor, "inline") || source_token_equals(cursor, "private")) {
				continue;
			}
			out_printf("GSETOK\t%d\t%d\t%d\tname_space\n", part.line, part.byte_column, (int)(part.finish - part.start));
		}
		i = cursor;
	}
}

static void emit_attribute_names() {
	if (g_index_only || !ensure_source_tokens()) {
		return;
	}
	const int count = (int)g_source_tokens->length();
	for (int i = 0; i + 1 < count; ++i) {
		if (!source_token_is_char(i, '[') || !source_token_is_char(i + 1, '[')) {
			continue;
		}
		if ((*g_source_tokens)[i + 1].start != (*g_source_tokens)[i].finish) {
			continue;
		}
		if (i + 2 < count && source_token_is_char(i + 2, '=')) {
			continue;
		}
		int cursor = i + 2;
		for (int guard = 0; cursor + 1 < count && guard < 32; ++cursor, ++guard) {
			const source_token &part = (*g_source_tokens)[cursor];
			if (part.kind == source_token_kind::left_brace || part.kind == source_token_kind::semicolon) {
				break;
			}
			if (source_token_is_char(cursor, ']') && source_token_is_char(cursor + 1, ']')
				&& (*g_source_tokens)[cursor + 1].start == part.finish) {
				++cursor;
				break;
			}
			if (part.kind == source_token_kind::identifier) {
				out_printf("GSETOK\t%d\t%d\t%d\tattribute\n", part.line, part.byte_column, (int)(part.finish - part.start));
			}
		}
		i = cursor;
	}
}

static void emit_type_name_at(location_t loc, tree type) {
	for (int guard = 0; type && TYPE_P(type) && guard < 8; ++guard) {
		const enum tree_code code = TREE_CODE(type);
		if (code == POINTER_TYPE || code == REFERENCE_TYPE || code == ARRAY_TYPE) {
			type = TREE_TYPE(type);
			continue;
		}
		break;
	}
	if (!type || !TYPE_P(type)) {
		return;
	}
	tree name = NULL_TREE;
	if (TREE_CODE(type) == TYPENAME_TYPE) {
		name = TYPENAME_TYPE_FULLNAME(type);
		if (name && TREE_CODE(name) == TEMPLATE_ID_EXPR) {
			name = TREE_OPERAND(name, 0);
		}
	}
	else {
		name = TYPE_NAME(TYPE_MAIN_VARIANT(type));
	}
	if (name && DECL_P(name)) {
		name = DECL_NAME(name);
	}
	if (!name || TREE_CODE(name) != IDENTIFIER_NODE) {
		return;
	}
	emit_at_name(loc, "type", name);
}

static tree decl_for_highlight(tree binding) {
	if (!binding) {
		return NULL_TREE;
	}
	if (TREE_CODE(binding) == BASELINK) {
		binding = BASELINK_FUNCTIONS(binding);
	}
	if (binding && TREE_CODE(binding) == OVERLOAD) {
		binding = OVL_FIRST(binding);
	}
	if (binding) {
		binding = strip_using_decl(binding);
	}
	if (binding && TREE_CODE(binding) == TEMPLATE_DECL) {
		binding = DECL_TEMPLATE_RESULT(binding);
	}
	if (binding && TYPE_P(binding)) {
		binding = TYPE_NAME(binding);
	}
	return (binding && DECL_P(binding)) ? binding : NULL_TREE;
}

static const char *highlight_kind_of(tree decl) {
	switch (TREE_CODE(decl)) {
		case NAMESPACE_DECL: return "name_space";
		case TYPE_DECL: return "type";
		case CONCEPT_DECL: return "type";
		case CONST_DECL: return "enum_member";
		case FUNCTION_DECL: return "function";
		case FIELD_DECL: return "member";
		case PARM_DECL: return DECL_TEMPLATE_PARM_P(decl) ? "type" : "parameter";
		case VAR_DECL: {
			tree ctx = DECL_CONTEXT(decl);
			return (!ctx || TREE_CODE(ctx) == NAMESPACE_DECL || TREE_CODE(ctx) == TRANSLATION_UNIT_DECL) ? "global" : "variable";
		}
		default: return nullptr;
	}
}

static void emit_component_token(const source_token &use, tree binding) {
	tree decl = decl_for_highlight(binding);
	if (!decl) {
		return;
	}
	const char *kind = highlight_kind_of(decl);
	if (!kind) {
		return;
	}
	const int use_len = (int)(use.finish - use.start);
	if (use_len <= 0) {
		return;
	}
	if (type_ref_already_emitted(use.line, use.byte_column, use_len)) {
		return;
	}
	if (!g_index_only) {
		out_printf("GSETOK\t%d\t%d\t%d\t%s\n", use.line, use.byte_column, use_len, kind);
	}
	if (main_file_under_root()) {
		emit_ref_at(main_input_filename, use.line, use.byte_column, decl, use_len);
	}
}

static void emit_redundant_prefix(const int *components, int count, tree *resolved, const source_token& use) {
	for (int remaining = count - 1; remaining > 0; --remaining) {
		bool namespace_prefix = true;
		for (int removed = 0; removed < remaining; ++removed) {
			tree binding = preferred_type_binding(resolved[removed]);
			if (!binding || TREE_CODE(binding) != NAMESPACE_DECL) {
				namespace_prefix = false;
				break;
			}
		}
		if (!namespace_prefix) {
			continue;
		}
		const LOOK_want want = remaining == count - 1 ? LOOK_want::NORMAL : LOOK_want::TYPE_NAMESPACE;
		tree shortened = visible_binding_at(source_token_identifier(components[remaining]), use, want);
		if (source_bindings_match(shortened, resolved[remaining])) {
			int final_scope = components[remaining] - 1;
			while (final_scope > components[remaining - 1] && (*g_source_tokens)[final_scope].kind != source_token_kind::scope) {
				--final_scope;
			}
			if ((*g_source_tokens)[final_scope].kind == source_token_kind::scope) {
				emit_source_qualifier((*g_source_tokens)[components[0]], (*g_source_tokens)[final_scope]);
			}
			return;
		}
	}
}

static tree shadowed_type_binding(int index, tree binding) {
	if (!binding || type_decl_from_binding(binding)) {
		return binding;
	}
	if (tree preferred = preferred_type_binding(binding); preferred && TREE_CODE(preferred) == NAMESPACE_DECL) {
		return binding;
	}
	tree retry = namespace_binding_at(g_scope, source_token_identifier(index));
	if (!retry) {
		return binding;
	}
	if (type_decl_from_binding(retry)) {
		return retry;
	}
	if (tree preferred = preferred_type_binding(retry); preferred && TREE_CODE(preferred) == NAMESPACE_DECL) {
		return retry;
	}
	return binding;
}

static void emit_dependent_member(int index) {
	if (g_index_only) {
		return;
	}
	const source_token &token = (*g_source_tokens)[index];
	int k = index - 2;
	int depth = 0;
	for (int guard = 0; k >= 0 && guard < 160; --k, ++guard) {
		const source_token_kind kind = (*g_source_tokens)[k].kind;
		if (kind == source_token_kind::greater) {
			++depth;
		}
		else if (kind == source_token_kind::less && --depth == 0) {
			break;
		}
	}
	const char *kind = "member";
	if (k > 0) {
		if (source_token_identifier(k - 1) && source_token_identifier(k - 1) == source_token_identifier(index)) {
			kind = "type";
		}
		else {
			int head = k - 1;
			while (head > 0 && ((*g_source_tokens)[head].kind == source_token_kind::identifier || (*g_source_tokens)[head].kind == source_token_kind::scope)) {
				--head;
			}
			if (source_token_equals(head, "typename")) {
				kind = "type";
			}
		}
	}
	out_printf("GSETOK\t%d\t%d\t%d\t%s\n", token.line, token.byte_column, (int)(token.finish - token.start), kind);
}

static bool standard_integer_type_chain(const int *components, int count) {
	if (count != 2 || !source_token_equals(components[0], "std")) {
		return false;
	}
	static const char *names[] = {
		"size_t",
		"ptrdiff_t",
		"int8_t",
		"int16_t",
		"int32_t",
		"int64_t",
		"uint8_t",
		"uint16_t",
		"uint32_t",
		"uint64_t",
		"int_fast8_t",
		"int_fast16_t",
		"int_fast32_t",
		"int_fast64_t",
		"uint_fast8_t",
		"uint_fast16_t",
		"uint_fast32_t",
		"uint_fast64_t",
		"int_least8_t",
		"int_least16_t",
		"int_least32_t",
		"int_least64_t",
		"uint_least8_t",
		"uint_least16_t",
		"uint_least32_t",
		"uint_least64_t",
		"intmax_t",
		"uintmax_t",
		"intptr_t",
		"uintptr_t",
	};
	for (const char *name : names) {
		if (source_token_equals(components[1], name)) {
			return true;
		}
	}
	return false;
}

static void scan_qualified_type_chain(int first, int limit, int exclude_final = -1, bool trailing_return_head = false) {
	if (first == exclude_final || using_declaration_chain_p(first)
		|| (!trailing_return_head && source_token_follows_member_access(first))) {
		return;
	}
	int components[32];
	const int count = parse_qualified_chain(first, limit, components, 32);
	if (count < 2) {
		if (first > 0 && (*g_source_tokens)[first - 1].kind == source_token_kind::scope) {
			if (first > 1 && (*g_source_tokens)[first - 2].kind == source_token_kind::greater
				&& (*g_source_tokens)[first - 1].start == (*g_source_tokens)[first - 2].finish) {
				emit_dependent_member(first);
			}
			return;
		}
		const source_token &use = (*g_source_tokens)[first];
		tree binding = visible_binding_at(source_token_identifier(first), use);
		if (first + 1 < limit && (*g_source_tokens)[first + 1].kind == source_token_kind::identifier) {
			binding = shadowed_type_binding(first, binding);
		}
		if (tree td = type_decl_from_binding(binding)) {
			emit_type_use_ref(use, td);
		}
		else {
			emit_component_token(use, binding);
		}
		return;
	}
	tree resolved[32];
	const source_token& use = (*g_source_tokens)[components[0]];
	resolved[0] = shadowed_type_binding(components[0], visible_binding_at(source_token_identifier(components[0]), use));
	if (!resolved[0]) {
		return;
	}
	if (tree td = type_decl_from_binding(resolved[0])) {
		emit_type_use_ref((*g_source_tokens)[components[0]], td);
	}
	else {
		emit_component_token((*g_source_tokens)[components[0]], resolved[0]);
	}
	bool chain_ok = true;
	for (int index = 1; index < count; ++index) {
		const LOOK_want want = index == count - 1 ? LOOK_want::NORMAL : LOOK_want::TYPE_NAMESPACE;
		resolved[index] = resolve_scope_component(resolved[index - 1], source_token_identifier(components[index]), want);
		if (!resolved[index]) {
			if (tree td = type_decl_from_binding(resolved[index - 1]); td && TREE_CODE(td) == TYPE_DECL && TREE_TYPE(td)
				&& (TREE_CODE(TREE_TYPE(td)) == TEMPLATE_TYPE_PARM || TREE_CODE(TREE_TYPE(td)) == TYPENAME_TYPE)) {
				const source_token &part = (*g_source_tokens)[components[index]];
				const char *dependent_kind = components[0] > 0 && source_token_equals(components[0] - 1, "typename") ? "type" : "member";
				if (!g_index_only) {
					out_printf("GSETOK\t%d\t%d\t%d\t%s\n", part.line, part.byte_column, (int)(part.finish - part.start), dependent_kind);
				}
			}
			chain_ok = false;
			break;
		}
		if (tree td = type_decl_from_binding(resolved[index])) {
			emit_type_use_ref((*g_source_tokens)[components[index]], td);
		}
		else {
			emit_component_token((*g_source_tokens)[components[index]], resolved[index]);
		}
	}
	if (!chain_ok) {
		return;
	}
	tree final_binding = preferred_type_binding(resolved[count - 1]);
	if (!final_binding || TREE_CODE(final_binding) == NAMESPACE_DECL) {
		return;
	}
	if (g_index_only || components[count - 1] == exclude_final || standard_integer_type_chain(components, count)) {
		return;
	}
	emit_redundant_prefix(components, count, resolved, use);
}

static void scan_binding_type_qualifiers(int opening, int closing, bool skip_nested = true) {
	if (!g_source_tokens || opening < 0 || closing <= opening) {
		return;
	}
	for (int index = opening + 1; index < closing; ++index) {
		const source_token& token = (*g_source_tokens)[index];
		if (skip_nested && token.kind == source_token_kind::left_brace && token.binding_scope && token.pair > index) {
			index = token.pair;
			continue;
		}
		if (token.kind == source_token_kind::identifier) {
			scan_qualified_type_chain(index, closing);
		}
	}
}

static bool source_token_matches_identifier(int index, tree identifier) {
	if (!identifier || TREE_CODE(identifier) != IDENTIFIER_NODE || !g_source_tokens || index < 0 || index >= (int)g_source_tokens->length()) {
		return false;
	}
	const source_token& token = (*g_source_tokens)[index];
	return token.kind == source_token_kind::identifier
		&& token.finish - token.start == (long)IDENTIFIER_LENGTH(identifier)
		&& strncmp(g_src + token.start, IDENTIFIER_POINTER(identifier), token.finish - token.start) == 0;
}

static void scan_parameter_names(int anchor, int finish) {
	if (g_index_only) {
		return;
	}
	int index = anchor + 1;
	while (index < finish) {
		const source_token_kind kind = (*g_source_tokens)[index].kind;
		if (kind == source_token_kind::left_paren) {
			break;
		}
		if (kind == source_token_kind::semicolon || kind == source_token_kind::left_brace) {
			return;
		}
		++index;
	}
	int depth = 0;
	for (; index + 1 < finish; ++index) {
		const source_token &token = (*g_source_tokens)[index];
		if (token.kind == source_token_kind::left_paren) {
			++depth;
			continue;
		}
		if (token.kind == source_token_kind::right_paren) {
			if (--depth == 0) {
				return;
			}
			continue;
		}
		if (depth != 1 || token.kind != source_token_kind::identifier) {
			continue;
		}
		const source_token &next = (*g_source_tokens)[index + 1];
		const bool ends = next.kind == source_token_kind::comma
			|| next.kind == source_token_kind::right_paren
			|| source_token_is_char(index + 1, '=');
		if (!ends) {
			continue;
		}
		const source_token &prev = (*g_source_tokens)[index - 1];
		if (prev.kind == source_token_kind::scope || source_token_is_char(index - 1, '.')) {
			continue;
		}
		out_printf("GSETOK\t%d\t%d\t%d\tparameter\n", token.line, token.byte_column, (int)(token.finish - token.start));
	}
}

static void emit_local_ref(const source_token &use, const source_token &declaration) {
	if (!main_input_filename || !main_file_under_root()) {
		return;
	}
	const int use_len = (int)(use.finish - use.start);
	if (use_len <= 0 || declaration.finish <= declaration.start) {
		return;
	}
	out_printf("GSEREF\t%s\t%d\t%d\t%d\t", main_input_filename, use.line, use.byte_column, use_len);
	for (long p = use.start; p < use.finish; ++p) {
		out_char(g_src[p]);
	}
	out_printf("\t%s\t%d\t%d\t", main_input_filename, declaration.line, declaration.byte_column);
	for (long p = declaration.start; p < declaration.finish; ++p) {
		out_char(g_src[p]);
	}
	out_char('\n');
}

static bool lambda_introducer_p(int index) {
	if (!source_token_is_char(index, '[')) {
		return false;
	}
	if (index == 0) {
		return true;
	}
	const source_token_kind previous = (*g_source_tokens)[index - 1].kind;
	return previous != source_token_kind::identifier && previous != source_token_kind::right_paren
		&& !source_token_is_char(index - 1, ']');
}

static int requires_expression_at(int brace) {
	if (brace < 1) {
		return -1;
	}
	if (source_token_equals(brace - 1, "requires")) {
		return brace - 1;
	}
	if ((*g_source_tokens)[brace - 1].kind != source_token_kind::right_paren) {
		return -1;
	}
	int parentheses = 0;
	for (int index = brace - 1, guard = 0; index >= 0 && guard < 256; --index, ++guard) {
		const source_token_kind kind = (*g_source_tokens)[index].kind;
		if (kind == source_token_kind::right_paren) {
			++parentheses;
			continue;
		}
		if (kind == source_token_kind::left_paren && --parentheses == 0) {
			return source_token_equals(index - 1, "requires") ? index - 1 : -1;
		}
	}
	return -1;
}

static bool lambda_body_brace_p(int brace) {
	for (int index = brace - 1, guard = 0; index >= 0 && guard < 96; --index, ++guard) {
		const source_token_kind kind = (*g_source_tokens)[index].kind;
		if (kind == source_token_kind::semicolon || kind == source_token_kind::left_brace
			|| kind == source_token_kind::right_brace) {
			return false;
		}
		if (!source_token_is_char(index, ']')) {
			continue;
		}
		int depth = 0;
		for (int back = index - 1, inner = 0; back >= 0 && inner < 96; --back, ++inner) {
			if (source_token_is_char(back, ']')) {
				++depth;
				continue;
			}
			if (!source_token_is_char(back, '[')) {
				continue;
			}
			if (depth > 0) {
				--depth;
				continue;
			}
			if (!lambda_introducer_p(back)) {
				return false;
			}
			const source_token_kind after = (*g_source_tokens)[index + 1].kind;
			return after == source_token_kind::left_paren || after == source_token_kind::less
				|| after == source_token_kind::left_brace;
		}
		return false;
	}
	return false;
}

static bool qualified_declarator_start_p(int index) {
	if (index == 0) {
		return true;
	}
	const source_token_kind previous = (*g_source_tokens)[index - 1].kind;
	if (previous == source_token_kind::semicolon || previous == source_token_kind::left_brace
		|| previous == source_token_kind::right_brace) {
		return true;
	}
	return source_token_equals(index - 1, "auto") || source_token_equals(index - 1, "export")
		|| source_token_equals(index - 1, "inline") || source_token_equals(index - 1, "static")
		|| source_token_equals(index - 1, "constexpr") || source_token_equals(index - 1, "consteval");
}

static int declarator_chain_limit(int index, int count) {
	for (int k = index, guard = 0; k < count && guard < 64; ++k, ++guard) {
		const source_token_kind kind = (*g_source_tokens)[k].kind;
		if (kind == source_token_kind::left_paren || kind == source_token_kind::semicolon
			|| kind == source_token_kind::left_brace) {
			return k;
		}
	}
	return count;
}

static void register_parameter_names(int open_paren, int limit, int *names, const char **kinds, int *count, int capacity) {
	int depth = 0;
	for (int index = open_paren, guard = 0; index < limit && guard < 256; ++index, ++guard) {
		const source_token_kind kind = (*g_source_tokens)[index].kind;
		if (kind == source_token_kind::left_paren) {
			++depth;
			continue;
		}
		if (kind == source_token_kind::right_paren) {
			if (--depth == 0) {
				return;
			}
			continue;
		}
		if (depth != 1 || kind != source_token_kind::identifier || *count >= capacity) {
			continue;
		}
		const source_token_kind next = (*g_source_tokens)[index + 1].kind;
		if (next != source_token_kind::comma && next != source_token_kind::right_paren && !source_token_is_char(index + 1, '=')) {
			continue;
		}
		if ((*g_source_tokens)[index - 1].kind == source_token_kind::scope || source_token_is_char(index - 1, '.')) {
			continue;
		}
		kinds[*count] = "parameter";
		names[(*count)++] = index;
	}
}

static void register_lambda_names(int introducer, int limit, int *names, const char **kinds, int *count, int capacity) {
	int cursor = introducer + 1;
	int depth = 1;
	for (int guard = 0; cursor < limit && guard < 256; ++cursor, ++guard) {
		if (source_token_is_char(cursor, '[')) {
			++depth;
			continue;
		}
		if (source_token_is_char(cursor, ']')) {
			if (--depth == 0) {
				break;
			}
			continue;
		}
		if (depth != 1 || (*g_source_tokens)[cursor].kind != source_token_kind::identifier || *count >= capacity) {
			continue;
		}
		const bool starts_capture = source_token_is_char(cursor - 1, '[')
			|| (*g_source_tokens)[cursor - 1].kind == source_token_kind::comma
			|| source_token_is_char(cursor - 1, '&');
		if (starts_capture && source_token_is_char(cursor + 1, '=')) {
			kinds[*count] = "variable";
			names[(*count)++] = cursor;
		}
	}
	if (cursor >= limit) {
		return;
	}
	++cursor;
	if (cursor < limit && (*g_source_tokens)[cursor].kind == source_token_kind::less) {
		int angle = 0;
		for (int guard = 0; cursor < limit && guard < 64; ++cursor, ++guard) {
			const source_token_kind kind = (*g_source_tokens)[cursor].kind;
			if (kind == source_token_kind::less) {
				++angle;
			}
			else if (kind == source_token_kind::greater && --angle == 0) {
				++cursor;
				break;
			}
		}
	}
	if (cursor < limit && (*g_source_tokens)[cursor].kind == source_token_kind::left_paren) {
		register_parameter_names(cursor, limit, names, kinds, count, capacity);
	}
}

static void scan_template_body(int open, int close) {
	if (g_index_only) {
		return;
	}
	int lambda_parms[64];
	const char *lambda_kinds[64];
	int lambda_count = 0;
	int local_vars[48];
	int local_var_count = 0;
	for (int index = open + 1; index < close; ++index) {
		const source_token &token = (*g_source_tokens)[index];
		if (lambda_introducer_p(index)) {
			register_lambda_names(index, close, lambda_parms, lambda_kinds, &lambda_count, 64);
		}
		else if (source_token_equals(index, "requires") && (*g_source_tokens)[index + 1].kind == source_token_kind::left_paren) {
			register_parameter_names(index + 1, close, lambda_parms, lambda_kinds, &lambda_count, 64);
		}
		if (token.kind == source_token_kind::less && source_token_is_char(index - 1, ']')) {
			int angle = 0;
			for (int cursor = index, guard = 0; cursor < close && guard < 64; ++cursor, ++guard) {
				const source_token_kind kind = (*g_source_tokens)[cursor].kind;
				if (kind == source_token_kind::less) {
					++angle;
					continue;
				}
				if (kind == source_token_kind::greater && --angle == 0) {
					break;
				}
				if (angle == 1 && kind == source_token_kind::identifier && cursor + 1 < close && lambda_count < 64) {
					const source_token_kind next = (*g_source_tokens)[cursor + 1].kind;
					if (next == source_token_kind::comma || next == source_token_kind::greater || source_token_is_char(cursor + 1, '=')) {
						lambda_kinds[lambda_count] = "type";
						lambda_parms[lambda_count] = cursor;
						const source_token &parm = (*g_source_tokens)[cursor];
						out_printf("GSETOK\t%d\t%d\t%d\t%s\n", parm.line, parm.byte_column, (int)(parm.finish - parm.start), lambda_kinds[lambda_count]);
						++lambda_count;
					}
				}
			}
			continue;
		}
		if (token.kind != source_token_kind::identifier) {
			continue;
		}
		if (source_token_equals(index, "auto")) {
			int cursor = index + 1;
			while (cursor < close && (source_token_is_char(cursor, '&') || source_token_is_char(cursor, '*'))) {
				++cursor;
			}
			if (cursor < close && source_token_is_char(cursor, '[')) {
				for (++cursor; cursor < close && !source_token_is_char(cursor, ']'); ++cursor) {
					const source_token &bind = (*g_source_tokens)[cursor];
					if (bind.kind == source_token_kind::identifier) {
						out_printf("GSETOK\t%d\t%d\t%d\tvariable\n", bind.line, bind.byte_column, (int)(bind.finish - bind.start));
					}
				}
				index = cursor;
			}
			continue;
		}
		const source_token &prev = (*g_source_tokens)[index - 1];
		bool dot = prev.kind == source_token_kind::other && source_token_is_char(index - 1, '.');
		const bool preceded_by_arrow = prev.kind == source_token_kind::greater && index >= 2
			&& source_token_is_char(index - 2, '-')
			&& prev.start == (*g_source_tokens)[index - 2].finish;
		if (preceded_by_arrow && trailing_return_arrow_at(index - 2)) {
			scan_qualified_type_chain(index, close, -1, true);
			continue;
		}
		bool arrow = preceded_by_arrow;
		if (!dot && !arrow && index >= 2 && source_token_equals(index - 1, "template")) {
			dot = source_token_is_char(index - 2, '.');
			arrow = !dot && index >= 3 && (*g_source_tokens)[index - 2].kind == source_token_kind::greater && source_token_is_char(index - 3, '-');
		}
		if (dot || arrow) {
			if (source_token_equals(index, "template")) {
				continue;
			}
			const bool call = index + 1 < close
				&& ((*g_source_tokens)[index + 1].kind == source_token_kind::left_paren || (*g_source_tokens)[index + 1].kind == source_token_kind::less);
			out_printf("GSETOK\t%d\t%d\t%d\t%s\n", token.line, token.byte_column, (int)(token.finish - token.start), call ? "function" : "member");
			continue;
		}
		if (prev.kind == source_token_kind::scope) {
			if (index > 1 && (*g_source_tokens)[index - 2].kind == source_token_kind::greater
				&& prev.start == (*g_source_tokens)[index - 2].finish) {
				emit_dependent_member(index);
			}
			continue;
		}
		tree id = source_token_identifier(index);
		if (!id) {
			continue;
		}
		if (index + 1 < close && local_var_count < 48) {
			const source_token &next = (*g_source_tokens)[index + 1];
			const bool decl_end = source_token_is_char(index + 1, '=') || next.kind == source_token_kind::semicolon || source_token_is_char(index + 1, ':');
			const bool decl_prev = prev.kind == source_token_kind::identifier || prev.kind == source_token_kind::greater
				|| source_token_is_char(index - 1, '&') || source_token_is_char(index - 1, '*');
			const bool keyword_prev = source_token_equals(index - 1, "return") || source_token_equals(index - 1, "case")
				|| source_token_equals(index - 1, "co_return") || source_token_equals(index - 1, "co_yield")
				|| source_token_equals(index - 1, "throw") || source_token_equals(index - 1, "goto")
				|| source_token_equals(index - 1, "new") || source_token_equals(index - 1, "delete")
				|| source_token_equals(index - 1, "sizeof") || source_token_equals(index - 1, "using")
				|| source_token_equals(index - 1, "operator") || source_token_equals(index - 1, "else");
			if (decl_end && decl_prev && !keyword_prev) {
				local_vars[local_var_count++] = index;
				out_printf("GSETOK\t%d\t%d\t%d\tvariable\n", token.line, token.byte_column, (int)(token.finish - token.start));
				continue;
			}
		}
		bool lambda_parm = false;
		for (int p = 0; p < lambda_count; ++p) {
			if (source_token_identifier(lambda_parms[p]) == id) {
				out_printf("GSETOK\t%d\t%d\t%d\t%s\n", token.line, token.byte_column, (int)(token.finish - token.start), lambda_kinds[p]);
				emit_local_ref(token, (*g_source_tokens)[lambda_parms[p]]);
				lambda_parm = true;
				break;
			}
		}
		if (lambda_parm) {
			continue;
		}
		bool local_use = false;
		for (int p = 0; p < local_var_count; ++p) {
			if (source_token_identifier(local_vars[p]) == id) {
				out_printf("GSETOK\t%d\t%d\t%d\tvariable\n", token.line, token.byte_column, (int)(token.finish - token.start));
				local_use = true;
				break;
			}
		}
		if (local_use) {
			continue;
		}
		int components[32];
		if (parse_qualified_chain(index, close, components, 32) >= 2) {
			scan_qualified_type_chain(index, close);
			continue;
		}
		tree binding = local_binding_at(id, token);
		if (!binding) {
			binding = class_binding(id);
		}
		if (!binding) {
			continue;
		}
		if (tree td = type_decl_from_binding(binding)) {
			emit_type_use_ref(token, td);
		}
		else {
			emit_component_token(token, binding);
		}
	}
}

static void scan_template_prototypes() {
	if (g_index_only || !ensure_source_tokens()) {
		return;
	}
	const int count = (int)g_source_tokens->length();
	struct ns_frame { int close; tree ns; };
	ns_frame stack[32];
	int depth = 0;
	for (int i = 0; i < count; ++i) {
		while (depth > 0 && i > stack[depth - 1].close) {
			--depth;
		}
		const source_token &token = (*g_source_tokens)[i];
		if (token.kind != source_token_kind::identifier) {
			continue;
		}
		tree scope = depth > 0 ? stack[depth - 1].ns : global_namespace;
		if ((source_token_equals(i, "class") || source_token_equals(i, "struct")) && i + 2 < count
			&& (*g_source_tokens)[i + 1].kind == source_token_kind::identifier
			&& (*g_source_tokens)[i + 2].kind == source_token_kind::semicolon) {
			const source_token &fwd = (*g_source_tokens)[i + 1];
			out_printf("GSETOK\t%d\t%d\t%d\ttype\n", fwd.line, fwd.byte_column, (int)(fwd.finish - fwd.start));
			i += 2;
			continue;
		}
		if (source_token_equals(i, "namespace")) {
			int cursor = i + 1;
			tree ns = scope;
			for (int guard = 0; cursor < count && guard < 24; ++cursor, ++guard) {
				const source_token_kind kind = (*g_source_tokens)[cursor].kind;
				if (kind == source_token_kind::left_brace || kind == source_token_kind::semicolon) {
					break;
				}
				if (source_token_is_char(cursor, '=')) {
					ns = NULL_TREE;
					break;
				}
				if (kind == source_token_kind::identifier && ns) {
					tree next_ns = resolve_scope_component(ns, source_token_identifier(cursor), LOOK_want::NAMESPACE);
					next_ns = next_ns ? preferred_type_binding(next_ns) : NULL_TREE;
					ns = next_ns && TREE_CODE(next_ns) == NAMESPACE_DECL ? next_ns : NULL_TREE;
				}
			}
			if (cursor < count && (*g_source_tokens)[cursor].kind == source_token_kind::left_brace
				&& (*g_source_tokens)[cursor].pair > cursor && ns && depth < 32) {
				stack[depth++] = { (*g_source_tokens)[cursor].pair, ns };
				i = cursor;
			}
			continue;
		}
		if (i + 1 < count && (*g_source_tokens)[i + 1].kind == source_token_kind::scope
			&& qualified_declarator_start_p(i)) {
			tree outer_scope = g_scope;
			g_scope = scope;
			scan_qualified_type_chain(i, declarator_chain_limit(i, count));
			g_scope = outer_scope;
		}
		if (!source_token_equals(i, "template") || i + 1 >= count || (*g_source_tokens)[i + 1].kind != source_token_kind::less) {
			continue;
		}
		int cursor = i + 1;
		int angle = 0;
		int parm_names[16];
		const char *parm_kinds[16];
		int parm_count = 0;
		for (int guard = 0; cursor < count && guard < 96; ++cursor, ++guard) {
			const source_token_kind kind = (*g_source_tokens)[cursor].kind;
			if (kind == source_token_kind::less) {
				++angle;
				continue;
			}
			if (kind == source_token_kind::greater) {
				if (--angle == 0) {
					break;
				}
				continue;
			}
			if (kind == source_token_kind::semicolon || kind == source_token_kind::left_brace) {
				angle = -1;
				break;
			}
			if (angle == 1 && kind == source_token_kind::identifier && cursor + 1 < count && parm_count < 16) {
				const source_token_kind next = (*g_source_tokens)[cursor + 1].kind;
				if (next == source_token_kind::comma || next == source_token_kind::greater || source_token_is_char(cursor + 1, '=')) {
					parm_names[parm_count++] = cursor;
				}
			}
		}
		if (angle != 0 || cursor >= count) {
			continue;
		}
		const int header_close = cursor;
		int region_end = -1;
		int definition_open = -1;
		int parens = 0;
		for (int k = header_close + 1, guard = 0; k < count && guard < 220; ++k, ++guard) {
			const source_token_kind kind = (*g_source_tokens)[k].kind;
			if (kind == source_token_kind::left_paren) {
				++parens;
			}
			else if (kind == source_token_kind::right_paren && parens > 0) {
				--parens;
			}
			else if (kind == source_token_kind::left_brace) {
				if (requires_expression_at(k) >= 0 && (*g_source_tokens)[k].pair > k) {
					k = (*g_source_tokens)[k].pair;
					continue;
				}
				definition_open = k;
				break;
			}
			else if (kind == source_token_kind::semicolon && parens == 0) {
				region_end = k;
				break;
			}
		}
		for (int p = 0; p < parm_count; ++p) {
			const source_token &parm = (*g_source_tokens)[parm_names[p]];
			parm_kinds[p] = "type";
			out_printf("GSETOK\t%d\t%d\t%d\t%s\n", parm.line, parm.byte_column, (int)(parm.finish - parm.start), parm_kinds[p]);
		}
		if (region_end < 0) {
			if (definition_open > header_close) {
				tree outer_scope = g_scope;
				g_scope = scope;
				for (int k = header_close + 1; k < definition_open; ++k) {
					if ((*g_source_tokens)[k].kind == source_token_kind::identifier
						&& (*g_source_tokens)[k + 1].kind == source_token_kind::scope
						&& (*g_source_tokens)[k - 1].kind != source_token_kind::scope) {
						scan_qualified_type_chain(k, definition_open);
					}
				}
				g_scope = outer_scope;
			}
			i = header_close;
			continue;
		}
		tree previous_scope = g_scope;
		auto_vec<tree, 32> *previous_locals = g_local_decls;
		g_scope = scope;
		g_local_decls = nullptr;
		for (int k = header_close + 1; k < region_end; ++k) {
			if ((*g_source_tokens)[k].kind != source_token_kind::left_brace || (*g_source_tokens)[k].pair <= k) {
				continue;
			}
			const int requires_index = requires_expression_at(k);
			if (requires_index >= 0) {
				scan_template_body(requires_index - 1, (*g_source_tokens)[k].pair);
				k = (*g_source_tokens)[k].pair;
			}
		}
		for (int k = i + 2; k < header_close; ++k) {
			if ((*g_source_tokens)[k].kind != source_token_kind::identifier) {
				continue;
			}
			bool is_parm_name = false;
			for (int p = 0; p < parm_count; ++p) {
				if (parm_names[p] == k) {
					is_parm_name = true;
					break;
				}
			}
			if (is_parm_name || (*g_source_tokens)[k - 1].kind == source_token_kind::scope) {
				continue;
			}
			scan_qualified_type_chain(k, header_close);
		}
		for (int k = header_close + 1; k < region_end; ++k) {
			const source_token &part = (*g_source_tokens)[k];
			if (part.kind != source_token_kind::identifier) {
				continue;
			}
			bool is_parm = false;
			for (int p = 0; p < parm_count; ++p) {
				if (source_token_identifier(parm_names[p]) == source_token_identifier(k)) {
					out_printf("GSETOK\t%d\t%d\t%d\t%s\n", part.line, part.byte_column, (int)(part.finish - part.start), parm_kinds[p]);
					is_parm = true;
					break;
				}
			}
			if (is_parm) {
				continue;
			}
			if (k > 0 && (*g_source_tokens)[k - 1].kind == source_token_kind::scope) {
				continue;
			}
			scan_qualified_type_chain(k, region_end);
		}
		scan_parameter_names(header_close, region_end);
		g_local_decls = previous_locals;
		g_scope = previous_scope;
		i = region_end;
	}
}

static bool module_directive_at(int index) {
	const int cursor = source_token_equals(index, "export") ? index + 1 : index;
	return source_token_equals(cursor, "module") || source_token_equals(cursor, "import");
}

static void emit_identifier_inventory() {
	if (!g_audit || !main_file_under_root() || !ensure_source_tokens()) {
		return;
	}
	const int count = (int)g_source_tokens->length();
	const char *stack[64];
	int depth = 0;
	for (int i = 0; i < count; ++i) {
		const source_token &token = (*g_source_tokens)[i];
		const bool statement_start = i == 0
			|| (*g_source_tokens)[i - 1].kind == source_token_kind::semicolon
			|| (*g_source_tokens)[i - 1].kind == source_token_kind::left_brace
			|| (*g_source_tokens)[i - 1].kind == source_token_kind::right_brace;
		if (statement_start && module_directive_at(i)) {
			while (i + 1 < count && (*g_source_tokens)[i].kind != source_token_kind::semicolon) {
				++i;
			}
			continue;
		}
		if (token.kind == source_token_kind::left_brace) {
			if (depth < 64) {
				stack[depth] = requires_expression_at(i) >= 0 ? "requires_expression"
					: (lambda_body_brace_p(i) ? "lambda_body" : "block");
			}
			++depth;
			continue;
		}
		if (token.kind == source_token_kind::right_brace) {
			if (depth > 0) {
				--depth;
			}
			continue;
		}
		if (token.kind != source_token_kind::identifier) {
			continue;
		}
		tree id = source_token_identifier(i);
		if (!id || IDENTIFIER_KEYWORD_P(id)) {
			continue;
		}
		const char *context = depth > 0 && depth <= 64 ? stack[depth - 1] : "block";
		out_printf("GSEIDENT\t%d\t%d\t%d\t%s\n", token.line, token.byte_column, (int)(token.finish - token.start), context);
	}
}

static void emit_static_assert_names() {
	if (g_index_only || !ensure_source_tokens()) {
		return;
	}
	const int count = (int)g_source_tokens->length();
	struct scope_frame { int close; tree ns; tree cls; };
	scope_frame stack[32];
	int depth = 0;
	tree previous_scope = g_scope;
	tree previous_class = g_class_context;
	tree previous_function = g_function;
	auto_vec<tree, 32> *previous_locals = g_local_decls;
	g_local_decls = nullptr;
	g_function = NULL_TREE;
	for (int i = 0; i < count; ++i) {
		while (depth > 0 && i > stack[depth - 1].close) {
			--depth;
		}
		const source_token &token = (*g_source_tokens)[i];
		if (token.kind == source_token_kind::left_brace) {
			if (token.pair > i) {
				i = token.pair;
			}
			continue;
		}
		if (token.kind != source_token_kind::identifier) {
			continue;
		}
		tree scope = depth > 0 ? stack[depth - 1].ns : global_namespace;
		tree enclosing_class = depth > 0 ? stack[depth - 1].cls : NULL_TREE;
		if (source_token_equals(i, "namespace")) {
			int cursor = i + 1;
			tree ns = scope;
			for (int guard = 0; cursor < count && guard < 24; ++cursor, ++guard) {
				const source_token_kind kind = (*g_source_tokens)[cursor].kind;
				if (kind == source_token_kind::left_brace || kind == source_token_kind::semicolon) {
					break;
				}
				if (source_token_is_char(cursor, '=')) {
					ns = NULL_TREE;
					break;
				}
				if (kind == source_token_kind::identifier && ns) {
					tree next_ns = resolve_scope_component(ns, source_token_identifier(cursor), LOOK_want::NAMESPACE);
					next_ns = next_ns ? preferred_type_binding(next_ns) : NULL_TREE;
					ns = next_ns && TREE_CODE(next_ns) == NAMESPACE_DECL ? next_ns : NULL_TREE;
				}
			}
			if (cursor < count && (*g_source_tokens)[cursor].kind == source_token_kind::left_brace
				&& (*g_source_tokens)[cursor].pair > cursor && ns && depth < 32) {
				stack[depth++] = { (*g_source_tokens)[cursor].pair, ns, NULL_TREE };
				i = cursor;
			}
			continue;
		}
		if ((source_token_equals(i, "class") || source_token_equals(i, "struct") || source_token_equals(i, "union"))
			&& !(i > 0 && source_token_equals(i - 1, "enum"))) {
			int name = -1;
			int open = -1;
			bool bases = false;
			int cursor = i + 1;
			for (int guard = 0; cursor < count && guard < 96; ++cursor, ++guard) {
				const source_token &part = (*g_source_tokens)[cursor];
				if (part.kind == source_token_kind::semicolon) {
					break;
				}
				if (part.kind == source_token_kind::left_brace) {
					open = cursor;
					break;
				}
				if (source_token_is_char(cursor, '[')) {
					int brackets = 0;
					for (; cursor < count; ++cursor) {
						if (source_token_is_char(cursor, '[')) {
							++brackets;
						}
						else if (source_token_is_char(cursor, ']') && --brackets == 0) {
							break;
						}
					}
					continue;
				}
				if (source_token_is_char(cursor, ':')) {
					bases = true;
					continue;
				}
				if (!bases && part.kind == source_token_kind::identifier && !source_token_equals(cursor, "final")) {
					name = cursor;
				}
			}
			if (open < 0 || name < 0 || (*g_source_tokens)[open].pair <= open || depth >= 32) {
				continue;
			}
			tree binding = resolve_scope_component(enclosing_class ? enclosing_class : scope, source_token_identifier(name));
			tree cls = binding_scope_type(binding);
			if (!cls || !CLASS_TYPE_P(cls)) {
				continue;
			}
			stack[depth++] = { (*g_source_tokens)[open].pair, scope, cls };
			i = open;
			continue;
		}
		if (!source_token_equals(i, "static_assert") || i + 1 >= count
			|| (*g_source_tokens)[i + 1].kind != source_token_kind::left_paren) {
			continue;
		}
		int parentheses = 0;
		int close = -1;
		for (int cursor = i + 1; cursor < count; ++cursor) {
			const source_token_kind kind = (*g_source_tokens)[cursor].kind;
			if (kind == source_token_kind::left_paren) {
				++parentheses;
			}
			else if (kind == source_token_kind::right_paren && --parentheses == 0) {
				close = cursor;
				break;
			}
		}
		if (close < 0) {
			continue;
		}
		g_scope = scope;
		g_class_context = enclosing_class;
		scan_binding_type_qualifiers(i + 1, close, false);
		g_scope = previous_scope;
		g_class_context = previous_class;
		i = close;
	}
	g_local_decls = previous_locals;
	g_function = previous_function;
	g_class_context = previous_class;
	g_scope = previous_scope;
}

static int declaration_scan_start(tree decl, int anchor) {
	if (TREE_CODE(decl) == FUNCTION_DECL && !source_token_matches_identifier(anchor, DECL_NAME(decl))
		&& !DECL_CONSTRUCTOR_P(decl) && !DECL_DESTRUCTOR_P(decl) && !DECL_OVERLOADED_OPERATOR_P(decl)) {
		return anchor;
	}
	for (int index = anchor - 1; index >= 0; --index) {
		const source_token_kind kind = (*g_source_tokens)[index].kind;
		if (kind == source_token_kind::semicolon || kind == source_token_kind::left_brace || kind == source_token_kind::right_brace) {
			return index + 1;
		}
	}
	return 0;
}

static int declaration_scan_finish(tree decl, int anchor) {
	int parentheses = 0;
	int braces = 0;
	for (int index = anchor; index < (int)g_source_tokens->length(); ++index) {
		const source_token_kind kind = (*g_source_tokens)[index].kind;
		if (kind == source_token_kind::left_paren) {
			++parentheses;
		}
		else if (kind == source_token_kind::right_paren && parentheses > 0) {
			--parentheses;
		}
		else if (kind == source_token_kind::left_brace) {
			if (parentheses == 0 && braces == 0 && (TREE_CODE(decl) == FUNCTION_DECL || real_type_decl(decl))) {
				return index;
			}
			++braces;
		}
		else if (kind == source_token_kind::right_brace && braces > 0) {
			--braces;
		}
		else if (kind == source_token_kind::semicolon && parentheses == 0 && braces == 0) {
			return index;
		}
	}
	return (int)g_source_tokens->length();
}

static int trailing_return_arrow(int anchor, int finish) {
	int parentheses = 0;
	for (int index = anchor + 1; index < finish; ++index) {
		const source_token_kind kind = (*g_source_tokens)[index].kind;
		if (kind == source_token_kind::left_paren) {
			++parentheses;
		}
		else if (kind == source_token_kind::right_paren && parentheses > 0) {
			--parentheses;
		}
		else if (parentheses == 0 && kind == source_token_kind::greater
			&& source_token_is_char(index - 1, '-')
			&& (*g_source_tokens)[index - 1].finish == (*g_source_tokens)[index].start) {
			return index;
		}
	}
	return -1;
}

static int find_decl_anchor(tree decl) {
	const location_t loc = DECL_SOURCE_LOCATION(decl);
	const int exact = source_token_at(loc, source_token_kind::identifier);
	if (exact >= 0 && source_token_matches_identifier(exact, DECL_NAME(decl))) {
		return exact;
	}
	tree id = DECL_NAME(decl);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE || !ensure_source_tokens()) {
		return exact;
	}
	const expanded_location xl = expand_cached(loc);
	if (!xl.file || xl.line <= 0) {
		return exact;
	}
	int low = 0;
	int high = (int)g_source_tokens->length();
	while (low < high) {
		const int mid = low + (high - low) / 2;
		if ((*g_source_tokens)[mid].line < xl.line) low = mid + 1;
		else high = mid;
	}
	for (int i = low; i < (int)g_source_tokens->length() && (*g_source_tokens)[i].line == xl.line; ++i) {
		if ((*g_source_tokens)[i].kind == source_token_kind::identifier && source_token_matches_identifier(i, id)) {
			return i;
		}
	}
	return exact;
}

static void scan_preceding_annotations(int start) {
	if (!g_src || start <= 0 || start >= (int)g_source_tokens->length()) {
		return;
	}
	// declaration_scan_start halts just past the first `}`/`{`/`;` it meets walking back from
	// the declarator. A preceding `[[= ... {} ...]]` annotation carries a braced-init `{}`, so
	// when one is present the region is cut right after that `}` and `start - 1` is a
	// right_brace -- whether `start` then lands on the `]]` closer (last entry braced) or on a
	// `,` before a later unbraced entry. Either way, recover the whole block from its `[[`.
	if ((*g_source_tokens)[start - 1].kind != source_token_kind::right_brace) {
		return;
	}
	for (int k = start - 1, guard = 0; k >= 0 && guard < 160; --k, ++guard) {
		const source_token &tk = (*g_source_tokens)[k];
		if (tk.kind == source_token_kind::semicolon) {
			return;
		}
		if (tk.kind == source_token_kind::right_brace && tk.pair >= 0 && tk.pair < k) {
			k = tk.pair;
			continue;
		}
		for (long p = tk.start; p + 1 < g_src_len && p < tk.finish; ++p) {
			if (g_src[p] == '[' && g_src[p + 1] == '[') {
				scan_binding_type_qualifiers(k, start, false);
				return;
			}
		}
	}
}

static void push_template_parms(tree tmpl, auto_vec<tree, 32> *locals) {
	if (!tmpl || TREE_CODE(tmpl) != TEMPLATE_DECL) {
		return;
	}
	for (tree level = DECL_TEMPLATE_PARMS(tmpl); level; level = TREE_CHAIN(level)) {
		tree vec = TREE_VALUE(level);
		if (!vec || TREE_CODE(vec) != TREE_VEC) {
			continue;
		}
		for (int i = 0; i < TREE_VEC_LENGTH(vec); ++i) {
			tree e = TREE_VEC_ELT(vec, i);
			tree p = e ? TREE_VALUE(e) : NULL_TREE;
			if (p && DECL_P(p)) {
				locals->safe_push(p);
			}
		}
	}
}

static tree enclosing_template(tree decl) {
	const enum tree_code code = TREE_CODE(decl);
	if ((code == FUNCTION_DECL || code == VAR_DECL || code == TYPE_DECL) && DECL_LANG_SPECIFIC(decl) && DECL_TEMPLATE_INFO(decl)) {
		return DECL_TI_TEMPLATE(decl);
	}
	if (code == TYPE_DECL) {
		tree t = TREE_TYPE(decl);
		if (t && RECORD_OR_UNION_TYPE_P(t) && CLASSTYPE_TEMPLATE_INFO(t)) {
			return CLASSTYPE_TI_TEMPLATE(t);
		}
	}
	tree ctx = DECL_CONTEXT(decl);
	if (ctx && RECORD_OR_UNION_TYPE_P(ctx) && CLASSTYPE_TEMPLATE_INFO(ctx)) {
		tree primary = CLASSTYPE_TI_TEMPLATE(ctx);
		if (primary && TREE_CODE(primary) == TEMPLATE_DECL) {
			for (tree spec = DECL_TEMPLATE_SPECIALIZATIONS(primary); spec; spec = TREE_CHAIN(spec)) {
				tree spec_tmpl = TREE_VALUE(spec);
				if (spec_tmpl && TREE_CODE(spec_tmpl) == TEMPLATE_DECL) {
					tree result = DECL_TEMPLATE_RESULT(spec_tmpl);
					if (result && TREE_TYPE(result) == ctx) {
						return spec_tmpl;
					}
				}
			}
		}
		return primary;
	}
	return NULL_TREE;
}

static bool body_has_template_for(int open, int close) {
	for (int i = open + 1; i + 1 < close; ++i) {
		if ((*g_source_tokens)[i].kind == source_token_kind::identifier && source_token_equals(i, "template")
			&& (*g_source_tokens)[i + 1].kind == source_token_kind::identifier && source_token_equals(i + 1, "for")) {
			return true;
		}
	}
	return false;
}

static void scan_declaration_type_qualifiers(tree decl, tree tmpl_override = NULL_TREE) {
	if (!decl || !DECL_P(decl) || (DECL_ARTIFICIAL(decl) && !real_type_decl(decl))
		|| ((TREE_CODE(decl) == VAR_DECL || TREE_CODE(decl) == FUNCTION_DECL) && DECL_LOCAL_DECL_P(decl))
		|| !in_main_file(DECL_SOURCE_LOCATION(decl)) || !ensure_source_tokens()) {
		return;
	}
	switch (TREE_CODE(decl)) {
		case FUNCTION_DECL:
		case VAR_DECL:
		case FIELD_DECL:
		case TYPE_DECL:
		case PARM_DECL:
		case CONCEPT_DECL:
		case TEMPLATE_DECL:
			break;
		default:
			return;
	}
	const int anchor = find_decl_anchor(decl);
	if (anchor < 0) {
		return;
	}
	const int start = declaration_scan_start(decl, anchor);
	const int finish = declaration_scan_finish(decl, anchor);
	const bool function_declarator = TREE_CODE(decl) == FUNCTION_DECL
		|| (TREE_CODE(decl) == TEMPLATE_DECL && DECL_TEMPLATE_RESULT(decl) && TREE_CODE(DECL_TEMPLATE_RESULT(decl)) == FUNCTION_DECL);
	const int trailing_return = function_declarator ? trailing_return_arrow(anchor, finish) : -1;
	tree previous_scope = g_scope;
	tree previous_function = g_function;
	tree previous_class = g_class_context;
	auto_vec<tree, 32> locals;
	auto_vec<tree, 32> *previous_locals = g_local_decls;
	tree declaration_scope = enclosing_namespace(decl);
	tree declaration_class = TYPE_P(DECL_CONTEXT(decl)) ? DECL_CONTEXT(decl) : NULL_TREE;
	tree declaration_function = TREE_CODE(decl) == FUNCTION_DECL ? decl : NULL_TREE;
	g_scope = declaration_scope;
	g_function = declaration_function;
	g_class_context = declaration_class;
	tree tmpl = tmpl_override ? tmpl_override : enclosing_template(decl);
	if (TREE_CODE(decl) == FUNCTION_DECL || tmpl) {
		g_local_decls = &locals;
		if (TREE_CODE(decl) == FUNCTION_DECL) {
			for (tree param = DECL_ARGUMENTS(decl); param; param = DECL_CHAIN(param)) {
				if (DECL_P(param)) {
					g_local_decls->safe_push(param);
				}
			}
		}
		push_template_parms(tmpl, &locals);
	}
	const bool qualified_declarator = anchor > start && (*g_source_tokens)[anchor - 1].kind == source_token_kind::scope;
	if (qualified_declarator) {
		g_scope = lexical_namespace_of(decl);
		g_class_context = NULL_TREE;
		g_function = NULL_TREE;
	}
	scan_preceding_annotations(start);
	for (int index = start; index < finish; ++index) {
		if (qualified_declarator && index == anchor) {
			g_scope = declaration_scope;
			g_class_context = declaration_class;
			g_function = declaration_function;
		}
		if ((*g_source_tokens)[index].kind == source_token_kind::identifier) {
			scan_qualified_type_chain(index, finish, anchor, trailing_return >= 0 && index == trailing_return + 1);
		}
	}
	g_scope = declaration_scope;
	g_class_context = declaration_class;
	g_function = declaration_function;
	if (TREE_CODE(decl) == FUNCTION_DECL && finish < (int)g_source_tokens->length()
		&& (*g_source_tokens)[finish].kind == source_token_kind::left_brace && (*g_source_tokens)[finish].pair > finish
		&& (tmpl || body_has_template_for(finish, (*g_source_tokens)[finish].pair))) {
		scan_template_body(finish, (*g_source_tokens)[finish].pair);
	}
	if (TREE_CODE(decl) == FUNCTION_DECL
		|| (TREE_CODE(decl) == TEMPLATE_DECL && DECL_TEMPLATE_RESULT(decl) && TREE_CODE(DECL_TEMPLATE_RESULT(decl)) == FUNCTION_DECL)) {
		scan_parameter_names(anchor, finish);
	}
	g_local_decls = previous_locals;
	g_class_context = previous_class;
	g_function = previous_function;
	g_scope = previous_scope;
}

static void emit_qual(location_t use_loc, tree decl) {
	if (g_index_only) return;
	if (!g_scope || !decl || !DECL_P(decl) || use_loc == UNKNOWN_LOCATION) return;
	tree id = DECL_NAME(decl);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE || IDENTIFIER_LENGTH(id) <= 0) return;
	const char *name = IDENTIFIER_POINTER(id);
	if (name[0] == '_' || name[0] == '.' || strchr(name, ' ')) return;
	const int located = source_token_at(get_start(use_loc), source_token_kind::identifier);
	if (located < 0) return;
	int head = located;
	while (head >= 2 && (*g_source_tokens)[head - 1].kind == source_token_kind::scope && (*g_source_tokens)[head - 2].kind == source_token_kind::identifier) {
		head -= 2;
	}
	if (using_declaration_chain_p(head)) return;
	int components[32];
	const int count = parse_qualified_chain(head, (int)g_source_tokens->length(), components, 32);
	if (count < 2) return;
	if (!source_token_matches_identifier(components[count - 1], id)) return;
	if (in_main_file(DECL_SOURCE_LOCATION(decl)) && find_decl_anchor(decl) == components[count - 1]) return;
	tree resolved[32];
	resolved[count - 1] = decl;
	tree ctx = CP_DECL_CONTEXT(decl);
	for (int index = count - 2; index >= 0; --index) {
		tree entity = NULL_TREE;
		for (int guard = 0; ctx && ctx != global_namespace && guard < 40; ++guard) {
			if (TREE_CODE(ctx) == NAMESPACE_DECL) {
				if (source_token_matches_identifier(components[index], DECL_NAME(ctx))) {
					entity = ctx;
					ctx = CP_DECL_CONTEXT(ctx);
					break;
				}
				if (!DECL_NAME(ctx) || DECL_NAMESPACE_INLINE_P(ctx)) {
					ctx = CP_DECL_CONTEXT(ctx);
					continue;
				}
				break;
			}
			if (TYPE_P(ctx)) {
				tree type_name = TYPE_NAME(ctx);
				if (type_name && DECL_P(type_name) && source_token_matches_identifier(components[index], DECL_NAME(type_name))) {
					entity = type_name;
					ctx = CP_DECL_CONTEXT(type_name);
				}
				break;
			}
			break;
		}
		if (!entity) return;
		resolved[index] = entity;
	}
	emit_redundant_prefix(components, count, resolved, (*g_source_tokens)[components[0]]);
}

struct emitted_targ_range {
	int line = 0;
	int column = 0;
	int end_line = 0;
	int end_column = 0;
};

static auto_vec<emitted_targ_range, 32> *g_emitted_targ_lists = nullptr;

static void emit_targ_range(const source_token& lt, const source_token& gt) {
	const int end_column = gt.column + 1;
	if (!g_emitted_targ_lists) {
		g_emitted_targ_lists = new auto_vec<emitted_targ_range, 32>();
	}
	for (const emitted_targ_range& r : *g_emitted_targ_lists) {
		if (r.line == lt.line && r.column == lt.column && r.end_line == gt.line && r.end_column == end_column) return;
	}
	g_emitted_targ_lists->safe_push({ .line = lt.line, .column = lt.column, .end_line = gt.line, .end_column = end_column });
	out_printf("GSETARG\t%s\t%d\t%d\t%d\t%d\n", main_input_filename, lt.line, lt.column, gt.line, end_column);
}

struct targ_list_span {
	int open = -1;
	int close = -1;
	bool brace_init = false;
};

static bool find_explicit_targ_list(location_t use, tree name_id, bool allow_brace, targ_list_span *out) {
	if (!main_input_filename || use == UNKNOWN_LOCATION || !name_id || TREE_CODE(name_id) != IDENTIFIER_NODE) return false;
	const location_t start = get_start(use);
	if (!in_main_file(start)) return false;
	const int first = source_token_at(start, source_token_kind::identifier);
	if (first < 0) return false;
	const int limit = (int)g_source_tokens->length();
	int cursor = first;
	while (cursor + 1 < limit && (*g_source_tokens)[cursor + 1].kind == source_token_kind::scope) {
		cursor += 2;
		if (cursor < limit && source_token_equals(cursor, "template")) ++cursor;
		if (cursor >= limit || (*g_source_tokens)[cursor].kind != source_token_kind::identifier) return false;
	}
	const source_token& name = (*g_source_tokens)[cursor];
	const long name_len = name.finish - name.start;
	if (name_len != (long)IDENTIFIER_LENGTH(name_id) || strncmp(g_src + name.start, IDENTIFIER_POINTER(name_id), name_len) != 0) return false;
	const int open = cursor + 1;
	if (open >= limit || (*g_source_tokens)[open].kind != source_token_kind::less) return false;
	if (open + 1 < limit && (*g_source_tokens)[open + 1].kind == source_token_kind::greater) return false;
	int close = -1;
	int angle = 1;
	int paren = 0;
	int brace = 0;
	for (int scan = open + 1; scan < limit && scan - open < 256; ++scan) {
		const source_token_kind kind = (*g_source_tokens)[scan].kind;
		if (kind == source_token_kind::left_paren) { ++paren; continue; }
		if (kind == source_token_kind::right_paren) { if (--paren < 0) return false; continue; }
		if (kind == source_token_kind::left_brace) { ++brace; continue; }
		if (kind == source_token_kind::right_brace) { if (--brace < 0) return false; continue; }
		if (paren > 0 || brace > 0) continue;
		if (kind == source_token_kind::semicolon) return false;
		if (kind == source_token_kind::less) ++angle;
		else if (kind == source_token_kind::greater && --angle == 0) { close = scan; break; }
	}
	if (close < 0 || close + 1 >= limit) return false;
	const source_token_kind next = (*g_source_tokens)[close + 1].kind;
	if (next == source_token_kind::left_paren) out->brace_init = false;
	else if (allow_brace && next == source_token_kind::left_brace) out->brace_init = true;
	else return false;
	out->open = open;
	out->close = close;
	return true;
}

static tree deduction_source_arg(tree arg, bool *lossy) {
	for (int guard = 0; arg && guard < 16; ++guard) {
		if (location_wrapper_p(arg)) {
			arg = tree_strip_any_location_wrapper(arg);
			continue;
		}
		const enum tree_code code = TREE_CODE(arg);
		if (code == NOP_EXPR || code == CONVERT_EXPR || code == NON_LVALUE_EXPR) {
			tree inner = TREE_OPERAND(arg, 0);
			if (!inner || !TREE_TYPE(arg) || !TREE_TYPE(inner)
				|| !same_type_ignoring_top_level_qualifiers_p(non_reference(TREE_TYPE(arg)), non_reference(TREE_TYPE(inner)))) {
				*lossy = true;
				return arg;
			}
			arg = inner;
			continue;
		}
		if (code == TARGET_EXPR) {
			tree init = TARGET_EXPR_INITIAL(arg);
			if (!init || TREE_CODE(init) != AGGR_INIT_EXPR || !AGGR_INIT_VIA_CTOR_P(init)) return arg;
			tree ctor = AGGR_INIT_EXPR_FN(init);
			if (ctor && TREE_CODE(ctor) == ADDR_EXPR) ctor = TREE_OPERAND(ctor, 0);
			if (!ctor || TREE_CODE(ctor) != FUNCTION_DECL || aggr_init_expr_nargs(init) < 2
				|| (!DECL_COPY_CONSTRUCTOR_P(ctor) && !DECL_MOVE_CONSTRUCTOR_P(ctor))) {
				*lossy = true;
				return arg;
			}
			arg = AGGR_INIT_EXPR_ARG(init, 1);
			continue;
		}
		return arg;
	}
	return arg;
}

static bool template_args_deducible(tree call, tree fn) {
	tree tmpl = DECL_TI_TEMPLATE(fn);
	if (!tmpl || TREE_CODE(tmpl) != TEMPLATE_DECL) return false;
	const int nargs = call_expr_nargs(call);
	if (nargs < 0) return false;
	tree *args = XALLOCAVEC(tree, nargs ? nargs : 1);
	for (int i = 0; i < nargs; ++i) {
		bool lossy = false;
		args[i] = deduction_source_arg(CALL_EXPR_ARG(call, i), &lossy);
		if (lossy || !args[i] || !TREE_TYPE(args[i])) return false;
	}
	tree targs = make_tree_vec(DECL_NTPARMS(tmpl));
	struct conversion **convs = XALLOCAVEC(struct conversion *, nargs ? nargs : 1);
	memset(convs, 0, sizeof(struct conversion *) * (size_t)(nargs ? nargs : 1));
	tree deduced = fn_type_unification(tmpl, NULL_TREE, targs, args, (unsigned)nargs, NULL_TREE, DEDUCE_CALL, LOOKUP_NORMAL, convs, false, false);
	return deduced == fn;
}

static void maybe_emit_deducible_template_args(tree call, tree fn, location_t use) {
	if (g_index_only) return;
	if (!DECL_LANG_SPECIFIC(fn) || !DECL_TEMPLATE_INFO(fn) || !DECL_USE_TEMPLATE(fn)) return;
	tree fntype = TREE_TYPE(fn);
	if (!fntype || TREE_CODE(fntype) != FUNCTION_TYPE) return;
	targ_list_span span;
	if (!find_explicit_targ_list(use, DECL_NAME(fn), false, &span)) return;
	if (!template_args_deducible(call, fn)) return;
	emit_targ_range((*g_source_tokens)[span.open], (*g_source_tokens)[span.close]);
}

static bool class_targs_deducible(tree type, tree tmpl, tree init) {
	tree placeholder = make_template_placeholder(tmpl);
	if (!placeholder || placeholder == error_mark_node) return false;
	tree deduced = do_auto_deduction(placeholder, init, placeholder, tf_none, adc_variable_type, NULL_TREE, LOOKUP_NORMAL);
	return deduced && deduced != error_mark_node && TYPE_P(deduced) && same_type_ignoring_top_level_qualifiers_p(deduced, type);
}

static void maybe_emit_deducible_class_targs(tree t) {
	if (g_index_only) return;
	tree type = TREE_TYPE(t);
	if (!type || !CLASS_TYPE_P(type) || !TYPE_LANG_SPECIFIC(type)) return;
	if (!CLASSTYPE_TEMPLATE_INFO(type) || !CLASSTYPE_USE_TEMPLATE(type)) return;
	tree tmpl = CLASSTYPE_TI_TEMPLATE(type);
	if (!tmpl || TREE_CODE(tmpl) != TEMPLATE_DECL || !ctad_template_p(tmpl)) return;
	location_t use = EXPR_LOCATION(t);
	if (use == UNKNOWN_LOCATION) use = g_use_anchor;
	targ_list_span span;
	if (!find_explicit_targ_list(use, DECL_NAME(tmpl), true, &span)) return;
	tree init = NULL_TREE;
	if (TREE_CODE(t) == AGGR_INIT_EXPR) {
		if (!AGGR_INIT_VIA_CTOR_P(t)) return;
		const int nargs = aggr_init_expr_nargs(t);
		auto_vec<tree, 8> args;
		for (int i = 1; i < nargs; ++i) {
			bool lossy = false;
			tree a = deduction_source_arg(AGGR_INIT_EXPR_ARG(t, i), &lossy);
			if (lossy || !a || !TREE_TYPE(a)) return;
			args.safe_push(a);
		}
		if (span.brace_init) {
			vec<constructor_elt, va_gc> *elts = NULL;
			for (unsigned i = 0; i < args.length(); ++i) {
				CONSTRUCTOR_APPEND_ELT(elts, NULL_TREE, args[i]);
			}
			init = build_constructor(init_list_type_node, elts);
			CONSTRUCTOR_IS_DIRECT_INIT(init) = true;
		}
		else {
			for (unsigned i = args.length(); i > 0; --i) {
				init = tree_cons(NULL_TREE, args[i - 1], init);
			}
		}
	}
	else if (TREE_CODE(t) == CONSTRUCTOR) {
		if (!span.brace_init || CONSTRUCTOR_IS_DESIGNATED_INIT(t)) return;
		vec<constructor_elt, va_gc> *elts = NULL;
		unsigned ix;
		tree val;
		FOR_EACH_CONSTRUCTOR_VALUE(CONSTRUCTOR_ELTS(t), ix, val) {
			bool lossy = false;
			tree a = deduction_source_arg(val, &lossy);
			if (lossy || !a || !TREE_TYPE(a)) return;
			CONSTRUCTOR_APPEND_ELT(elts, NULL_TREE, a);
		}
		init = build_constructor(init_list_type_node, elts);
		CONSTRUCTOR_IS_DIRECT_INIT(init) = true;
	}
	else {
		return;
	}
	if (!class_targs_deducible(type, tmpl, init)) return;
	emit_targ_range((*g_source_tokens)[span.open], (*g_source_tokens)[span.close]);
}

static tree callee_identifier(tree fn) {
	for (int guard = 0; fn && guard < 8; ++guard) {
		switch (TREE_CODE(fn)) {
			case TEMPLATE_ID_EXPR: fn = TREE_OPERAND(fn, 0); continue;
			case OVERLOAD: fn = OVL_FIRST(fn); continue;
			case BASELINK: fn = BASELINK_FUNCTIONS(fn); continue;
			case OBJ_TYPE_REF: {
				tree ref_class = obj_type_ref_class(fn);
				tree binfo = ref_class && RECORD_OR_UNION_TYPE_P(ref_class) ? TYPE_BINFO(ref_class) : NULL_TREE;
				tree vfn = binfo ? lookup_vfn_in_binfo(OBJ_TYPE_REF_TOKEN(fn), binfo) : NULL_TREE;
				if (!vfn) return NULL_TREE;
				fn = vfn;
				continue;
			}
			case FUNCTION_DECL:
			case TEMPLATE_DECL: return DECL_NAME(fn);
			case IDENTIFIER_NODE: return fn;
			default: return NULL_TREE;
		}
	}
	return NULL_TREE;
}

static const char *template_id_kind(tree fn) {
	for (int guard = 0; fn && guard < 8; ++guard) {
		switch (TREE_CODE(fn)) {
			case TEMPLATE_ID_EXPR: fn = TREE_OPERAND(fn, 0); continue;
			case OVERLOAD: fn = OVL_FIRST(fn); continue;
			case BASELINK: fn = BASELINK_FUNCTIONS(fn); continue;
			case TEMPLATE_DECL: fn = DECL_TEMPLATE_RESULT(fn); continue;
			case FUNCTION_DECL: return "function";
			case CONCEPT_DECL: return "type";
			case VAR_DECL: {
				tree ctx = DECL_CONTEXT(fn);
				return (!ctx || TREE_CODE(ctx) == NAMESPACE_DECL || TREE_CODE(ctx) == TRANSLATION_UNIT_DECL) ? "global" : "variable";
			}
			case TYPE_DECL: return "type";
			default: return nullptr;
		}
	}
	return nullptr;
}

static tree callee_decl(tree fn) {
	for (int guard = 0; fn && guard < 8; ++guard) {
		switch (TREE_CODE(fn)) {
			case TEMPLATE_ID_EXPR: fn = TREE_OPERAND(fn, 0); continue;
			case OVERLOAD: fn = OVL_FIRST(fn); continue;
			case BASELINK: fn = BASELINK_FUNCTIONS(fn); continue;
			case TEMPLATE_DECL: fn = DECL_TEMPLATE_RESULT(fn); continue;
			case FUNCTION_DECL: return fn;
			default: return NULL_TREE;
		}
	}
	return NULL_TREE;
}

static void print_ns_qualified(tree ns) {
	const char *stack[32];
	int n = 0;
	for (tree c = ns; c && TREE_CODE(c) == NAMESPACE_DECL && c != global_namespace && n < 32; c = DECL_CONTEXT(c)) {
		tree id = DECL_NAME(c);
		if (id && TREE_CODE(id) == IDENTIFIER_NODE) {
			const char *s = IDENTIFIER_POINTER(id);
			if (!(IDENTIFIER_LENGTH(id) >= 2 && s[0] == '_' && s[1] == '_'))
				stack[n++] = s;
		}
	}
	for (int i = n - 1; i >= 0; --i) {
		out_str(stack[i]);
		if (i) out_str("::");
	}
}

static tree template_id_decl(tree fn) {
	for (int guard = 0; fn && guard < 8; ++guard) {
		switch (TREE_CODE(fn)) {
			case TEMPLATE_ID_EXPR: fn = TREE_OPERAND(fn, 0); continue;
			case OVERLOAD: fn = OVL_FIRST(fn); continue;
			case BASELINK: fn = BASELINK_FUNCTIONS(fn); continue;
			case TEMPLATE_DECL: fn = DECL_TEMPLATE_RESULT(fn); continue;
			case FUNCTION_DECL:
			case VAR_DECL:
			case TYPE_DECL:
			case CONCEPT_DECL: return fn;
			default: return NULL_TREE;
		}
	}
	return NULL_TREE;
}

static void walk_bind_expr(tree bind) {
	int closing = source_token_at(EXPR_LOCATION(bind), source_token_kind::right_brace);
	int opening = closing >= 0 ? (*g_source_tokens)[closing].pair : -1;
	if (opening >= 0) {
		(*g_source_tokens)[opening].binding_scope = true;
	}
	const unsigned start = g_local_decls ? g_local_decls->length() : 0;
	if (g_local_decls) {
		for (tree decl = BIND_EXPR_VARS(bind); decl; decl = DECL_CHAIN(decl)) {
			if (DECL_P(decl)) {
				g_local_decls->safe_push(decl);
			}
		}
	}
	tree body = BIND_EXPR_BODY(bind);
	if (body) {
		walk_expr_tree(&body);
	}
	if (opening >= 0) {
		scan_binding_type_qualifiers(opening, closing);
	}
	if (g_local_decls) {
		g_local_decls->truncate(start);
	}
}

static void scan_function_body_tokens(tree fndecl) {
	if (!fndecl || !in_main_file(DECL_SOURCE_LOCATION(fndecl)) || !ensure_source_tokens()) {
		return;
	}
	const int anchor = source_token_at(DECL_SOURCE_LOCATION(fndecl), source_token_kind::identifier);
	if (anchor < 0) {
		return;
	}
	int parens = 0;
	int open = -1;
	for (int i = anchor; i < (int)g_source_tokens->length(); ++i) {
		const source_token_kind k = (*g_source_tokens)[i].kind;
		if (k == source_token_kind::left_paren) {
			++parens;
		}
		else if (k == source_token_kind::right_paren) {
			if (parens > 0) --parens;
		}
		else if (parens == 0 && k == source_token_kind::semicolon) {
			return;
		}
		else if (parens == 0 && k == source_token_kind::left_brace) {
			open = i;
			break;
		}
	}
	if (open < 0) {
		return;
	}
	const int close = (*g_source_tokens)[open].pair;
	if (close > open) {
		scan_binding_type_qualifiers(open, close, false);
	}
}

static void walk_function_body(tree fndecl, tree body) {
	auto_vec<tree, 32> locals;
	auto_vec<tree, 32> *previous_decls = g_local_decls;
	tree previous_function = g_function;
	const unsigned start = previous_decls ? previous_decls->length() : 0;
	if (!g_local_decls) {
		g_local_decls = &locals;
	}
	g_function = fndecl;
	for (tree param = DECL_ARGUMENTS(fndecl); param; param = DECL_CHAIN(param)) {
		if (DECL_P(param)) {
			g_local_decls->safe_push(param);
		}
	}
	walk_expr_tree(&body);
	scan_function_body_tokens(fndecl);
	g_local_decls->truncate(start);
	g_function = previous_function;
	g_local_decls = previous_decls;
}

static void walk_with_anchor(tree node, location_t anchor) {
	location_t previous_anchor = g_use_anchor;
	g_use_anchor = anchor;
	walk_expr_tree(&node);
	g_use_anchor = previous_anchor;
}

static tree walk_cb(tree *tp, int *walk_subtrees, void *) {
	tree t = *tp;
	if (!t) return NULL_TREE;

	if (location_wrapper_p(t)) {
		tree inner = tree_strip_any_location_wrapper(t);
		if (inner && DECL_P(inner)) {
			emit_decl(inner, EXPR_LOCATION(t));
			emit_ref(EXPR_LOCATION(t), inner, ident_len(inner));
			emit_qual(EXPR_LOCATION(t), inner);
		}
		*walk_subtrees = 0;
		return NULL_TREE;
	}

	switch (TREE_CODE(t)) {
		case BIND_EXPR: {
			walk_bind_expr(t);
			*walk_subtrees = 0;
			break;
		}
		case DECL_EXPR: {
			tree decl = DECL_EXPR_DECL(t);
			if (g_local_decls && decl && DECL_P(decl) && !DECL_ARTIFICIAL(decl)) {
				g_local_decls->safe_push(decl);
			}
			emit_decl(decl, DECL_SOURCE_LOCATION(decl));
			if (decl && TREE_CODE(decl) == VAR_DECL) {
				tree init = DECL_INITIAL(decl);
				if (init) {
					walk_with_anchor(init, DECL_SOURCE_LOCATION(decl));
					scan_decl_designators(decl, init);
				}
			}
			else if (decl && TREE_CODE(decl) == TYPE_DECL) {
				if (real_type_decl(decl) && in_main_file(DECL_SOURCE_LOCATION(decl))) {
					process_type_decl(decl);
					if (g_local_decls) g_local_decls->safe_push(decl);
				}
				tree aliased = TREE_TYPE(decl);
				if (aliased && TREE_CODE(aliased) == DECLTYPE_TYPE) {
					tree expr = DECLTYPE_TYPE_EXPR(aliased);
					if (expr) walk_expr_tree(&expr);
				}
				if (aliased && TREE_CODE(aliased) == TYPENAME_TYPE) {
					emit_type_name_at(DECL_SOURCE_LOCATION(decl), aliased);
				}
			}
			break;
		}
		case RANGE_FOR_STMT: {
			if (tree decl = RANGE_FOR_DECL(t); decl && DECL_P(decl)) {
				emit_decl(decl, DECL_SOURCE_LOCATION(decl));
			}
			break;
		}
		case COMPONENT_REF: {
			tree field = TREE_OPERAND(t, 1);
			location_t highlight = EXPR_LOCATION(t);
			if (highlight == UNKNOWN_LOCATION) highlight = g_use_anchor;
			if (field && TREE_CODE(field) == FIELD_DECL && !DECL_ARTIFICIAL(field)) {
				if (tree source = coroutine_frame_source_decl(field)) {
					emit_at_name(highlight, kind_of(source), DECL_NAME(source));
					emit_ref(highlight, source, ident_len(source));
				}
				else {
					emit_at_name(highlight, "member", DECL_NAME(field));
					emit_member_ref(highlight, field);
				}
			}
			else if (field && TREE_CODE(field) == IDENTIFIER_NODE && IDENTIFIER_LENGTH(field) > 0) {
				emit_at_name(highlight, "member", field);
			}
			break;
		}
		case CALL_EXPR: {
			tree fn = CALL_EXPR_FN(t);
			location_t use = fn ? EXPR_LOCATION(fn) : UNKNOWN_LOCATION;
			if (fn) fn = tree_strip_any_location_wrapper(fn);
			if (fn && TREE_CODE(fn) == ADDR_EXPR) fn = TREE_OPERAND(fn, 0);
			if (fn) fn = tree_strip_any_location_wrapper(fn);
			if (fn && TREE_CODE(fn) == FUNCTION_DECL && !DECL_ARTIFICIAL(fn) && !DECL_OVERLOADED_OPERATOR_P(fn)) {
				tree fnid = DECL_NAME(fn);
				bool synth_callee = fnid && TREE_CODE(fnid) == IDENTIFIER_NODE && IDENTIFIER_LENGTH(fnid) >= 2 && IDENTIFIER_POINTER(fnid)[0] == '_' && IDENTIFIER_POINTER(fnid)[1] == '_';
				if (!synth_callee) {
					if (use == UNKNOWN_LOCATION) use = EXPR_LOCATION(t);
					location_t highlight = use;
					if (highlight == UNKNOWN_LOCATION && call_expr_nargs(t) > 0 && CALL_EXPR_ARG(t, 0)) {
						highlight = EXPR_LOCATION(CALL_EXPR_ARG(t, 0));
					}
					if (highlight == UNKNOWN_LOCATION) highlight = g_use_anchor;
					emit_at_name(highlight, "function", DECL_NAME(fn));
					emit_call_ref(use, fn, ident_len(fn));
					emit_qual(use, fn);
					maybe_emit_deducible_template_args(t, fn, use);
				}
			}
			else if (tree id = callee_identifier(fn)) {
				const bool is_operator = IDENTIFIER_LENGTH(id) >= 8 && strncmp(IDENTIFIER_POINTER(id), "operator", 8) == 0;
				if (!is_operator && TREE_CODE(id) == IDENTIFIER_NODE && IDENTIFIER_LENGTH(id) > 0) {
					if (use == UNKNOWN_LOCATION) use = EXPR_LOCATION(t);
					location_t highlight = use;
					if (highlight == UNKNOWN_LOCATION && call_expr_nargs(t) > 0 && CALL_EXPR_ARG(t, 0)) {
						highlight = EXPR_LOCATION(CALL_EXPR_ARG(t, 0));
					}
					if (highlight == UNKNOWN_LOCATION) highlight = g_use_anchor;
					emit_at_name(highlight, "function", id);
					if (tree cd = callee_decl(fn)) {
						emit_call_ref(use, cd, (int)IDENTIFIER_LENGTH(id));
						emit_qual(use, cd);
					}
				}
			}
			const location_t call_anchor = use != UNKNOWN_LOCATION ? use : EXPR_LOCATION(t);
			if (call_anchor != UNKNOWN_LOCATION) {
				if (tree fn_expr = CALL_EXPR_FN(t)) {
					walk_with_anchor(fn_expr, call_anchor);
				}
				for (int i = 0; i < call_expr_nargs(t); ++i) {
					if (tree arg = CALL_EXPR_ARG(t, i)) {
						walk_with_anchor(arg, call_anchor);
					}
				}
				*walk_subtrees = 0;
			}
			break;
		}
		case TEMPLATE_ID_EXPR: {
			tree id = callee_identifier(t);
			const char *k = template_id_kind(t);
			if (k && id && TREE_CODE(id) == IDENTIFIER_NODE && IDENTIFIER_LENGTH(id) > 0) {
				emit_at_name(EXPR_LOCATION(t), k, id);
				if (tree td = template_id_decl(t)) {
					expanded_location dx = expand_cached(DECL_SOURCE_LOCATION(td));
					if (dx.file && dx.line > 0 && under_root_cached(dx.file))
						emit_call_ref(EXPR_LOCATION(t), td, (int)IDENTIFIER_LENGTH(id));
					emit_qual(EXPR_LOCATION(t), td);
				}
			}
			if (tree args = TREE_OPERAND(t, 1); args && TREE_CODE(args) == TREE_VEC) {
				location_t anchor = EXPR_LOCATION(t) != UNKNOWN_LOCATION ? EXPR_LOCATION(t) : g_use_anchor;
				for (int i = 0; i < TREE_VEC_LENGTH(args); ++i) {
					tree a = TREE_VEC_ELT(args, i);
					if (a && TYPE_P(a)) {
						emit_type_name_at(anchor, a);
					}
				}
			}
			break;
		}
		case STATIC_CAST_EXPR:
		case CONST_CAST_EXPR:
		case DYNAMIC_CAST_EXPR:
		case REINTERPRET_CAST_EXPR:
		case CAST_EXPR: {
			location_t anchor = EXPR_LOCATION(t) != UNKNOWN_LOCATION ? EXPR_LOCATION(t) : g_use_anchor;
			emit_type_name_at(anchor, TREE_TYPE(t));
			break;
		}
		case SCOPE_REF: {
			tree name = TREE_OPERAND(t, 1);
			location_t highlight = EXPR_LOCATION(t);
			if (highlight == UNKNOWN_LOCATION) highlight = g_use_anchor;
			if (name && DECL_P(name) && !DECL_ARTIFICIAL(name)) {
				emit_at_name(highlight, kind_of(name), DECL_NAME(name));
				emit_qual(EXPR_LOCATION(t), name);
			}
			else if (name && TREE_CODE(name) == IDENTIFIER_NODE && IDENTIFIER_LENGTH(name) > 0)
				emit_at_name(highlight, "member", name);
			break;
		}
		case ADDR_EXPR:
		case NOP_EXPR:
		case CONVERT_EXPR:
		case NON_LVALUE_EXPR:
		case VIEW_CONVERT_EXPR: {
			tree op = TREE_OPERAND(t, 0);
			if (op && DECL_P(op) && !DECL_ARTIFICIAL(op)) {
				location_t highlight = EXPR_LOCATION(t);
				if (highlight == UNKNOWN_LOCATION) highlight = g_use_anchor;
				const char *k = kind_of(op);
				if (k) {
					emit_at_name(highlight, k, DECL_NAME(op));
					if (EXPR_LOCATION(t) != UNKNOWN_LOCATION) emit_qual(EXPR_LOCATION(t), op);
				}
			}
			break;
		}
		case REQUIRES_EXPR: {
			for (tree parm = REQUIRES_EXPR_PARMS(t); parm; parm = DECL_CHAIN(parm)) {
				if (DECL_P(parm)) {
					emit_decl(parm, DECL_SOURCE_LOCATION(parm));
					if (g_local_decls) g_local_decls->safe_push(parm);
				}
			}
			break;
		}
		case CONSTRUCTOR: {
			tree type = TREE_TYPE(t);
			if (type && RECORD_OR_UNION_TYPE_P(type) && CONSTRUCTOR_NELTS(t) > 0 && !g_index_only && ensure_source_tokens()) {
				location_t inner = UNKNOWN_LOCATION;
				tree probe = t;
				walk_tree_without_duplicates(&probe, first_loc_cb, &inner);
				/* The nearest located subexpression can sit inside a nested
				   lambda body, whose own brace would otherwise be mistaken for
				   the initializer list; an all-constant list has no located
				   subexpression at all, so fall back to the anchor of the
				   expression that owns it and search forward instead.  */
				const location_t anchor = inner != UNKNOWN_LOCATION ? inner : g_use_anchor;
				if (anchor != UNKNOWN_LOCATION) {
					const int at = source_token_lower_bound(anchor);
					int open = -1;
					for (int index = at, guard = 0; index >= 0 && guard < 256; --index, ++guard) {
						const source_token &candidate = (*g_source_tokens)[index];
						if (candidate.kind == source_token_kind::left_brace && candidate.pair > at && designator_list_p(index, type)) {
							open = index;
							break;
						}
					}
					for (int index = at, guard = 0; open < 0 && index < (int)g_source_tokens->length() && guard < 32; ++index, ++guard) {
						const source_token &candidate = (*g_source_tokens)[index];
						if (candidate.kind == source_token_kind::semicolon) {
							break;
						}
						if (candidate.kind == source_token_kind::left_brace && candidate.pair > index && designator_list_p(index, type)) {
							open = index;
						}
					}
					if (open >= 0) {
						scan_designators(open, type);
					}
				}
			}
			break;
		}
		case LAMBDA_EXPR: {
			// The lambda's operator() is walked by its own on_pre_genericize pass, where
			// source locations are still intact. Walking it here (from the enclosing body)
			// would traverse the already-lowered tree and emit garbage-located tokens.
			break;
		}
		default:
			if (TREE_CODE(t) == AGGR_INIT_EXPR || TREE_CODE(t) == CONSTRUCTOR) {
				maybe_emit_deducible_class_targs(t);
			}
			if (g_use_anchor != UNKNOWN_LOCATION && DECL_P(t) && !DECL_ARTIFICIAL(t) && (TREE_CODE(t) == FUNCTION_DECL || TREE_CODE(t) == VAR_DECL || TREE_CODE(t) == CONST_DECL)) {
				emit_at_name(g_use_anchor, kind_of(t), DECL_NAME(t));
				emit_qual(g_use_anchor, t);
			}
			else if (EXPR_P(t) && EXPR_LOCATION(t) != UNKNOWN_LOCATION) {
				const location_t anchor = EXPR_LOCATION(t);
				const int operands = cp_tree_operand_length(t);
				for (int i = 0; i < operands; ++i) {
					tree operand = TREE_OPERAND(t, i);
					if (operand) {
						walk_with_anchor(operand, anchor);
					}
				}
				*walk_subtrees = 0;
			}
			break;
	}
	return NULL_TREE;
}

static bool is_synthesized_special(tree fndecl) {
	if (DECL_ARTIFICIAL(fndecl)) return true;
	tree id = DECL_NAME(fndecl);
	if (id && TREE_CODE(id) == IDENTIFIER_NODE && IDENTIFIER_LENGTH(id) >= 2) {
		const char *s = IDENTIFIER_POINTER(id);
		if (s[0] == '_' && s[1] == '_') return true;
	}
	return false;
}

static bool is_coroutine_actor(tree fndecl) {
	if (!fndecl || TREE_CODE(fndecl) != FUNCTION_DECL || !DECL_LANG_SPECIFIC(fndecl)) return false;
	if (!DECL_COROUTINE_P(fndecl) || DECL_RAMP_P(fndecl)) return false;
	tree ramp = DECL_RAMP_FN(fndecl);
	return ramp && DECL_ACTOR_FN(ramp) == fndecl;
}

static bool is_lambda_op(tree fndecl) {
	if (!fndecl || TREE_CODE(fndecl) != FUNCTION_DECL) return false;
	tree ctx = DECL_CONTEXT(fndecl);
	return ctx && TYPE_P(ctx) && LAMBDA_TYPE_P(ctx);
}

static bool is_user_structor(tree fndecl) {
	if (!fndecl || TREE_CODE(fndecl) != FUNCTION_DECL || DECL_ARTIFICIAL(fndecl)) return false;
	if (!DECL_CONSTRUCTOR_P(fndecl) && !DECL_DESTRUCTOR_P(fndecl)) return false;
	return !DECL_CLONED_FUNCTION_P(fndecl);
}

/* A capture with an initializer introduces a name that survives only as a
   closure field: the lowered body refers to the field, never to the source
   position that declared it.  Left alone the name is uncoloured and the
   qualified-name scanner is free to resolve it against an unrelated namespace,
   so claim the span here — closures are genericized before the body that owns
   them, which is before that scanner runs.  */
static void emit_lambda_captures(tree closure_type) {
	if (!closure_type || !TYPE_P(closure_type) || !LAMBDA_TYPE_P(closure_type)) {
		return;
	}
	tree lambda = CLASSTYPE_LAMBDA_EXPR(closure_type);
	if (!lambda) {
		return;
	}
	for (tree capture = LAMBDA_EXPR_CAPTURE_LIST(lambda); capture; capture = TREE_CHAIN(capture)) {
		tree field = TREE_PURPOSE(capture);
		if (!field || TREE_CODE(field) != FIELD_DECL) {
			continue;
		}
		tree id = DECL_NAME(field);
		if (!id || TREE_CODE(id) != IDENTIFIER_NODE || IDENTIFIER_LENGTH(id) <= 0) {
			continue;
		}
		tree init = TREE_VALUE(capture);
		if (init) {
			init = tree_strip_any_location_wrapper(init);
			if (init && (TREE_CODE(init) == ADDR_EXPR || TREE_CODE(init) == INDIRECT_REF)) {
				init = tree_strip_any_location_wrapper(TREE_OPERAND(init, 0));
			}
		}
		if (init && DECL_P(init) && DECL_NAME(init) == id) {
			continue;
		}
		const int len = (int)IDENTIFIER_LENGTH(id);
		int line = 0;
		int col = 0;
		if (!locate_name(DECL_SOURCE_LOCATION(field), IDENTIFIER_POINTER(id), len, &line, &col)) {
			continue;
		}
		if (type_ref_already_emitted(line, col, len)) {
			continue;
		}
		if (!g_index_only) {
			out_printf("GSETOK\t%d\t%d\t%d\tvariable\n", line, col, len);
		}
		if (main_file_under_root()) {
			emit_ref_at(main_input_filename, line, col, field, len);
		}
	}
}

static void on_pre_genericize(void *gcc_data, void *) {
	tree fndecl = (tree)gcc_data;
	if (!fndecl || TREE_CODE(fndecl) != FUNCTION_DECL) return;
	if (is_synthesized_special(fndecl) && !is_coroutine_actor(fndecl) && !is_lambda_op(fndecl) && !is_user_structor(fndecl)) return;
	if (is_lambda_op(fndecl)) {
		emit_lambda_captures(DECL_CONTEXT(fndecl));
	}
	for (tree p = DECL_ARGUMENTS(fndecl); p; p = DECL_CHAIN(p)) {
		emit_decl(p, DECL_SOURCE_LOCATION(p));
	}
	tree body = DECL_SAVED_TREE(fndecl);
	if (body) {
		if (g_walked && g_walked->add(body_walk_identity(fndecl))) return;
		tree prev_scope = g_scope;
		g_scope = enclosing_namespace(fndecl);
		walk_function_body(fndecl, body);
		g_scope = prev_scope;
	}
}

static void process_decl(tree d);

static void emit_type_members(tree type) {
	if (!type || !TYPE_P(type)) return;
	if (RECORD_OR_UNION_TYPE_P(type)) {
		for (tree f = TYPE_FIELDS(type); f; f = DECL_CHAIN(f)) {
			if (TREE_CODE(f) == TYPE_DECL && DECL_SELF_REFERENCE_P(f)) continue;
			walk_fn_default_args(f);
			process_decl(f);
		}
	} else if (TREE_CODE(type) == ENUMERAL_TYPE) {
		for (tree v = TYPE_VALUES(type); v; v = TREE_CHAIN(v)) {
			tree cst = TREE_VALUE(v);
			if (cst && TREE_CODE(cst) == CONST_DECL) {
				emit_sym(cst);
				emit_decl(cst, DECL_SOURCE_LOCATION(cst));
			}
		}
	}
}

static void process_type_decl(tree d) {
	if (!d || TREE_CODE(d) != TYPE_DECL || DECL_SELF_REFERENCE_P(d)) return;
	emit_type(d);
	tree t = TREE_TYPE(d);
	if (t && TYPE_P(t) && TYPE_NAME(t) == d && in_main_file(DECL_SOURCE_LOCATION(d))) {
		if (RECORD_OR_UNION_TYPE_P(t)) {
			if (tree binfo = TYPE_BINFO(t)) {
				for (int i = 0; i < BINFO_N_BASE_BINFOS(binfo); ++i) {
					tree base = BINFO_TYPE(BINFO_BASE_BINFO(binfo, i));
					if (base && TREE_CODE(base) == TYPENAME_TYPE) {
						emit_type_name_at(DECL_SOURCE_LOCATION(d), base);
					}
				}
			}
		}
		emit_type_members(t);
	}
}

static void emit_fn_params(tree fn) {
	if (!fn || TREE_CODE(fn) != FUNCTION_DECL) return;
	for (tree p = DECL_ARGUMENTS(fn); p; p = DECL_CHAIN(p))
		emit_decl(p, DECL_SOURCE_LOCATION(p));
}

static void walk_attr_annotations(tree attrs) {
	for (tree a = attrs; a; a = TREE_CHAIN(a)) {
		if (!annotation_p(a)) {
			continue;
		}
		tree value = TREE_VALUE(a);
		if (!value) {
			continue;
		}
		walk_expr_tree(&value);
	}
}

static void walk_annotations(tree decl) {
	if (!decl) return;
	switch (TREE_CODE(decl)) {
		case FIELD_DECL:
		case VAR_DECL:
		case FUNCTION_DECL:
		case CONST_DECL:
			walk_attr_annotations(DECL_ATTRIBUTES(decl));
			break;
		case TYPE_DECL: {
			walk_attr_annotations(DECL_ATTRIBUTES(decl));
			tree t = TREE_TYPE(decl);
			if (t && TYPE_P(t) && TYPE_NAME(t) == decl) {
				walk_attr_annotations(TYPE_ATTRIBUTES(t));
			}
			break;
		}
		default:
			break;
	}
}

static void process_decl(tree d) {
	if (!d || !DECL_P(d)) return;
	tree wrapping_tmpl = NULL_TREE;
	if (TREE_CODE(d) == TEMPLATE_DECL) {
		wrapping_tmpl = d;
		emit_template_params(d);
		for (tree spec = DECL_TEMPLATE_SPECIALIZATIONS(d); spec; spec = TREE_CHAIN(spec)) {
			tree spec_tmpl = TREE_VALUE(spec);
			if (!spec_tmpl || TREE_CODE(spec_tmpl) != TEMPLATE_DECL || !in_main_file(DECL_SOURCE_LOCATION(spec_tmpl))) {
				continue;
			}
			emit_template_params(spec_tmpl);
			tree spec_result = DECL_TEMPLATE_RESULT(spec_tmpl);
			if (spec_result && TREE_CODE(spec_result) == TYPE_DECL && !DECL_SELF_REFERENCE_P(spec_result)) {
				scan_declaration_type_qualifiers(spec_result, spec_tmpl);
			}
		}
		tree r = DECL_TEMPLATE_RESULT(d);
		if (!r || !DECL_P(r)) return;
		if ((TREE_CODE(r) == FUNCTION_DECL || TREE_CODE(r) == VAR_DECL) && DECL_SOURCE_LOCATION(d) != DECL_SOURCE_LOCATION(r)) {
			scan_declaration_type_qualifiers(d, d);
			emit_at_name(DECL_SOURCE_LOCATION(d), kind_of(r), DECL_NAME(d));
		}
		d = r;
	}
	if (TREE_CODE(d) != TYPE_DECL || !DECL_SELF_REFERENCE_P(d)) {
		scan_declaration_type_qualifiers(d, wrapping_tmpl);
	}
	walk_annotations(d);
	if (TREE_CODE(d) == CONCEPT_DECL) {
		emit_concept(d);
		return;
	}
	emit_sym(d);
	if (TREE_CODE(d) == TYPE_DECL) {
		if (tree aliased = TREE_TYPE(d); aliased && TREE_CODE(aliased) == TYPENAME_TYPE) {
			emit_type_name_at(DECL_SOURCE_LOCATION(d), aliased);
		}
		process_type_decl(d);
	}
	else {
		emit_decl(d, DECL_SOURCE_LOCATION(d));
		walk_fn_default_args(d);
		emit_fn_params(d);
	}
}

static void walk_template_bodies(tree decl);

static tree body_walk_identity(tree fndecl) {
	if (fndecl && TREE_CODE(fndecl) == FUNCTION_DECL && DECL_CLONED_FUNCTION_P(fndecl)) {
		if (tree origin = DECL_CLONED_FUNCTION(fndecl)) return origin;
	}
	return fndecl;
}

static void walk_fn_body(tree fndecl) {
	if (!fndecl || TREE_CODE(fndecl) != FUNCTION_DECL) return;
	if (!in_main_file(DECL_SOURCE_LOCATION(fndecl))) return;
	if (g_walked && g_walked->add(body_walk_identity(fndecl))) return;
	tree prev_scope = g_scope;
	g_scope = enclosing_namespace(fndecl);
	for (tree p = DECL_ARGUMENTS(fndecl); p; p = DECL_CHAIN(p)) {
		emit_decl(p, DECL_SOURCE_LOCATION(p));
	}
	tree body = DECL_SAVED_TREE(fndecl);
	if (body) walk_function_body(fndecl, body);
	g_scope = prev_scope;
}

static void walk_record_members(tree type) {
	if (!type || !TYPE_P(type) || !RECORD_OR_UNION_TYPE_P(type)) return;
	for (tree f = TYPE_FIELDS(type); f; f = DECL_CHAIN(f))
		walk_template_bodies(f);
}

static void walk_template_bodies(tree decl) {
	if (!decl || !DECL_P(decl)) return;
	if (!in_main_file(DECL_SOURCE_LOCATION(decl))) return;
	switch (TREE_CODE(decl)) {
		case TEMPLATE_DECL: {
			tree r = DECL_TEMPLATE_RESULT(decl);
			if (tree ci = get_constraints(r ? r : decl)) {
				tree treqs = CI_TEMPLATE_REQS(ci);
				tree dreqs = CI_DECLARATOR_REQS(ci);
				if (treqs) walk_expr_tree(&treqs);
				if (dreqs) walk_expr_tree(&dreqs);
			}
			if (r) walk_template_bodies(r);
			break;
		}
		case FUNCTION_DECL:
			walk_fn_body(decl);
			break;
		case CONCEPT_DECL: {
			tree init = DECL_INITIAL(decl);
			if (init) walk_expr_tree(&init);
			break;
		}
		case TYPE_DECL: {
			if (DECL_SELF_REFERENCE_P(decl)) break;
			tree t = TREE_TYPE(decl);
			if (t && TYPE_P(t) && TYPE_NAME(t) == decl && RECORD_OR_UNION_TYPE_P(t))
				walk_record_members(t);
			break;
		}
		case VAR_DECL: {
			tree init = DECL_INITIAL(decl);
			if (init) walk_with_anchor(init, DECL_SOURCE_LOCATION(decl));
			break;
		}
		default:
			break;
	}
}

static void walk_ns(tree ns) {
	if (!ns || TREE_CODE(ns) != NAMESPACE_DECL) return;
	if (g_visited_ns && g_visited_ns->add(ns)) return;
	if (cp_binding_level *lvl = NAMESPACE_LEVEL(ns)) {
		for (tree d = lvl->names; d; d = DECL_CHAIN(d)) {
			if (!DECL_P(d)) continue;
			process_decl(d);
			walk_template_bodies(d);
			if (TREE_CODE(d) == NAMESPACE_DECL && d != ns)
				walk_ns(d);
		}
	}
	hash_table<named_decl_hash> *bindings = DECL_NAMESPACE_BINDINGS(ns);
	if (!bindings) return;
	tree bind;
	hash_table<named_decl_hash>::iterator hti;
	FOR_EACH_HASH_TABLE_ELEMENT (*bindings, bind, tree, hti) {
		if (!bind) continue;
		if (TREE_CODE(bind) == NAMESPACE_DECL) {
			walk_ns(bind);
		}
		else if (TREE_CODE(bind) == BINDING_VECTOR) {
			const unsigned clusters = BINDING_VECTOR_NUM_CLUSTERS(bind);
			for (unsigned c = 0; c < clusters; ++c) {
				binding_cluster &cl = BINDING_VECTOR_CLUSTER(bind, c);
				for (unsigned s = 0; s < BINDING_VECTOR_SLOTS_PER_CLUSTER; ++s) {
					if (cl.slots[s].is_lazy()) continue;
					tree slot_bind = cl.slots[s].u.binding;
					if (!slot_bind) continue;
					if (TREE_CODE(slot_bind) == NAMESPACE_DECL) {
						walk_ns(slot_bind);
						continue;
					}
					tree stype = MAYBE_STAT_TYPE(slot_bind);
					if (stype && DECL_P(stype) && in_main_file(DECL_SOURCE_LOCATION(stype))) {
						process_decl(stype);
						walk_template_bodies(stype);
					}
					for (ovl_iterator it(MAYBE_STAT_DECL(slot_bind)); it; ++it) {
						tree d = *it;
						if (d && DECL_P(d) && in_main_file(DECL_SOURCE_LOCATION(d))) {
							process_decl(d);
							walk_template_bodies(d);
						}
					}
				}
			}
		}
	}
}

static void on_finish_decl(void *gcc_data, void *) {
	tree d = (tree)gcc_data;
	if (!d) return;
	if (TREE_CODE(d) == TEMPLATE_DECL) {
		tree r = DECL_TEMPLATE_RESULT(d);
		if (r && (TREE_CODE(r) == FUNCTION_DECL || TREE_CODE(r) == VAR_DECL || TREE_CODE(r) == TYPE_DECL)) {
			record_lexical_namespace(r);
			emit_template_params(d);
			scan_declaration_type_qualifiers(r, d);
			if (TREE_CODE(r) == TYPE_DECL && TREE_TYPE(r) && TREE_CODE(TREE_TYPE(r)) == TYPENAME_TYPE) {
				emit_type_name_at(DECL_SOURCE_LOCATION(r), TREE_TYPE(r));
			}
		}
		return;
	}
	if (TREE_CODE(d) != FUNCTION_DECL && TREE_CODE(d) != VAR_DECL) return;
	tree ctx = DECL_CONTEXT(d);
	if (!ctx || (TREE_CODE(ctx) != NAMESPACE_DECL && !TYPE_P(ctx))) return;
	record_lexical_namespace(d);
	scan_declaration_type_qualifiers(d);
	if (TREE_CODE(ctx) != NAMESPACE_DECL) return;
	tree id = DECL_NAME(d);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE) return;
	if (IDENTIFIER_LENGTH(id) >= 1 && IDENTIFIER_POINTER(id)[0] == '_') return;
	emit_decl(d, DECL_SOURCE_LOCATION(d));
	walk_fn_default_args(d);
	emit_fn_params(d);
	emit_sym(d);
}

static void on_finish_parse_function(void *gcc_data, void *) {
	tree fndecl = (tree)gcc_data;
	record_lexical_namespace(fndecl);
	if (!fndecl || TREE_CODE(fndecl) != FUNCTION_DECL) {
		return;
	}
	tree ctx = DECL_CONTEXT(fndecl);
	if (!ctx || !TYPE_P(ctx)) {
		return;
	}
	tree owner = TYPE_NAME(ctx);
	if (owner && DECL_P(owner) && in_main_file(DECL_SOURCE_LOCATION(owner))) {
		return;
	}
	scan_declaration_type_qualifiers(fndecl);
}

static void on_finish_type(void *gcc_data, void *) {
	tree type = (tree)gcc_data;
	if (!type || !TYPE_P(type) || LAMBDA_TYPE_P(type)) return;
	tree name = TYPE_NAME(type);
	if (!name || TREE_CODE(name) != TYPE_DECL || !in_main_file(DECL_SOURCE_LOCATION(name))) return;
	emit_sym(name);
	emit_type(name);
	emit_type_members(type);
}

static void on_finish(void *, void *) {
	emit_directive_names();
	emit_attribute_names();
	emit_identifier_inventory();
	scan_template_prototypes();
	emit_static_assert_names();
	walk_ns(global_namespace);
	if (g_out) {
		out_str("GSEDONE\n");
	}
	out_flush();
	if (g_out) {
		fflush(g_out);
	}
}


int plugin_init(struct plugin_name_args *info, struct plugin_gcc_version *version) {
	if (!plugin_default_version_check(version, &gcc_version)) return 1;
	g_walked = new hash_set<tree>();
	g_visited_ns = new hash_set<tree>();
	g_emitted = new hash_set<tree>();
	g_emitted_defs = new hash_set<tree>();
	for (int i = 0; i < info->argc; ++i) {
		if (info->argv[i].key && strcmp(info->argv[i].key, "out") == 0 && info->argv[i].value) {
			g_out = fopen(info->argv[i].value, "w");
		}
		if (info->argv[i].key && strcmp(info->argv[i].key, "root") == 0 && info->argv[i].value && g_root_count < 8) {
			g_roots[g_root_count] = xstrdup(info->argv[i].value);
			g_root_lens[g_root_count] = strlen(g_roots[g_root_count]);
			++g_root_count;
		}
		if (info->argv[i].key && strcmp(info->argv[i].key, "index") == 0) {
			g_index_only = true;
		}
		if (info->argv[i].key && strcmp(info->argv[i].key, "audit") == 0) {
			g_audit = true;
		}
	}
	atexit(out_flush);
	register_callback(info->base_name, PLUGIN_PRE_GENERICIZE, on_pre_genericize, nullptr);
	register_callback(info->base_name, PLUGIN_FINISH_PARSE_FUNCTION, on_finish_parse_function, nullptr);
	register_callback(info->base_name, PLUGIN_FINISH_DECL, on_finish_decl, nullptr);
	register_callback(info->base_name, PLUGIN_FINISH_TYPE, on_finish_type, nullptr);
	register_callback(info->base_name, PLUGIN_FINISH, on_finish, nullptr);
	return 0;
}
