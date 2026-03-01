#include "renderer.h"

GLfloat vertices[] = {
    -0.5f, -0.5f, 0.0f,
     0.5f, -0.5f, 0.0f,
     0.0f,  0.5f, 0.0f
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

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Renderer::Destroy()
{

}

void Renderer::Render()
{
    glUseProgram(m_shader_program);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}
