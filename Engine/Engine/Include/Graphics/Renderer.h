#pragma once

#include "Graphics/Camera.h"
#include "Graphics/RenderComponent.h"
#include "Graphics/Shader.h"

namespace Engine {
	class Renderer {
	public:
		Renderer() = default;

		static void initialize();
		void loadShader(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);
		void loadShaders(const std::vector<std::pair<std::string, std::string>>& shaders);
		void addComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void removeComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void renderObject(const RenderQueueEntry& entry);
		void renderObjects();

		static void beginFrame();
		static void endFrame();

		Camera& getCamera() { return camera; }
	private:
		Camera camera;

		std::unordered_map<GLuint, Shader> shaders;
		std::vector<std::weak_ptr<RenderComponent>> renderComponents;
	};
}
