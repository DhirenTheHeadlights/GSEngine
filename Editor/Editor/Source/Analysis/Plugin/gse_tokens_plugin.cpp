#include "gcc-plugin.h"
#include "plugin-version.h"
#include "coretypes.h"
#include "tree.h"
#include "intl.h"
#include "stringpool.h"
#include "cp/cp-tree.h"
#include "cp/name-lookup.h"
#include "hash-set.h"

int plugin_is_GPL_compatible;

static FILE *g_out = nullptr;
static const char *g_root = nullptr;
static size_t g_root_len = 0;
static hash_set<tree> *g_walked = nullptr;
static hash_set<tree> *g_visited_ns = nullptr;
static hash_set<tree> *g_emitted = nullptr;
static hash_set<tree> *g_emitted_defs = nullptr;

static tree walk_cb(tree *tp, int *walk_subtrees, void *);
static void walk_fn_body(tree fndecl);

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

static bool under_root(const char *file) {
	if (!file || !g_root) return false;
	for (size_t i = 0; i < g_root_len; ++i) {
		char a = file[i];
		if (!a) return false;
		if (a == '\\') a = '/';
		char b = g_root[i];
		if (b == '\\') b = '/';
		if (ascii_lower(a) != ascii_lower(b)) return false;
	}
	return true;
}

static const char *kind_of(tree t) {
	switch (TREE_CODE(t)) {
		case VAR_DECL: {
			tree ctx = DECL_CONTEXT(t);
			if (!ctx || TREE_CODE(ctx) == NAMESPACE_DECL || TREE_CODE(ctx) == TRANSLATION_UNIT_DECL)
				return "global";
			return "variable";
		}
		case PARM_DECL: return "parameter";
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

static bool in_main_file(location_t loc) {
	if (loc == UNKNOWN_LOCATION || !main_input_filename) return false;
	expanded_location xl = expand_location(loc);
	return xl.file && same_file(xl.file, main_input_filename);
}

static void emit(location_t loc, const char *kind, int len) {
	if (!kind || len <= 0 || !in_main_file(loc)) return;
	expanded_location xl = expand_location(loc);
	if (xl.line <= 0 || xl.column <= 0) return;
	fprintf(g_out ? g_out : stderr, "GSETOK\t%d\t%d\t%d\t%s\n", xl.line, xl.column, len, kind);
}

static void emit_decl(tree d, location_t loc) {
	if (!d || !DECL_P(d) || DECL_ARTIFICIAL(d)) return;
	emit(loc, kind_of(d), ident_len(d));
}

static void emit_type(tree d) {
	if (!d || TREE_CODE(d) != TYPE_DECL || DECL_SELF_REFERENCE_P(d)) return;
	emit(DECL_SOURCE_LOCATION(d), "type", ident_len(d));
}

static void emit_type_name(tree type, int depth = 0);

static void emit_type_identifier(tree id) {
	if (id && DECL_P(id)) id = DECL_NAME(id);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE || IDENTIFIER_LENGTH(id) <= 0) return;
	const char *s = IDENTIFIER_POINTER(id);
	if (s[0] != '_' && s[0] != '.' && !strchr(s, ' '))
		fprintf(g_out ? g_out : stderr, "GSETYPE\t%s\n", s);
}

static void emit_template_arg_type_names(tree arg, int depth) {
	if (!arg || depth > 24) return;
	if (TYPE_P(arg)) {
		emit_type_name(arg, depth + 1);
		return;
	}
	if (DECL_P(arg)) {
		if (TREE_CODE(arg) == TYPE_DECL || TREE_CODE(arg) == TEMPLATE_DECL)
			emit_type_identifier(arg);
		return;
	}
	if (TREE_CODE(arg) == TREE_VEC) {
		for (int i = 0; i < TREE_VEC_LENGTH(arg); ++i)
			emit_template_arg_type_names(TREE_VEC_ELT(arg, i), depth + 1);
		return;
	}
	if (TREE_CODE(arg) == TREE_LIST) {
		for (tree a = arg; a; a = TREE_CHAIN(a))
			emit_template_arg_type_names(TREE_VALUE(a), depth + 1);
		return;
	}
	if (TREE_CODE(arg) == TEMPLATE_ID_EXPR) {
		emit_template_arg_type_names(TREE_OPERAND(arg, 0), depth + 1);
		emit_template_arg_type_names(TREE_OPERAND(arg, 1), depth + 1);
	}
}

static void emit_type_name(tree type, int depth) {
	if (!type || !TYPE_P(type) || depth > 24) return;
	if (TREE_CODE(type) == TYPENAME_TYPE) {
		tree fn = TYPENAME_TYPE_FULLNAME(type);
		if (fn && TREE_CODE(fn) == TEMPLATE_ID_EXPR) fn = TREE_OPERAND(fn, 0);
		emit_type_identifier(fn);
		return;
	}
	tree name = TYPE_NAME(type);
	if (name) {
		if (TREE_CODE(name) == TYPE_DECL) {
			name = DECL_NAME(name);
		}
		if (name && TREE_CODE(name) == IDENTIFIER_NODE) {
			emit_type_identifier(name);
			if (CLASS_TYPE_P(type) && CLASSTYPE_TEMPLATE_INFO(type)) {
				tree args = CLASSTYPE_TI_ARGS(type);
				emit_template_arg_type_names(args, depth + 1);
			}
			return;
		}
	}
	if (TREE_CODE(type) == POINTER_TYPE || TREE_CODE(type) == REFERENCE_TYPE)
		emit_type_name(TREE_TYPE(type), depth + 1);
}

static void emit_fn_signature_types(tree decl) {
	if (decl && TREE_CODE(decl) == TEMPLATE_DECL) decl = DECL_TEMPLATE_RESULT(decl);
	if (!decl || TREE_CODE(decl) != FUNCTION_DECL) return;
	tree fntype = TREE_TYPE(decl);
	if (!fntype || (TREE_CODE(fntype) != FUNCTION_TYPE && TREE_CODE(fntype) != METHOD_TYPE)) return;
	emit_type_name(TREE_TYPE(fntype));
	for (tree a = TYPE_ARG_TYPES(fntype); a && a != void_list_node; a = TREE_CHAIN(a)) {
		emit_type_name(TREE_VALUE(a));
		tree def = TREE_PURPOSE(a);
		if (def && !DECL_P(def) && TREE_CODE(def) != DEFERRED_PARSE) walk_tree_without_duplicates(&def, walk_cb, nullptr);
	}
}

static void emit_template_params(tree tmpl) {
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
		if (s[0] != '_' && s[0] != '.' && !strchr(s, ' '))
			fprintf(g_out ? g_out : stderr, "GSETPARAM\t%s\n", s);
	}
}

static void print_qualified_prefix(FILE *out, tree decl);

static bool function_definition_p(tree decl) {
	return decl && TREE_CODE(decl) == FUNCTION_DECL && DECL_INITIAL(decl);
}

static void emit_concept(tree cdecl) {
	if (!cdecl) return;
	tree id = DECL_NAME(cdecl);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE || IDENTIFIER_LENGTH(id) <= 0) return;
	emit(DECL_SOURCE_LOCATION(cdecl), "type", IDENTIFIER_LENGTH(id));
	const char *s = IDENTIFIER_POINTER(id);
	if (s[0] == '_' || s[0] == '.' || strchr(s, ' ')) return;
	if (in_main_file(DECL_SOURCE_LOCATION(cdecl)))
		fprintf(g_out ? g_out : stderr, "GSETYPE\t%s\n", s);
	expanded_location xl = expand_location(DECL_SOURCE_LOCATION(cdecl));
	if (xl.file && xl.line > 0 && xl.column > 0 && in_main_file(DECL_SOURCE_LOCATION(cdecl))) {
		FILE *out = g_out ? g_out : stderr;
		fprintf(out, "GSESYM\t%s\tconcept_decl\t%s\t%d\t%d\t", s, xl.file, xl.line, xl.column);
		print_qualified_prefix(out, cdecl);
		fputs(s, out);
		fputs("\t\tdefinition\n", out);
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

static void emit_sym(tree decl) {
	if (!decl || !DECL_P(decl) || DECL_ARTIFICIAL(decl)) return;
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
	if (loc == UNKNOWN_LOCATION) return;
	expanded_location xl = expand_location(loc);
	if (!xl.file || xl.line <= 0 || xl.column <= 0 || !in_main_file(loc)) return;
	FILE *out = g_out ? g_out : stderr;
	fprintf(out, "GSESYM\t%s\t%s\t%s\t%d\t%d\t", name, kind, xl.file, xl.line, xl.column);
	print_qualified_prefix(out, decl);
	fputs(name, out);
	fputs("\t\t", out);
	fputs(is_definition ? "definition" : "declaration", out);
	fputc('\n', out);
}

static void print_qualified_prefix(FILE *out, tree decl) {
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
		fputs(stack[i], out);
		fputs("::", out);
	}
}

static void emit_ref(location_t use_loc, tree decl, int len) {
	if (!decl || !DECL_P(decl) || len <= 0) return;
	tree id = DECL_NAME(decl);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE) return;
	const char *name = IDENTIFIER_POINTER(id);
	if (name[0] == '_' || name[0] == '.' || strchr(name, ' ')) return;
	if (use_loc == UNKNOWN_LOCATION) return;
	expanded_location ux = expand_location(use_loc);
	if (!ux.file || ux.line <= 0 || ux.column <= 0 || !under_root(ux.file)) return;
	expanded_location dx = expand_location(DECL_SOURCE_LOCATION(decl));
	const char *def_file = (dx.file && dx.line > 0 && dx.column > 0) ? dx.file : "";
	FILE *out = g_out ? g_out : stderr;
	fprintf(out, "GSEREF\t%s\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t", ux.file, ux.line, ux.column, len, name, def_file, dx.line, dx.column);
	print_qualified_prefix(out, decl);
	fputs(name, out);
	fputc('\n', out);
}

static void emit_call_ref(location_t caret, tree decl, int fallback_len) {
	const location_t startloc = get_start(caret);
	const expanded_location cx = expand_location(caret);
	const expanded_location sx = expand_location(startloc);
	if (sx.file && cx.file && strcmp(sx.file, cx.file) == 0 && sx.line == cx.line && cx.column > sx.column) {
		emit_ref(startloc, decl, cx.column - sx.column);
	}
	else {
		emit_ref(caret, decl, fallback_len);
	}
}

static tree callee_identifier(tree fn) {
	for (int guard = 0; fn && guard < 8; ++guard) {
		switch (TREE_CODE(fn)) {
			case TEMPLATE_ID_EXPR: fn = TREE_OPERAND(fn, 0); continue;
			case OVERLOAD: fn = OVL_FIRST(fn); continue;
			case BASELINK: fn = BASELINK_FUNCTIONS(fn); continue;
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

static tree walk_cb(tree *tp, int *walk_subtrees, void *) {
	tree t = *tp;
	if (!t) return NULL_TREE;

	if (location_wrapper_p(t)) {
		tree inner = tree_strip_any_location_wrapper(t);
		if (inner && DECL_P(inner)) {
			emit_decl(inner, EXPR_LOCATION(t));
			emit_ref(EXPR_LOCATION(t), inner, ident_len(inner));
		}
		*walk_subtrees = 0;
		return NULL_TREE;
	}

	switch (TREE_CODE(t)) {
		case DECL_EXPR: {
			tree decl = DECL_EXPR_DECL(t);
			emit_decl(decl, DECL_SOURCE_LOCATION(decl));
			if (decl && TREE_CODE(decl) == VAR_DECL) {
				emit_type_name(TREE_TYPE(decl));
				tree init = DECL_INITIAL(decl);
				if (init) walk_tree_without_duplicates(&init, walk_cb, nullptr);
			}
			else if (decl && TREE_CODE(decl) == TYPE_DECL) {
				tree id = DECL_NAME(decl);
				if (id && TREE_CODE(id) == IDENTIFIER_NODE && IDENTIFIER_LENGTH(id) > 0) {
					const char *s = IDENTIFIER_POINTER(id);
					if (s[0] != '_' && s[0] != '.' && !strchr(s, ' '))
						fprintf(g_out ? g_out : stderr, "GSETYPE\t%s\n", s);
				}
				tree aliased = TREE_TYPE(decl);
				emit_type_name(aliased);
				if (aliased && TREE_CODE(aliased) == DECLTYPE_TYPE) {
					tree expr = DECLTYPE_TYPE_EXPR(aliased);
					if (expr) walk_tree_without_duplicates(&expr, walk_cb, nullptr);
				}
			}
			break;
		}
		case RANGE_FOR_STMT: {
			if (tree decl = RANGE_FOR_DECL(t); decl && DECL_P(decl)) {
				emit_decl(decl, DECL_SOURCE_LOCATION(decl));
				if (TREE_CODE(decl) == VAR_DECL) emit_type_name(TREE_TYPE(decl));
			}
			break;
		}
		case COMPONENT_REF: {
			tree field = TREE_OPERAND(t, 1);
			if (field && TREE_CODE(field) == FIELD_DECL && !DECL_ARTIFICIAL(field)) {
				emit(EXPR_LOCATION(t), "member", ident_len(field));
				emit_ref(EXPR_LOCATION(t), field, ident_len(field));
			}
			else if (field && TREE_CODE(field) == IDENTIFIER_NODE && IDENTIFIER_LENGTH(field) > 0) {
				emit(EXPR_LOCATION(t), "member", (int)IDENTIFIER_LENGTH(field));
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
					emit(use, "function", ident_len(fn));
					emit_call_ref(use, fn, ident_len(fn));
				}
			}
			else if (tree id = callee_identifier(fn)) {
				if (TREE_CODE(id) == IDENTIFIER_NODE && IDENTIFIER_LENGTH(id) > 0) {
					if (use == UNKNOWN_LOCATION) use = EXPR_LOCATION(t);
					emit(use, "function", (int)IDENTIFIER_LENGTH(id));
					if (tree cd = callee_decl(fn))
						emit_call_ref(use, cd, (int)IDENTIFIER_LENGTH(id));
				}
			}
			break;
		}
		case TEMPLATE_ID_EXPR: {
			tree id = callee_identifier(t);
			const char *k = template_id_kind(t);
			if (k && id && TREE_CODE(id) == IDENTIFIER_NODE && IDENTIFIER_LENGTH(id) > 0) {
				emit(EXPR_LOCATION(t), k, (int)IDENTIFIER_LENGTH(id));
				if (tree td = template_id_decl(t)) {
					expanded_location dx = expand_location(DECL_SOURCE_LOCATION(td));
					if (dx.file && dx.line > 0 && under_root(dx.file))
						emit_call_ref(EXPR_LOCATION(t), td, (int)IDENTIFIER_LENGTH(id));
				}
			}
			break;
		}
		case SCOPE_REF: {
			tree name = TREE_OPERAND(t, 1);
			if (name && DECL_P(name) && !DECL_ARTIFICIAL(name))
				emit(EXPR_LOCATION(t), kind_of(name), ident_len(name));
			else if (name && TREE_CODE(name) == IDENTIFIER_NODE && IDENTIFIER_LENGTH(name) > 0)
				emit(EXPR_LOCATION(t), "member", (int)IDENTIFIER_LENGTH(name));
			break;
		}
		case ADDR_EXPR:
		case NOP_EXPR:
		case CONVERT_EXPR:
		case NON_LVALUE_EXPR:
		case VIEW_CONVERT_EXPR: {
			tree op = TREE_OPERAND(t, 0);
			if (op && DECL_P(op) && !DECL_ARTIFICIAL(op) && EXPR_LOCATION(t) != UNKNOWN_LOCATION) {
				const char *k = kind_of(op);
				if (k) emit(EXPR_LOCATION(t), k, ident_len(op));
			}
			break;
		}
		case LAMBDA_EXPR: {
			tree closure = LAMBDA_EXPR_CLOSURE(t);
			tree op = closure ? lambda_function(closure) : NULL_TREE;
			if (op && TREE_CODE(op) == TEMPLATE_DECL) op = DECL_TEMPLATE_RESULT(op);
			if (op && TREE_CODE(op) == FUNCTION_DECL) walk_fn_body(op);
			break;
		}
		default:
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

static void on_pre_genericize(void *gcc_data, void *) {
	tree fndecl = (tree)gcc_data;
	if (!fndecl || TREE_CODE(fndecl) != FUNCTION_DECL || is_synthesized_special(fndecl)) return;
	tree fntype = TREE_TYPE(fndecl);
	if (fntype) emit_type_name(TREE_TYPE(fntype));
	for (tree p = DECL_ARGUMENTS(fndecl); p; p = DECL_CHAIN(p)) {
		emit_decl(p, DECL_SOURCE_LOCATION(p));
		emit_type_name(TREE_TYPE(p));
	}
	tree body = DECL_SAVED_TREE(fndecl);
	if (body) {
		if (g_walked) g_walked->add(fndecl);
		walk_tree_without_duplicates(&body, walk_cb, nullptr);
	}
}

static void process_decl(tree d);

static void emit_type_members(tree type) {
	if (!type || !TYPE_P(type)) return;
	if (RECORD_OR_UNION_TYPE_P(type)) {
		for (tree f = TYPE_FIELDS(type); f; f = DECL_CHAIN(f)) {
			if (TREE_CODE(f) == TYPE_DECL && DECL_SELF_REFERENCE_P(f)) continue;
			if (TREE_CODE(f) == FIELD_DECL) emit_type_name(TREE_TYPE(f));
			emit_fn_signature_types(f);
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
	if (t && TYPE_P(t) && TYPE_NAME(t) == d && in_main_file(DECL_SOURCE_LOCATION(d)))
		emit_type_members(t);
}

static void emit_fn_params(tree fn) {
	if (!fn || TREE_CODE(fn) != FUNCTION_DECL) return;
	for (tree p = DECL_ARGUMENTS(fn); p; p = DECL_CHAIN(p))
		emit_decl(p, DECL_SOURCE_LOCATION(p));
}

static tree ann_type_cb(tree *tp, int *, void *) {
	tree t = *tp;
	if (t && (TREE_CODE(t) == CONSTRUCTOR || TREE_CODE(t) == TARGET_EXPR || TREE_CODE(t) == AGGR_INIT_EXPR)) {
		emit_type_name(TREE_TYPE(t));
	}
	return NULL_TREE;
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
		emit_type_name(TREE_TYPE(value));
		walk_tree_without_duplicates(&value, ann_type_cb, nullptr);
		walk_tree_without_duplicates(&value, walk_cb, nullptr);
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
	if (TREE_CODE(d) == TEMPLATE_DECL) {
		emit_template_params(d);
		tree r = DECL_TEMPLATE_RESULT(d);
		if (!r || !DECL_P(r)) return;
		d = r;
	}
	walk_annotations(d);
	if (TREE_CODE(d) == CONCEPT_DECL) {
		emit_concept(d);
		return;
	}
	emit_sym(d);
	if (TREE_CODE(d) == TYPE_DECL) {
		process_type_decl(d);
	}
	else {
		emit_decl(d, DECL_SOURCE_LOCATION(d));
		emit_fn_signature_types(d);
		emit_fn_params(d);
		if (TREE_CODE(d) == VAR_DECL) emit_type_name(TREE_TYPE(d));
	}
}

static void walk_template_bodies(tree decl);

static void walk_fn_body(tree fndecl) {
	if (!fndecl || TREE_CODE(fndecl) != FUNCTION_DECL) return;
	if (!in_main_file(DECL_SOURCE_LOCATION(fndecl))) return;
	if (g_walked && g_walked->add(fndecl)) return;
	for (tree p = DECL_ARGUMENTS(fndecl); p; p = DECL_CHAIN(p)) {
		emit_decl(p, DECL_SOURCE_LOCATION(p));
		emit_type_name(TREE_TYPE(p));
	}
	tree body = DECL_SAVED_TREE(fndecl);
	if (body) walk_tree_without_duplicates(&body, walk_cb, nullptr);
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
				if (treqs) walk_tree_without_duplicates(&treqs, walk_cb, nullptr);
				if (dreqs) walk_tree_without_duplicates(&dreqs, walk_cb, nullptr);
			}
			if (r) walk_template_bodies(r);
			break;
		}
		case FUNCTION_DECL:
			walk_fn_body(decl);
			break;
		case CONCEPT_DECL: {
			tree init = DECL_INITIAL(decl);
			if (init) walk_tree_without_duplicates(&init, walk_cb, nullptr);
			break;
		}
		case TYPE_DECL: {
			if (DECL_SELF_REFERENCE_P(decl)) break;
			tree t = TREE_TYPE(decl);
			if (t && TYPE_P(t) && TYPE_NAME(t) == decl && RECORD_OR_UNION_TYPE_P(t))
				walk_record_members(t);
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
	if (!d || (TREE_CODE(d) != FUNCTION_DECL && TREE_CODE(d) != VAR_DECL)) return;
	tree ctx = DECL_CONTEXT(d);
	if (!ctx || TREE_CODE(ctx) != NAMESPACE_DECL) return;
	tree id = DECL_NAME(d);
	if (!id || TREE_CODE(id) != IDENTIFIER_NODE) return;
	if (IDENTIFIER_LENGTH(id) >= 1 && IDENTIFIER_POINTER(id)[0] == '_') return;
	emit_decl(d, DECL_SOURCE_LOCATION(d));
	emit_fn_signature_types(d);
	emit_fn_params(d);
	emit_sym(d);
}

static void on_finish_type(void *gcc_data, void *) {
	tree type = (tree)gcc_data;
	if (!type || !TYPE_P(type)) return;
	tree name = TYPE_NAME(type);
	if (!name || TREE_CODE(name) != TYPE_DECL || !in_main_file(DECL_SOURCE_LOCATION(name))) return;
	emit_sym(name);
	emit_type(name);
	emit_type_members(type);
}

static void on_finish(void *, void *) {
	walk_ns(global_namespace);
	if (g_out) {
		fputs("GSEDONE\n", g_out);
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
			if (g_out) setvbuf(g_out, nullptr, _IONBF, 0);
		}
		if (info->argv[i].key && strcmp(info->argv[i].key, "root") == 0 && info->argv[i].value) {
			g_root = xstrdup(info->argv[i].value);
			g_root_len = strlen(g_root);
		}
	}
	register_callback(info->base_name, PLUGIN_PRE_GENERICIZE, on_pre_genericize, nullptr);
	register_callback(info->base_name, PLUGIN_FINISH_DECL, on_finish_decl, nullptr);
	register_callback(info->base_name, PLUGIN_FINISH_TYPE, on_finish_type, nullptr);
	register_callback(info->base_name, PLUGIN_FINISH, on_finish, nullptr);
	return 0;
}
