module gse.log;

import std;

auto gse::log::process_id_text() -> std::string {
	return "web";
}

auto gse::log::describe_os_thread() -> void {
}

auto gse::log::console_output() -> bool {
	return false;
}
