#include <GL/freeglut.h>
#include <cmath>

// BUILD COMMAND:
// g++ src/main.cpp -Ilibs/freeglut/include -Llibs/freeglut/lib/x64 -lfreeglut -lopengl32 -lglu32 -o app.exe

// Sun elevation in normalized device coords Y (-1 bottom .. +1 top)
static float sunElevation = -0.2f; // default: sunset (orangey)
static bool sunVisible = true;     // when false, force midday sky and hide sun

// Utility mix for floats
static float mixf(float a, float b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return a + (b - a) * t;
}

// Utility mix for RGB
static void mixColor(const float a[3], const float b[3], float t, float out[3]) {
    for (int i = 0; i < 3; ++i) out[i] = mixf(a[i], b[i], t);
}

// Draw a filled circle (triangle fan)
static void drawCircle(float cx, float cy, float radius, const float color[3]) {
    glColor3fv(color);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    const int segs = 48;
    for (int i = 0; i <= segs; ++i) {
        float a = (float)i / (float)segs * 2.0f * 3.14159265f;
        glVertex2f(cx + cosf(a) * radius, cy + sinf(a) * radius);
    }
    glEnd();
}

// Draw a simple tree using circles for foliage and a rectangle trunk
static void drawTree(float x, float y, float scale) {
    // trunk
    glColor3f(0.55f, 0.27f, 0.07f);
    glBegin(GL_QUADS);
        glVertex2f(x - 0.04f * scale, y);
        glVertex2f(x + 0.04f * scale, y);
        glVertex2f(x + 0.04f * scale, y + 0.35f * scale);
        glVertex2f(x - 0.04f * scale, y + 0.35f * scale);
    glEnd();

    float dark[3] = {0.13f, 0.55f, 0.13f};
    float mid[3]  = {0.18f, 0.60f, 0.18f};
    float light[3]= {0.22f, 0.66f, 0.22f};

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
    float radius = mixf(0.03f, 0.12f, (sunElevation + 1.0f) / 2.0f); // sun smaller at night

    if (sunVisible) drawCircle(sunX, sunY, radius, sunColor);

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

    glBegin(GL_QUADS);
        glColor3f(groundTopTint[0], groundTopTint[1], groundTopTint[2]);
        glVertex2f(-1.0f, -1.0f);
        glVertex2f( 1.0f, -1.0f);
        glVertex2f( 1.0f, -0.4f);
        glVertex2f(-1.0f, -0.4f);
    glEnd();

    // muted ground at night
    float rubberColor[3] = {0.6f, 0.18f, 0.18f};
    rubberColor[0] *= mixf(0.5f, 1.0f, tGround);
    rubberColor[1] *= mixf(0.5f, 1.0f, tGround);
    rubberColor[2] *= mixf(0.5f, 1.0f, tGround);

    glColor3f(rubberColor[0], rubberColor[1], rubberColor[2]);
    glBegin(GL_POLYGON);
        glVertex2f(-0.75f, -0.4f);
        glVertex2f( 0.75f, -0.4f);
        glVertex2f( 0.55f, -0.85f);
        glVertex2f(-0.55f, -0.85f);
    glEnd();

    // Draw trees left and right
    drawTree(-0.85f, -0.4f, 1.0f);
    drawTree(-0.65f, -0.5f, 0.9f);
    drawTree(-0.45f, -0.45f, 1.1f);
    drawTree(0.45f, -0.45f, 1.0f);
    drawTree(0.65f, -0.5f, 0.95f);
    drawTree(0.85f, -0.4f, 1.05f);

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

    glutDisplayFunc(display);
    glutSpecialFunc(specialKeys);
    glutReshapeFunc([](int w, int h){
        glViewport(0,0,w,h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1, 1, -1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    });

    glutMainLoop();
    return 0;
}
