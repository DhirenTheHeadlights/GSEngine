#pragma once

#include "ID.h"

namespace Engine {
	class EngineComponent {
	public:
		EngineComponent(ID* id) : id(id) {}
		virtual ~EngineComponent() = default;
	private:
		ID* id;
	};
}