#pragma once

namespace game {
	bool initialize();
	bool update();
	bool render();
	bool close();

	void set_input_handling_flag(bool enabled);
}
