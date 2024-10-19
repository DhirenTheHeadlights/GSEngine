#include "Engine/Graphics/Renderer.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

#include "Engine/Platform/ErrorReporting.h"
#include "Engine/Platform/Platform.h"

void Engine::Renderer::initialize() {
	shader.createShaderProgram(RESOURCES_PATH "grid.vert", RESOURCES_PATH "grid.frag");

	enableReportGlErrors();
}

void Engine::Renderer::setCameraInformation(const Camera& camera) const {
	shader.use();
	shader.setMat4("view", value_ptr(camera.getViewMatrix()));
	shader.setMat4("projection", value_ptr(glm::perspective(glm::radians(45.0f), static_cast<float>(Platform::getFrameBufferSize().x) / static_cast<float>(Platform::getFrameBufferSize().y), 0.1f, 10000.0f)));
	shader.setMat4("model", value_ptr(glm::mat4(1.0f)));
}

void Engine::Renderer::addRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent) {
	renderComponents.push_back(renderComponent);
}

void Engine::Renderer::removeRenderComponent(const std::shared_ptr<RenderComponent>& renderComponent) {
	std::erase_if(renderComponents, [&](const std::weak_ptr<RenderComponent>& component) {
		return component.lock() == renderComponent;
	});
}

void Engine::Renderer::renderObjects() {
	shader.use();
	for (const auto& renderComponent : renderComponents) {
		if (const auto component = renderComponent.lock()) {
			shader.setMat4("transform", value_ptr(component->getTransform()));
			glBindVertexArray(component->getVAO());
			glBindTexture(GL_TEXTURE_2D, component->getTextureID());
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}
		else {
			removeRenderComponent(renderComponent.lock());
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