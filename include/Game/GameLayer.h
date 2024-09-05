#pragma once

#include <iostream>
#include <sstream>
#include <glm/vec2.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <gl2d/gl2d.h>

#include "imgui.h"
#include "imfilebrowser.h"

#include "PlatformInput.h"
#include "PlatformFunctions.h"

#include "Engine/Graphics/Camera.h"

bool initGame();
bool gameLogic(float deltaTime);
void closeGame();
