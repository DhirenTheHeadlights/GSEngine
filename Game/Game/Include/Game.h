#pragma once

namespace Game {
	bool initialize();
	bool update();
	bool render();
	bool close();

	void setResizingEnabled(bool enabled);
}
