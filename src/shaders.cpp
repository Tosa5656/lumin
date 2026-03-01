#include "shaders.h"

Shader::Shader(GLuint shader, GLuint type)
{
    m_shader = shader;
    m_type = type;
}

Shader::~Shader()
{

}

ShadersManager::ShadersManager()
{

}

ShadersManager::~ShadersManager()
{

}

Shader ShadersManager::CreateShader(std::string shader_path, GLuint type)
{
    std::string shader_code = ReadShaderFile(shader_path);
    const GLchar* shader_source = shader_code.c_str();

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shader_source, NULL);
    glCompileShader(shader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return Shader(shader, type);
}

GLuint ShadersManager::CreateProgram(Shader vertex_shader, Shader fragment_shader)
{
    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader.GetShaderId());
    glAttachShader(shader_program, fragment_shader.GetShaderId());
    glLinkProgram(shader_program);

    GLint success;
    GLchar infoLog[512];
    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shader_program, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertex_shader.GetShaderId());
    glDeleteShader(fragment_shader.GetShaderId());

    return shader_program;
}

std::string ShadersManager::ReadShaderFile(std::string shader_path)
{
    std::ifstream ifs(shader_path);
    std::string line;
    std::stringstream ss;

    if (ifs.is_open())
    {
        while (std::getline(ifs, line))
            ss << line << std::endl;
    }

    return ss.str();
}