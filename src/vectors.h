#pragma once

#include <iostream>

struct Vector2
{
    Vector2()
    {
        this->x = 0;
        this->y = 0;
    }

    Vector2(int x, int y)
    {
        this->x = x;
        this->y = y;
    }

    int x = 0;
    int y = 0;
};

struct Vector3
{
    Vector3()
    {
        this->x = 0;
        this->y = 0;
        this->z = 0;
    }

    Vector3(int x, int y, int z)
    {
        this->x = x;
        this->y = y;
        this->z = z;
    }

    int x = 0;
    int y = 0;
    int z = 0;
};


struct Vector4
{
    Vector4()
    {
        this->x = 0;
        this->y = 0;
        this->z = 0;
        this->w = 0;
    }

    Vector4(int x, int y, int z, int w)
    {
        this->x = x;
        this->y = y;
        this->z = z;
        this->w = w;
    }

    int x = 0;
    int y = 0;
    int z = 0;
    int w = 0;
};
