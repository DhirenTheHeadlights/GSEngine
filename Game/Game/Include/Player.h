#pragma once

#include <Engine.h>

namespace game {
	struct jetpack_hook;
	struct player_hook;

	class player final : public gse::object {
	public:
		player();
	private:
		std::unordered_map<int, gse::vec3<>> m_wasd;

		friend jetpack_hook;
		friend player_hook;
	};
}