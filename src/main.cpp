#include <GL/freeglut.h>
// BUILD COMMAND:
// g++ src/main.cpp -Ilibs/freeglut/include -Llibs/freeglut/lib -lfreeglut -lopengl32 -lglu32 -o app.exe

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLES);
        glColor3f(1, 0, 0); glVertex2f(-0.5f, -0.5f);
        glColor3f(0, 1, 0); glVertex2f( 0.5f, -0.5f);
        glColor3f(0, 0, 1); glVertex2f( 0.0f,  0.5f);
    glEnd();
    glFlush();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutCreateWindow("Scene 1");
    glutDisplayFunc(display);
     glutInitWindowSize(800, 800);
    glutMainLoop();
    return 0;
}
