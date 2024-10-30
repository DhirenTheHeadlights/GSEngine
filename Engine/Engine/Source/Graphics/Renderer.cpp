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

	// G-buffer setup
	glGenFramebuffers(1, &gBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

	// Position texture
	glGenTextures(1, &gPosition);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

	// Normal texture
	glGenTextures(1, &gNormal);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

	// Albedo + Specular texture
	glGenTextures(1, &gAlbedoSpec);
	glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);

	// Specify color attachments
	constexpr GLuint attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, attachments);

	// Depth renderbuffer
	GLuint rboDepth;
	glGenRenderbuffers(1, &rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screenWidth, screenHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

	// Check framebuffer completeness
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "G-Buffer not complete!" << '\n';
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Generate and initialize SSBO for lights
	glGenBuffers(1, &ssboLights);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboLights);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboLights);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// Set texture unit samplers in the lighting shader
	lightingShader.use();
	lightingShader.setInt("gPosition", 0);
	lightingShader.setInt("gNormal", 1);
	lightingShader.setInt("gAlbedoSpec", 2);
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

void Engine::Renderer::renderObject(const LightRenderQueueEntry& entry) {
	if (const auto it = lightShaders.find(entry.shaderKey); it != lightShaders.end()) {
		it->second.use();
		it->second.setMat4("model", glm::mat4(1.0f));
		it->second.setMat4("view", camera.getViewMatrix());
		it->second.setMat4("projection", glm::perspective(glm::radians(45.0f), static_cast<float>(Platform::getFrameBufferSize().x) / static_cast<float>(Platform::getFrameBufferSize().y), 0.1f, 10000.0f));

		it->second.setVec3("color", entry.shaderEntry.color);
		it->second.setFloat("intensity", entry.shaderEntry.intensity);
	}
	else {
		std::cerr << "Shader program key not found: " << entry.shaderKey << '\n';
	}
}


void Engine::Renderer::renderObjects() {
	camera.updateCameraVectors();
	if (!Platform::mouseVisible) camera.processMouseMovement(Input::getMouse().delta);

	// Geometry pass
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Collect all LightRenderQueueEntries into lightData for SSBO
	std::vector<LightShaderEntry> lightData;
	lightData.reserve(lightSourceComponents.size());

	for (const auto& lightSourceComponent : lightSourceComponents) {
		if (const auto lightSourceComponentPtr = lightSourceComponent.lock()) {
			for (const auto& entry : lightSourceComponentPtr->getRenderQueueEntries()) {
				renderObject(entry); 
				lightData.push_back(entry.shaderEntry);
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

	// Lighting pass
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);  // Disable depth testing for the lighting pass

	lightingShader.use();

	// Bind the G-buffer textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);

	// Set the camera position (viewPos) for specular calculations
	lightingShader.setVec3("viewPos", camera.getPosition().as<Units::Meters>());

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

void Engine::Renderer::beginFrame() {
	glViewport(0, 0, Platform::getFrameBufferSize().x, Platform::getFrameBufferSize().y);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Engine::Renderer::endFrame() {
	glfwSwapBuffers(Platform::window);
	glfwPollEvents();
}