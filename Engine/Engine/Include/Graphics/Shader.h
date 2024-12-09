#pragma once

#include <string>
#include <unordered_map>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace Engine {
    class Shader {
    public:
        Shader() = default;
		Shader(const std::string& vertexPath, const std::string& fragmentPath, const std::string& geometryPath = "");

		void createShaderProgram(const std::string& vertexPath, const std::string& fragmentPath, const std::string& geometryPath = "");
        void use() const;

        // Utility functions to set uniform values
        void setBool(const std::string& name, bool value) const;
        void setInt(const std::string& name, int value) const;
		void setIntArray(const std::string& name, const int* values, unsigned int count) const;
        void setFloat(const std::string& name, float value) const;
        void setMat4(const std::string& name, const glm::mat4& value) const;
		void setMat4Array(const std::string& name, const glm::mat4* values, unsigned int count) const;
		void setVec3(const std::string& name, const glm::vec3& value) const;

    	unsigned int getID() const { return ID; }
    private:
        unsigned int ID = 0;

        static std::string loadShaderSource(const std::string& filePath);

        void cacheUniformLocations();
        
        std::unordered_map<std::string, GLint> uniforms;

        static void checkCompileErrors(unsigned int shader, const std::string& type);
    };
}