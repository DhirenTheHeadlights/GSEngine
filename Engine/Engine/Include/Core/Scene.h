#pragma once

#include <string>
#include <vector>

#include "Core/Object/Object.h"

namespace Engine {
	class Scene {
	public:
		Scene(const std::string& name) : name(name) {};

		void addObject(const std::shared_ptr<Object>& object);
		void removeObject(const std::shared_ptr<Object>& object);

		void initialize();
		void run();
		void exit();
	private:
		std::string name;
		std::vector<std::weak_ptr<Object>> objects;

		bool isActive = false;
	};
}