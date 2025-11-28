// When building as part of the embedded system, use GLEW+freeglut
#ifdef EMBEDDED_MODE
#include <GL/glew.h>
#include <GL/freeglut.h>
#include "utils.h"
#else
// Standalone build uses standard GLUT
#include <GL/glut.h>
#endif

#include <cmath>
#include <cstdio>
#include <vector>

// Window dimensions
const int WINDOW_WIDTH = 1200;
const int WINDOW_HEIGHT = 800;

// Camera parameters
float cameraAngleX = -15.0f;  // Tilted downward
float cameraAngleY = 10.0f;   // Slight angle to the side
float cameraDistance = 2.8f;  // Unzoomed a bit
float cameraHeight = 0.95f;   // Raised camera height

// Color definitions (hex to RGB normalized)
struct Color {
    float r, g, b;
    Color(float red, float green, float blue) : r(red), g(green), b(blue) {}
};

// Scene colors
Color cakeColor(1.0f, 0.965f, 0.91f);        // #FFF6E8 - cream white
Color plateColor(0.945f, 0.718f, 0.204f);    // #F1B734 - warm gold
Color strawberryColor(0.824f, 0.282f, 0.227f); // #D2483A - red
Color groundColor(0.541f, 0.639f, 0.294f);   // #8AA34B - grass green
Color benchWoodColor(0.647f, 0.353f, 0.165f); // #A55A2A - warm brown
Color benchLegColor(0.294f, 0.184f, 0.114f); // #4B2F1D - dark brown
Color treeLeafColor(0.365f, 0.498f, 0.227f); // #5D7F3A - soft green
Color treeTrunkColor(0.29f, 0.18f, 0.11f);   // Dark brown
Color horizonColor(0.9f, 0.85f, 0.75f);      // Beige
Color goldenLightColor(1.0f, 0.769f, 0.478f); // #FFC47A - warm golden

const float PI = 3.14159265359f;

// Helper function to set color
void setColor(const Color& c) {
    glColor3f(c.r, c.g, c.b);
}

// ============================================================================
// PRIMITIVE DRAWING FUNCTIONS
// ============================================================================

// Draw a flat plane
void drawPlane(float width, float depth, const Color& color) {
    setColor(color);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-width/2, 0, -depth/2);
    glVertex3f(width/2, 0, -depth/2);
    glVertex3f(width/2, 0, depth/2);
    glVertex3f(-width/2, 0, depth/2);
    glEnd();
}

// Draw a box (rectangular prism)
void drawBox(float width, float height, float depth, const Color& color) {
    setColor(color);
    
    float w = width / 2.0f;
    float h = height / 2.0f;
    float d = depth / 2.0f;
    
    // Front face
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-w, -h, d);
    glVertex3f(w, -h, d);
    glVertex3f(w, h, d);
    glVertex3f(-w, h, d);
    glEnd();
    
    // Back face
    glBegin(GL_QUADS);
    glNormal3f(0, 0, -1);
    glVertex3f(-w, -h, -d);
    glVertex3f(-w, h, -d);
    glVertex3f(w, h, -d);
    glVertex3f(w, -h, -d);
    glEnd();
    
    // Top face
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-w, h, -d);
    glVertex3f(-w, h, d);
    glVertex3f(w, h, d);
    glVertex3f(w, h, -d);
    glEnd();
    
    // Bottom face
    glBegin(GL_QUADS);
    glNormal3f(0, -1, 0);
    glVertex3f(-w, -h, -d);
    glVertex3f(w, -h, -d);
    glVertex3f(w, -h, d);
    glVertex3f(-w, -h, d);
    glEnd();
    
    // Right face
    glBegin(GL_QUADS);
    glNormal3f(1, 0, 0);
    glVertex3f(w, -h, -d);
    glVertex3f(w, h, -d);
    glVertex3f(w, h, d);
    glVertex3f(w, -h, d);
    glEnd();
    
    // Left face
    glBegin(GL_QUADS);
    glNormal3f(-1, 0, 0);
    glVertex3f(-w, -h, -d);
    glVertex3f(-w, -h, d);
    glVertex3f(-w, h, d);
    glVertex3f(-w, h, -d);
    glEnd();
}

// Draw a cylinder
void drawCylinder(float radius, float height, int segments, const Color& color) {
    setColor(color);
    
    // Top circle
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glVertex3f(0, height, 0);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * PI * i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glVertex3f(x, height, z);
    }
    glEnd();
    
    // Bottom circle
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, -1, 0);
    glVertex3f(0, 0, 0);
    for (int i = segments; i >= 0; i--) {
        float angle = 2.0f * PI * i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glVertex3f(x, 0, z);
    }
    glEnd();
    
    // Side surface
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * PI * i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glNormal3f(x / radius, 0, z / radius);
        glVertex3f(x, 0, z);
        glVertex3f(x, height, z);
    }
    glEnd();
}

// Draw an ellipsoid (simplified sphere with different radii)
void drawEllipsoid(float rx, float ry, float rz, int slices, int stacks, const Color& color) {
    setColor(color);
    
    for (int i = 0; i < stacks; i++) {
        float lat0 = PI * (-0.5f + (float)i / stacks);
        float z0 = sin(lat0);
        float zr0 = cos(lat0);
        
        float lat1 = PI * (-0.5f + (float)(i + 1) / stacks);
        float z1 = sin(lat1);
        float zr1 = cos(lat1);
        
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2 * PI * (float)j / slices;
            float x = cos(lng);
            float y = sin(lng);
            
            glNormal3f(x * zr0, y * zr0, z0);
            glVertex3f(rx * x * zr0, ry * y * zr0, rz * z0);
            
            glNormal3f(x * zr1, y * zr1, z1);
            glVertex3f(rx * x * zr1, ry * y * zr1, rz * z1);
        }
        glEnd();
    }
}

// Draw a sphere (using ellipsoid with equal radii)
void drawSphere(float radius, int slices, int stacks, const Color& color) {
    drawEllipsoid(radius, radius, radius, slices, stacks, color);
}

// Draw a flat circle (for flower centers, etc.)
void drawCircle(float radius, int segments, const Color& color) {
    setColor(color);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glVertex3f(0, 0, 0);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * PI * i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glVertex3f(x, 0, z);
    }
    glEnd();
}

// ============================================================================
// SCENE COMPONENT FUNCTIONS
// ============================================================================

// Draw grass tufts (small dark green ovals scattered on ground)
void drawGrassTufts() {
    Color darkGrass(0.4f, 0.5f, 0.2f);
    
    // Scattered grass patches
    float positions[][3] = {
        {-3.0f, 0.01f, -2.0f}, {-2.5f, 0.01f, 1.5f}, {-1.0f, 0.01f, -1.0f},
        {1.5f, 0.01f, -2.5f}, {2.8f, 0.01f, 0.5f}, {0.5f, 0.01f, 2.0f},
        {-4.0f, 0.01f, 0.0f}, {3.5f, 0.01f, -1.5f}, {-0.5f, 0.01f, 3.0f},
        {2.0f, 0.01f, 2.5f}, {-3.5f, 0.01f, 2.8f}, {4.0f, 0.01f, 1.8f}
    };
    
    for (int i = 0; i < 12; i++) {
        glPushMatrix();
        glTranslatef(positions[i][0], positions[i][1], positions[i][2]);
        glScalef(1.5f, 1.0f, 1.0f);
        drawCircle(0.15f, 8, darkGrass);
        glPopMatrix();
    }
}

// Draw the park bench
void drawBench() {
    float benchWidth = 3.0f;
    float plankThickness = 0.08f;
    float plankDepth = 1.0f;  // Increased depth for cake
    float seatHeight = 0.6f;
    float backrestHeight = 1.2f;
    
    // Legs (4 legs)
    float legWidth = 0.08f;
    float legHeight = seatHeight;
    
    glPushMatrix();
    // Front left leg
    glTranslatef(-benchWidth/2 + 0.15f, legHeight/2, plankDepth/2 - 0.05f);
    drawBox(legWidth, legHeight, legWidth, benchLegColor);
    glPopMatrix();
    
    glPushMatrix();
    // Front right leg
    glTranslatef(benchWidth/2 - 0.15f, legHeight/2, plankDepth/2 - 0.05f);
    drawBox(legWidth, legHeight, legWidth, benchLegColor);
    glPopMatrix();
    
    glPushMatrix();
    // Back left leg
    glTranslatef(-benchWidth/2 + 0.15f, legHeight/2, -plankDepth/2 + 0.05f);
    drawBox(legWidth, legHeight, legWidth, benchLegColor);
    glPopMatrix();
    
    glPushMatrix();
    // Back right leg
    glTranslatef(benchWidth/2 - 0.15f, legHeight/2, -plankDepth/2 + 0.05f);
    drawBox(legWidth, legHeight, legWidth, benchLegColor);
    glPopMatrix();
    
    // Seat planks (3 planks)
    for (int i = 0; i < 3; i++) {
        glPushMatrix();
        float zOffset = -plankDepth/2 + (i * plankDepth / 2.5f) + 0.05f;
        glTranslatef(0, seatHeight, zOffset);
        drawBox(benchWidth, plankThickness, plankDepth/3.5f, benchWoodColor);
        glPopMatrix();
    }
    
    // Backrest planks (2 planks) - slightly angled
    for (int i = 0; i < 2; i++) {
        glPushMatrix();
        glTranslatef(0, backrestHeight - 0.1f - (i * 0.15f), -plankDepth/2 + 0.05f);
        glRotatef(-10, 1, 0, 0);  // Slight backward angle
        drawBox(benchWidth, plankThickness, plankDepth/4.0f, benchWoodColor);
        glPopMatrix();
    }
    
    // Back support posts
    glPushMatrix();
    glTranslatef(-benchWidth/2 + 0.15f, (seatHeight + backrestHeight)/2, -plankDepth/2 + 0.05f);
    drawBox(legWidth, backrestHeight - seatHeight, legWidth, benchLegColor);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(benchWidth/2 - 0.15f, (seatHeight + backrestHeight)/2, -plankDepth/2 + 0.05f);
    drawBox(legWidth, backrestHeight - seatHeight, legWidth, benchLegColor);
    glPopMatrix();
}

// Draw the cake plate (flat rounded rectangle)
void drawCakePlate() {
    glPushMatrix();
    glTranslatef(0, 0.02f, 0);  // Slightly raised
    glScalef(1.2f, 1.0f, 1.2f);
    drawCylinder(0.33f, 0.02f, 32, plateColor);
    glPopMatrix();
}

// Draw the main cake (cylinder)
void drawCake() {
    drawCylinder(0.30f, 0.15f, 32, cakeColor);
}

// Draw a strawberry (red ellipsoid with dots)
void drawStrawberry() {
    // Main strawberry body
    drawEllipsoid(0.06f, 0.08f, 0.06f, 12, 12, strawberryColor);
    
    // Darker dots on strawberry
    Color darkRed(0.6f, 0.1f, 0.1f);
    glPushMatrix();
    glTranslatef(0.03f, 0.04f, 0.04f);
    drawSphere(0.008f, 6, 6, darkRed);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-0.03f, 0.02f, 0.03f);
    drawSphere(0.008f, 6, 6, darkRed);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(0.02f, -0.02f, -0.03f);
    drawSphere(0.008f, 6, 6, darkRed);
    glPopMatrix();
}

// Draw a simple flower (petal fan with center)
void drawFlower(const Color& petalColor, const Color& centerColor, float size) {
    int numPetals = 6;
    
    // Draw petals
    for (int i = 0; i < numPetals; i++) {
        glPushMatrix();
        float angle = 2.0f * PI * i / numPetals;
        glRotatef(angle * 180.0f / PI, 0, 1, 0);
        glTranslatef(size * 0.4f, 0, 0);
        glScalef(1.5f, 1.0f, 0.8f);
        drawCircle(size * 0.25f, 8, petalColor);
        glPopMatrix();
    }
    
    // Draw center
    glPushMatrix();
    glTranslatef(0, 0.01f, 0);
    drawCircle(size * 0.15f, 12, centerColor);
    glPopMatrix();
}

// Draw a rose (spiral disc)
void drawRose(const Color& roseColor, float size) {
    // Outer layers
    for (int layer = 0; layer < 4; layer++) {
        float layerSize = size * (1.0f - layer * 0.15f);
        glPushMatrix();
        glTranslatef(0, layer * 0.015f, 0);
        glRotatef(layer * 30, 0, 1, 0);
        drawCircle(layerSize, 8, roseColor);
        glPopMatrix();
    }
}

// Draw a leaf (teardrop shape)
void drawLeaf() {
    Color leafGreen(0.3f, 0.6f, 0.2f);
    setColor(leafGreen);
    
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glVertex3f(0, 0, 0);  // Base point
    
    // Create teardrop shape
    for (int i = 0; i <= 16; i++) {
        float t = (float)i / 16.0f;
        float angle = PI * t;
        float radius = 0.08f * sin(angle);
        float x = radius * cos(angle * 2);
        float z = -0.12f * t;
        glVertex3f(x, 0, z);
    }
    glEnd();
}

// Draw all cake toppings
void drawCakeToppings() {
    // Position toppings on top of cake, slightly to one side
    float cakeTopY = 0.16f;
    
    // Two strawberries near back edge
    glPushMatrix();
    glTranslatef(-0.10f, cakeTopY, -0.17f);
    drawStrawberry();
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(0.03f, cakeTopY, -0.19f);
    glRotatef(20, 0, 1, 0);
    drawStrawberry();
    glPopMatrix();
    
    // Large red flower
    Color redFlower(0.9f, 0.2f, 0.2f);
    Color yellowCenter(1.0f, 0.9f, 0.2f);
    glPushMatrix();
    glTranslatef(0.10f, cakeTopY, 0.07f);
    drawFlower(redFlower, yellowCenter, 0.09f);
    glPopMatrix();
    
    // Two purple roses
    Color purpleRose(0.6f, 0.3f, 0.7f);
    glPushMatrix();
    glTranslatef(-0.14f, cakeTopY, 0.03f);
    drawRose(purpleRose, 0.06f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-0.03f, cakeTopY, 0.14f);
    drawRose(purpleRose, 0.05f);
    glPopMatrix();
    
    // Small white flower
    Color whiteFlower(1.0f, 1.0f, 1.0f);
    glPushMatrix();
    glTranslatef(0.17f, cakeTopY, -0.03f);
    drawFlower(whiteFlower, yellowCenter, 0.045f);
    glPopMatrix();
    
    // Green leaf
    glPushMatrix();
    glTranslatef(0.07f, cakeTopY, -0.10f);
    glRotatef(45, 0, 1, 0);
    drawLeaf();
    glPopMatrix();
}

// Draw a stylized tree
void drawTree(float trunkHeight, float leafRadius) {
    // Trunk
    glPushMatrix();
    glTranslatef(0, trunkHeight/2, 0);
    drawBox(0.2f, trunkHeight, 0.2f, treeTrunkColor);
    glPopMatrix();
    
    // Leaves (3 overlapping spheres for volume)
    glPushMatrix();
    glTranslatef(0, trunkHeight + leafRadius * 0.7f, 0);
    drawSphere(leafRadius, 12, 12, treeLeafColor);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(leafRadius * 0.3f, trunkHeight + leafRadius * 0.8f, leafRadius * 0.2f);
    drawSphere(leafRadius * 0.8f, 12, 12, treeLeafColor);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-leafRadius * 0.3f, trunkHeight + leafRadius * 0.75f, -leafRadius * 0.2f);
    drawSphere(leafRadius * 0.85f, 12, 12, treeLeafColor);
    glPopMatrix();
}

// Draw all background trees
void drawBackgroundTrees() {
    // Left side trees
    glPushMatrix();
    glTranslatef(-5.0f, 0, -3.0f);
    drawTree(1.2f, 1.0f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-4.0f, 0, -4.5f);
    drawTree(1.5f, 1.2f);
    glPopMatrix();
    
    // Right side trees
    glPushMatrix();
    glTranslatef(4.5f, 0, -3.5f);
    drawTree(1.3f, 1.1f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(5.5f, 0, -5.0f);
    drawTree(1.6f, 1.3f);
    glPopMatrix();
    
    // Center back trees
    glPushMatrix();
    glTranslatef(-1.5f, 0, -6.0f);
    drawTree(1.8f, 1.4f);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(2.0f, 0, -6.5f);
    drawTree(1.7f, 1.35f);
    glPopMatrix();
}

// Draw horizon strip
void drawHorizon() {
    glPushMatrix();
    glTranslatef(0, 0.005f, -4.0f);
    setColor(horizonColor);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-20, 0, -3);
    glVertex3f(20, 0, -3);
    glVertex3f(20, 0, 3);
    glVertex3f(-20, 0, 3);
    glEnd();
    glPopMatrix();
}

// ============================================================================
// MAIN SCENE RENDERING
// ============================================================================

void drawScene() {
    // 1. Ground plane
    drawPlane(20.0f, 20.0f, groundColor);
    
    // 2. Grass tufts
    drawGrassTufts();
    
    // 3. Horizon strip
    drawHorizon();
    
    // 4. Background trees
    drawBackgroundTrees();
    
    // 5. Bench
    glPushMatrix();
    glTranslatef(0, 0, 0);
    drawBench();
    glPopMatrix();
    
    // 6. Cake plate on bench (slightly off-center)
    glPushMatrix();
    glTranslatef(-0.2f, 0.66f, 0.0f);  // On bench seat
    drawCakePlate();
    glPopMatrix();
    
    // 7. Cake on plate
    glPushMatrix();
    glTranslatef(-0.2f, 0.68f, 0.0f);
    drawCake();
    glPopMatrix();
    
    // 8. Cake toppings
    glPushMatrix();
    glTranslatef(-0.2f, 0.68f, 0.0f);
    drawCakeToppings();
    glPopMatrix();
}

// ============================================================================
// OPENGL SETUP AND CALLBACKS
// ============================================================================

void initGL() {
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    
    // Enable lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    // Set up golden hour lighting
    GLfloat lightPos[] = {-5.0f, 2.0f, 8.0f, 0.0f};  // Directional light (w=0)
    GLfloat lightAmbient[] = {0.4f, 0.35f, 0.3f, 1.0f};  // Warm ambient
    GLfloat lightDiffuse[] = {goldenLightColor.r, goldenLightColor.g, goldenLightColor.b, 1.0f};
    GLfloat lightSpecular[] = {0.2f, 0.2f, 0.2f, 1.0f};  // Minimal specular
    
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
    
    // Material properties - matte finish
    GLfloat matSpecular[] = {0.1f, 0.1f, 0.1f, 1.0f};
    GLfloat matShininess[] = {5.0f};
    glMaterialfv(GL_FRONT, GL_SPECULAR, matSpecular);
    glMaterialfv(GL_FRONT, GL_SHININESS, matShininess);
    
    // Enable color material for easy color changes
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    
    // Set background color (warm sky)
    glClearColor(0.85f, 0.75f, 0.65f, 1.0f);
    
    // Smooth shading (can be changed to GL_FLAT for flat shading)
    glShadeModel(GL_SMOOTH);
    
    // Enable normalization for proper lighting with scaling
    glEnable(GL_NORMALIZE);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    // Set up camera with proper rotation first, then position
    glTranslatef(0.0f, -cameraHeight, -cameraDistance);
    glRotatef(cameraAngleX, 1.0f, 0.0f, 0.0f);
    glRotatef(cameraAngleY, 0.0f, 1.0f, 0.0f);
    glTranslatef(0.2f, -0.75f, 0.0f);  // Center on cake position
    
    // Draw the entire scene
    drawScene();
    
#ifdef EMBEDDED_MODE
    // draw UI buttons overlay from utils (keep same UI across scenes)
    int ww = glutGet(GLUT_WINDOW_WIDTH);
    int wh = glutGet(GLUT_WINDOW_HEIGHT);
    drawUIButtons(ww, wh);
    // draw subtitles (use utils' subtitle system; only visible when enabled by playback)
    subtitle_draw(ww, wh);
#endif
    
    glutSwapBuffers();
}

void reshape(int width, int height) {
    if (height == 0) height = 1;
    float aspect = (float)width / (float)height;
    
    glViewport(0, 0, width, height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, aspect, 0.1f, 100.0f);
    
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27:  // ESC key
            exit(0);
            break;
        case 'w':
            cameraAngleX -= 2.0f;
            break;
        case 's':
            cameraAngleX += 2.0f;
            break;
        case 'a':
            cameraAngleY -= 2.0f;
            break;
        case 'd':
            cameraAngleY += 2.0f;
            break;
        case 'q':
            cameraDistance -= 0.2f;
            if (cameraDistance < 2.0f) cameraDistance = 2.0f;
            break;
        case 'e':
            cameraDistance += 0.2f;
            if (cameraDistance > 20.0f) cameraDistance = 20.0f;
            break;
        case 'r':
            cameraHeight += 0.2f;
            break;
        case 'f':
            cameraHeight -= 0.2f;
            break;
        case 't':  // Toggle flat/smooth shading
            static bool flatShading = false;
            flatShading = !flatShading;
            glShadeModel(flatShading ? GL_FLAT : GL_SMOOTH);
            break;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    switch (key) {
        case GLUT_KEY_UP:
            cameraAngleX -= 2.0f;
            break;
        case GLUT_KEY_DOWN:
            cameraAngleX += 2.0f;
            break;
        case GLUT_KEY_LEFT:
            cameraAngleY -= 2.0f;
            break;
        case GLUT_KEY_RIGHT:
            cameraAngleY += 2.0f;
            break;
    }
    glutPostRedisplay();
}

// ============================================================================
// EMBEDDED SCENE INTERFACE (matching cake.cpp pattern)
// ============================================================================

#ifdef EMBEDDED_MODE
// Wrapper functions for embedded mode (called from utils.cpp)
void cake_display() { display(); }
void cake_reshape(int w, int h) { reshape(w, h); }
void cake_keyboard(unsigned char key, int x, int y) { keyboard(key, x, y); }
void cake_special(int key, int x, int y) { specialKeys(key, x, y); }
void cake_idle() {
    if (audio_isPlaying()) {
        glutPostRedisplay();
    }
}

// Embedded initialization helper - call once before first render when embedding
void cake_init_embedded() {
    initGL();
}
#endif

// ============================================================================
// MAIN FUNCTION
// ============================================================================

#ifndef EMBEDDED_MODE
int main(int argc, char** argv) {
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Cake_Scene 2");
    
    // Initialize OpenGL
    initGL();
    
    // Register callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    
    // Print controls
    printf("=== Golden Hour Cake Scene Controls ===\n");
    printf("W/S or UP/DOWN arrows: Rotate camera up/down\n");
    printf("A/D or LEFT/RIGHT arrows: Rotate camera left/right\n");
    printf("Q/E: Zoom in/out\n");
    printf("R/F: Move camera height up/down\n");
    printf("T: Toggle flat/smooth shading\n");
    printf("ESC: Exit\n");
    printf("=======================================\n");
    
    // Start main loop
    glutMainLoop();
    
    return 0;
}
#endif
