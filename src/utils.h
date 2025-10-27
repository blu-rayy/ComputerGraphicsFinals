#pragma once
#include <GL/freeglut.h>

// clamp-mix for floats
float mixf(float a, float b, float t);

// mix two RGB colors
void mixColor(const float a[3], const float b[3], float t, float out[3]);

// Draw a filled circle at cx,cy with given radius and color
void drawCircle(float cx, float cy, float radius, const float color[3]);
