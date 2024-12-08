#pragma once

#include "Graphics/RenderComponent.h"
#include "Graphics/3D/Camera.h"
#include "Lights/LightSourceComponent.h"

namespace Engine::Renderer {
	class Group {
	public:
		void addRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void addLightSourceComponent(const std::shared_ptr<LightSourceComponent>& lightSourceComponent);
		void removeRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void removeLightSourceComponent(const std::shared_ptr<LightSourceComponent>& lightSourceComponent);

		auto getRenderComponents() -> std::vector<std::weak_ptr<RenderComponent>>& {
			return renderComponents;
		}
		auto getLightSourceComponents() -> std::vector<std::weak_ptr<LightSourceComponent>>& {
			return lightSourceComponents;
		}

		std::vector<GLuint> depthMaps;
		std::vector<GLuint> depthMapFBOs;
	private:
		std::vector<std::weak_ptr<RenderComponent>> renderComponents;
		std::vector<std::weak_ptr<LightSourceComponent>> lightSourceComponents;
	};

	void initialize();
	void renderObjects(Group& group);

	Camera& getCamera();
}
