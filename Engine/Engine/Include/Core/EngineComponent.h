#pragma once

#include "ID.h"

namespace gse {
	class engine_component {
	public:
		engine_component(ID* id) : id(id) {}
		virtual ~engine_component() = default;
	private:
		ID* id;
	};
}