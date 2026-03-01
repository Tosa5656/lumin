#include <iostream>
#include <fstream>
#include <sstream>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

class Shader
{
public:
    Shader(GLuint shader, GLuint type);
    ~Shader();

    GLuint GetShaderId() { return m_shader; }
    GLuint GetType() { return m_type; }
private:
    GLuint m_shader;
    GLuint m_type;
};

class ShadersManager
{
public:
    ShadersManager();
    ~ShadersManager();

    Shader CreateShader(std::string shader_path, GLuint type);
    GLuint CreateProgram(Shader vertex_shader, Shader fragment_shader);
private:
    std::string ReadShaderFile(std::string shader_path);
};