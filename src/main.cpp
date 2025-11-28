#include <windows.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <mmsystem.h>

#include <cmath>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <chrono>

#include "utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* BUILD COMMAND

 g++ src/main.cpp src/utils.cpp src/cake.cpp -Ilibs/include -Ilibs/freeglut/include -Llibs/freeglut/lib/x64 -Llibs -lglew32 -lfreeglut -lopengl32 -lglu32 -lwinmm -o app.exe 

*/

// Sun elevation in normalized device coords Y (-1 bottom .. +1 top)
static float sunElevation = 1.0f; // default: start at very top (midday) before playback
static bool sunVisible = true;     // when false, force midday sky and hide sun

// Seesaw animation
static float seesawAngle = 0.0f;

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
// persistent mesh helper for non-axis-aligned geometry

static Mesh mesh_roof_left = {0,0,0,GL_TRIANGLES};
static Mesh mesh_roof_mid  = {0,0,0,GL_TRIANGLES};
static Mesh mesh_roof_right = {0,0,0,GL_TRIANGLES};
static Mesh mesh_inside_fence = {0,0,0,GL_TRIANGLES};
static Mesh mesh_outside_right_fence = {0,0,0,GL_TRIANGLES};
static Mesh mesh_found_out_right = {0,0,0,GL_TRIANGLES};
static Mesh mesh_found_out_left = {0,0,0,GL_TRIANGLES};
static Mesh mesh_found_left = {0,0,0,GL_TRIANGLES};
static Mesh mesh_found_mid = {0,0,0,GL_TRIANGLES};
static Mesh mesh_found_right = {0,0,0,GL_TRIANGLES};
static Mesh mesh_climb_ramp = {0,0,0,GL_TRIANGLES};
static Mesh mesh_slide_floor = {0,0,0,GL_TRIANGLES};
static Mesh mesh_left_rail = {0,0,0,GL_TRIANGLE_STRIP};
static Mesh mesh_right_rail = {0,0,0,GL_TRIANGLE_STRIP};
static Mesh mesh_left_outline = {0,0,0,GL_LINE_STRIP};
static Mesh mesh_right_outline = {0,0,0,GL_LINE_STRIP};
static Mesh mesh_center_strip = {0,0,0,GL_TRIANGLE_STRIP};
static Mesh mesh_slide_end_fan = {0,0,0,GL_TRIANGLE_FAN};
// mesh_unit_quad is defined non-static below so utils can reference it
Mesh mesh_unit_quad = {0,0,0,GL_TRIANGLE_FAN};

// seesaw meshes
static Mesh mesh_seesaw_support_base = {0,0,0,GL_TRIANGLE_FAN};
static Mesh mesh_seesaw_cap = {0,0,0,GL_TRIANGLE_FAN};
static Mesh mesh_seesaw_plank = {0,0,0,GL_TRIANGLE_FAN};
static Mesh mesh_seesaw_grip = {0,0,0,GL_TRIANGLE_FAN};
static Mesh mesh_seesaw_left_seat = {0,0,0,GL_TRIANGLE_FAN};
static Mesh mesh_seesaw_left_handle = {0,0,0,GL_TRIANGLE_FAN};
static Mesh mesh_seesaw_right_seat = {0,0,0,GL_TRIANGLE_FAN};
static Mesh mesh_seesaw_right_handle = {0,0,0,GL_TRIANGLE_FAN};

// Line meshes for fence posts, horizontals and floor
static Mesh mesh_inside_posts_thick = {0,0,0,GL_LINES};
static Mesh mesh_inside_posts_thin  = {0,0,0,GL_LINES};
static Mesh mesh_inside_horiz_top   = {0,0,0,GL_LINES};
static Mesh mesh_inside_horiz_bottom= {0,0,0,GL_LINES};

static Mesh mesh_outside_posts_thick = {0,0,0,GL_LINES};
static Mesh mesh_outside_posts_thin  = {0,0,0,GL_LINES};

static Mesh mesh_floor_line = {0,0,0,GL_LINES};

static float winAspect = 1.0f;
static int winW = 1280;
static int winH = 720;

// Audio, subtitles and UI moved to utils.*
// current scene index remains here for scene management
static int currentScene = 1;

// Bloom/post-process parameters removed from this build

// Forward declarations
static void createResources();
static void renderSceneContents();

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

    // Helper to create simple mesh (triangulate a convex polygon fan)
    auto makeMeshFromPoly = [](const std::vector<float>& verts, Mesh &out){
        out.count = (GLsizei)(verts.size() / 2);
        out.mode = GL_TRIANGLE_FAN;
        glGenBuffers(1, &out.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (GLEW_ARB_vertex_array_object || GLEW_VERSION_3_0) {
            glGenVertexArrays(1, &out.vao);
            glBindVertexArray(out.vao);
            glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        } else {
            out.vao = 0;
        }
    };

    // Create meshes for playground convex polygons (use triangle fan order)
    makeMeshFromPoly(std::vector<float>{ -0.7f, 0.5f, -0.45f, 0.8f, -0.4f, 0.8f, -0.65f, 0.5f }, mesh_roof_left);
    makeMeshFromPoly(std::vector<float>{ -0.44f, 0.8f, -0.4f, 0.8f, -0.4f, 0.5f, -0.44f, 0.5f }, mesh_roof_mid);
    makeMeshFromPoly(std::vector<float>{ -0.44f, 0.8f, -0.4f, 0.8f, -0.15f, 0.5f, -0.2f, 0.5f }, mesh_roof_right);

    makeMeshFromPoly(std::vector<float>{ -0.65f, 0.2f, -0.43f, 0.17f, -0.43f, -0.2f, -0.65f, -0.2f }, mesh_inside_fence);
    makeMeshFromPoly(std::vector<float>{ -0.16f, 0.0f, 0.03f, 0.0f, 0.03f, -0.49f, -0.16f, -0.49f }, mesh_outside_right_fence);

    makeMeshFromPoly(std::vector<float>{ 0.03f, 0.08f, 0.08f, 0.08f, 0.08f, -0.62f, 0.03f, -0.62f }, mesh_found_out_right);
    makeMeshFromPoly(std::vector<float>{ -0.85f, 0.2f, -0.8f, 0.2f, -0.8f, -0.65f, -0.85f, -0.65f }, mesh_found_out_left);
    makeMeshFromPoly(std::vector<float>{ -0.69f, 0.5f, -0.64f, 0.5f, -0.64f, -0.2f, -0.69f, -0.2f }, mesh_found_left);
    makeMeshFromPoly(std::vector<float>{ -0.45f, 0.5f, -0.4f, 0.5f, -0.4f, -0.65f, -0.45f, -0.65f }, mesh_found_mid);
    makeMeshFromPoly(std::vector<float>{ -0.21f, 0.5f, -0.16f, 0.5f, -0.16f, -0.62f, -0.21f, -0.62f }, mesh_found_right);

    makeMeshFromPoly(std::vector<float>{ -0.69f, -0.2f, -0.43f, -0.2f, -0.62f, -0.7f, -0.9f, -0.65f }, mesh_climb_ramp);
    makeMeshFromPoly(std::vector<float>{ 0.02f, -0.63f, 0.095f, -0.57f, 0.095f, -0.62f, 0.02f, -0.68f }, mesh_slide_floor);

    // create unit quad centered at origin (vertices CCW)
    std::vector<float> unitQuad = { -0.5f, -0.5f,  0.5f, -0.5f,  0.5f, 0.5f,  -0.5f, 0.5f };
    makeMeshFromPoly(unitQuad, mesh_unit_quad);

    // Create seesaw meshes (object-space coordinates used directly in drawSeesaw)
    makeMeshFromPoly(std::vector<float>{ -0.12f, 0.0f, 0.12f, 0.0f, 0.08f, 0.18f, -0.08f, 0.18f }, mesh_seesaw_support_base);
    makeMeshFromPoly(std::vector<float>{ -0.1f, 0.18f, 0.1f, 0.18f, 0.1f, 0.2f, -0.1f, 0.2f }, mesh_seesaw_cap);
    makeMeshFromPoly(std::vector<float>{ -0.35f, -0.03f, 0.35f, -0.03f, 0.35f, 0.03f, -0.35f, 0.03f }, mesh_seesaw_plank);
    makeMeshFromPoly(std::vector<float>{ -0.08f, -0.03f, 0.08f, -0.03f, 0.08f, 0.03f, -0.08f, 0.03f }, mesh_seesaw_grip);
    makeMeshFromPoly(std::vector<float>{ -0.35f, 0.03f, -0.22f, 0.03f, -0.22f, 0.11f, -0.35f, 0.11f }, mesh_seesaw_left_seat);
    makeMeshFromPoly(std::vector<float>{ -0.285f, 0.11f, -0.275f, 0.11f, -0.275f, 0.19f, -0.285f, 0.19f }, mesh_seesaw_left_handle);
    makeMeshFromPoly(std::vector<float>{ 0.22f, 0.03f, 0.35f, 0.03f, 0.35f, 0.11f, 0.22f, 0.11f }, mesh_seesaw_right_seat);
    makeMeshFromPoly(std::vector<float>{ 0.275f, 0.11f, 0.285f, 0.11f, 0.285f, 0.19f, 0.275f, 0.19f }, mesh_seesaw_right_handle);

    // helper to upload arbitrary vertex arrays as a mesh with given mode (lines/strip/etc.)
    auto uploadLines = [&](const std::vector<float>& verts, GLenum mode, Mesh &out){
        out.count = (GLsizei)(verts.size() / 2);
        out.mode = mode;
        glGenBuffers(1, &out.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (GLEW_ARB_vertex_array_object || GLEW_VERSION_3_0) {
            glGenVertexArrays(1, &out.vao);
            glBindVertexArray(out.vao);
            glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        } else {
            out.vao = 0;
        }
    };

    // Create fence post and horizontal line meshes (replace immediate-mode GL_LINES)
    std::vector<float> insideThick = {
        -0.63f, 0.19f,  -0.63f, -0.2f,
        -0.59f, 0.19f,  -0.59f, -0.2f,
        -0.55f, 0.18f,  -0.55f, -0.2f,
        -0.51f, 0.18f,  -0.51f, -0.2f,
        -0.475f,0.17f,  -0.475f,-0.2f
    };
    uploadLines(insideThick, GL_LINES, mesh_inside_posts_thick);

    std::vector<float> insideThin = {
        -0.62f, 0.19f,  -0.62f, -0.2f,
        -0.58f, 0.19f,  -0.58f, -0.2f,
        -0.54f, 0.18f,  -0.54f, -0.2f,
        -0.50f, 0.18f,  -0.50f, -0.2f,
        -0.465f,0.17f,  -0.465f,-0.2f
    };
    uploadLines(insideThin, GL_LINES, mesh_inside_posts_thin);

    std::vector<float> insideHorizTop = { -0.65f, 0.17f,  -0.43f, 0.13f };
    std::vector<float> insideHorizBottom = { -0.65f, -0.17f,  -0.43f, -0.17f };
    uploadLines(insideHorizTop, GL_LINES, mesh_inside_horiz_top);
    uploadLines(insideHorizBottom, GL_LINES, mesh_inside_horiz_bottom);

    std::vector<float> outsideThick = {
        -0.14f, 0.0f, -0.14f, -0.49f,
        -0.1f,  0.0f, -0.1f,  -0.49f,
        -0.06f, 0.0f, -0.06f, -0.49f,
        -0.02f, 0.0f, -0.02f, -0.49f,
         0.02f, 0.0f,  0.02f, -0.49f
    };
    uploadLines(outsideThick, GL_LINES, mesh_outside_posts_thick);

    std::vector<float> outsideThin = {
        -0.13f, 0.0f, -0.13f, -0.49f,
        -0.09f, 0.0f, -0.09f, -0.49f,
        -0.05f, 0.0f, -0.05f, -0.49f,
        -0.01f, 0.0f, -0.01f, -0.49f,
         0.03f, 0.0f,  0.03f, -0.49f
    };
    uploadLines(outsideThin, GL_LINES, mesh_outside_posts_thin);

    std::vector<float> floorLine = { -0.69f, -0.2f,  -0.16f, -0.2f };
    uploadLines(floorLine, GL_LINES, mesh_floor_line);

    // Create slide procedural meshes (rails, stripes, outlines) so drawSlide can be cheap
    {
        const int SEGMENTS = 80;
        const float START_X = -0.3f;
        const float START_Y = -0.10f;
        const float END_X   = 0.1f;
        const float END_Y   = -0.6f;
        const float gap = 0.055f;
        const float leftRailWidth   = 0.032f;
        const float rightRailWidth  = 0.042f;

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

        float leftInner  =  gap * 0.5f;
        float leftOuter  =  gap * 0.5f + leftRailWidth;
        float rightInner = -gap * 0.5f;
        float rightOuter = -gap * 0.5f - rightRailWidth;

        float leftLenFrac = 1.0f;
        int leftSegMax = (int)std::floor(SEGMENTS * leftLenFrac);
        if (leftSegMax < 2) leftSegMax = 2;
        float rightLenFrac = 0.82f;
        int rightSegMax = (int)std::floor(SEGMENTS * rightLenFrac);
        if (rightSegMax < 2) rightSegMax = 2;

        // helper to upload a verts vector as a mesh with given mode
        auto upload = [&](const std::vector<float>& verts, GLenum mode, Mesh &out){
            out.count = (GLsizei)(verts.size() / 2);
            out.mode = mode;
            glGenBuffers(1, &out.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
            glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            if (GLEW_ARB_vertex_array_object || GLEW_VERSION_3_0) {
                glGenVertexArrays(1, &out.vao);
                glBindVertexArray(out.vao);
                glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
            } else {
                out.vao = 0;
            }
        };

        // left rail (triangle strip outer/inner interleaved)
        std::vector<float> leftRailVerts;
        for (int i = 0; i <= leftSegMax; ++i) {
            float ox = cx[i] + nx[i] * leftOuter;
            float oy = cy[i] + ny[i] * leftOuter;
            float ix = cx[i] + nx[i] * leftInner;
            float iy = cy[i] + ny[i] * leftInner;
            leftRailVerts.push_back(ox); leftRailVerts.push_back(oy);
            leftRailVerts.push_back(ix); leftRailVerts.push_back(iy);
        }
        upload(leftRailVerts, GL_TRIANGLE_STRIP, mesh_left_rail);

        // right rail
        std::vector<float> rightRailVerts;
        for (int i = 0; i <= rightSegMax; ++i) {
            float ox = cx[i] + nx[i] * rightOuter;
            float oy = cy[i] + ny[i] * rightOuter;
            float ix = cx[i] + nx[i] * rightInner;
            float iy = cy[i] + ny[i] * rightInner;
            rightRailVerts.push_back(ox); rightRailVerts.push_back(oy);
            rightRailVerts.push_back(ix); rightRailVerts.push_back(iy);
        }
        upload(rightRailVerts, GL_TRIANGLE_STRIP, mesh_right_rail);

        // outlines (line strips along outer rails)
        std::vector<float> leftOutlineVerts;
        for (int i = 0; i <= leftSegMax; ++i) {
            float ox = cx[i] + nx[i] * leftOuter;
            float oy = cy[i] + ny[i] * leftOuter;
            leftOutlineVerts.push_back(ox); leftOutlineVerts.push_back(oy);
        }
        upload(leftOutlineVerts, GL_LINE_STRIP, mesh_left_outline);

        std::vector<float> rightOutlineVerts;
        for (int i = 0; i <= rightSegMax; ++i) {
            float ox = cx[i] + nx[i] * rightOuter;
            float oy = cy[i] + ny[i] * rightOuter;
            rightOutlineVerts.push_back(ox); rightOutlineVerts.push_back(oy);
        }
        upload(rightOutlineVerts, GL_LINE_STRIP, mesh_right_outline);

        // center stripe: determine center start/end as in original drawSlide
        const float midColCenterVal = (0.90f + 0.56f) * 0.5f;
        float leaveGap = 0.03f;
        int centerStartIdx = 0;
        for (int i = 0; i <= SEGMENTS; ++i) {
            if (cy[i] <= (START_Y - leaveGap)) { centerStartIdx = i; break; }
        }
        int centerEndIdx = (leftSegMax < rightSegMax) ? leftSegMax : rightSegMax;
        if (centerStartIdx < centerEndIdx) {
            std::vector<float> centerVerts;
            for (int i = centerStartIdx; i <= centerEndIdx; ++i) {
                float lix = cx[i] + nx[i] * leftInner;
                float liy = cy[i] + ny[i] * leftInner;
                float rix = cx[i] + nx[i] * rightInner;
                float riy = cy[i] + ny[i] * rightInner;
                // triangle strip: rix, riy, lix, liy interleaved to form a strip from right to left
                centerVerts.push_back(rix); centerVerts.push_back(riy);
                centerVerts.push_back(lix); centerVerts.push_back(liy);
            }
            upload(centerVerts, GL_TRIANGLE_STRIP, mesh_center_strip);
            // if the rails are asymmetric, create a triangle fan to cap the longer side (match original immediate-mode behavior)
            if (leftSegMax != rightSegMax) {
                int iShort = (leftSegMax < rightSegMax) ? leftSegMax : rightSegMax;
                int iLong  = (leftSegMax > rightSegMax) ? leftSegMax : rightSegMax;
                std::vector<float> fanVerts;
                if (leftSegMax > rightSegMax) {
                    float ax = cx[iShort] + nx[iShort] * rightInner;
                    float ay = cy[iShort] + ny[iShort] * rightInner;
                    // center point
                    fanVerts.push_back(ax); fanVerts.push_back(ay);
                    for (int i = iShort; i <= leftSegMax; ++i) {
                        float lx = cx[i] + nx[i] * leftInner;
                        float ly = cy[i] + ny[i] * leftInner;
                        fanVerts.push_back(lx); fanVerts.push_back(ly);
                    }
                } else {
                    float ax = cx[iShort] + nx[iShort] * leftInner;
                    float ay = cy[iShort] + ny[iShort] * leftInner;
                    fanVerts.push_back(ax); fanVerts.push_back(ay);
                    for (int i = iShort; i <= rightSegMax; ++i) {
                        float rx = cx[i] + nx[i] * rightInner;
                        float ry = cy[i] + ny[i] * rightInner;
                        fanVerts.push_back(rx); fanVerts.push_back(ry);
                    }
                }
                if (!fanVerts.empty()) upload(fanVerts, GL_TRIANGLE_FAN, mesh_slide_end_fan);
            }
        }
    }
}

// Draw a simple tree using circles for foliage and a rectangle trunk
static void drawTree(float x, float y, float scale, float tGround) {
    // trunk -- darken slightly at night
    float trunkBase[3] = {0.55f, 0.27f, 0.07f};
    float trunkMod = mixf(0.6f, 1.0f, tGround); // darker at night
    float trunk[3] = { trunkBase[0] * trunkMod, trunkBase[1] * trunkMod, trunkBase[2] * trunkMod };
    // set normal for trunk quad so lighting behaves predictably
    glNormal3f(0.0f, 0.0f, 1.0f);
    glColor3f(trunk[0], trunk[1], trunk[2]);
    // Draw trunk using the unit quad mesh (centered at origin). Place and scale accordingly.
    drawUnitQuad(x, y + 0.175f * scale, 0.08f * scale, 0.35f * scale);

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
    drawMesh(mesh_seesaw_support_base);
    
    // Support top cap (darker cyan with metallic look)
    float capColor[3] = { supportColor[0] * 0.75f, supportColor[1] * 0.75f, supportColor[2] * 0.85f };
    glColor3f(capColor[0], capColor[1], capColor[2]);
    drawMesh(mesh_seesaw_cap);
    
    // Rotating plank assembly
    glPushMatrix();
    glTranslatef(0.0f, 0.19f, 0.0f);
    glRotatef(angle, 0.0f, 0.0f, 1.0f);
    
    // Main plank - vivid yellow/gold
    float plankBase[3] = {1.0f, 0.9f, 0.05f};
    float plankMod = mixf(0.7f, 1.0f, tGround);
    float plankColor[3] = { plankBase[0] * plankMod, plankBase[1] * plankMod, plankBase[2] * plankMod };
    
    glColor3f(plankColor[0], plankColor[1], plankColor[2]);
    drawMesh(mesh_seesaw_plank);
    
    // Center grip/pivot area (orange accent)
    float gripBase[3] = {1.0f, 0.65f, 0.0f};
    float gripColor[3] = { gripBase[0] * plankMod, gripBase[1] * plankMod, gripBase[2] * plankMod };
    glColor3f(gripColor[0], gripColor[1], gripColor[2]);
    drawMesh(mesh_seesaw_grip);
    
    // Left seat - vivid red/magenta
    float leftSeatBase[3] = {1.0f, 0.1f, 0.35f};
    float seatMod = mixf(0.65f, 1.0f, tGround);
    float leftSeatColor[3] = { leftSeatBase[0] * seatMod, leftSeatBase[1] * seatMod, leftSeatBase[2] * seatMod };
    
    glColor3f(leftSeatColor[0], leftSeatColor[1], leftSeatColor[2]);
    drawMesh(mesh_seesaw_left_seat);
    
    // Left handle (red/magenta)
    drawMesh(mesh_seesaw_left_handle);
    
    // Right seat - bright lime green
    float rightSeatBase[3] = {0.2f, 0.95f, 0.25f};
    float rightSeatColor[3] = { rightSeatBase[0] * seatMod, rightSeatBase[1] * seatMod, rightSeatBase[2] * seatMod };
    
    glColor3f(rightSeatColor[0], rightSeatColor[1], rightSeatColor[2]);
    drawMesh(mesh_seesaw_right_seat);
    
    // Right handle (lime green)
    drawMesh(mesh_seesaw_right_handle);
    
    glPopMatrix();
    glPopMatrix();
}

// compute sky colors based on sunElevation
static void computeSkyColors(float e, float top[3], float bottom[3]) {
    const float nightTop[3]     = {0.02f, 0.06f, 0.25f};  // dark blue
    const float nightBottom[3]  = {0.005f, 0.02f, 0.10f}; // darker blue

    const float dayTop[3]       = {0.00f, 0.749f, 1.00f};   // #00BFFF
    const float dayBottom[3]    = {0.55f, 0.85f, 1.00f};    // light sky

    const float cSoftViolet[3]  = {0.749f, 0.439f, 1.000f}; // #BF70FF
    const float cPeachCoral[3]  = {1.000f, 0.541f, 0.439f}; // #FF8A70
    const float cSunsetRed[3]   = {1.000f, 0.231f, 0.188f}; // #FF3B30

    // Map elevation to a sunset progress that accelerates near the horizon
    float e_norm = (e + 1.0f) * 0.5f;                 // 0..1 (1=top)
    // Start transition near 3/4 window height, complete as it nears horizon
    float sSunset = 1.0f - smoothstepf(0.35f, 0.75f, e_norm); // later start keeps day blue longer
    sSunset = smoothstepf(0.0f, 1.0f, sSunset);       // extra ease

    // HSV mixing to avoid muddy blends
    auto sampleTop = [&](float t, float out[3]){
        const float stops[] = {0.0f, 0.55f, 1.0f};
        const float* cols[] = { dayTop, cSoftViolet, cPeachCoral };
        int n = 3;
        if (t <= stops[0]) { mixColorHSV(cols[0], cols[0], 0.0f, out); return; }
        if (t >= stops[n-1]) { mixColorHSV(cols[n-2], cols[n-1], 1.0f, out); return; }
        int i = 0; for (; i < n-1; ++i) if (t >= stops[i] && t <= stops[i+1]) break;
        float lt = (t - stops[i]) / (stops[i+1] - stops[i]);
        mixColorHSV(cols[i], cols[i+1], lt, out);
    };
    auto sampleBottom = [&](float t, float out[3]){
        const float stops[] = {0.0f, 0.65f, 1.0f};
        const float* cols[] = { dayBottom, cPeachCoral, cSunsetRed };
        int n = 3;
        if (t <= stops[0]) { mixColorHSV(cols[0], cols[0], 0.0f, out); return; }
        if (t >= stops[n-1]) { mixColorHSV(cols[n-2], cols[n-1], 1.0f, out); return; }
        int i = 0; for (; i < n-1; ++i) if (t >= stops[i] && t <= stops[i+1]) break;
        float lt = (t - stops[i]) / (stops[i+1] - stops[i]);
        mixColorHSV(cols[i], cols[i+1], lt, out);
    };

    float topTarget[3], bottomTarget[3];
    sampleTop(sSunset, topTarget);
    sampleBottom(sSunset, bottomTarget);

    if (e >= 0.0f) {
        float tEase = smoothstepf(0.0f, 1.0f, sSunset);
        mixColorHSV(dayTop, topTarget, tEase, top);
        mixColorHSV(dayBottom, bottomTarget, tEase, bottom);
    } else {
        float tNight = smoothstepf(0.0f, 1.0f, e + 1.0f);
        float fullTop[3], fullBottom[3];
        sampleTop(1.0f, fullTop);
        sampleBottom(1.0f, fullBottom);
        mixColorHSV(nightTop, fullTop, tNight, top);
        mixColorHSV(nightBottom, fullBottom, tNight, bottom);
    }
}

static void drawSkyGradient() {
    float top[3], bottom[3];
    float e_for_sky = sunVisible ? sunElevation : 1.0f;
    computeSkyColors(e_for_sky, top, bottom);

    glBegin(GL_QUADS);
        glColor3f(top[0], top[1], top[2]);    glVertex2f(-1.0f, 1.0f);
        glColor3f(top[0], top[1], top[2]);    glVertex2f( 1.0f, 1.0f);
        glColor3f(bottom[0], bottom[1], bottom[2]); glVertex2f( 1.0f, -1.0f);
        glColor3f(bottom[0], bottom[1], bottom[2]); glVertex2f(-1.0f, -1.0f);
    glEnd();
}

// Additive radial glow for simple bloom around bright sources (e.g., sun)
static void drawRadialGlow(float cx, float cy, float baseRadius, const float color[3], float strength) {
    if (strength <= 0.001f) return;
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    glPushMatrix();
    glTranslatef(cx, cy, 0.0f);
    float sx = 1.0f, sy = 1.0f;
    if (winAspect >= 1.0f) sx = 1.0f / winAspect; else sy = winAspect;
    glScalef(sx, sy, 1.0f);

    const int layers = 6;
    for (int i = 0; i < layers; ++i) {
        float t = (float)i / (float)(layers - 1);
        float r = baseRadius * mixf(1.4f, 4.5f, t);
        float a = strength * mixf(0.35f, 0.02f, t);
        float col[3] = { color[0] * a, color[1] * a, color[2] * a };
        drawCircle(0.0f, 0.0f, r, col);
    }

    glPopMatrix();
    glPopAttrib();
}

// Subtle horizon haze to warm the lower sky during sunset
static void drawHorizonHaze(float sunsetFactor) {
    if (sunsetFactor <= 0.0f) return;
    float alpha = mixf(0.0f, 0.25f, sunsetFactor);
    float col[4] = {1.0f, 0.55f, 0.18f, alpha};
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
        glColor4f(col[0], col[1], col[2], col[3]); glVertex2f(-1.0f, -0.2f);
        glColor4f(col[0], col[1], col[2], col[3]); glVertex2f( 1.0f, -0.2f);
        glColor4f(col[0], col[1], col[2], 0.0f);  glVertex2f( 1.0f,  0.5f);
        glColor4f(col[0], col[1], col[2], 0.0f);  glVertex2f(-1.0f,  0.5f);
    glEnd();
    glPopAttrib();
}

// Simple cloud drawing: overlapping circles. Clouds are animated horizontally.
struct Cloud { float bx, by, scale, speed; };
static Cloud clouds[] = {
    { -0.8f, 0.72f, 0.24f, 0.03f },
    {  0.15f, 0.78f, 0.18f, 0.02f },
    {  0.7f, 0.66f, 0.30f, 0.015f }
};

// animation timer for clouds (seconds)
static float lastCloudTime = 0.0f;

// === Star field ===
struct Star { float x, y, size, intensity, speed, phase; };
static std::vector<Star> gStars;
static bool gStarsInit = false;
static void initStars(int count = 160) {
    if (gStarsInit) return;
    gStars.clear(); gStars.reserve(count);
    auto frand = [](float seed){ float s = sinf(seed) * 43758.5453f; return s - floorf(s); };
    for (int i = 0; i < count; ++i) {
        float u = frand(12.9898f * (i + 1));
        float v = frand(78.233f  * (i + 11));
        float w = frand(45.164f  * (i + 37));
        float q = frand(9.751f   * (i + 97));
        // Spread across the sky, keep a few near horizon too but avoid ground band too much
        float x = -1.0f + 2.0f * u;
        float y = -0.05f + 1.05f * v; // mostly above ground
        float size = 0.0032f + 0.0045f * w; // ~1.5-3 px depending on res
        float intensity = 0.55f + 0.45f * (1.0f - w*w); // bias towards brighter
        float speed = 0.6f + 1.6f * q; // twinkle speed
        float phase = q * 6.2831853f;
        gStars.push_back({x,y,size,intensity,speed,phase});
    }
    gStarsInit = true;
}

static void drawStars(float nightFactor) {
    if (nightFactor <= 0.001f || gStars.empty()) return;
    float t = glutGet(GLUT_ELAPSED_TIME) * 0.001f;
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (const auto &s : gStars) {
        // gentle twinkle
        float tw = 0.8f + 0.2f * sinf(s.phase + s.speed * t);
        float alpha = nightFactor * s.intensity * tw;
        float col[3] = { alpha, alpha, alpha };

        glPushMatrix();
        glTranslatef(s.x, s.y, 0.0f);
        float sx = 1.0f, sy = 1.0f;
        if (winAspect >= 1.0f) sx = 1.0f / winAspect; else sy = winAspect;
        glScalef(sx, sy, 1.0f);
        drawCircle(0.0f, 0.0f, s.size, col);
        glPopMatrix();
    }
    glPopAttrib();
}

// === Cloud system ===
static void drawCloud(float cx, float cy, float s, float tGround) {
    // clouds are white but dim at night slightly
    float base[3] = {1.0f, 1.0f, 1.0f};
    float shade = mixf(0.7f, 1.0f, tGround);
    float col[3] = { base[0] * shade, base[1] * shade, base[2] * shade };
    // three overlapping circles
    drawCircle(cx, cy, 0.50f * s, col);
    drawCircle(cx - 0.35f * s, cy + 0.05f * s, 0.38f * s, col);
    drawCircle(cx + 0.35f * s, cy + 0.05f * s, 0.38f * s, col);
}

static void drawClouds(float tGround) {
    for (int i = 0; i < (int)(sizeof(clouds)/sizeof(clouds[0])); ++i) {
        Cloud &c = clouds[i];
        drawCloud(c.bx, c.by, c.scale, tGround);
    }
}

// Idle/update function moves clouds based on elapsed time so they always animate
static void updateCloudsIdle() {
    float t = glutGet(GLUT_ELAPSED_TIME) * 0.001f; // seconds
    if (lastCloudTime == 0.0f) lastCloudTime = t;
    float dt = t - lastCloudTime;
    lastCloudTime = t;
    if (dt <= 0.0f) return;
    for (int i = 0; i < (int)(sizeof(clouds)/sizeof(clouds[0])); ++i) {
        clouds[i].bx += clouds[i].speed * dt;
        // wrap around when off-screen (range approx -1.5 .. +1.5)
        if (clouds[i].bx > 1.5f) clouds[i].bx -= 3.0f;
        if (clouds[i].bx < -1.5f) clouds[i].bx += 3.0f;
    }
    // update sun automatically during playback if enabled (moved to utils)
    if (audio_isPlaying() && audio_getDurationSeconds() > 0.0 && audio_isAutoSunEnabled()) {
        using clock = std::chrono::steady_clock;
        double elapsed = std::chrono::duration<double>(clock::now() - audio_getStartTime()).count();
        if (elapsed >= audio_getDurationSeconds()) {
            // stop automatic mode at end of audio
            audio_setAutoSunEnabled(false);
            subtitle_enable(false);
            // ensure sun reaches final elevation (slightly below middle)
            float endE = -0.1f;
            sunElevation = endE;
        } else if (elapsed > 0.0) {
            float startE = 1.0f; // very top
            float endE = -0.1f;   // slightly below middle (one more frame down)
            double frac = elapsed / audio_getDurationSeconds();
            if (frac < 0.0) frac = 0.0; if (frac > 1.0) frac = 1.0;
            sunElevation = (float)((1.0 - frac) * startE + frac * endE);
        }
    }
    glutPostRedisplay();
}

void drawSlide(float pgShade) {
    const float leftCol[3]  = {0.56f * pgShade, 0.56f * pgShade, 0.56f * pgShade};
    const float rightCol[3] = {0.90f * pgShade, 0.90f * pgShade, 0.90f * pgShade};
    const float midColCenter[3] = { (0.90f + 0.56f) * 0.5f * pgShade, (0.90f + 0.56f) * 0.5f * pgShade, (0.90f + 0.56f) * 0.5f * pgShade };

    glColor3f(leftCol[0], leftCol[1], leftCol[2]);
    drawMesh(mesh_left_rail);

    glColor3f(rightCol[0], rightCol[1], rightCol[2]);
    drawMesh(mesh_right_rail);

    if (mesh_center_strip.count > 0) {
        glColor3f(midColCenter[0], midColCenter[1], midColCenter[2]);
        drawMesh(mesh_center_strip);
    }

    if (mesh_slide_end_fan.count > 0) {
        glColor3f(midColCenter[0], midColCenter[1], midColCenter[2]);
        drawMesh(mesh_slide_end_fan);
    }

    glLineWidth(1.0f);
    glColor3f(0.18f * pgShade, 0.18f * pgShade, 0.18f * pgShade);
    drawMesh(mesh_left_outline);
    drawMesh(mesh_right_outline);
}

// === Rendering ===
static void renderSceneContents() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawSkyGradient();

    // Stars appear from sunset to night, vanish towards day; draw behind clouds
    if (!gStarsInit) initStars();
    {
        float e_for_sky = sunVisible ? sunElevation : 1.0f;
        float e_norm = (e_for_sky + 1.0f) * 0.5f; // 0..1
        // Fully off by late sunset (~0.55), fully on by night (~0.30)
        float starFactor = 1.0f - smoothstepf(0.30f, 0.55f, e_norm);
        drawStars(starFactor);
    }

    // draw clouds in the sky (compute a small day/night factor)
    float cloudShade;
    if (!sunVisible) cloudShade = 1.0f;
    else if (sunElevation >= 0.0f) {
        float t = smoothstepf(0.0f, 1.0f, sunElevation);
        const float centerBias = 0.40f;
        cloudShade = centerBias + (1.0f - centerBias) * t;
    } else cloudShade = (sunElevation + 1.0f) / 2.0f;
    drawClouds(cloudShade);

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
    float e_norm_for_size = (sunElevation + 1.0f) * 0.5f;
    float radius = mixf(0.04f, 0.14f, smoothstepf(0.0f, 1.0f, e_norm_for_size));

    if (sunVisible) {
        glPushMatrix();
        glTranslatef(sunX, sunY, 0.0f);
        float sx = 1.0f, sy = 1.0f;
        if (winAspect >= 1.0f) sx = 1.0f / winAspect; else sy = winAspect;
        glScalef(sx, sy, 1.0f);
        drawCircle(0.0f, 0.0f, radius, sunColor);
        glPopMatrix();
    }
    if (sunVisible) {
        float e_norm = (sunElevation + 1.0f) * 0.5f;
        float sunsetFactor = 1.0f - smoothstepf(0.2f, 1.0f, e_norm);
        float bloomStrength = mixf(0.05f, 0.32f, sunsetFactor);
        drawRadialGlow(sunX, sunY, radius, sunColor, bloomStrength);
    }

    {
        float e_norm = (sunElevation + 1.0f) * 0.5f;
        float sunsetFactor = 1.0f - smoothstepf(0.25f, 0.95f, e_norm);
        drawHorizonHaze(sunsetFactor);
    }

    float groundTopTint[3];
    float gt_day[3] = {0.8f, 0.8f, 0.8f};
    float gt_night[3] = {0.12f, 0.12f, 0.15f};

    float tGround;
    if (!sunVisible) tGround = 1.0f;
    else if (sunElevation >= 0.0f) {
        const float centerBias = 0.40f;
        float t = smoothstepf(0.0f, 1.0f, sunElevation);
        tGround = centerBias + (1.0f - centerBias) * t;
    } else tGround = (sunElevation + 1.0f) / 2.0f;
    mixColor(gt_night, gt_day, tGround, groundTopTint);

    if (sunVisible) {
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

        float e_norm = (sunElevation + 1.0f) * 0.5f;
        float e_smooth = smoothstepf(0.0f, 1.0f, e_norm);
        float intensity = mixf(0.18f, 1.85f, e_smooth);
        float warmColor[3] = {1.0f, 0.55f, 0.18f};
        float finalColor[3];
        float warmFactor = mixf(0.35f, 0.85f, 1.0f - e_smooth);
        mixColor(sunColor, warmColor, warmFactor, finalColor);

        float lightDiffuse[4]  = { finalColor[0] * intensity, finalColor[1] * intensity, finalColor[2] * intensity, 1.0f };
        float lightAmbient[4]  = { finalColor[0] * (0.06f + 0.32f * e_smooth), finalColor[1] * (0.06f + 0.32f * e_smooth), finalColor[2] * (0.06f + 0.32f * e_smooth), 1.0f };
        float specMul = mixf(0.25f, 0.75f, e_smooth);
        float lightSpecular[4] = { specMul, specMul, specMul, 1.0f };
        glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
        glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
        glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);

        float linearAtt  = mixf(0.9f, 0.12f, e_smooth);
        float quadAtt    = mixf(0.9f, 0.03f, e_smooth);
        glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, linearAtt);
        glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, quadAtt);

        float matSpec[4] = { mixf(0.2f, 0.8f, e_smooth), mixf(0.2f, 0.8f, e_smooth), mixf(0.2f, 0.8f, e_smooth), 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mixf(12.0f, 48.0f, e_smooth));

        glPushMatrix(); glLoadIdentity(); glTranslatef(sunX, sunY, 0.6f);
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
    glNormal3f(0.0f, 0.0f, 1.0f);
    glColor3f(groundTopTint[0], groundTopTint[1], groundTopTint[2]);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);

    float rubberColor[3] = {0.6f, 0.18f, 0.18f};
    rubberColor[0] *= mixf(0.5f, 1.0f, tGround);
    rubberColor[1] *= mixf(0.5f, 1.0f, tGround);
    rubberColor[2] *= mixf(0.5f, 1.0f, tGround);

    // rubber trapezoid
    glBindBuffer(GL_ARRAY_BUFFER, vboHandles[1]);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, nullptr);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glColor3f(rubberColor[0], rubberColor[1], rubberColor[2]);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableClientState(GL_VERTEX_ARRAY);

    // trees
    drawTree(-0.85f, -0.4f, 1.0f, tGround);
    drawTree(-0.65f, -0.5f, 0.9f, tGround);
    drawTree(-0.45f, -0.45f, 1.1f, tGround);
    drawTree(0.45f, -0.45f, 1.0f, tGround);
    drawTree(0.65f, -0.5f, 0.95f, tGround);
    drawTree(0.85f, -0.4f, 1.05f, tGround);

    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);

    float pgShade = mixf(0.5f, 1.0f, tGround);

    glColor3f(0.8f * pgShade, 0.5f * pgShade, 0.1f * pgShade);
    drawMesh(mesh_roof_left);
    glColor3f(0.8f * pgShade, 0.5f * pgShade, 0.1f * pgShade);
    drawMesh(mesh_roof_mid);
    glColor3f(0.8f * pgShade, 0.5f * pgShade, 0.1f * pgShade);
    drawMesh(mesh_roof_right);

    glColor3f(0.7f * pgShade, 0.3f * pgShade, 0.1f * pgShade);
    drawMesh(mesh_inside_fence);

    glLineWidth(15.0f);
    glColor3f(0.6f * pgShade, 0.27f * pgShade, 0.1f * pgShade);
    drawMesh(mesh_inside_posts_thick);
    glLineWidth(3.0f);
    glColor3f(0.8f * pgShade, 0.5f * pgShade, 0.1f * pgShade);
    drawMesh(mesh_inside_posts_thin);

    glLineWidth(10.0f);
    glColor3f(0.6f * pgShade, 0.25f * pgShade, 0.1f * pgShade);
    drawMesh(mesh_inside_horiz_top);
    drawMesh(mesh_inside_horiz_bottom);

    glColor3f(0.7f * pgShade, 0.3f * pgShade, 0.1f * pgShade);
    drawMesh(mesh_outside_right_fence);

    glLineWidth(28.0f);
    glColor3f(0.6f * pgShade, 0.27f * pgShade, 0.1f * pgShade);
    drawMesh(mesh_outside_posts_thick);
    glLineWidth(3.0f);
    glColor3f(0.8f * pgShade, 0.5f * pgShade, 0.1f * pgShade);
    drawMesh(mesh_outside_posts_thin);

    glColor3f(0.55f * pgShade, 0.27f * pgShade, 0.07f * pgShade);
    drawMesh(mesh_found_out_right);
    drawMesh(mesh_found_out_left);
    drawMesh(mesh_found_left);
    drawMesh(mesh_found_mid);
    drawMesh(mesh_found_right);

    glColor3f(0.55f * pgShade, 0.27f * pgShade, 0.07f * pgShade);
    glLineWidth(3.0f);
    drawMesh(mesh_floor_line);

    glColor3f(0.8f * pgShade, 0.5f * pgShade, 0.1f * pgShade);
    drawMesh(mesh_climb_ramp);

    glColor3f(0.7f * pgShade, 0.7f * pgShade, 0.7f * pgShade);
    drawMesh(mesh_slide_floor);
    drawSlide(pgShade);

    glPopAttrib();

    drawSeesaw(0.5f, -0.5f, seesawAngle, 0.7f, tGround);
}

// === Input Callbacks ===
// Special keys handler: arrow up/down control sun elevation; left/right nudge seesaw
static void specialKeys(int key, int /*x*/, int /*y*/) {
    const float step = 0.05f;
    if (key == GLUT_KEY_UP) {
        // manual control disables automatic sun animation
        audio_setAutoSunEnabled(false);
        if (!sunVisible) {
            sunVisible = true;
            sunElevation = -1.0f + step;
        } else {
            sunElevation += step;
            if (sunElevation > 1.0f) sunElevation = 1.0f;
        }
    } else if (key == GLUT_KEY_DOWN) {
        // manual control disables automatic sun animation
        audio_setAutoSunEnabled(false);
        if (!sunVisible) {
            sunVisible = true;
            sunElevation = 1.0f - step;
        } else {
            sunElevation -= step;
            if (sunElevation < -1.0f) sunElevation = -1.0f;
        }
    } else if (key == GLUT_KEY_LEFT) {
        // tilt seesaw left
        seesawAngle -= 5.0f;
    } else if (key == GLUT_KEY_RIGHT) {
        // tilt seesaw right
        seesawAngle += 5.0f;
    }
    glutPostRedisplay();
}

static void keyboard(unsigned char key, int /*x*/, int /*y*/) {
    if (key == 'a' || key == 'A') {
        audio_setAutoSunEnabled(!audio_isAutoSunEnabled());
        std::printf("Auto sun %s\n", audio_isAutoSunEnabled() ? "enabled" : "disabled");
    }
}

static void display() {
    // Render scene directly to default framebuffer
    renderSceneContents();
    // draw subtitle overlay (utils decides whether to actually draw)
    subtitle_draw(winW, winH);
    // draw UI buttons (bottom-right)
    drawUIButtons(winW, winH);
    glutSwapBuffers();
}

static void reshape(int w, int h) {
    if (h == 0) h = 1;
    winAspect = (float)w / (float)h;
    winW = w; winH = h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // (No post-process to resize)
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    // Request a depth buffer so embedded 3D scenes (cake) render correctly
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("Waguri's Favorite Place");
    
    glewExperimental = GL_TRUE; 
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        std::fprintf(stderr, "GLEW initialization Failed: %s\n", glewGetErrorString(glewErr));
        return 1;
    }

    // Create VBO/VAO resource
    createResources();

    // initialize subtitle text and timed entries for scene1
    subtitle_setText("Subaru and I would often come here to play");
    {
        std::vector<SubtitleEntry> entries;
        SubtitleEntry e;
        e.text = "Subaru and I would often come here to play."; e.start = 0.0f; e.end = 3.0f; entries.push_back(e);
        e.text = "I really love this park."; e.start = 4.0f; e.end = 6.0f; entries.push_back(e);
        e.text = "I have made a lot of memories here..."; e.start = 9.5f; e.end = 11.5f; entries.push_back(e);
        e.text = "So I was hoping I could add another one today."; e.start = 12.0f; e.end = 15.0f; entries.push_back(e);
        subtitle_setEntries(entries);
    }

    glutDisplayFunc(display);
    glutSpecialFunc(specialKeys);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(onMouseClick);
    glutIdleFunc(updateCloudsIdle);
    glutReshapeFunc(reshape);

    glutMainLoop();
    return 0;
}
