#include "renderer.h"
#include <cmath>

GLfloat vertices[] = {
    0.5f,  0.5f, 0.0f,
    0.5f, -0.5f, 0.0f,
   -0.5f, -0.5f, 0.0f,
   -0.5f,  0.5f, 0.0f
};
GLuint indices[] = {
    0, 1, 3,
    1, 2, 3
};

Renderer::Renderer()
{

}

Renderer::~Renderer()
{

}

void Renderer::Init()
{
    Shader vertex_shader = m_shaders_manager.CreateShader("shaders/vertex.glsl", GL_VERTEX_SHADER);
    Shader fragment_shader = m_shaders_manager.CreateShader("shaders/fragment.glsl", GL_FRAGMENT_SHADER);

    m_shader_program = m_shaders_manager.CreateProgram(vertex_shader, fragment_shader);

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Renderer::Destroy()
{

}

void Renderer::Render()
{
    GLfloat timeValue = glfwGetTime();
    GLfloat redValue = (cos(timeValue) / 3) + 0.3;
    GLfloat greenValue = (sin(timeValue) / 2) + 0.5;
    GLint vertexColorLocation = glGetUniformLocation(m_shader_program, "color");
    glUseProgram(m_shader_program);
    glUniform4f(vertexColorLocation, redValue, greenValue, 0.25f, 1.0f);

    glUseProgram(m_shader_program);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
