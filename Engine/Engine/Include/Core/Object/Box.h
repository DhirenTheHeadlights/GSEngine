#pragma once
#include "Engine.h"

namespace gse {
	class box : public object {
	public:
		box(const vec3<length>& position, const vec3<length>& size) 
			: object("Box"), m_position(position), m_size(size) {}

		void initialize() override;
		void update() override {}
		void render() override {}

	private:
		vec3<length> m_position;
		vec3<length> m_size;
	};
}