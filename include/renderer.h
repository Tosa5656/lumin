#pragma once

#include <iostream>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

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