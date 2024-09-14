#include "BoundingBox.h"
#include "Engine.h"

void Engine::drawBoundingBox(BoundingBox& boundingBox, const glm::mat4& viewProjectionMatrix, const bool moving, const glm::vec3& color) {
    const GLuint shaderID = Engine::shader.getID();

    if (!boundingBox.setGrid || moving) {

        constexpr float cellSize = 10.0f;

        const glm::vec3 min = boundingBox.lowerBound;
        const glm::vec3 max = boundingBox.upperBound;

        boundingBox.gridVertices.clear();

        // Generate grid lines for each face
        for (float y = min.y; y <= max.y; y += cellSize) {
            for (float x = min.x; x <= max.x; x += cellSize) {
                // Vertical lines (front and back faces)
                boundingBox.gridVertices.push_back(x);
                boundingBox.gridVertices.push_back(y);
                boundingBox.gridVertices.push_back(min.z);
                boundingBox.gridVertices.push_back(x);
                boundingBox.gridVertices.push_back(y);
                boundingBox.gridVertices.push_back(max.z);

                // Horizontal lines along x-axis (front and back faces)
                if (x + cellSize <= max.x) {
                    float xEnd = x + cellSize;
                    // Front face
                    boundingBox.gridVertices.push_back(x);
                    boundingBox.gridVertices.push_back(y);
                    boundingBox.gridVertices.push_back(min.z);
                    boundingBox.gridVertices.push_back(xEnd);
                    boundingBox.gridVertices.push_back(y);
                    boundingBox.gridVertices.push_back(min.z);

                    // Back face
                    boundingBox.gridVertices.push_back(x);
                    boundingBox.gridVertices.push_back(y);
                    boundingBox.gridVertices.push_back(max.z);
                    boundingBox.gridVertices.push_back(xEnd);
                    boundingBox.gridVertices.push_back(y);
                    boundingBox.gridVertices.push_back(max.z);
                }

                // **New Code: Horizontal lines along z-axis on left and right faces**
                if (x == min.x || x == max.x) {
                    for (float z = min.z; z + cellSize <= max.z; z += cellSize) {
                        float zEnd = z + cellSize;
                        // Left or Right face at x
                        boundingBox.gridVertices.push_back(x);
                        boundingBox.gridVertices.push_back(y);
                        boundingBox.gridVertices.push_back(z);
                        boundingBox.gridVertices.push_back(x);
                        boundingBox.gridVertices.push_back(y);
                        boundingBox.gridVertices.push_back(zEnd);
                    }
                }

				// Vertical lines along y-axis on left and right faces
				if (y + cellSize <= max.y) {
					float yEnd = y + cellSize;
					for (float z = min.z; z <= max.z; z += cellSize) {
						// Left face at x = min.x
						boundingBox.gridVertices.push_back(x);
						boundingBox.gridVertices.push_back(y);
						boundingBox.gridVertices.push_back(z);
						boundingBox.gridVertices.push_back(x);
						boundingBox.gridVertices.push_back(yEnd);
						boundingBox.gridVertices.push_back(z);

						// Right face at x = max.x
						boundingBox.gridVertices.push_back(x);
						boundingBox.gridVertices.push_back(y);
						boundingBox.gridVertices.push_back(z);
						boundingBox.gridVertices.push_back(x);
						boundingBox.gridVertices.push_back(yEnd);
						boundingBox.gridVertices.push_back(z);
					}
				}
            }
        }

        // Top and bottom faces grid lines
        for (float z = min.z; z <= max.z; z += cellSize) {
            for (float x = min.x; x <= max.x; x += cellSize) {
                if (x + cellSize <= max.x) {
                    float xEnd = x + cellSize;
                    // Lines along x-axis on top and bottom faces
                    // Top face at y = max.y
                    boundingBox.gridVertices.push_back(x);
                    boundingBox.gridVertices.push_back(max.y);
                    boundingBox.gridVertices.push_back(z);
                    boundingBox.gridVertices.push_back(xEnd);
                    boundingBox.gridVertices.push_back(max.y);
                    boundingBox.gridVertices.push_back(z);

                    // Bottom face at y = min.y
                    boundingBox.gridVertices.push_back(x);
                    boundingBox.gridVertices.push_back(min.y);
                    boundingBox.gridVertices.push_back(z);
                    boundingBox.gridVertices.push_back(xEnd);
                    boundingBox.gridVertices.push_back(min.y);
                    boundingBox.gridVertices.push_back(z);
                }

                if (z + cellSize <= max.z) {
                    float zEnd = z + cellSize;
                    // Lines along z-axis on top and bottom faces
                    // Top face at y = max.y
                    boundingBox.gridVertices.push_back(x);
                    boundingBox.gridVertices.push_back(max.y);
                    boundingBox.gridVertices.push_back(z);
                    boundingBox.gridVertices.push_back(x);
                    boundingBox.gridVertices.push_back(max.y);
                    boundingBox.gridVertices.push_back(zEnd);

                    // Bottom face at y = min.y
                    boundingBox.gridVertices.push_back(x);
                    boundingBox.gridVertices.push_back(min.y);
                    boundingBox.gridVertices.push_back(z);
                    boundingBox.gridVertices.push_back(x);
                    boundingBox.gridVertices.push_back(min.y);
                    boundingBox.gridVertices.push_back(zEnd);
                }
            }
        }

        // Upload data to GPU (same as before)
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

    // Draw the grid
    glBindVertexArray(boundingBox.gridVAO);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(boundingBox.gridVertices.size() / 3));
    glBindVertexArray(0);  // Unbind if necessary
}

