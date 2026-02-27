#pragma once

#include <iostream>

struct Vector2
{
public:
    Vector2()
    {
        x = 0;
        y = 0;
    }

    Vector2(int X, int Y)
    {
        x = X;
        y = Y;
    }

    int x = 0;
    int y = 0;
};