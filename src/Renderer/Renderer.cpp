#include "Renderer/Renderer.h"

void SetBackgroundColor(Color color)
{
	glClearColor(color.r, color.g, color.b, color.a);
}

std::string ReadShaderSource(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ: " << filePath << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}