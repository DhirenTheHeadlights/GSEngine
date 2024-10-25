#pragma once

#include "Material.h"
#include "Graphics/Camera.h"
#include "Graphics/RenderComponent.h"
#include "Graphics/Shader.h"

namespace Engine {
	class Renderer {
	public:
		Renderer() = default;

		void initialize();
		void addComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void removeComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void renderObject(const RenderQueueEntry& entry);
		void renderObjects();

		static void beginFrame();
		static void endFrame();

		Camera& getCamera() { return camera; }
	private:
		Camera camera;

		std::unordered_map<std::string, Material> materials;
		std::vector<std::weak_ptr<RenderComponent>> renderComponents;
	};
}
