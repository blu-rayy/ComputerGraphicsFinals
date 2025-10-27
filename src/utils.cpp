#include "utils.h"
#include <cmath>

/* BUILD COMMAND

g++ src/main.cpp src/utils.cpp -Ilibs/include -Ilibs/freeglut/include -Llibs/freeglut/lib/x64 -Llibs -lglew32 -lfreeglut -lopengl32 -lglu32 -o app.exe

*/

float mixf(float a, float b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return a + (b - a) * t;
}

void mixColor(const float a[3], const float b[3], float t, float out[3]) {
    for (int i = 0; i < 3; ++i) out[i] = mixf(a[i], b[i], t);
}

void drawCircle(float cx, float cy, float radius, const float color[3]) {
    glColor3fv(color);
    // 2D geometry lies on XY plane; set normal so fixed-function lighting affects it
    glNormal3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    const int segs = 48;
    for (int i = 0; i <= segs; ++i) {
        float a = (float)i / (float)segs * 2.0f * 3.14159265f;
        glVertex2f(cx + cosf(a) * radius, cy + sinf(a) * radius);
    }
    glEnd();
}
