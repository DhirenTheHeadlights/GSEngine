module gse.assert;

import gse.win32;

auto gse::break_into_debugger() noexcept -> void {
	if (win32::IsDebuggerPresent()) {
		win32::DebugBreak();
	}
}
