#include <GL/freeglut.h>
// BUILD COMMAND:
// g++ src/main.cpp -Ilibs/freeglut/include -Llibs/freeglut/lib -lfreeglut -lopengl32 -lglu32 -o app.exe

void display() {
    // Set sky blue background color (R, G, B)
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Draw white triangle
    glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 1.0f, 1.0f); glVertex2f(-0.5f, -0.5f);
        glColor3f(1.0f, 1.0f, 1.0f); glVertex2f( 0.5f, -0.5f);
        glColor3f(1.0f, 1.0f, 1.0f); glVertex2f( 0.0f,  0.5f);
    glEnd();
    glFlush();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutCreateWindow("Scene 1");
    glutDisplayFunc(display);
     glutInitWindowSize(600, 600);
    glutMainLoop();
    return 0;
}
