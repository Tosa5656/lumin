#include "Renderer/Renderer.h"

void SetBackgroundColor(Color color)
{
	glClearColor(color.r, color.g, color.b, color.a);
}