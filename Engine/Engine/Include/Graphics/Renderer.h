#pragma once

#include "Material.h"
#include "Graphics/Camera.h"
#include "Graphics/RenderComponent.h"
#include "Graphics/Shader.h"
#include "Lights/LightSourceComponent.h"

namespace Engine {
	class Renderer {
	public:
		Renderer() = default;

		void initialize();
		void addRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void addLightSourceComponent(const std::shared_ptr<LightSourceComponent>& lightSourceComponent);
		void removeRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void removeLightSourceComponent(const std::shared_ptr<LightSourceComponent>& lightSourceComponent);
		void renderObjects();

		static Camera& getCamera() { return camera; }
	private:
		void renderObject(const RenderQueueEntry& entry);
		void renderObject(const LightRenderQueueEntry& entry);
		void renderLightingPass(const std::vector<LightShaderEntry>& lightData, const glm::mat4& lightSpaceMatrix) const;
		void renderShadowPass(const glm::mat4& lightSpaceMatrix) const;

		static Camera camera;

		std::unordered_map<std::string, Material> materials;
		std::vector<std::weak_ptr<RenderComponent>> renderComponents;
		std::unordered_map<std::string, Shader> lightShaders;
		std::vector<std::weak_ptr<LightSourceComponent>> lightSourceComponents;

		GLuint gBuffer = 0;
		GLuint gPosition = 0;
		GLuint gNormal = 0;
		GLuint gAlbedoSpec = 0;
		GLuint ssboLights = 0;

		Shader lightingShader;

		GLuint depthMapFBO = 0;
		GLuint depthMap = 0;
		Shader shadowShader;
		GLsizei shadowWidth = 1024, shadowHeight = 1024;

		const float nearPlane = 1.0f;
		const float farPlane = 100.0f;
	};
}
