#include "Graphics/Renderer.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

#include "Core/JsonParser.h"
#include "Input/Input.h"
#include "Platform/ErrorReporting.h"
#include "Platform/Platform.h"

void Engine::Renderer::initialize() {
	enableReportGlErrors();

	JsonParse::parse(
		JsonParse::loadJson(RESOURCES_PATH "Shaders/Shaders.json"),
		[&](const std::string& key, const nlohmann::json& value) {
			materials[key] = Material(value["vertex"], value["fragment"], key);
		}
	);
}

void Engine::Renderer::addComponent(const std::shared_ptr<RenderComponent>& renderComponent) {
	renderComponents.push_back(renderComponent);
}

void Engine::Renderer::removeComponent(const std::shared_ptr<RenderComponent>& renderComponent) {
	std::erase_if(renderComponents, [&](const std::weak_ptr<RenderComponent>& component) {
		return !component.owner_before(renderComponent) && !renderComponent.owner_before(component);
		});
}

void Engine::Renderer::renderObject(const RenderQueueEntry& entry) {
	materials[entry.shaderProgram].use(camera.getViewMatrix(), glm::perspective(glm::radians(45.0f), static_cast<float>(Platform::getFrameBufferSize().x) / static_cast<float>(Platform::getFrameBufferSize().y), 0.1f, 10000.0f), entry.modelMatrix);

	glBindVertexArray(entry.VAO);
	glDrawElements(entry.drawMode, entry.vertexCount, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}

void Engine::Renderer::renderObjects() {
	camera.updateCameraVectors();
	camera.processMouseMovement(Input::getMouse().delta);

	for (const auto& renderComponent : renderComponents) {
		if (const auto renderComponentPtr = renderComponent.lock()) {
			renderObject(renderComponentPtr->getQueueEntry());
		}
		else {
			removeComponent(renderComponent.lock());
		}
	}
}

void Engine::Renderer::beginFrame() {
	glViewport(0, 0, Platform::getFrameBufferSize().x, Platform::getFrameBufferSize().y);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Engine::Renderer::endFrame() {
	glfwSwapBuffers(Platform::window);
	glfwPollEvents();
}