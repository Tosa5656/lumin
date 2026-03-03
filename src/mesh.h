#pragma once

#include <iostream>
#include <vector>

#include <glad/gl.h>

#include "colors.h"

class Mesh
{
public:
    Mesh();
    Mesh(std::vector<GLfloat> vertices, std::vector<GLint> indices, Color color);
};