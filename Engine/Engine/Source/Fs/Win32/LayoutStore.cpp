module gse.fs:layout_store_impl;

import gse.win32;
import std;

import :layout_store;

auto gse::layout_store::temp_suffix() -> std::string {
	return std::format("{}", win32::GetCurrentProcessId());
}
