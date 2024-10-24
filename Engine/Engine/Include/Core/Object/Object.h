#pragma once

#include <typeindex>
#include <vector>

#include "Core/ID.h"
#include "Graphics/BoundingBox.h"

namespace Engine {
	class Object {
	public:
		explicit Object(const std::shared_ptr<ID>& id) : id(id) {}
		virtual ~Object() = default;

		ID* getId() const { return id.get(); }

		bool operator==(const Object& other) const {
			return id == other.id;
		}

		template <typename T>
		void addComponent(std::shared_ptr<T> component) {
			components.insert({ typeid(T), component });
		}

		template <typename T>
		std::shared_ptr<T> getComponent() const {
			return components.at(typeid(T));
		}
	protected:
		std::shared_ptr<ID> id;

		std::unordered_map<std::type_index, std::shared_ptr<void>> components;
	};
}