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

struct Vector3
{
public:
    Vector3()
    {
        x = 0;
        y = 0;
        z = 0;
    }

    Vector3(int X, int Y, int Z)
    {
        x = X;
        y = Y;
        z = Z;
    }

    int x = 0;
    int y = 0;
    int z = 0;
};