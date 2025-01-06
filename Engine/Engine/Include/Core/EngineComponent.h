#pragma once

#include <typeindex>

#include "ID.h"

namespace gse {
	struct component {
		component(id* id) : parent_id(id) {}
		id* parent_id;
	};
}