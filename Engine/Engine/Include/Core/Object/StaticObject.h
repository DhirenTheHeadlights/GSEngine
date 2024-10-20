#pragma once

#include "Engine/Include/Core/Object/Object.h"

namespace Engine {
	class StaticObject : public Object {
	public:
		StaticObject(const std::shared_ptr<ID>& id) : Object(id) {}
		~StaticObject() = default;

		///////////////////
		// Empty for now //
		///////////////////
	};
}