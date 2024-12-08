#include "Graphics/3D/Renderer3D.h"

#include <glm/gtx/string_cast.hpp>

#include "Core/JsonParser.h"
#include "Core/ResourcePaths.h"
#include "Graphics/Shader.h"
#include "Graphics/3D/CubeMap.h"
#include "Graphics/3D/Material.h"
#include "Graphics/3D/Lights/PointLight.h"
#include "Platform/GLFW/ErrorReporting.h"
#include "Platform/GLFW/Input.h"
#include "Platform/GLFW/Window.h"

namespace {
	Engine::Camera camera;

	std::unordered_map<std::string, Engine::Material> materials;
	std::unordered_map<std::string, Engine::Shader> deferredRenderingShaders;
	std::unordered_map<std::string, Engine::Shader> forwardRenderingShaders;
	std::unordered_map<std::string, Engine::Shader> lightingShaders;

	GLuint gBuffer = 0;
	GLuint gPosition = 0;
	GLuint gNormal = 0;
	GLuint gAlbedoSpec = 0;
	GLuint ssboLights = 0;
	GLuint lightSpaceBlockUBO = 0;

	Engine::CubeMap reflectionCubeMap;

	float shadowWidth = 4096;
	float shadowHeight = 4096;

	Engine::Length nearPlane = Engine::meters(10.0f);
	Engine::Length farPlane = Engine::meters(1000.f);

	bool depthMapDebug = false;

	void loadShaders(const std::string& shaderPath, const std::string& shaderFileName, std::unordered_map<std::string, Engine::Shader>& shaders) {
		Engine::JsonParse::parse(
			Engine::JsonParse::loadJson(shaderPath + shaderFileName),
			[&](const std::string& key, const nlohmann::json& value) {
				shaders.emplace(key, Engine::Shader(shaderPath + value["vertex"].get<std::string>(),
					shaderPath + value["fragment"].get<std::string>()));
			}
		);
	}
}

void Engine::Renderer::Group::addRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent) {
	renderComponents.push_back(renderComponent);
}

void Engine::Renderer::Group::addLightSourceComponent(const std::shared_ptr<LightSourceComponent>& lightSourceComponent) {
	lightSourceComponents.push_back(lightSourceComponent);

	if (const auto pointLight = std::dynamic_pointer_cast<PointLight>(lightSourceComponent); pointLight) {
		pointLight->getShadowMap().create(static_cast<int>((shadowWidth + shadowHeight) / 2.f), true);

		depthMaps.push_back(pointLight->getShadowMap().getTextureID());
		depthMapFBOs.push_back(pointLight->getShadowMap().getFrameBufferID());
	}
	else {
		GLuint depthMap, depthMapFbo;
		glGenTextures(1, &depthMap);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, static_cast<GLsizei>(shadowWidth), static_cast<GLsizei>(shadowHeight), 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		constexpr float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

		glGenFramebuffers(1, &depthMapFbo);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		depthMaps.push_back(depthMap);
		depthMapFBOs.push_back(depthMapFbo);
	}
}

void Engine::Renderer::Group::removeRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent) {
	std::erase_if(renderComponents, [&](const std::weak_ptr<RenderComponent>& component) {
		return !component.owner_before(renderComponent) && !renderComponent.owner_before(component);
		});
}

void Engine::Renderer::Group::removeLightSourceComponent(const std::shared_ptr<LightSourceComponent>& lightSourceComponent) {
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

void Engine::Renderer::initialize() {
	enableReportGlErrors();

	const std::string shaderPath = std::string(ENGINE_RESOURCES_PATH) + "Shaders/";

	const std::string objectShadersPath = shaderPath + "Object/";
	JsonParse::parse(
		JsonParse::loadJson(objectShadersPath + "object_shaders.json"),
		[&](const std::string& key, const nlohmann::json& value) {
			materials.emplace(key, Material(objectShadersPath + value["vertex"].get<std::string>(),
			objectShadersPath + value["fragment"].get<std::string>(), key));
		}
	);

	loadShaders(shaderPath + "DeferredRendering/", "deferred_rendering.json", deferredRenderingShaders);
	loadShaders(shaderPath + "ForwardRendering/", "forward_rendering.json", forwardRenderingShaders);
	loadShaders(shaderPath + "Lighting/", "light_shaders.json", lightingShaders);

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

	// Set up the UBO for light space matrices
	glGenBuffers(1, &lightSpaceBlockUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, lightSpaceBlockUBO);
	constexpr size_t bufferSize = sizeof(glm::mat4) * 10; // MAX_LIGHTS is 10 in the shader
	glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(bufferSize), nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 4, lightSpaceBlockUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	// Initialize SSBO for lights
	glGenBuffers(1, &ssboLights);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, ssboLights);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	const auto& lightingShader = deferredRenderingShaders["LightingPass"];

	lightingShader.use();
	lightingShader.setInt("gPosition", 0);
	lightingShader.setInt("gNormal", 1);
	lightingShader.setInt("gAlbedoSpec", 2);
	lightingShader.setBool("depthMapDebug", depthMapDebug);

	reflectionCubeMap.create(1024);
}

namespace {
	void renderObject(const Engine::RenderQueueEntry& entry, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
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

	void renderObjectForward(const Engine::Shader& forwardRenderingShader, const Engine::RenderQueueEntry& entry, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
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

	void renderObject(const Engine::LightRenderQueueEntry& entry) {
		if (const auto it = lightingShaders.find(entry.shaderKey); it != lightingShaders.end()) {
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

	void renderLightingPass(const Engine::Shader& lightingShader, const std::vector<Engine::LightShaderEntry>& lightData, const std::vector<glm::mat4>& lightSpaceMatrices, const std::vector<GLuint>& depthMapFBOs) {
		if (lightData.empty()) {
			return;
		}

		lightingShader.use();

		// Update SSBO with light data
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
		glBufferData(GL_SHADER_STORAGE_BUFFER, lightData.size() * sizeof(Engine::LightShaderEntry), lightData.data(), GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, ssboLights);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Update UBO with light space matrices
		glBindBuffer(GL_UNIFORM_BUFFER, lightSpaceBlockUBO);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, lightSpaceMatrices.size() * sizeof(glm::mat4), lightSpaceMatrices.data());
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// Lighting pass
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);

		std::vector<GLint> shadowMapUnits(depthMapFBOs.size());
		for (size_t i = 0; i < depthMapFBOs.size(); ++i) {
			glActiveTexture(GL_TEXTURE3 + i);
			glBindTexture(GL_TEXTURE_2D, depthMapFBOs[i]);
			shadowMapUnits[i] = 3 + i;
		}

		lightingShader.setIntArray("shadowMaps", shadowMapUnits.data(), static_cast<unsigned>(shadowMapUnits.size()));
		lightingShader.setMat4Array("lightSpaceMatrices", lightSpaceMatrices.data(), static_cast<unsigned>(lightSpaceMatrices.size()));

		// Pass other G-buffer textures
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, gPosition);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, gNormal);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);

		constexpr GLuint bindingUnit = 5;
		reflectionCubeMap.bind(bindingUnit);
		lightingShader.setInt("environmentMap", bindingUnit);

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

	void renderShadowPass(const Engine::Shader& shadowShader, const std::vector<std::weak_ptr<Engine::RenderComponent>>& renderComponents, const glm::mat4& lightSpaceMatrix, const GLuint depthMapFBO) {
		shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

		glViewport(0, 0, static_cast<GLsizei>(shadowWidth), static_cast<GLsizei>(shadowHeight)); 
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
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
		glViewport(0, 0, Engine::Window::getFrameBufferSize().x, Engine::Window::getFrameBufferSize().y); // Restore viewport
	}

	glm::vec3 ensureNonCollinearUp(const glm::vec3& direction, const glm::vec3& up) {
		constexpr float epsilon = 0.001f;

		const glm::vec3 normalizedDirection = normalize(direction);
		glm::vec3 normalizedUp = normalize(up);
		const float dotProduct = dot(normalizedDirection, normalizedUp);

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


	glm::mat4 calculateLightSpaceMatrix(const std::shared_ptr<Engine::Light>& light) {
		const auto& entry = light->getRenderQueueEntry().shaderEntry;
		const glm::vec3 lightDirection = entry.direction;

		glm::vec3 lightPos(0.0f);
		glm::mat4 lightProjection(1.0f);

		if (entry.lightType == static_cast<int>(Engine::LightType::Directional)) {
			lightPos = -lightDirection * 10.0f;
			lightProjection = glm::ortho(-10000.0f, 10000.0f, -10000.0f, 1000.0f,
				nearPlane.as<Engine::Meters>(), farPlane.as<Engine::Meters>());
		}
		else if (entry.lightType == static_cast<int>(Engine::LightType::Spot)) {
			lightPos = entry.position;
			const float cutoff = entry.cutOff;
			lightProjection = glm::perspective(cutoff, 1.0f, nearPlane.as<Engine::Meters>(), farPlane.as<Engine::Meters>());
		}

		const glm::mat4 lightView = lookAt(
			lightPos,
			lightPos + lightDirection,
			ensureNonCollinearUp(lightDirection, glm::vec3(0.0f, 1.0f, 0.0f))
		);

		return lightProjection * lightView;
	}
}

void Engine::Renderer::renderObjects(Group& group) {
	const auto& renderComponents = group.getRenderComponents();
	const auto& lightSourceComponents = group.getLightSourceComponents();

	camera.updateCameraVectors();
	if (!Window::isMouseVisible()) camera.processMouseMovement(Input::getMouse().delta);

	Debug::addImguiCallback([] {
		ImGui::Begin("Near/Far Plane");
		Debug::unitSlider<Length, Meters>("Near Plane", nearPlane, meters(0.1f), meters(100.0f));
		Debug::unitSlider<Length, Meters>("Far Plane", farPlane, meters(10.0f), meters(10000.0f));
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
			group.removeLightSourceComponent(lightSourceComponent.lock());
		}
	}

	const auto& forwardRenderingShader = forwardRenderingShaders["ForwardRendering"];
	forwardRenderingShader.use();

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
	glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(lightData.size()) * sizeof(LightShaderEntry), lightData.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboLights);

	reflectionCubeMap.update(glm::vec3(0.f), glm::perspective(glm::radians(45.0f), 1.0f, nearPlane.as<Meters>(), farPlane.as<Meters>()),
		[&group, renderComponents, forwardRenderingShader](const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
			for (const auto& renderComponent : renderComponents) {
				if (const auto renderComponentPtr = renderComponent.lock()) {
					for (auto entries = renderComponentPtr->getQueueEntries(); const auto & entry : entries) {
						renderObjectForward(forwardRenderingShader, entry, viewMatrix, projectionMatrix);
					}
				}
				else {
					group.removeRenderComponent(renderComponent.lock());
				}
			}
		});

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	std::vector<glm::mat4> lightSpaceMatrices;

	const auto& shadowShader = deferredRenderingShaders["ShadowPass"];
	shadowShader.use();

	for (size_t i = 0; i < lightSourceComponents.size(); ++i) {
		for (const auto& light : lightSourceComponents[i].lock()->getLights()) {
			if (const auto pointLight = std::dynamic_pointer_cast<PointLight>(light); pointLight) {
				const auto lightPos = pointLight->getRenderQueueEntry().shaderEntry.position;

				pointLight->getShadowMap().update(lightPos, glm::mat4(1.f), [&](const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
					shadowShader.use();
					shadowShader.setMat4("view", viewMatrix);     
					shadowShader.setMat4("projection", projectionMatrix);
					shadowShader.setVec3("lightPos", lightPos);
					shadowShader.setFloat("farPlane", farPlane.as<Meters>());

					for (const auto& renderComponent : renderComponents) {
						if (const auto renderComponentPtr = renderComponent.lock()) {
							for (const auto& entry : renderComponentPtr->getQueueEntries()) {
								shadowShader.setMat4("model", entry.modelMatrix);  // Object's model matrix
								glBindVertexArray(entry.VAO);
								glDrawElements(entry.drawMode, entry.vertexCount, GL_UNSIGNED_INT, nullptr);
							}
						}
					}
					});
			}
			else {
				lightSpaceMatrices.push_back(calculateLightSpaceMatrix(light));
				renderShadowPass(shadowShader, renderComponents, lightSpaceMatrices.back(), group.depthMapFBOs[i]);
			}
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
			group.removeRenderComponent(renderComponent.lock());
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
			group.removeLightSourceComponent(lightSourceComponent.lock());
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

	renderLightingPass(deferredRenderingShaders["LightingPass"], lightData, lightSpaceMatrices, group.depthMaps);
}

Engine::Camera& Engine::Renderer::getCamera() {
	return camera;
}