#include <GL/freeglut.h>
#include <cmath>

// BUILD COMMAND:
// g++ src/main.cpp -Ilibs/freeglut/include -Llibs/freeglut/lib -lfreeglut -lopengl32 -lglu32 -o app.exe

void display() {
    // set sky blue background color
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // ground
    glBegin(GL_QUADS);
        glColor3f(0.8f, 0.8f, 0.8f);
        glVertex2f(-1.0f, -1.0f);
        glVertex2f( 1.0f, -1.0f);
        glVertex2f( 1.0f, -0.4f);
        glVertex2f(-1.0f, -0.4f);
    glEnd();

    // reddish rubber area of ground — simple trapezoid
    glColor3f(0.6f, 0.18f, 0.18f);
    glBegin(GL_POLYGON);
        glVertex2f(-0.75f, -0.4f);
        glVertex2f( 0.75f, -0.4f);
        glVertex2f( 0.55f, -0.85f);
        glVertex2f(-0.55f, -0.85f);
    glEnd();

    // Helper function to draw a circle
    auto drawCircle = [](float cx, float cy, float radius, float r, float g, float b) {
        glColor3f(r, g, b);
        glBegin(GL_POLYGON);
        for (int i = 0; i < 360; i += 10) {
            float angle = i * 3.14159f / 180.0f;
            glVertex2f(cx + radius * cos(angle), cy + radius * sin(angle));
        }
        glEnd();
    };

    // Helper function to draw a tree at position (x, y)
    auto drawTree = [&drawCircle](float x, float y, float scale) {
        // Tree trunk (brown rectangle) - taller trunk
        glColor3f(0.55f, 0.27f, 0.07f);
        glBegin(GL_QUADS);
            glVertex2f(x - 0.04f * scale, y);
            glVertex2f(x + 0.04f * scale, y);
            glVertex2f(x + 0.04f * scale, y + 0.35f * scale);
            glVertex2f(x - 0.04f * scale, y + 0.35f * scale);
        glEnd();
        
        // Tree foliage - circular clusters for a normal tree look
        // Bottom center cluster to connect trunk (darker green)
        drawCircle(x, y + 0.40f * scale, 0.12f * scale, 0.13f, 0.55f, 0.13f);
        
        // Bottom left cluster (darker green)
        drawCircle(x - 0.10f * scale, y + 0.55f * scale, 0.12f * scale, 0.13f, 0.55f, 0.13f);
        
        // Bottom right cluster (darker green)
        drawCircle(x + 0.10f * scale, y + 0.55f * scale, 0.12f * scale, 0.13f, 0.55f, 0.13f);
        
        // Middle left cluster (medium green)
        drawCircle(x - 0.08f * scale, y + 0.70f * scale, 0.13f * scale, 0.18f, 0.60f, 0.18f);
        
        // Middle right cluster (medium green)
        drawCircle(x + 0.08f * scale, y + 0.70f * scale, 0.13f * scale, 0.18f, 0.60f, 0.18f);
        
        // Center cluster (lighter green)
        drawCircle(x, y + 0.85f * scale, 0.14f * scale, 0.22f, 0.66f, 0.22f);
        
        // Top cluster (lighter green)
        drawCircle(x, y + 1.00f * scale, 0.12f * scale, 0.25f, 0.70f, 0.25f);
    };

    // Draw trees on the left side
    drawTree(-0.85f, -0.4f, 1.0f);
    drawTree(-0.65f, -0.5f, 0.9f);
    drawTree(-0.45f, -0.45f, 1.1f);

    // Draw trees on the right side
    drawTree(0.45f, -0.45f, 1.0f);
    drawTree(0.65f, -0.5f, 0.95f);
    drawTree(0.85f, -0.4f, 1.05f);

    glFlush();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
    glutInitWindowSize(1200, 600); // fixed window size
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Scene 1");

    glutDisplayFunc(display);
    glutReshapeFunc([](int, int){}); 

    glutMainLoop();
    return 0;
}
