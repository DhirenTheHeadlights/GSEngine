#include "Graphics/Renderer.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include "Core/JsonParser.h"
#include "Core/ResourcePaths.h"
#include "Platform/GLFW/ErrorReporting.h"
#include "Platform/GLFW/Input.h"
#include "Platform/GLFW/Window.h"

Engine::Camera Engine::Renderer::camera;

void Engine::Renderer::initialize() {
	enableReportGlErrors();

	const std::string shaderPath = std::string(ENGINE_RESOURCES_PATH) + "Shaders/";

	// Load material shaders
	JsonParse::parse(
		JsonParse::loadJson(shaderPath + "Shaders.json"),
		[&](const std::string& key, const nlohmann::json& value) {
			materials.emplace(key, Material(shaderPath + value["vertex"].get<std::string>(),
			shaderPath + value["fragment"].get<std::string>(), key));
		}
	);

	// Load light shaders
	JsonParse::parse(
		JsonParse::loadJson(shaderPath + "LightShaders.json"),
		[&](const std::string& key, const nlohmann::json& value) {
			lightShaders.emplace(key, Shader(shaderPath + value["vertex"].get<std::string>(),
			shaderPath + value["fragment"].get<std::string>()));
		}
	);

	// Set up the lighting shader
	lightingShader.createShaderProgram(shaderPath + "lighting_pass.vert", shaderPath + "lighting_pass.frag");

	const GLsizei screenWidth = Window::getFrameBufferSize().x;
	const GLsizei screenHeight = Window::getFrameBufferSize().y;

	glGenFramebuffers(1, &gBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

	// Set up G-buffer textures
	glGenTextures(1, &gPosition);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

	glGenTextures(1, &gNormal);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

	glGenTextures(1, &gAlbedoSpec);
	glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);

	constexpr GLuint attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, attachments);

	GLuint rboDepth;
	glGenRenderbuffers(1, &rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screenWidth, screenHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
	

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Initialize SSBO for lights
	glGenBuffers(1, &ssboLights);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboLights);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	lightingShader.use();
	lightingShader.setInt("gPosition", 0);
	lightingShader.setInt("gNormal", 1);
	lightingShader.setInt("gAlbedoSpec", 2);

	this->shadowHeight = 1024;
	this->shadowWidth = 1024;

	shadowShader.createShaderProgram(shaderPath + "shadow.vert", shaderPath + "shadow.frag");

	shadowCubeMap.create(1024);

	forwardRenderingShader.createShaderProgram(shaderPath + "forward_rendering.vert", shaderPath + "forward_rendering.frag");
}

void Engine::Renderer::addRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent) {
	renderComponents.push_back(renderComponent);
}

void Engine::Renderer::addLightSourceComponent(const std::shared_ptr<LightSourceComponent>& lightSourceComponent) {
	lightSourceComponents.push_back(lightSourceComponent);

	// Generate depth map and FBO for each light
	GLuint depthMap, depthMapFBO;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowWidth, shadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	constexpr float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glGenFramebuffers(1, &depthMapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	depthMaps.push_back(depthMap);
	depthMapFBOs.push_back(depthMapFBO);
}

void Engine::Renderer::removeRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent) {
	std::erase_if(renderComponents, [&](const std::weak_ptr<RenderComponent>& component) {
		return !component.owner_before(renderComponent) && !renderComponent.owner_before(component);
		});
}

void Engine::Renderer::removeLightSourceComponent(const std::shared_ptr<LightSourceComponent>& lightSourceComponent) {
	const auto it = std::ranges::find_if(lightSourceComponents,
	                                     [&](const std::weak_ptr<LightSourceComponent>& component) {
		                                     return !component.owner_before(lightSourceComponent) && !lightSourceComponent.owner_before(component);
	                                     });

	if (it != lightSourceComponents.end()) {
		const size_t index = std::distance(lightSourceComponents.begin(), it);

		glDeleteTextures(1, &depthMaps[index]);
		glDeleteFramebuffers(1, &depthMapFBOs[index]);

		depthMaps.erase(depthMaps.begin() + index);
		depthMapFBOs.erase(depthMapFBOs.begin() + index);
		lightSourceComponents.erase(it);
	}
}

void Engine::Renderer::renderObject(const RenderQueueEntry& entry, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
	if (const auto it = materials.find(entry.materialKey); it != materials.end()) {
		it->second.use(viewMatrix, projectionMatrix, entry.modelMatrix);
		it->second.shader.setVec3("color", entry.color);
		it->second.shader.setBool("useTexture", entry.textureID != 0);

		if (entry.textureID != 0) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, entry.textureID);
			it->second.shader.setInt("diffuseTexture", 0); 
		}

		glBindVertexArray(entry.VAO);
		glDrawElements(entry.drawMode, entry.vertexCount, GL_UNSIGNED_INT, nullptr);
		glBindVertexArray(0);

		if (entry.textureID != 0) {
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}
	else {
		std::cerr << "Shader program key not found: " << entry.materialKey << '\n';
	}
}

void Engine::Renderer::renderObjectForward(const RenderQueueEntry& entry, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const {
	forwardRenderingShader.setVec3("color", entry.color);
	forwardRenderingShader.setBool("useTexture", entry.textureID != 0);
	forwardRenderingShader.setMat4("model", entry.modelMatrix);
	forwardRenderingShader.setMat4("view", viewMatrix);
	forwardRenderingShader.setMat4("projection", projectionMatrix);
	forwardRenderingShader.setVec3("viewPos", glm::vec3(0.f));

	if (entry.textureID != 0) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, entry.textureID);
		forwardRenderingShader.setInt("diffuseTexture", 0);
	}

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);

	glBindVertexArray(entry.VAO);
	glDrawElements(entry.drawMode, entry.vertexCount, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);

	if (entry.textureID != 0) {
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

void Engine::Renderer::renderObject(const LightRenderQueueEntry& entry) {
	if (const auto it = lightShaders.find(entry.shaderKey); it != lightShaders.end()) {
		it->second.use();
		it->second.setMat4("model", glm::mat4(1.0f));
		it->second.setMat4("view", camera.getViewMatrix());
		it->second.setMat4("projection", camera.getProjectionMatrix());

		it->second.setVec3("color", entry.shaderEntry.color);
		it->second.setFloat("intensity", entry.shaderEntry.intensity);
	}
	else {
		std::cerr << "Shader program key not found: " << entry.shaderKey << '\n';
	}
}

void Engine::Renderer::renderLightingPass(const std::vector<LightShaderEntry>& lightData, const std::vector<glm::mat4>& lightSpaceMatrices) const {
	if (lightData.empty()) {
		return;
	}
	//dhiren sucks nuts (mmmmmmmfgh)
	// Update SSBO with light data
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
	glBufferData(GL_SHADER_STORAGE_BUFFER, lightData.size() * sizeof(LightShaderEntry), lightData.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboLights);  // Binding point 0
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Lighting pass
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	lightingShader.use();

	// Pass shadow maps and light space matrices for each light
	for (size_t i = 0; i < depthMaps.size(); ++i) {
		glActiveTexture(GL_TEXTURE3 + i);
		glBindTexture(GL_TEXTURE_2D, depthMaps[i]);
		lightingShader.setInt("shadowMaps[" + std::to_string(i) + "]", 3 + i);  // Texture unit starts at 3
		lightingShader.setMat4("lightSpaceMatrices[" + std::to_string(i) + "]", lightSpaceMatrices[i]);
	}

	// Pass other G-buffer textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);

	GLuint bindingUnit = 3 + static_cast<unsigned>(depthMaps.size());
	shadowCubeMap.bind(bindingUnit);
	lightingShader.setInt("environmentMap", static_cast<int>(bindingUnit));

	// Render a full-screen quad to process lighting
	static unsigned int quadVAO = 0;
	static unsigned int quadVBO = 0;

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

	glEnable(GL_DEPTH_TEST);  // Re-enable depth testing after the lighting pass
}

void Engine::Renderer::renderShadowPass(const glm::mat4& lightSpaceMatrix) const {
	shadowShader.use();
	shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

	glViewport(0, 0, shadowWidth, shadowHeight);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_DEPTH_BUFFER_BIT);

	for (const auto& renderComponent : renderComponents) {
		if (const auto renderComponentPtr = renderComponent.lock()) {
			for (const auto& entry : renderComponentPtr->getQueueEntries()) {
				shadowShader.setMat4("model", entry.modelMatrix);

				glBindVertexArray(entry.VAO);
				glDrawElements(entry.drawMode, entry.vertexCount, GL_UNSIGNED_INT, nullptr);
				glBindVertexArray(0);
			}
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, Window::getFrameBufferSize().x, Window::getFrameBufferSize().y); // Restore viewport
}

glm::vec3 Engine::Renderer::ensureNonCollinearUp(const glm::vec3& direction, const glm::vec3& up) {
	constexpr float epsilon = 0.001f;

	const glm::vec3 normalizedDirection = glm::normalize(direction);
	glm::vec3 normalizedUp = glm::normalize(up);
	const float dotProduct = glm::dot(normalizedDirection, normalizedUp);

	if (glm::abs(dotProduct) > 1.0f - epsilon) {
		if (glm::abs(normalizedDirection.y) > 0.9f) {
			normalizedUp = glm::vec3(0.0f, 0.0f, 1.0f); // Z-axis
		}
		else {
			normalizedUp = glm::vec3(0.0f, 1.0f, 0.0f); // Y-axis
		}
	}

	return normalizedUp;
}


glm::mat4 Engine::Renderer::calculateLightSpaceMatrix(const std::shared_ptr<Light>& light) const {
	switch (static_cast<LightType>(light->getRenderQueueEntry().shaderEntry.lightType)) {
	case LightType::Directional: {
		const glm::vec3 lightDirection = light->getRenderQueueEntry().shaderEntry.direction;
		const glm::vec3 lightPos = -lightDirection * 10.0f;
		const glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, nearPlane, farPlane);
		const glm::mat4 lightView = lookAt(lightPos, lightPos + lightDirection, ensureNonCollinearUp(lightDirection, glm::vec3(0.0f, 1.0f, 0.0f)));
		return lightProjection * lightView;
	}
	case LightType::Spot: {
		const glm::vec3 lightPos = light->getRenderQueueEntry().shaderEntry.position;
		const glm::vec3 lightDirection = light->getRenderQueueEntry().shaderEntry.direction;
		const float cutoff = glm::radians(light->getRenderQueueEntry().shaderEntry.cutOff * 2.f);
		const glm::mat4 lightProjection = glm::perspective(cutoff, 1.0f, nearPlane, farPlane);
		const glm::mat4 lightView = lookAt(lightPos, lightPos + lightDirection, ensureNonCollinearUp(lightDirection, glm::vec3(0.0f, 1.0f, 0.0f)));
		return lightProjection * lightView;
	}
	case LightType::Point:

		break;
	}

	return { 1.0f };
}


void Engine::Renderer::renderObjects() {
	camera.updateCameraVectors();
	if (!Window::isMouseVisible()) camera.processMouseMovement(Input::getMouse().delta);

	Debug::addImguiCallback([this] {
		ImGui::Begin("Near/Far Plane");
		ImGui::SliderFloat("Near Plane", &nearPlane, 0.1f, 10.0f);
		ImGui::SliderFloat("Far Plane", &farPlane, 10.0f, 10000.0f);
		ImGui::End();
		});

	// Grab all LightRenderQueueEntries from LightSourceComponents
	std::vector<LightShaderEntry> lightData;
	lightData.reserve(lightSourceComponents.size());
	for (const auto& lightSourceComponent : lightSourceComponents) {
		if (const auto lightSourceComponentPtr = lightSourceComponent.lock()) {
			for (const auto& entry : lightSourceComponentPtr->getRenderQueueEntries()) {
				lightData.push_back(entry.shaderEntry);
			}
		}
		else {
			removeLightSourceComponent(lightSourceComponent.lock());
		}
	}

	// Bind ssbo for cube map
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
	glBufferData(GL_SHADER_STORAGE_BUFFER, lightData.size() * sizeof(LightShaderEntry), lightData.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboLights);  // Binding point 0

	shadowCubeMap.update(glm::vec3(0.f), glm::perspective(glm::radians(45.0f), 1.0f, nearPlane, farPlane), [this](const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboLights);  // Binding point 0

		forwardRenderingShader.use();

		for (const auto& renderComponent : renderComponents) {
			if (const auto renderComponentPtr = renderComponent.lock()) {
				for (auto entries = renderComponentPtr->getQueueEntries(); const auto & entry : entries) {
					renderObjectForward(entry, viewMatrix, projectionMatrix);
				}
			}
			else {
				removeRenderComponent(renderComponent.lock());
			}
		}
	});

	// Unbind ssbo
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	std::vector<glm::mat4> lightSpaceMatrices;

	for (size_t i = 0; i < lightSourceComponents.size(); ++i) {
		for (const auto& light : lightSourceComponents[i].lock()->getLights()) {
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBOs[i]);
			glClear(GL_DEPTH_BUFFER_BIT);

			lightSpaceMatrices.push_back(calculateLightSpaceMatrix(light));
			renderShadowPass(lightSpaceMatrices.back());
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Geometry pass
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (const auto& renderComponent : renderComponents) {
		if (const auto renderComponentPtr = renderComponent.lock()) {
			for (auto entries = renderComponentPtr->getQueueEntries(); const auto & entry : entries) {
				renderObject(entry, camera.getViewMatrix(), camera.getProjectionMatrix());
			}
		}
		else {
			removeRenderComponent(renderComponent.lock());
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Light glow pass
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDisable(GL_DEPTH_TEST);

	for (const auto& lightSourceComponent : lightSourceComponents) {
		if (const auto lightSourceComponentPtr = lightSourceComponent.lock()) {
			for (const auto& entry : lightSourceComponentPtr->getRenderQueueEntries()) {
				renderObject(entry); 
			}
		}
		else {
			removeLightSourceComponent(lightSourceComponent.lock());
		}
	}

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	if (Window::getFbo().has_value()) {
		glBindFramebuffer(GL_FRAMEBUFFER, Window::getFbo().value());
	}
	else {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	renderLightingPass(lightData, lightSpaceMatrices);
}