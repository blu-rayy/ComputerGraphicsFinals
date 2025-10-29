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

// Seesaw animation
static float seesawAngle = 0.0f;
static float seesawDirection = 1.0f;

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

// Draw a seesaw (Japanese playground style)
static void drawSeesaw(float x, float y, float angle, float scale, float tGround) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glScalef(scale, scale, 1.0f);
    
    // Center support - vibrant cyan/turquoise metal
    float supportBase[3] = {0.1f, 0.75f, 0.95f};
    float supportMod = mixf(0.65f, 1.0f, tGround);
    float supportColor[3] = { supportBase[0] * supportMod, supportBase[1] * supportMod, supportBase[2] * supportMod };
    
    glNormal3f(0.0f, 0.0f, 1.0f);
    
    // Support base (wider trapezoid)
    glColor3f(supportColor[0], supportColor[1], supportColor[2]);
    glBegin(GL_QUADS);
        glVertex2f(-0.12f, 0.0f);
        glVertex2f(0.12f, 0.0f);
        glVertex2f(0.08f, 0.18f);
        glVertex2f(-0.08f, 0.18f);
    glEnd();
    
    // Support top cap (darker cyan with metallic look)
    float capColor[3] = { supportColor[0] * 0.75f, supportColor[1] * 0.75f, supportColor[2] * 0.85f };
    glColor3f(capColor[0], capColor[1], capColor[2]);
    glBegin(GL_QUADS);
        glVertex2f(-0.1f, 0.18f);
        glVertex2f(0.1f, 0.18f);
        glVertex2f(0.1f, 0.2f);
        glVertex2f(-0.1f, 0.2f);
    glEnd();
    
    // Rotating plank assembly
    glPushMatrix();
    glTranslatef(0.0f, 0.19f, 0.0f);
    glRotatef(angle, 0.0f, 0.0f, 1.0f);
    
    // Main plank - vivid yellow/gold
    float plankBase[3] = {1.0f, 0.9f, 0.05f};
    float plankMod = mixf(0.7f, 1.0f, tGround);
    float plankColor[3] = { plankBase[0] * plankMod, plankBase[1] * plankMod, plankBase[2] * plankMod };
    
    glColor3f(plankColor[0], plankColor[1], plankColor[2]);
    glBegin(GL_QUADS);
        glVertex2f(-0.35f, -0.03f);
        glVertex2f(0.35f, -0.03f);
        glVertex2f(0.35f, 0.03f);
        glVertex2f(-0.35f, 0.03f);
    glEnd();
    
    // Center grip/pivot area (orange accent)
    float gripBase[3] = {1.0f, 0.65f, 0.0f};
    float gripColor[3] = { gripBase[0] * plankMod, gripBase[1] * plankMod, gripBase[2] * plankMod };
    glColor3f(gripColor[0], gripColor[1], gripColor[2]);
    glBegin(GL_QUADS);
        glVertex2f(-0.08f, -0.03f);
        glVertex2f(0.08f, -0.03f);
        glVertex2f(0.08f, 0.03f);
        glVertex2f(-0.08f, 0.03f);
    glEnd();
    
    // Left seat - vivid red/magenta
    float leftSeatBase[3] = {1.0f, 0.1f, 0.35f};
    float seatMod = mixf(0.65f, 1.0f, tGround);
    float leftSeatColor[3] = { leftSeatBase[0] * seatMod, leftSeatBase[1] * seatMod, leftSeatBase[2] * seatMod };
    
    glColor3f(leftSeatColor[0], leftSeatColor[1], leftSeatColor[2]);
    glBegin(GL_QUADS);
        glVertex2f(-0.35f, 0.03f);
        glVertex2f(-0.22f, 0.03f);
        glVertex2f(-0.22f, 0.11f);
        glVertex2f(-0.35f, 0.11f);
    glEnd();
    
    // Left handle (red/magenta)
    glBegin(GL_QUADS);
        glVertex2f(-0.285f, 0.11f);
        glVertex2f(-0.275f, 0.11f);
        glVertex2f(-0.275f, 0.19f);
        glVertex2f(-0.285f, 0.19f);
    glEnd();
    
    // Right seat - bright lime green
    float rightSeatBase[3] = {0.2f, 0.95f, 0.25f};
    float rightSeatColor[3] = { rightSeatBase[0] * seatMod, rightSeatBase[1] * seatMod, rightSeatBase[2] * seatMod };
    
    glColor3f(rightSeatColor[0], rightSeatColor[1], rightSeatColor[2]);
    glBegin(GL_QUADS);
        glVertex2f(0.22f, 0.03f);
        glVertex2f(0.35f, 0.03f);
        glVertex2f(0.35f, 0.11f);
        glVertex2f(0.22f, 0.11f);
    glEnd();
    
    // Right handle (lime green)
    glBegin(GL_QUADS);
        glVertex2f(0.275f, 0.11f);
        glVertex2f(0.285f, 0.11f);
        glVertex2f(0.285f, 0.19f);
        glVertex2f(0.275f, 0.19f);
    glEnd();
    
    glPopMatrix();
    glPopMatrix();
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

void drawSlide() {
    const int SEGMENTS = 80;
    const float START_X = -0.3f;  
    const float START_Y = -0.10f; 
    const float END_X   = 0.1f;   
    const float END_Y   = -0.6f;  

    const float gap             = 0.055f; 
    const float leftRailWidth   = 0.032f; 
    const float rightRailWidth  = 0.042f; 
    const float outlineWidth    = 0.0045f;

    std::vector<float> cx(SEGMENTS + 1), cy(SEGMENTS + 1);
    std::vector<float> nx(SEGMENTS + 1), ny(SEGMENTS + 1);

    for (int i = 0; i <= SEGMENTS; ++i) {
        float t = (float)i / SEGMENTS;
        cx[i] = START_X + (END_X - START_X) * t;
        cy[i] = START_Y + (END_Y - START_Y) * t - (0.18f * sinf(t * M_PI));

        float dx_dt = (END_X - START_X);
        float dy_dt = (END_Y - START_Y) - 0.18f * (M_PI * cosf(t * M_PI));
        float nnx = -dy_dt, nny = dx_dt;
        float nlen = sqrtf(nnx*nnx + nny*nny);
        if (nlen == 0.0f) nlen = 1.0f;
        nx[i] = nnx / nlen; ny[i] = nny / nlen;
    }

    auto drawRail = [&](float innerDist, float outerDist, const float fillCol[3], int segMax) {
        glColor3f(fillCol[0], fillCol[1], fillCol[2]);
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= segMax; ++i) {
            float ox = cx[i] + nx[i] * outerDist;
            float oy = cy[i] + ny[i] * outerDist;
            float ix = cx[i] + nx[i] * innerDist;
            float iy = cy[i] + ny[i] * innerDist;
            glVertex2f(ox, oy);
            glVertex2f(ix, iy);
        }
        glEnd();

        const float midCol[3] = { (0.90f + 0.56f) * 0.5f, (0.90f + 0.56f) * 0.5f, (0.90f + 0.56f) * 0.5f }; // ~0.73 gray
        float delta = outerDist - innerDist;              
        float inset = fabsf(delta) * 0.20f;               
        bool leftSide = (outerDist > innerDist);          
        float stripeInner = leftSide ? innerDist + inset : innerDist - inset;
        float stripeOuter = leftSide ? outerDist - inset : outerDist + inset;
        glColor3f(midCol[0], midCol[1], midCol[2]);
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= segMax; ++i) {
            float ox = cx[i] + nx[i] * stripeOuter;
            float oy = cy[i] + ny[i] * stripeOuter;
            float ix = cx[i] + nx[i] * stripeInner;
            float iy = cy[i] + ny[i] * stripeInner;
            glVertex2f(ox, oy);
            glVertex2f(ix, iy);
        }
        glEnd();

        glLineWidth(1.0f);
        glColor3f(0.18f, 0.18f, 0.18f);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= segMax; ++i) {
            float ox = cx[i] + nx[i] * outerDist;
            float oy = cy[i] + ny[i] * outerDist;
            glVertex2f(ox, oy);
        }
        glEnd();
    };

    float leftInner  =  gap * 0.5f;                 
    float leftOuter  =  gap * 0.5f + leftRailWidth;   
    float rightInner = -gap * 0.5f;                  
    float rightOuter = -gap * 0.5f - rightRailWidth;  

    const float leftCol[3]  = {0.56f, 0.56f, 0.56f};
    const float rightCol[3] = {0.90f, 0.90f, 0.90f};

    float leftLenFrac = 1.0f;  
    if (leftLenFrac < 0.05f) leftLenFrac = 0.05f;
    int leftSegMax = (int)std::floor(SEGMENTS * leftLenFrac);
    if (leftSegMax < 2) leftSegMax = 2;

    float rightLenFrac = 0.82f; 
    if (rightLenFrac < 0.05f) rightLenFrac = 0.05f; 
    int rightSegMax = (int)std::floor(SEGMENTS * rightLenFrac);
    if (rightSegMax < 2) rightSegMax = 2;

    drawRail(leftInner,  leftOuter,  leftCol, leftSegMax);
    drawRail(rightInner, rightOuter, rightCol, rightSegMax);

    const float midColCenter[3] = { (0.90f + 0.56f) * 0.5f, (0.90f + 0.56f) * 0.5f, (0.90f + 0.56f) * 0.5f }; // ~0.73 gray
    float leaveGap = 0.03f; 
    int centerStartIdx = 0;
    for (int i = 0; i <= SEGMENTS; ++i) {
        if (cy[i] <= (START_Y - leaveGap)) { centerStartIdx = i; break; }
    }

    int centerEndIdx = (leftSegMax < rightSegMax) ? leftSegMax : rightSegMax;
    if (centerStartIdx < centerEndIdx) {
        glColor3f(midColCenter[0], midColCenter[1], midColCenter[2]);
        glBegin(GL_QUAD_STRIP);
        for (int i = centerStartIdx; i <= centerEndIdx; ++i) {
            float lix = cx[i] + nx[i] * leftInner;
            float liy = cy[i] + ny[i] * leftInner;
            float rix = cx[i] + nx[i] * rightInner;
            float riy = cy[i] + ny[i] * rightInner;
            glVertex2f(lix, liy);
            glVertex2f(rix, riy);
        }
        glEnd();
    }

    if (leftSegMax != rightSegMax) {
        int iShort = (leftSegMax < rightSegMax) ? leftSegMax : rightSegMax;
        int iLong  = (leftSegMax > rightSegMax) ? leftSegMax : rightSegMax;
        glColor3f(midColCenter[0], midColCenter[1], midColCenter[2]);
        glBegin(GL_TRIANGLE_FAN);
        if (leftSegMax > rightSegMax) {
            float ax = cx[iShort] + nx[iShort] * rightInner;
            float ay = cy[iShort] + ny[iShort] * rightInner;
            glVertex2f(ax, ay);
            for (int i = iShort; i <= leftSegMax; ++i) {
                float lx = cx[i] + nx[i] * leftInner;
                float ly = cy[i] + ny[i] * leftInner;
                glVertex2f(lx, ly);
            }
        } else {
            float ax = cx[iShort] + nx[iShort] * leftInner;
            float ay = cy[iShort] + ny[iShort] * leftInner;
            glVertex2f(ax, ay);
            for (int i = iShort; i <= rightSegMax; ++i) {
                float rx = cx[i] + nx[i] * rightInner;
                float ry = cy[i] + ny[i] * rightInner;
                glVertex2f(rx, ry);
            }
        }
        glEnd();
    }
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
    glColor3f(0.8, 0.5, 0.1);
    glBegin(GL_POLYGON);
    glVertex2f(-0.7, 0.5);
    glVertex2f(-0.45, 0.8);
    glVertex2f(-0.4, 0.8);
    glVertex2f(-0.65, 0.5);
    glEnd();

    // roof mid
    glColor3f(0.8, 0.5, 0.1);
    glBegin(GL_POLYGON);
    glVertex2f(-0.44, 0.8);
    glVertex2f(-0.4, 0.8);
    glVertex2f(-0.4, 0.5);
    glVertex2f(-0.44, 0.5);
    glEnd();

    // roof right
    glColor3f(0.8, 0.5, 0.1);
    glBegin(GL_POLYGON);
    glVertex2f(-0.44, 0.8);
    glVertex2f(-0.4, 0.8);
    glVertex2f(-0.15, 0.5);
    glVertex2f(-0.2, 0.5);
    glEnd();

    // ground
    glColor3f(1.0, 1.0, 1.0);
    glBegin(GL_POLYGON);
    glVertex2f(-1.0, -0.6);
    glVertex2f(1.0, -0.6);
    glVertex2f(1.0, -1.0);
    glVertex2f(-1.0, -1.0);
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
    glLineWidth(15.0);
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
    glLineWidth(15.0);
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
    glLineWidth(15.0);
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
    glLineWidth(13.0);
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
    glLineWidth(13.0);
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
    glColor3f(0.6, 0.25, 0.1);
    glLineWidth(10.0);
    glBegin(GL_LINES);
    glVertex2f(-0.65, 0.17);
    glVertex2f(-0.43, 0.13);
    glEnd();

    glColor3f(0.6, 0.25, 0.1);
    glLineWidth(10.0);
    glBegin(GL_LINES);
    glVertex2f(-0.65, -0.17);
    glVertex2f(-0.43, -0.17);
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

    // foundation outside left fence
    glColor3f(0.55, 0.27, 0.07);
    glBegin(GL_POLYGON);
    glVertex2f(-0.85, 0.2);
    glVertex2f(-0.8, 0.2);
    glVertex2f(-0.8, -0.65);
    glVertex2f(-0.85, -0.65);
    glEnd();

    // foundation left
    glColor3f(0.55, 0.27, 0.07);
    glBegin(GL_POLYGON);
    glVertex2f(-0.69, 0.5);
    glVertex2f(-0.64, 0.5);
    glVertex2f(-0.64, -0.2);
    glVertex2f(-0.69, -0.2);
    glEnd();

    // foundation mid
    glColor3f(0.55, 0.27, 0.07);
    glBegin(GL_POLYGON);
    glVertex2f(-0.45, 0.5);
    glVertex2f(-0.4, 0.5);
    glVertex2f(-0.4, -0.65);
    glVertex2f(-0.45, -0.65);
    glEnd();

    // foundation right
    glColor3f(0.55, 0.27, 0.07);
    glBegin(GL_POLYGON);
    glVertex2f(-0.21, 0.5);
    glVertex2f(-0.16, 0.5);
    glVertex2f(-0.16, -0.62);
    glVertex2f(-0.21, -0.62);
    glEnd();

    // floor line
    glColor3f(0.55, 0.27, 0.07);
    glLineWidth(3.0);
    glBegin(GL_LINES);
    glVertex2f(-0.69, -0.2);
    glVertex2f(-0.16, -0.2);
    glEnd();

    // climbing ramp
    glColor3f(0.8, 0.5, 0.1);
    glBegin(GL_POLYGON);
    glVertex2f(-0.69, -0.2);
    glVertex2f(-0.43, -0.2);
    glVertex2f(-0.62, -0.7);
    glVertex2f(-0.9, -0.65);
    glEnd();

    // slide floor
    glColor3f(0.7f, 0.7f, 0.7f);
    glBegin(GL_POLYGON);
    glVertex2f(0.02, -0.63);
    glVertex2f(0.095, -0.57);
    glVertex2f(0.095, -0.62);
    glVertex2f(0.02, -0.68);
    glEnd();
    
    drawSlide();

    glPopAttrib();

    // Draw seesaw (positioned to the right of the slide, scaled down for depth)
    drawSeesaw(0.5f, -0.5f, seesawAngle, 0.7f, tGround);

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

