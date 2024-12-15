#pragma once

#include "ID.h"

namespace gse {
	class engine_component {
	public:
		engine_component(id* id) : m_id(id) {}
		virtual ~engine_component() = default;

		id* get_id() const { return m_id; }
	private:
		id* m_id;
	};
}