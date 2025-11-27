#include <windows.h>
#include <cstdio>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* BUILD COMMAND
g++ src/cake.cpp -Ilibs/include -Ilibs/freeglut/include -Llibs/freeglut/lib/x64 -Llibs -lglew32 -lfreeglut -lopengl32 -lglu32 -o cake.exe
*/

// Camera parameters - Fixed anime-style perspective
static float cameraAngleY = 25.0f;  // Rotation around Y-axis
static float cameraAngleX = 25.0f;  // Tilt angle
static float cameraDistance = 4.2f;

// Animation disabled for static scene
static float rotationAngle = 0.0f;
static bool autoRotate = false;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Draw a solid cylinder (for various cake/ribbon parts)
void drawCylinder(float radius, float height, int slices) {
    GLUquadric* quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluQuadricTexture(quad, GL_TRUE);
    
    glPushMatrix();
    gluCylinder(quad, radius, radius, height, slices, 1);
    
    // Top cap
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, height);
    gluDisk(quad, 0.0, radius, slices, 1);
    glPopMatrix();
    
    // Bottom cap
    glPushMatrix();
    glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
    gluDisk(quad, 0.0, radius, slices, 1);
    glPopMatrix();
    
    glPopMatrix();
    gluDeleteQuadric(quad);
}

// Draw a sphere (for flower centers, decorations)
void drawSphere(float radius, int slices, int stacks) {
    GLUquadric* quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluSphere(quad, radius, slices, stacks);
    gluDeleteQuadric(quad);
}

// Draw a petal using triangles (for flowers)
void drawPetal(float length, float width) {
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);  // Center
    
    int segments = 12;
    for (int i = 0; i <= segments; i++) {
        float angle = (M_PI * i) / segments - M_PI / 2.0f;
        float x = length * cosf(angle);
        float y = width * sinf(angle);
        glVertex3f(x, y, 0.0f);
    }
    glEnd();
}

// ============================================================================
// SCENE COMPONENTS
// ============================================================================

// Draw wooden table surface
void drawTable() {
    glPushMatrix();
    
    // Wood material - warm brown
    GLfloat wood_ambient[] = {0.35f, 0.25f, 0.18f, 1.0f};
    GLfloat wood_diffuse[] = {0.62f, 0.45f, 0.32f, 1.0f};
    GLfloat wood_specular[] = {0.15f, 0.12f, 0.10f, 1.0f};
    GLfloat wood_shininess = 8.0f;
    
    glMaterialfv(GL_FRONT, GL_AMBIENT, wood_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, wood_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, wood_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, wood_shininess);
    
    // Large flat table surface
    glTranslatef(0.0f, -0.9f, 0.0f);
    glScalef(8.0f, 0.1f, 6.0f);
    glutSolidCube(1.0f);
    
    glPopMatrix();
}

// Draw the gold plate/base
void drawGoldPlate() {
    glPushMatrix();
    
    // Set gold material properties
    GLfloat gold_ambient[] = {0.24725f, 0.1995f, 0.0745f, 1.0f};
    GLfloat gold_diffuse[] = {0.75164f, 0.60648f, 0.22648f, 1.0f};
    GLfloat gold_specular[] = {0.628281f, 0.555802f, 0.366065f, 1.0f};
    GLfloat gold_shininess = 51.2f;
    
    glMaterialfv(GL_FRONT, GL_AMBIENT, gold_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, gold_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, gold_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, gold_shininess);
    
    // Draw plate as a flat cylinder
    glTranslatef(0.0f, -0.8f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawCylinder(1.5f, 0.08f, 48);
    
    // Inner rim detail
    glTranslatef(0.0f, 0.0f, 0.08f);
    glColor3f(0.85f, 0.65f, 0.25f);
    drawCylinder(1.35f, 0.03f, 48);
    
    glPopMatrix();
}

// Draw subtle vertical stripes on box surface
void drawBoxDecorations() {
    // Subtle cream-colored vertical ridges/pattern
    GLfloat decoration_ambient[] = {0.88f, 0.86f, 0.82f, 1.0f};
    GLfloat decoration_diffuse[] = {0.92f, 0.90f, 0.86f, 1.0f};
    GLfloat decoration_specular[] = {0.15f, 0.15f, 0.15f, 1.0f};
    
    glMaterialfv(GL_FRONT, GL_AMBIENT, decoration_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, decoration_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, decoration_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, 12.0f);
    
    int numStripes = 24;
    for (int i = 0; i < numStripes; i++) {
        glPushMatrix();
        float angle = (360.0f / numStripes) * i;
        glRotatef(angle, 0.0f, 1.0f, 0.0f);
        glTranslatef(1.02f, 0.2f, 0.0f);
        glScalef(0.02f, 0.9f, 0.05f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }
}

// Draw the main gift box/cake body
void drawGiftBox() {
    glPushMatrix();
    
    // White/cream matte material - softer, more anime-like
    GLfloat white_ambient[] = {0.92f, 0.90f, 0.86f, 1.0f};
    GLfloat white_diffuse[] = {0.98f, 0.96f, 0.92f, 1.0f};
    GLfloat white_specular[] = {0.35f, 0.35f, 0.32f, 1.0f};
    GLfloat white_shininess = 15.0f;
    
    glMaterialfv(GL_FRONT, GL_AMBIENT, white_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, white_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, white_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, white_shininess);
    
    // Main box body (slightly rounded top)
    glTranslatef(0.0f, -0.3f, 0.0f);
    
    // Bottom section (cylindrical)
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    drawCylinder(1.0f, 0.8f, 48);
    glPopMatrix();
    
    // Top section (slightly domed)
    glPushMatrix();
    glTranslatef(0.0f, 0.8f, 0.0f);
    glScalef(1.0f, 0.3f, 1.0f);
    drawSphere(1.0f, 48, 24);
    glPopMatrix();
    
    // Draw decorative patterns
    drawBoxDecorations();
    
    glPopMatrix();
}

// Draw a single rose
void drawRose(float size, const float color[3]) {
    glColor3fv(color);
    
    GLfloat mat_ambient[] = {color[0] * 0.3f, color[1] * 0.3f, color[2] * 0.3f, 1.0f};
    GLfloat mat_diffuse[] = {color[0], color[1], color[2], 1.0f};
    GLfloat mat_specular[] = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat mat_shininess = 20.0f;
    
    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, mat_shininess);
    
    // Center bud
    drawSphere(size * 0.2f, 16, 16);
    
    // Inner petals (tighter)
    int numPetals = 8;
    for (int i = 0; i < numPetals; i++) {
        glPushMatrix();
        float angle = (360.0f / numPetals) * i;
        glRotatef(angle, 0.0f, 1.0f, 0.0f);
        glTranslatef(size * 0.15f, 0.0f, 0.0f);
        glRotatef(30.0f, 0.0f, 0.0f, 1.0f);
        glScalef(1.0f, 1.0f, 0.3f);
        drawSphere(size * 0.25f, 12, 12);
        glPopMatrix();
    }
    
    // Outer petals (more open)
    for (int i = 0; i < numPetals; i++) {
        glPushMatrix();
        float angle = (360.0f / numPetals) * i + 22.5f;
        glRotatef(angle, 0.0f, 1.0f, 0.0f);
        glTranslatef(size * 0.3f, -size * 0.05f, 0.0f);
        glRotatef(50.0f, 0.0f, 0.0f, 1.0f);
        glScalef(1.2f, 1.0f, 0.25f);
        drawSphere(size * 0.3f, 12, 12);
        glPopMatrix();
    }
}

// Draw a daisy/simple flower
void drawDaisy(float size, const float petalColor[3], const float centerColor[3]) {
    // Center
    glColor3fv(centerColor);
    GLfloat center_ambient[] = {centerColor[0] * 0.3f, centerColor[1] * 0.3f, centerColor[2] * 0.3f, 1.0f};
    GLfloat center_diffuse[] = {centerColor[0], centerColor[1], centerColor[2], 1.0f};
    glMaterialfv(GL_FRONT, GL_AMBIENT, center_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, center_diffuse);
    
    drawSphere(size * 0.15f, 16, 16);
    
    // Petals
    glColor3fv(petalColor);
    GLfloat petal_ambient[] = {petalColor[0] * 0.3f, petalColor[1] * 0.3f, petalColor[2] * 0.3f, 1.0f};
    GLfloat petal_diffuse[] = {petalColor[0], petalColor[1], petalColor[2], 1.0f};
    glMaterialfv(GL_FRONT, GL_AMBIENT, petal_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, petal_diffuse);
    
    int numPetals = 12;
    for (int i = 0; i < numPetals; i++) {
        glPushMatrix();
        float angle = (360.0f / numPetals) * i;
        glRotatef(angle, 0.0f, 1.0f, 0.0f);
        glTranslatef(size * 0.25f, 0.0f, 0.0f);
        glScalef(1.5f, 1.0f, 0.3f);
        drawSphere(size * 0.2f, 12, 12);
        glPopMatrix();
    }
}

// Draw a leaf
void drawLeaf(float size, const float color[3]) {
    glColor3fv(color);
    
    GLfloat leaf_ambient[] = {color[0] * 0.3f, color[1] * 0.3f, color[2] * 0.3f, 1.0f};
    GLfloat leaf_diffuse[] = {color[0], color[1], color[2], 1.0f};
    GLfloat leaf_specular[] = {0.1f, 0.15f, 0.1f, 1.0f};
    
    glMaterialfv(GL_FRONT, GL_AMBIENT, leaf_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, leaf_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, leaf_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, 15.0f);
    
    glPushMatrix();
    glScalef(size * 1.5f, size * 0.8f, size * 0.2f);
    drawSphere(0.2f, 12, 12);
    glPopMatrix();
}

// Draw all decorative flowers on the cake
void drawFlowers() {
    // Colors
    float pink[3] = {0.92f, 0.45f, 0.58f};
    float darkPink[3] = {0.8f, 0.25f, 0.42f};
    float lightPink[3] = {0.95f, 0.75f, 0.82f};
    float white[3] = {0.98f, 0.96f, 0.94f};
    float yellow[3] = {0.98f, 0.85f, 0.35f};
    float red[3] = {0.85f, 0.25f, 0.28f};
    float green[3] = {0.35f, 0.65f, 0.40f};
    
    glPushMatrix();
    glTranslatef(0.0f, 0.5f, 0.0f);
    
    // Front center large rose (pink)
    glPushMatrix();
    glTranslatef(0.0f, -0.05f, 0.75f);
    glRotatef(-10.0f, 1.0f, 0.0f, 0.0f);
    drawRose(0.35f, pink);
    glPopMatrix();
    
    // Left front daisy
    glPushMatrix();
    glTranslatef(-0.45f, -0.1f, 0.6f);
    glRotatef(-20.0f, 0.0f, 1.0f, 0.0f);
    drawDaisy(0.3f, lightPink, yellow);
    glPopMatrix();
    
    // Right front rose (red/orange)
    glPushMatrix();
    glTranslatef(0.5f, -0.08f, 0.5f);
    glRotatef(25.0f, 0.0f, 1.0f, 0.0f);
    drawRose(0.3f, red);
    glPopMatrix();
    
    // Back left rose
    glPushMatrix();
    glTranslatef(-0.4f, -0.02f, -0.3f);
    glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
    drawRose(0.28f, darkPink);
    glPopMatrix();
    
    // Back right daisy
    glPushMatrix();
    glTranslatef(0.35f, 0.0f, -0.35f);
    glRotatef(160.0f, 0.0f, 1.0f, 0.0f);
    drawDaisy(0.25f, white, yellow);
    glPopMatrix();
    
    // Top center small daisy
    glPushMatrix();
    glTranslatef(-0.1f, 0.15f, 0.1f);
    glRotatef(-45.0f, 0.0f, 1.0f, 0.0f);
    drawDaisy(0.22f, lightPink, yellow);
    glPopMatrix();
    
    // Scattered leaves around flowers
    glPushMatrix();
    glTranslatef(-0.3f, -0.12f, 0.5f);
    glRotatef(45.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(20.0f, 1.0f, 0.0f, 0.0f);
    drawLeaf(0.25f, green);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(0.3f, -0.1f, 0.4f);
    glRotatef(-30.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(-15.0f, 1.0f, 0.0f, 0.0f);
    drawLeaf(0.22f, green);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(0.0f, 0.05f, -0.5f);
    glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(10.0f, 1.0f, 0.0f, 0.0f);
    drawLeaf(0.2f, green);
    glPopMatrix();
    
    glPopMatrix();
}

// Draw a ribbon strip (for the bow and wrapping)
void drawRibbonStrip(float length, float width, float thickness) {
    // Create a ribbon using a flattened box
    glPushMatrix();
    glScalef(length, thickness, width);
    glutSolidCube(1.0f);
    glPopMatrix();
}

// Draw the decorative ribbon and bow
void drawRibbon() {
    // Ribbon material (pink/rose gold)
    float ribbonColor[3] = {0.85f, 0.55f, 0.45f};
    
    GLfloat ribbon_ambient[] = {ribbonColor[0] * 0.4f, ribbonColor[1] * 0.4f, ribbonColor[2] * 0.4f, 1.0f};
    GLfloat ribbon_diffuse[] = {ribbonColor[0], ribbonColor[1], ribbonColor[2], 1.0f};
    GLfloat ribbon_specular[] = {0.5f, 0.4f, 0.4f, 1.0f};
    GLfloat ribbon_shininess = 30.0f;
    
    glMaterialfv(GL_FRONT, GL_AMBIENT, ribbon_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, ribbon_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, ribbon_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, ribbon_shininess);
    
    glColor3fv(ribbonColor);
    
    // Vertical ribbon (front to back)
    glPushMatrix();
    glTranslatef(0.0f, 0.1f, 0.0f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    drawRibbonStrip(0.12f, 2.2f, 0.04f);
    glPopMatrix();
    
    // Horizontal ribbon (left to right)
    glPushMatrix();
    glTranslatef(0.0f, 0.1f, 0.0f);
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    drawRibbonStrip(0.12f, 2.2f, 0.04f);
    glPopMatrix();
    
    // Bow on top - center knot
    glPushMatrix();
    glTranslatef(0.0f, 0.7f, 0.0f);
    drawSphere(0.12f, 16, 16);
    
    // Left bow loop
    glPushMatrix();
    glTranslatef(-0.25f, 0.1f, 0.0f);
    glRotatef(-30.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(20.0f, 0.0f, 1.0f, 0.0f);
    glScalef(1.0f, 0.6f, 0.4f);
    glutSolidTorus(0.08f, 0.2f, 16, 24);
    glPopMatrix();
    
    // Right bow loop
    glPushMatrix();
    glTranslatef(0.25f, 0.1f, 0.0f);
    glRotatef(30.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(-20.0f, 0.0f, 1.0f, 0.0f);
    glScalef(1.0f, 0.6f, 0.4f);
    glutSolidTorus(0.08f, 0.2f, 16, 24);
    glPopMatrix();
    
    // Front bow loop
    glPushMatrix();
    glTranslatef(0.0f, 0.08f, 0.22f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(-10.0f, 0.0f, 1.0f, 0.0f);
    glScalef(0.4f, 1.0f, 0.5f);
    glutSolidTorus(0.08f, 0.18f, 16, 24);
    glPopMatrix();
    
    // Back bow loop
    glPushMatrix();
    glTranslatef(0.0f, 0.08f, -0.22f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(10.0f, 0.0f, 1.0f, 0.0f);
    glScalef(0.4f, 1.0f, 0.5f);
    glutSolidTorus(0.08f, 0.18f, 16, 24);
    glPopMatrix();
    
    // Left ribbon tail
    glPushMatrix();
    glTranslatef(-0.35f, -0.15f, 0.0f);
    glRotatef(-45.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(15.0f, 1.0f, 0.0f, 0.0f);
    drawRibbonStrip(0.1f, 0.35f, 0.03f);
    glPopMatrix();
    
    // Right ribbon tail
    glPushMatrix();
    glTranslatef(0.35f, -0.15f, 0.0f);
    glRotatef(45.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(-15.0f, 1.0f, 0.0f, 0.0f);
    drawRibbonStrip(0.1f, 0.35f, 0.03f);
    glPopMatrix();
    
    glPopMatrix();
}

// ============================================================================
// MAIN RENDERING
// ============================================================================

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glLoadIdentity();
    
    // Set up camera - static anime-style view
    glTranslatef(0.0f, -0.3f, -cameraDistance);
    glRotatef(cameraAngleX, 1.0f, 0.0f, 0.0f);
    glRotatef(cameraAngleY + (autoRotate ? rotationAngle : 0.0f), 0.0f, 1.0f, 0.0f);
    
    // Draw all scene components
    drawTable();
    drawGoldPlate();
    drawGiftBox();
    drawRibbon();
    drawFlowers();
    
    glutSwapBuffers();
}

void setupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_LIGHT2);
    
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    
    // Key light (main illumination, warm afternoon light)
    GLfloat light0_position[] = {3.0f, 4.0f, 2.5f, 1.0f};
    GLfloat light0_diffuse[] = {1.0f, 0.95f, 0.88f, 1.0f};
    GLfloat light0_specular[] = {0.95f, 0.92f, 0.88f, 1.0f};
    GLfloat light0_ambient[] = {0.35f, 0.34f, 0.32f, 1.0f};
    
    glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light0_specular);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light0_ambient);
    
    // Fill light (softer, ambient bounce light)
    GLfloat light1_position[] = {-2.0f, 1.5f, 2.0f, 1.0f};
    GLfloat light1_diffuse[] = {0.55f, 0.52f, 0.48f, 1.0f};
    GLfloat light1_specular[] = {0.25f, 0.24f, 0.22f, 1.0f};
    GLfloat light1_ambient[] = {0.05f, 0.05f, 0.05f, 1.0f};
    
    glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1_specular);
    glLightfv(GL_LIGHT1, GL_AMBIENT, light1_ambient);
    
    // Rim light (back lighting for depth)
    GLfloat light2_position[] = {0.0f, 1.5f, -3.0f, 1.0f};
    GLfloat light2_diffuse[] = {0.35f, 0.35f, 0.4f, 1.0f};
    GLfloat light2_specular[] = {0.3f, 0.3f, 0.3f, 1.0f};
    
    glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_diffuse);
    glLightfv(GL_LIGHT2, GL_SPECULAR, light2_specular);
    
    // Global ambient light (warmer, more atmospheric)
    GLfloat global_ambient[] = {0.32f, 0.30f, 0.28f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    float aspect = (float)w / (float)h;
    
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, aspect, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int x, int y) {
    switch(key) {
        case 'w': case 'W':
            cameraAngleX += 5.0f;
            break;
        case 's': case 'S':
            cameraAngleX -= 5.0f;
            break;
        case 'a': case 'A':
            cameraAngleY -= 5.0f;
            break;
        case 'd': case 'D':
            cameraAngleY += 5.0f;
            break;
        case '+': case '=':
            cameraDistance -= 0.3f;
            if (cameraDistance < 2.0f) cameraDistance = 2.0f;
            break;
        case '-': case '_':
            cameraDistance += 0.3f;
            break;
        case 'r': case 'R':
            autoRotate = !autoRotate;
            printf("Auto-rotation: %s\n", autoRotate ? "ON" : "OFF");
            break;
        case 27: // ESC
            exit(0);
            break;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    switch(key) {
        case GLUT_KEY_UP:
            cameraAngleX += 5.0f;
            break;
        case GLUT_KEY_DOWN:
            cameraAngleX -= 5.0f;
            break;
        case GLUT_KEY_LEFT:
            cameraAngleY -= 5.0f;
            break;
        case GLUT_KEY_RIGHT:
            cameraAngleY += 5.0f;
            break;
    }
    glutPostRedisplay();
}

void idle() {
    // Subtle auto-rotation (only if enabled)
    if (autoRotate) {
        rotationAngle += 0.1f;
        if (rotationAngle > 360.0f) rotationAngle -= 360.0f;
        glutPostRedisplay();
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("Decorative Gift Box Scene");
    
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        fprintf(stderr, "GLEW initialization failed: %s\n", glewGetErrorString(glewErr));
        return 1;
    }
    
    // OpenGL settings
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glClearColor(0.96f, 0.94f, 0.90f, 1.0f);  // Warm soft background
    glShadeModel(GL_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    setupLighting();
    
    // Register callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutIdleFunc(idle);
    
    printf("Controls:\n");
    printf("  Arrow Keys / WASD - Rotate camera\n");
    printf("  +/- - Zoom in/out\n");
    printf("  R - Toggle auto-rotation (currently OFF)\n");
    printf("  ESC - Exit\n\n");
    
    glutMainLoop();
    return 0;
}
