#include "Engine.h"
#include "BoundingBox.h"

void Engine::drawBoundingBox(BoundingBox& boundingBox, const glm::mat4& viewProjectionMatrix, const bool moving, const glm::vec3& color) {
	const GLuint shaderID = Engine::shader.getID();

    if (!boundingBox.setGrid || moving) { 

	    constexpr float cellSize = 10.0f;

	    const glm::vec3 min = boundingBox.lowerBound;
	    const glm::vec3 max = boundingBox.upperBound;

        std::cout << "min: " << min.x << ", " << min.y << ", " << min.z << std::endl;
        std::cout << "max: " << max.x << ", " << max.y << ", " << max.z << std::endl;

        boundingBox.gridVertices.clear();

        // Generate grid lines for each face
        for (float y = min.y; y <= max.y; y += cellSize) {
            for (float x = min.x; x <= max.x; x += cellSize) {
                // Vertical lines (front and back)
                boundingBox.gridVertices.push_back(x); boundingBox.gridVertices.push_back(y); boundingBox.gridVertices.push_back(min.z);
                boundingBox.gridVertices.push_back(x); boundingBox.gridVertices.push_back(y); boundingBox.gridVertices.push_back(max.z);

                // Horizontal lines (front and back)
                if (x + cellSize <= max.x) {
                    boundingBox.gridVertices.push_back(x); boundingBox.gridVertices.push_back(y); boundingBox.gridVertices.push_back(min.z);
                    boundingBox.gridVertices.push_back(x + cellSize); boundingBox.gridVertices.push_back(y); boundingBox.gridVertices.push_back(min.z);
                    boundingBox.gridVertices.push_back(x); boundingBox.gridVertices.push_back(y); boundingBox.gridVertices.push_back(max.z);
                    boundingBox.gridVertices.push_back(x + cellSize); boundingBox.gridVertices.push_back(y); boundingBox.gridVertices.push_back(max.z);
                }
            }
            // Horizontal lines (left and right)
            if (y + cellSize <= max.y) {
                for (float z = min.z; z <= max.z; z += cellSize) {
                    boundingBox.gridVertices.push_back(min.x); boundingBox.gridVertices.push_back(y); boundingBox.gridVertices.push_back(z);
                    boundingBox.gridVertices.push_back(min.x); boundingBox.gridVertices.push_back(y + cellSize); boundingBox.gridVertices.push_back(z);
                    boundingBox.gridVertices.push_back(max.x); boundingBox.gridVertices.push_back(y); boundingBox.gridVertices.push_back(z);
                    boundingBox.gridVertices.push_back(max.x); boundingBox.gridVertices.push_back(y + cellSize); boundingBox.gridVertices.push_back(z);
                }
            }
        }

        // Top and bottom faces grid lines
        for (float z = min.z; z <= max.z; z += cellSize) {
            for (float x = min.x; x <= max.x; x += cellSize) {
                // Lines along x-axis on top and bottom
                boundingBox.gridVertices.push_back(x); boundingBox.gridVertices.push_back(max.y); boundingBox.gridVertices.push_back(z);
                boundingBox.gridVertices.push_back(x + cellSize); boundingBox.gridVertices.push_back(max.y); boundingBox.gridVertices.push_back(z);

                boundingBox.gridVertices.push_back(x); boundingBox.gridVertices.push_back(min.y); boundingBox.gridVertices.push_back(z);
                boundingBox.gridVertices.push_back(x + cellSize); boundingBox.gridVertices.push_back(min.y); boundingBox.gridVertices.push_back(z);

                // Lines along z-axis on top and bottom
                if (x + cellSize <= max.x) {
                    boundingBox.gridVertices.push_back(x); boundingBox.gridVertices.push_back(max.y); boundingBox.gridVertices.push_back(z);
                    boundingBox.gridVertices.push_back(x); boundingBox.gridVertices.push_back(max.y); boundingBox.gridVertices.push_back(z + cellSize);

                    boundingBox.gridVertices.push_back(x); boundingBox.gridVertices.push_back(min.y); boundingBox.gridVertices.push_back(z);
                    boundingBox.gridVertices.push_back(x); boundingBox.gridVertices.push_back(min.y); boundingBox.gridVertices.push_back(z + cellSize);
                }
            }
        }

        std::cout << "gridVertices.size(): " << boundingBox.gridVertices.size() << std::endl;

        if (boundingBox.gridVAO == 0) {
            glGenVertexArrays(1, &boundingBox.gridVAO);
            glGenBuffers(1, &boundingBox.gridVBO);
        }

        glBindVertexArray(boundingBox.gridVAO);
        glBindBuffer(GL_ARRAY_BUFFER, boundingBox.gridVBO);
        glBufferData(GL_ARRAY_BUFFER, boundingBox.gridVertices.size() * sizeof(float), boundingBox.gridVertices.data(), moving ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), static_cast<void*>(nullptr));
        glEnableVertexAttribArray(0);

        boundingBox.setGrid = true;
    }

    glUniform3fv(Engine::shader.getUniformLocation("color"), 1, glm::value_ptr(color));
    glUniformMatrix4fv(Engine::shader.getUniformLocation("viewProjection"), 1, GL_FALSE, glm::value_ptr(viewProjectionMatrix));
    glDrawArrays(GL_LINES, 0, boundingBox.gridVertices.size() / 3);
    glBindVertexArray(0);  // Unbind if necessary
}
