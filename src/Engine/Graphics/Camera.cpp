#include "Engine/Graphics/Camera.h"

void Engine::Camera::processMouseMovement(glm::vec2 offset) {
    offset *= mouseSensitivity;
    yaw += offset.x;
    pitch -= offset.y;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    updateCameraVectors();
}
 
void Engine::Camera::updateCameraVectors() {
    glm::vec3 newFront(0.0f, 0.0f, 0.0f);
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}