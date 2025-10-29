#pragma once
// glew must be included before freeglut to expose extension entrypoints
#include <GL/glew.h>
#include <GL/freeglut.h>

// Mesh descriptor used for persistent VBO/VAO meshes
struct Mesh { GLuint vbo; GLuint vao; GLsizei count; GLenum mode; };

// clamp-mix for floats
float mixf(float a, float b, float t);

// mix two RGB colors
void mixColor(const float a[3], const float b[3], float t, float out[3]);

// Draw a filled circle at cx,cy with given radius and color
void drawCircle(float cx, float cy, float radius, const float color[3]);

// Draw a persistent mesh (VBO/VAO)
void drawMesh(const Mesh &m);

// Draw a unit-centered quad transformed to (cx,cy) scaled by (w,h)
void drawUnitQuad(float cx, float cy, float w, float h);

// Unit quad mesh defined in main.cpp (extern so utils can use it)
extern Mesh mesh_unit_quad;
