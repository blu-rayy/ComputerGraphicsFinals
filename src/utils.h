#pragma once
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <string>
#include <vector>
#include <chrono>

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

// --- Subtitle / Audio / UI API ---
// Simple timed subtitle entry
struct SubtitleEntry { std::string text; float start; float end; };

// Subtitle control
void subtitle_setText(const std::string &t);
void subtitle_setEntries(const std::vector<SubtitleEntry> &entries);
void subtitle_draw(int windowW, int windowH);
void subtitle_enable(bool enable);

// Audio control
void audio_playScene(int sceneIndex);
void audio_restartScene();
bool audio_isPlaying();
double audio_getDurationSeconds();
std::chrono::steady_clock::time_point audio_getStartTime();
// Auto-sun control (auto adjusts sun during audio playback)
void audio_setAutoSunEnabled(bool enable);
bool audio_isAutoSunEnabled();

// UI overlay (buttons) and mouse handler
void drawUIButtons(int windowW, int windowH);
void onMouseClick(int button, int state, int x, int y);
