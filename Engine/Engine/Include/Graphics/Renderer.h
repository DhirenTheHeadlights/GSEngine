#pragma once

#include "Graphics/Camera.h"
#include "Graphics/Shader.h"

class RenderComponent;

namespace Engine {
	class Renderer {
	public:
		Renderer() = default;

		void initialize();
		void addComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void removeComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void renderObjects();

		static void beginFrame();
		static void endFrame();

		Camera& getCamera() { return camera; }
	private:
		Shader shader;
		Camera camera;
		
		std::vector<std::weak_ptr<RenderComponent>> renderComponents;
	};
}
