#include <windows.h>
#include <cstdio>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <cmath>
#include <vector>
#include <cstdlib>

#include "utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* BUILD COMMAND

 g++ src/main.cpp src/utils.cpp -Ilibs/include -Ilibs/freeglut/include -Llibs/freeglut/lib/x64 -Llibs -lglew32 -lfreeglut -lopengl32 -lglu32 -o app.exe

*/

// Sun elevation in normalized device coords Y (-1 bottom .. +1 top)
static float sunElevation = -0.2f; // default: sunset (orangey)
static bool sunVisible = true;     // when false, force midday sky and hide sun

// vertex data (source for VBOs)
static const GLfloat groundVerts[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
     1.0f, -0.4f,
    -1.0f, -0.4f
};

static const GLfloat trapVerts[] = {
    -0.75f, -0.4f,
     0.75f, -0.4f,
     0.55f, -0.85f,
    -0.55f, -0.85f
};

static GLuint vboHandles[2] = {0, 0}; // 0: ground, 1: trap
static GLuint vaoHandles[2] = {0, 0}; // 0: ground, 1: trap
static bool haveVBO = false;
// current window aspect (width/height). Updated in reshape callback.
static float winAspect = 1.0f;

// Create VBOs and VAOs. Must be called after an OpenGL context exists and after glewInit().
static void createResources() {
    if (haveVBO || vboHandles[0] != 0) return; // already attempted

    // Check for VBO support
    if (!(GLEW_ARB_vertex_buffer_object || GLEW_VERSION_1_5)) {
        std::fprintf(stderr, "ERROR: VBOs not supported by this OpenGL implementation. Exiting.\n");
        std::exit(1);
    }

    // Create VBOs
    glGenBuffers(2, vboHandles);

    glBindBuffer(GL_ARRAY_BUFFER, vboHandles[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVerts), groundVerts, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, vboHandles[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(trapVerts), trapVerts, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Create VAOs if supported
    if (GLEW_ARB_vertex_array_object || GLEW_VERSION_3_0) {
        glGenVertexArrays(2, vaoHandles);

        // ground
        glBindVertexArray(vaoHandles[0]);
        glBindBuffer(GL_ARRAY_BUFFER, vboHandles[0]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // trapezoid
        glBindVertexArray(vaoHandles[1]);
        glBindBuffer(GL_ARRAY_BUFFER, vboHandles[1]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    } else {
        // If VAOs aren't available, we'll still use VBOs and set attribute pointers per-draw.
        vaoHandles[0] = vaoHandles[1] = 0;
    }

    haveVBO = true;
}

// Utility functions for colors and circle drawing are in utils.cpp

// Draw a simple tree using circles for foliage and a rectangle trunk
static void drawTree(float x, float y, float scale, float tGround) {
    // trunk -- darken slightly at night
    float trunkBase[3] = {0.55f, 0.27f, 0.07f};
    float trunkMod = mixf(0.6f, 1.0f, tGround); // darker at night
    float trunk[3] = { trunkBase[0] * trunkMod, trunkBase[1] * trunkMod, trunkBase[2] * trunkMod };
    // set normal for trunk quad so lighting behaves predictably
    glNormal3f(0.0f, 0.0f, 1.0f);
    glColor3f(trunk[0], trunk[1], trunk[2]);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.04f * scale, y);
        glVertex2f(x + 0.04f * scale, y);
        glVertex2f(x + 0.04f * scale, y + 0.35f * scale);
        glVertex2f(x - 0.04f * scale, y + 0.35f * scale);
    glEnd();

    // foliage colors (base), we'll modulate by tGround so they dim at night
    float darkBase[3] = {0.13f, 0.55f, 0.13f};
    float midBase[3]  = {0.18f, 0.60f, 0.18f};
    float lightBase[3]= {0.22f, 0.66f, 0.22f};

    float foliageFactor = mixf(0.5f, 1.0f, tGround);
    float dark[3] = { darkBase[0] * foliageFactor, darkBase[1] * foliageFactor, darkBase[2] * foliageFactor };
    float mid[3]  = { midBase[0]  * foliageFactor, midBase[1]  * foliageFactor, midBase[2]  * foliageFactor };
    float light[3]= { lightBase[0]* foliageFactor, lightBase[1]* foliageFactor, lightBase[2]* foliageFactor };

    drawCircle(x, y + 0.40f * scale, 0.12f * scale, dark);
    drawCircle(x - 0.10f * scale, y + 0.55f * scale, 0.12f * scale, dark);
    drawCircle(x + 0.10f * scale, y + 0.55f * scale, 0.12f * scale, dark);
    drawCircle(x - 0.08f * scale, y + 0.70f * scale, 0.13f * scale, mid);
    drawCircle(x + 0.08f * scale, y + 0.70f * scale, 0.13f * scale, mid);
    drawCircle(x, y + 0.85f * scale, 0.14f * scale, light);
    drawCircle(x, y + 1.00f * scale, 0.12f * scale, light);
}

// compute sky colors based on sunElevation
static void computeSkyColors(float e, float top[3], float bottom[3]) {
    const float nightTop[3]   = {0.02f, 0.04f, 0.20f};
    const float nightBottom[3]= {0.01f, 0.01f, 0.05f};

    const float sunsetTop[3]  = {0.90f, 0.45f, 0.20f};
    const float sunsetBottom[3]={0.95f, 0.60f, 0.30f};

    const float dayTop[3]     = {0.18f, 0.68f, 0.98f}; 
    const float dayBottom[3]  = {0.55f, 0.85f, 1.00f}; 

    // 'e' is provided by caller (-1 .. +1).  If e>=0: sunset->day, else night->sunset
    if (e >= 0.0f) {
        const float centerBias = 0.40f;
        float t = centerBias + (1.0f - centerBias) * mixf(0.0f, 1.0f, e);
        mixColor(sunsetTop, dayTop, t, top);
        mixColor(sunsetBottom, dayBottom, t, bottom);
    } else {
        float t = mixf(0.0f, 1.0f, e + 1.0f);
        mixColor(nightTop, sunsetTop, t, top);
        mixColor(nightBottom, sunsetBottom, t, bottom);
    }
}

static void drawSkyGradient() {
    float top[3], bottom[3];
    // if the sun is hidden, force full daytime sky (midday)
    float e_for_sky = sunVisible ? sunElevation : 1.0f;
    computeSkyColors(e_for_sky, top, bottom);

    glBegin(GL_QUADS);
        glColor3f(top[0], top[1], top[2]);    glVertex2f(-1.0f, 1.0f);
        glColor3f(top[0], top[1], top[2]);    glVertex2f( 1.0f, 1.0f);
        glColor3f(bottom[0], bottom[1], bottom[2]); glVertex2f( 1.0f, -1.0f);
        glColor3f(bottom[0], bottom[1], bottom[2]); glVertex2f(-1.0f, -1.0f);
    glEnd();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    drawSkyGradient();

    float sunColor[3];
    if (sunElevation >= 0.0f) {
        sunColor[0] = 1.0f; sunColor[1] = 0.95f; sunColor[2] = 0.6f;
    } else {
        float t = (sunElevation + 1.0f); 
        float nightish[3] = {0.9f, 0.9f, 1.0f};
        float orange[3]   = {1.0f, 0.6f, 0.0f};
        mixColor(nightish, orange, t, sunColor);
    }

    float sunX = 0.0f;
    float sunY = sunElevation;
    // slightly larger sun: increase min/max radius
    float radius = mixf(0.04f, 0.14f, (sunElevation + 1.0f) / 2.0f); // sun smaller at night

    if (sunVisible) {
        glPushMatrix();
        glTranslatef(sunX, sunY, 0.0f);
        float sx = 1.0f, sy = 1.0f;
        if (winAspect >= 1.0f) {
            sx = 1.0f / winAspect;
        } else {
            sy = winAspect;
        }
        glScalef(sx, sy, 1.0f);
        drawCircle(0.0f, 0.0f, radius, sunColor);
        glPopMatrix();
    }

    // ground darkens at night
    float groundTopTint[3];
    float gt_day[3] = {0.8f, 0.8f, 0.8f};
    float gt_night[3] = {0.12f, 0.12f, 0.15f};

    float tGround;
    if (!sunVisible) {
        tGround = 1.0f;
    } else if (sunElevation >= 0.0f) {
        const float centerBias = 0.40f; 
        tGround = centerBias + (1.0f - centerBias) * mixf(0.0f, 1.0f, sunElevation);
    } else {
        tGround = (sunElevation + 1.0f) / 2.0f;
    }
    mixColor(gt_night, gt_day, tGround, groundTopTint);

    // setup lighting centered on sun (dynamic intensity + range based on elevation)
    if (sunVisible) {
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // normalize elevation to [0..1] (0 = lowest, 1 = highest)
    float e_norm = (sunElevation + 1.0f) * 0.5f;
    // soften curve so midday gets proportionally brighter
    float e_smooth = powf(e_norm, 0.6f);

    // intensity stronger during day, but tone down the midday peak
    float intensity = mixf(0.15f, 2.5f, e_smooth);

    // warm but not overwhelmingly so at noon
    float warmColor[3] = {1.0f, 0.55f, 0.18f};
    float finalColor[3];
    // warmFactor: bias toward warmth at midday but cap below 1.0
    float warmFactor = mixf(0.15f, 0.85f, e_smooth);
    mixColor(sunColor, warmColor, warmFactor, finalColor);

        float lightDiffuse[4]  = { finalColor[0] * intensity, finalColor[1] * intensity, finalColor[2] * intensity, 1.0f };
    // ambient slightly higher at midday but reduced from previous extremes
    float lightAmbient[4]  = { finalColor[0] * (0.06f + 0.32f * e_smooth), finalColor[1] * (0.06f + 0.32f * e_smooth), finalColor[2] * (0.06f + 0.32f * e_smooth), 1.0f };
    float specMul = mixf(0.3f, 0.8f, e_smooth);
    float lightSpecular[4] = { specMul, specMul, specMul, 1.0f };

        glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
        glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
        glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);

        // attenuation: decrease attenuation when sun is higher (wider radius)
    // attenuation: much less attenuation at midday so light reaches farther
    // increase attenuation at midday so the scene doesn't get overly washed
    float linearAtt  = mixf(0.9f, 0.08f, e_smooth);
    float quadAtt    = mixf(0.9f, 0.02f, e_smooth);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, linearAtt);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, quadAtt);

    // stronger specular highlight / shininess at midday
    float matSpec[4] = { mixf(0.2f, 0.8f, e_smooth), mixf(0.2f, 0.8f, e_smooth), mixf(0.2f, 0.8f, e_smooth), 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mixf(12.0f, 48.0f, e_smooth));

        // Position the light at the sun's world-space location (slightly in front)
    // position the light further above the scene so midday illumination covers more
    // position the light slightly closer to the scene to reduce overall wash at noon
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(sunX, sunY, 0.6f);
    float lightPos[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glPopMatrix();
    } else {
        glDisable(GL_LIGHT0);
        glDisable(GL_LIGHTING);
        glDisable(GL_COLOR_MATERIAL);
    }

    // draw ground 
    glBindBuffer(GL_ARRAY_BUFFER, vboHandles[0]);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, nullptr);
    // set normal so lighting affects this plane
    glNormal3f(0.0f, 0.0f, 1.0f);
    glColor3f(groundTopTint[0], groundTopTint[1], groundTopTint[2]);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);

    // muted ground at night
    float rubberColor[3] = {0.6f, 0.18f, 0.18f};
    rubberColor[0] *= mixf(0.5f, 1.0f, tGround);
    rubberColor[1] *= mixf(0.5f, 1.0f, tGround);
    rubberColor[2] *= mixf(0.5f, 1.0f, tGround);

    // Draw rubber trapezoid 
    glBindBuffer(GL_ARRAY_BUFFER, vboHandles[1]);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, nullptr);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glColor3f(rubberColor[0], rubberColor[1], rubberColor[2]);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);

    

    // Draw trees left and right
    drawTree(-0.85f, -0.4f, 1.0f, tGround);
    drawTree(-0.65f, -0.5f, 0.9f, tGround);
    drawTree(-0.45f, -0.45f, 1.1f, tGround);
    drawTree(0.45f, -0.45f, 1.0f, tGround);
    drawTree(0.65f, -0.5f, 0.95f, tGround);
    drawTree(0.85f, -0.4f, 1.05f, tGround);

    // Draw playground structure (in front of trees)
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);

    // roof left
    glColor3f(0.8f, 0.5f, 0.1f);
    glBegin(GL_POLYGON);
    glVertex2f(-0.7f, 0.5f);
    glVertex2f(-0.45f, 0.8f);
    glVertex2f(-0.4f, 0.8f);
    glVertex2f(-0.65f, 0.5f);
    glEnd();

    // roof mid
    glColor3f(0.8f, 0.5f, 0.1f);
    glBegin(GL_POLYGON);
    glVertex2f(-0.44f, 0.8f);
    glVertex2f(-0.4f, 0.8f);
    glVertex2f(-0.4f, 0.5f);
    glVertex2f(-0.44f, 0.5f);
    glEnd();

    // roof right
    glColor3f(0.8f, 0.5f, 0.1f);
    glBegin(GL_POLYGON);
    glVertex2f(-0.44f, 0.8f);
    glVertex2f(-0.4f, 0.8f);
    glVertex2f(-0.15f, 0.5f);
    glVertex2f(-0.2f, 0.5f);
    glEnd();

    //inside fence 
    glColor3f(0.7, 0.3, 0.1);
    glBegin(GL_POLYGON);
    glVertex2f(-0.65, 0.2);
    glVertex2f(-0.43, 0.17);
    glVertex2f(-0.43, -0.2);
    glVertex2f(-0.65, -0.2);
    glEnd();

    //inside fence lines
    glColor3f(0.6, 0.27, 0.1);
    glLineWidth(28.0);
    glBegin(GL_LINES);
    glVertex2f(-0.63, 0.19);
    glVertex2f(-0.63, -0.2);
    glEnd();
    glColor3f(0.8, 0.5, 0.1);
    glLineWidth(3.0);
    glBegin(GL_LINES);
    glVertex2f(-0.62, 0.19);
    glVertex2f(-0.62, -0.2);
    glEnd();

    glColor3f(0.6, 0.27, 0.1);
    glLineWidth(28.0);
    glBegin(GL_LINES);
    glVertex2f(-0.59, 0.19);
    glVertex2f(-0.59, -0.2);
    glEnd();
    glColor3f(0.8, 0.5, 0.1);
    glLineWidth(3.0);
    glBegin(GL_LINES);
    glVertex2f(-0.58, 0.19);
    glVertex2f(-0.58, -0.2);
    glEnd();

    glColor3f(0.6, 0.27, 0.1);
    glLineWidth(28.0);
    glBegin(GL_LINES);
    glVertex2f(-0.55, 0.18);
    glVertex2f(-0.55, -0.2);
    glEnd();
    glColor3f(0.8, 0.5, 0.1);
    glLineWidth(3.0);
    glBegin(GL_LINES);
    glVertex2f(-0.54, 0.18);
    glVertex2f(-0.54, -0.2);
    glEnd();

    glColor3f(0.6, 0.27, 0.1);
    glLineWidth(28.0);
    glBegin(GL_LINES);
    glVertex2f(-0.51, 0.18);
    glVertex2f(-0.51, -0.2);
    glEnd();
    glColor3f(0.8, 0.5, 0.1);
    glLineWidth(3.0);
    glBegin(GL_LINES);
    glVertex2f(-0.50, 0.18);
    glVertex2f(-0.50, -0.2);
    glEnd();

    glColor3f(0.6, 0.27, 0.1);
    glLineWidth(28.0);
    glBegin(GL_LINES);
    glVertex2f(-0.475, 0.17);
    glVertex2f(-0.475, -0.2);
    glEnd();
    glColor3f(0.8, 0.5, 0.1);
    glLineWidth(3.0);
    glBegin(GL_LINES);
    glVertex2f(-0.465, 0.17);
    glVertex2f(-0.465, -0.2);
    glEnd();

    //inside fence horizontal lines
    glColor3f(0.5, 0.25, 0.1);
    glLineWidth(10.0);
    glBegin(GL_LINES);
    glVertex2f(-0.65, 0.19);
    glVertex2f(-0.43, 0.16);
    glEnd();
    glColor3f(0.5, 0.25, 0.1);
    glLineWidth(10.0);
    glBegin(GL_LINES);
    glVertex2f(-0.65, -0.16);
    glVertex2f(-0.43, -0.16);
    glEnd();

    //outside right fence
    glColor3f(0.7, 0.3, 0.1);
    glBegin(GL_POLYGON);
    glVertex2f(-0.16, 0.0);
    glVertex2f(0.03, 0.0);
    glVertex2f(0.03, -0.49);
    glVertex2f(-0.16, -0.49);
    glEnd();

    glColor3f(0.6, 0.27, 0.1);
    glLineWidth(28.0);
    glBegin(GL_LINES);
    glVertex2f(-0.14, 0.0);
    glVertex2f(-0.14, -0.49);
    glEnd();
    glColor3f(0.8, 0.5, 0.1);
    glLineWidth(3.0);
    glBegin(GL_LINES);
    glVertex2f(-0.13, 0.0);
    glVertex2f(-0.13, -0.49);
    glEnd();

    glColor3f(0.6, 0.27, 0.1);
    glLineWidth(28.0);
    glBegin(GL_LINES);
    glVertex2f(-0.1, 0.0);
    glVertex2f(-0.1, -0.49);
    glEnd();
    glColor3f(0.8, 0.5, 0.1);
    glLineWidth(3.0);
    glBegin(GL_LINES);
    glVertex2f(-0.09, 0.0);
    glVertex2f(-0.09, -0.49);
    glEnd();

    glColor3f(0.6, 0.27, 0.1);
    glLineWidth(28.0);
    glBegin(GL_LINES);
    glVertex2f(-0.06, 0.0);
    glVertex2f(-0.06, -0.49);
    glEnd();
    glColor3f(0.8, 0.5, 0.1);
    glLineWidth(3.0);
    glBegin(GL_LINES);
    glVertex2f(-0.05, 0.0);
    glVertex2f(-0.05, -0.49);
    glEnd();

    glColor3f(0.6, 0.27, 0.1);
    glLineWidth(28.0);
    glBegin(GL_LINES);
    glVertex2f(-0.02, 0.0);
    glVertex2f(-0.02, -0.49);
    glEnd();
    glColor3f(0.8, 0.5, 0.1);
    glLineWidth(3.0);
    glBegin(GL_LINES);
    glVertex2f(-0.01, 0.0);
    glVertex2f(-0.01, -0.49);
    glEnd();

    glColor3f(0.6, 0.27, 0.1);
    glLineWidth(28.0);
    glBegin(GL_LINES);
    glVertex2f(0.02, 0.0);
    glVertex2f(0.02, -0.49);
    glEnd();
    glColor3f(0.8, 0.5, 0.1);
    glLineWidth(3.0);
    glBegin(GL_LINES);
    glVertex2f(0.03, 0.0);
    glVertex2f(0.03, -0.49);
    glEnd();

    // foundation outside right fence
    glColor3f(0.55, 0.27, 0.07);
    glBegin(GL_POLYGON);
    glVertex2f(0.03, 0.08);
    glVertex2f(0.08, 0.08);
    glVertex2f(0.08, -0.62);
    glVertex2f(0.03, -0.62);
    glEnd();

    // foundation left
    glColor3f(0.55f, 0.27f, 0.07f);
    glBegin(GL_POLYGON);
    glVertex2f(-0.69f, 0.5f);
    glVertex2f(-0.64f, 0.5f);
    glVertex2f(-0.64f, -0.2f);
    glVertex2f(-0.69f, -0.2f);
    glEnd();

    // foundation mid
    glColor3f(0.55f, 0.27f, 0.07f);
    glBegin(GL_POLYGON);
    glVertex2f(-0.45f, 0.5f);
    glVertex2f(-0.4f, 0.5f);
    glVertex2f(-0.4f, -0.65f);
    glVertex2f(-0.45f, -0.65f);
    glEnd();

    // foundation right
    glColor3f(0.55f, 0.27f, 0.07f);
    glBegin(GL_POLYGON);
    glVertex2f(-0.21f, 0.5f);
    glVertex2f(-0.16f, 0.5f);
    glVertex2f(-0.16f, -0.62f);
    glVertex2f(-0.21f, -0.62f);
    glEnd();

    // floor line
    glColor3f(0.55f, 0.27f, 0.07f);
    glLineWidth(30.0f);
    glBegin(GL_LINES);
    glVertex2f(-0.69f, -0.2f);
    glVertex2f(-0.16f, -0.2f);
    glEnd();

    // climbing ramp
    glColor3f(0.8f, 0.5f, 0.1f);
    glBegin(GL_POLYGON);
    glVertex2f(-0.69f, -0.2f);
    glVertex2f(-0.43f, -0.2f);
    glVertex2f(-0.62f, -0.7f);
    glVertex2f(-0.9f, -0.65f);
    glEnd();

    glPopAttrib();

    glFlush();
}

// up and down arrow to move sun
static void specialKeys(int key, int x, int y) {
    (void)x; (void)y;
    const float step = 0.06f;
    if (key == GLUT_KEY_UP) {
        if (sunElevation < 1.0f) {
            sunElevation += step;
            if (sunElevation > 1.0f) sunElevation = 1.0f;
            sunVisible = true;
        } else {
            sunVisible = false;
        }
    } else if (key == GLUT_KEY_DOWN) {
        if (!sunVisible) {
            sunVisible = true;
            sunElevation = 1.0f - step;
        } else {
            sunElevation -= step;
            if (sunElevation < -1.0f) sunElevation = -1.0f;
        }
    }
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
    glutInitWindowSize(1920, 1080);
    glutCreateWindow("Scene 1 - Sky & Sun");
    
    glewExperimental = GL_TRUE; 
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        std::fprintf(stderr, "GLEW initialization Failed: %s\n", glewGetErrorString(glewErr));
        return 1;
    }
    // Create VBO/VAO resource
    createResources();
    glutDisplayFunc(display);
    glutSpecialFunc(specialKeys);
    glutReshapeFunc([](int w, int h){
        if (h == 0) h = 1;
        winAspect = (float)w / (float)h;
        glViewport(0,0,w,h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        // Keep a fixed logical coordinate system (-1..1 both axes);
        // we'll correct the sun drawing separately so circles stay circular.
        glOrtho(-1, 1, -1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    });

    glutMainLoop();
    return 0;
}

