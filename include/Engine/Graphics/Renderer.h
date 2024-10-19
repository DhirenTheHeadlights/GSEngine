#pragma once

#include "Engine/Graphics/Camera.h"
#include "Engine/Graphics/RenderComponent.h"
#include "Engine/Graphics/Shader.h"

namespace Engine {
	class Renderer {
	public:
		Renderer() = default;

		void initialize();
		void setCameraInformation(const Camera& camera) const;
		void addRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void removeRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void renderObjects();
		static void beginFrame();
		static void endFrame();
	private:
		Shader shader;
		
		std::vector<std::weak_ptr<RenderComponent>> renderComponents;
	};
}
