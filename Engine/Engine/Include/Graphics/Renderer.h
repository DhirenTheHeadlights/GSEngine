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
		void removeComponent(const std::shared_ptr<RenderComponent>& renderComponent);
		void removeLightSourceComponent(const std::shared_ptr<LightSourceComponent>& lightSourceComponent);
		void renderObject(const RenderQueueEntry& entry);
		void renderObjects();

		static void beginFrame();
		static void endFrame();

		Camera& getCamera() { return camera; }
	private:
		Camera camera;

		std::unordered_map<std::string, Material> materials;
		std::vector<std::weak_ptr<RenderComponent>> renderComponents;
		std::unordered_map<std::string, Shader> lightShaders;
		std::vector<std::weak_ptr<LightSourceComponent>> lightSourceComponents;

		GLuint gBuffer;
		GLuint gPosition;
		GLuint gNormal;
		GLuint gAlbedoSpec;
		GLuint ssboLights;

		Shader lightingShader;
	};
}
