#include <windows.h>
#include <cstdio>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <cmath>
#include <cstdlib>

#include "utils.h"

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

    // draw ground 
    glBindBuffer(GL_ARRAY_BUFFER, vboHandles[0]);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, nullptr);
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