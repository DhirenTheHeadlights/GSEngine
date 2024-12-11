#pragma once
#include <glad/glad.h>

void GLAPIENTRY gl_debug_output(GLenum source,
	GLenum type,
	unsigned int id,
	GLenum severity,
	GLsizei length,
	const char* message,
	const void* user_param);

void enable_report_gl_errors();

