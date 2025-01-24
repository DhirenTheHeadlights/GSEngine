module;

#include <glad/glad.h>

export module gse.platform.glfw.error_reporting;

export void GLAPIENTRY gl_debug_output(GLenum source,
	GLenum type,
	unsigned int id,
	GLenum severity,
	GLsizei length,
	const char* message,
	const void* user_param);

export void enable_report_gl_errors();

