#pragma once

#include <iostream>

class Color
{
public:
	Color(float red, float green, float blue)
	{
		r = red;
		g = green;
		b = blue;
		a = 1.0f;
	};

	Color(float red, float green, float blue, float alpha) 
	{
		r = red;
		g = green;
		b = blue;
		a = alpha;
	};

	float r = 0, g = 0, b = 0, a = 0;
};