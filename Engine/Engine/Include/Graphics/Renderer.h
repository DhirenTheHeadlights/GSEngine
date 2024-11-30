#pragma once

#include "Material.h"
#include "Graphics/Camera.h"
#include "Graphics/CubeMap.h"
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
		void renderObject(const RenderQueueEntry& entry, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
		void renderObjectForward(const RenderQueueEntry& entry, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const;
		void renderObject(const LightRenderQueueEntry& entry);
		void renderLightingPass(const std::vector<LightShaderEntry>& lightData, const std::vector<glm::mat4>& lightSpaceMatrices) const;
		void renderShadowPass(const glm::mat4& lightSpaceMatrix) const;
		static glm::vec3 ensureNonCollinearUp(const glm::vec3& direction, const glm::vec3& up);

		glm::mat4 calculateLightSpaceMatrix(const std::shared_ptr<Light>& light) const;

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

		CubeMap shadowCubeMap;

		Shader lightingShader;
		Shader shadowShader;
		Shader forwardRenderingShader;

		std::vector<GLuint> depthMaps;
		std::vector<GLuint> depthMapFBOs;

		GLsizei shadowWidth = 0;
		GLsizei shadowHeight = 0;

		float nearPlane = 1.0f;
		float farPlane = 10000.f;
	};
}
