#pragma once

#include <typeindex>

#include "ID.h"

namespace gse {
	class component {
	public:
		component(id* id) : m_id(id) {}
		virtual ~component() = default;

		id* get_id() const { return m_id; }
	private:
		id* m_id;
	};
}