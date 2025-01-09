#include "Core/EngineComponent.h"

#include "Core/ObjectRegistry.h"

gse::component::component(const std::uint32_t initial_unique_id) 
	: parent_id(initial_unique_id) {}