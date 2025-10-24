#include <GL/freeglut.h>

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
