#include "Graphics/Renderer.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

#include "Core/JsonParser.h"
#include "Input/Input.h"
#include "Platform/ErrorReporting.h"
#include "Platform/Platform.h"

void Engine::Renderer::initialize() {
	enableReportGlErrors();

	const std::string shaderPath = std::string(RESOURCES_PATH) + "Shaders/";

	JsonParse::parse(
		JsonParse::loadJson(shaderPath + "Shaders.json"),
		[&](const std::string& key, const nlohmann::json& value) {
			materials.emplace(key, Material(shaderPath + value["vertex"].get<std::string>(), 
											shaderPath + value["fragment"].get<std::string>(), key));
		}
	);

	JsonParse::parse(
		JsonParse::loadJson(shaderPath + "LightShaders.json"),
		[&](const std::string& key, const nlohmann::json& value) {
			lightShaders.emplace(key, Shader(shaderPath + value["vertex"].get<std::string>(),
											 shaderPath + value["fragment"].get<std::string>()));
		}
	);

	lightingShader.createShaderProgram(shaderPath + "lighting_pass.vert", shaderPath + "lighting_pass.frag");

	const GLsizei screenWidth = Platform::getFrameBufferSize().x;
	const GLsizei screenHeight = Platform::getFrameBufferSize().y;

	glDisable(GL_LIGHTING);

	glGenFramebuffers(1, &gBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

	glGenTextures(1, &gPosition);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

	glGenTextures(1, &gNormal);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

	glGenTextures(1, &gAlbedoSpec);
	glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);

	// Tell OpenGL which color attachments we'll use (for multiple render targets)
	constexpr GLuint attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, attachments);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "G-Buffer not complete!" << '\n';
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Engine::Renderer::addRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent) {
	renderComponents.push_back(renderComponent);
}

void Engine::Renderer::addLightSourceComponent(const std::shared_ptr<LightSourceComponent>& lightSourceComponent) {
	lightSourceComponents.push_back(lightSourceComponent);
}

void Engine::Renderer::removeComponent(const std::shared_ptr<RenderComponent>& renderComponent) {
	std::erase_if(renderComponents, [&](const std::weak_ptr<RenderComponent>& component) {
		return !component.owner_before(renderComponent) && !renderComponent.owner_before(component);
		});
}

void Engine::Renderer::removeLightSourceComponent(const std::shared_ptr<LightSourceComponent>& lightSourceComponent) {
	std::erase_if(lightSourceComponents, [&](const std::weak_ptr<LightSourceComponent>& component) {
		return !component.owner_before(lightSourceComponent) && !lightSourceComponent.owner_before(component);
		});
}

void Engine::Renderer::renderObject(const RenderQueueEntry& entry) {
	if (const auto it = materials.find(entry.materialKey); it != materials.end()) {
		it->second.use(camera.getViewMatrix(), glm::perspective(glm::radians(45.0f), static_cast<float>(Platform::getFrameBufferSize().x) / static_cast<float>(Platform::getFrameBufferSize().y), 0.1f, 10000.0f), entry.modelMatrix);
		it->second.shader.setVec3("color", entry.color);
		it->second.shader.setBool("useTexture", entry.textureID != 0);

		glBindVertexArray(entry.VAO);
		glDrawElements(entry.drawMode, entry.vertexCount, GL_UNSIGNED_INT, nullptr);
		glBindVertexArray(0);
	}
	else {
		std::cerr << "Shader program key not found: " << entry.materialKey << '\n';
	}
}

void Engine::Renderer::renderObject(const LightRenderQueueEntry& entry) {
	if (const auto it = lightShaders.find(entry.shaderKey); it != lightShaders.end()) {
		it->second.use();

		// Set up the light type based on the entry's properties
		it->second.setInt("lightType", static_cast<int>(entry.type)); // Assuming lightType is set in LightRenderQueueEntry
		it->second.setVec3("color", entry.color);
		it->second.setFloat("intensity", entry.intensity);

		if (entry.type == LightType::Point || entry.type == LightType::Spot) {
			it->second.setVec3("position", entry.position);
			it->second.setFloat("constant", entry.constant);
			it->second.setFloat("linear", entry.linear);
			it->second.setFloat("quadratic", entry.quadratic);
		}
		if (entry.type == LightType::Directional || entry.type == LightType::Spot) {
			it->second.setVec3("direction", entry.direction);
		}
		if (entry.type == LightType::Spot) {
			it->second.setFloat("cutOff", entry.cutOff);
			it->second.setFloat("outerCutOff", entry.outerCutOff);
		}
	}
	else {
		std::cerr << "Shader program key not found: " << entry.shaderKey << '\n';
	}

	// Bind G-buffer textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
}

void Engine::Renderer::renderObjects() {
	camera.updateCameraVectors();
	camera.processMouseMovement(Input::getMouse().delta);

	for (const auto& renderComponent : renderComponents) {
		if (const auto renderComponentPtr = renderComponent.lock()) {
			for (auto entries = renderComponentPtr->getQueueEntries(); const auto & entry : entries) {
				renderObject(entry);  // Geometry pass
			}
		}
		else {
			removeComponent(renderComponent.lock());
		}
	}

	// Collect all LightRenderQueueEntries into lightData for SSBO
	std::vector<LightRenderQueueEntry> lightData;
	lightData.reserve(lightSourceComponents.size());

	for (const auto& lightSourceComponent : lightSourceComponents) {
		if (const auto lightSourceComponentPtr = lightSourceComponent.lock()) {
			for (const auto& entry : lightSourceComponentPtr->getRenderQueueEntries()) {
				lightData.push_back(entry);
			}
		}
		else {
			removeLightSourceComponent(lightSourceComponent.lock());
		}
	}

	if (lightData.empty()) {
		return;
	}

	// Update SSBO with light data
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
	glBufferData(GL_SHADER_STORAGE_BUFFER, lightData.size() * sizeof(LightRenderQueueEntry), lightData.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboLights);  // Binding point 0
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Perform the Lighting Pass
	lightingShader.use();

	// Bind the G-buffer textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);

	// Bind the SSBO for lights
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboLights);

	// Set the camera position (viewPos) for specular calculations
	lightingShader.setVec3("viewPos", camera.getPosition().as<Units::Meters>());

	// Render a full-screen quad to process lighting
	static unsigned int quadVAO = 0;
	static unsigned int quadVBO;

	if (quadVAO == 0) {
		constexpr float quadVertices[] = {
			// Positions   // TexCoords
			-1.0f,  1.0f,  0.0f, 1.0f,
			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,

			-1.0f,  1.0f,  0.0f, 1.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f
		};
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), static_cast<void*>(nullptr));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
	}

	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void Engine::Renderer::beginFrame() {
	glViewport(0, 0, Platform::getFrameBufferSize().x, Platform::getFrameBufferSize().y);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Engine::Renderer::endFrame() {
	glfwSwapBuffers(Platform::window);
	glfwPollEvents();
}