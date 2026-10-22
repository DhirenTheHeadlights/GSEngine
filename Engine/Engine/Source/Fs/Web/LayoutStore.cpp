module gse.fs:layout_store_impl;

import std;

import :layout_store;

auto gse::layout_store::temp_suffix() -> std::string {
	return "web";
}
