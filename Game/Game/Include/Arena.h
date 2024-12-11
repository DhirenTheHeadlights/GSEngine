#pragma once

#include "Engine.h"

namespace game {
	class arena final : public gse::object {
	public:
		arena() : object("Arena") {}

		void initialize() override;
		void update() override;
		void render() override;
	private:
		gse::length
			m_width   = gse::meters(1000.f),
			m_height  = gse::meters(1000.f),
			m_depth	  = gse::meters(1000.f);
	};
}