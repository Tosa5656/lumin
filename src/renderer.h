#pragma once

#include <iostream>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "shaders.h"

inline bool is_glfw_inited = false;

inline bool InitGLFW()
{
    if(!is_glfw_inited)
    {
        if(glfwInit())
        {
            is_glfw_inited = true;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return true;
    }
}

inline bool DestroyGLFW()
{
    if(is_glfw_inited)
    {
        glfwTerminate();
        is_glfw_inited = true;
        return true;
    }
    else
    {
        return false;
    }
}

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void Init();
    void Destroy();

    void Render();
private:
    GLuint m_vao, m_vbo, m_ebo;
    ShadersManager m_shaders_manager;
    GLuint m_shader_program;
};