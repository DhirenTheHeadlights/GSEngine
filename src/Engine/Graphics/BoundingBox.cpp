#include "Engine.h"
#include "BoundingBox.h"

void Engine::drawBoundingBox(BoundingBox& boundingBox, const glm::mat4& viewProjectionMatrix, const bool moving, const glm::vec3& color) {
    GLuint shaderID = Engine::shader.getID();

    // Check if the VBO and VAO need to be initialized or updated
    if (boundingBox.gridVertices.empty() || moving) {
        glm::vec3 min = boundingBox.lowerBound; // Lower-left-back corner
        glm::vec3 max = boundingBox.upperBound; // Upper-right-front corner

        // Clear previous vertices and calculate new ones
        boundingBox.gridVertices.clear();

        // Vertices of the rectangular prism (8 corners)
        glm::vec3 v0 = min; // (min.x, min.y, min.z) Lower-left-back
        glm::vec3 v1 = { max.x, min.y, min.z }; // Lower-right-back
        glm::vec3 v2 = { max.x, max.y, min.z }; // Upper-right-back
        glm::vec3 v3 = { min.x, max.y, min.z }; // Upper-left-back

        glm::vec3 v4 = { min.x, min.y, max.z }; // Lower-left-front
        glm::vec3 v5 = { max.x, min.y, max.z }; // Lower-right-front
        glm::vec3 v6 = { max.x, max.y, max.z }; // Upper-right-front
        glm::vec3 v7 = { min.x, max.y, max.z }; // Upper-left-front

        // Define the 12 edges of the rectangular prism
        std::vector<float> prismVertices = {
            // Back face
            v0.x, v0.y, v0.z, v1.x, v1.y, v1.z, // Edge v0-v1
            v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, // Edge v1-v2
            v2.x, v2.y, v2.z, v3.x, v3.y, v3.z, // Edge v2-v3
            v3.x, v3.y, v3.z, v0.x, v0.y, v0.z, // Edge v3-v0

            // Front face
            v4.x, v4.y, v4.z, v5.x, v5.y, v5.z, // Edge v4-v5
            v5.x, v5.y, v5.z, v6.x, v6.y, v6.z, // Edge v5-v6
            v6.x, v6.y, v6.z, v7.x, v7.y, v7.z, // Edge v6-v7
            v7.x, v7.y, v7.z, v4.x, v4.y, v4.z, // Edge v7-v4

            // Connecting edges between front and back faces
            v0.x, v0.y, v0.z, v4.x, v4.y, v4.z, // Edge v0-v4
            v1.x, v1.y, v1.z, v5.x, v5.y, v5.z, // Edge v1-v5
            v2.x, v2.y, v2.z, v6.x, v6.y, v6.z, // Edge v2-v6
            v3.x, v3.y, v3.z, v7.x, v7.y, v7.z  // Edge v3-v7
        };

        // Update the bounding box gridVertices
        boundingBox.gridVertices = prismVertices;

        // Setup or update buffers only if the VAO/VBO haven't been initialized or vertices have changed
        if (boundingBox.gridVAO == 0) {
            glGenVertexArrays(1, &boundingBox.gridVAO);
            glGenBuffers(1, &boundingBox.gridVBO);
        }

        glBindVertexArray(boundingBox.gridVAO);
        glBindBuffer(GL_ARRAY_BUFFER, boundingBox.gridVBO);
        glBufferData(GL_ARRAY_BUFFER, boundingBox.gridVertices.size() * sizeof(float), boundingBox.gridVertices.data(), GL_DYNAMIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }

    // Bind the VAO for drawing
    glBindVertexArray(boundingBox.gridVAO);

    // Update the uniforms
    glUniform3fv(Engine::shader.getUniformLocation("color"), 1, glm::value_ptr(color));
    glUniformMatrix4fv(Engine::shader.getUniformLocation("viewProjection"), 1, GL_FALSE, glm::value_ptr(viewProjectionMatrix));

    // Draw the bounding box lines
    glDrawArrays(GL_LINES, 0, boundingBox.gridVertices.size() / 3);

    // Optional: Unbind VAO, only needed if working with multiple VAOs
    // glBindVertexArray(0);
}
