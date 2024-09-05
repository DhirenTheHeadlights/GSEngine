#include "Arena.h"

using namespace Game;

void Arena::initialize() {
    generateGridLines();
    setupBuffers();
}

void Arena::generateGridLines() {
    // Generate gridlines for the arena walls
    for (float i = -width / 2; i <= width / 2; i += 1.0f) {
        // Horizontal lines on front face
        gridVertices.push_back(-width / 2); gridVertices.push_back(i); gridVertices.push_back(depth / 2);
        gridVertices.push_back(width / 2); gridVertices.push_back(i); gridVertices.push_back(depth / 2);

        // Vertical lines on front face
        gridVertices.push_back(i); gridVertices.push_back(-height / 2); gridVertices.push_back(depth / 2);
        gridVertices.push_back(i); gridVertices.push_back(height / 2); gridVertices.push_back(depth / 2);
    }
    for (float i = -width / 2; i <= width / 2; i += 1.0f) {
		// Horizontal lines on back face
		gridVertices.push_back(-width / 2); gridVertices.push_back(i); gridVertices.push_back(-depth / 2);
		gridVertices.push_back(width / 2); gridVertices.push_back(i); gridVertices.push_back(-depth / 2);

		// Vertical lines on back face
		gridVertices.push_back(i); gridVertices.push_back(-height / 2); gridVertices.push_back(-depth / 2);
		gridVertices.push_back(i); gridVertices.push_back(height / 2); gridVertices.push_back(-depth / 2);
	}
    for (float i = -depth / 2; i <= depth / 2; i += 1.0f) {
        // Horizontal lines on left face
        gridVertices.push_back(-width / 2); gridVertices.push_back(-height / 2); gridVertices.push_back(i);
        gridVertices.push_back(width / 2); gridVertices.push_back(-height / 2); gridVertices.push_back(i);

        // Vertical lines on left face
        gridVertices.push_back(-width / 2); gridVertices.push_back(i); gridVertices.push_back(-depth / 2);
        gridVertices.push_back(-width / 2); gridVertices.push_back(i); gridVertices.push_back(depth / 2);
    }
    for (float i = -depth / 2; i <= depth / 2; i += 1.0f) {
		// Horizontal lines on right face
		gridVertices.push_back(-width / 2); gridVertices.push_back(height / 2); gridVertices.push_back(i);
		gridVertices.push_back(width / 2); gridVertices.push_back(height / 2); gridVertices.push_back(i);

		// Vertical lines on right face
		gridVertices.push_back(width / 2); gridVertices.push_back(i); gridVertices.push_back(-depth / 2);
		gridVertices.push_back(width / 2); gridVertices.push_back(i); gridVertices.push_back(depth / 2);
	}
}

void Arena::setupBuffers() {
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);

    glBindVertexArray(gridVAO);

    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), &gridVertices[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Arena::render(const glm::mat4& view, const glm::mat4& projection) {
    // Bind grid VAO and render gridlines
    glBindVertexArray(gridVAO);

    // Set up shaders and pass view/projection matrices
    // Shader should be set before calling this function
    glDrawArrays(GL_LINES, 0, gridVertices.size() / 3);

    glBindVertexArray(0);
}
