#pragma once

#include <iostream>
#include <algorithm>

class Color
{
public:
    Color() {};
    
    Color(float R, float G, float B, float A)
    {
        r = R;
        g = G;
        b = B;
        a = A;
    }

    Color(int R, int G, int B, int A)
    {
        r = std::clamp(R, 0, 255) / 255;
        g = std::clamp(G, 0, 255) / 255;
        b = std::clamp(B, 0, 255) / 255;
        a = std::clamp(A, 0, 255) / 255;
    }
   
    float r = 0, g = 0, b = 0, a = 0;
};