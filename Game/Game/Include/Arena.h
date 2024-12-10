#pragma once

#include "Engine.h"

namespace Game {
	class Arena final : public gse::object {
	public:
		Arena() : object("Arena") {}

		void initialize() override;
		void update() override;
		void render() override;
	private:
		gse::length
			width   = gse::meters(1000.f),
			height	= gse::meters(1000.f),
			depth	= gse::meters(1000.f);
	};
}