#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <Color/Color.h>
#include <glad/glad.h>

void SetBackgroundColor(Color color);

std::string ReadShaderSource(const std::string& filePath);