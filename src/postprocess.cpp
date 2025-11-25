#include "postprocess.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <GL/glew.h>

// Simple embedded GLSL sources (GLSL 120 for compatibility)
static const char *vs_src = R"GLSL(
#version 120
attribute vec2 aPos;
varying vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";

static const char *fs_extract = R"GLSL(
#version 120
uniform sampler2D uScene;
uniform float uThreshold;
varying vec2 vUV;
void main() {
    vec3 c = texture2D(uScene, vUV).rgb;
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    if (lum > uThreshold) {
        // soft knee
        float k = (lum - uThreshold) / max(lum, 1e-6);
        gl_FragColor = vec4(c * k, 1.0);
    } else {
        gl_FragColor = vec4(0.0);
    }
}
)GLSL";

static const char *fs_blur = R"GLSL(
#version 120
uniform sampler2D uTex;
uniform vec2 uTexelOffset; // direction / texel
varying vec2 vUV;
void main() {
    // 9-tap gaussian-ish weights
    float w0 = 0.2270270270;
    float w1 = 0.1945945946;
    float w2 = 0.1216216216;
    float w3 = 0.0540540541;
    vec3 sum = texture2D(uTex, vUV).rgb * w0;
    sum += texture2D(uTex, vUV + uTexelOffset * 1.0).rgb * w1;
    sum += texture2D(uTex, vUV - uTexelOffset * 1.0).rgb * w1;
    sum += texture2D(uTex, vUV + uTexelOffset * 2.0).rgb * w2;
    sum += texture2D(uTex, vUV - uTexelOffset * 2.0).rgb * w2;
    sum += texture2D(uTex, vUV + uTexelOffset * 3.0).rgb * w3;
    sum += texture2D(uTex, vUV - uTexelOffset * 3.0).rgb * w3;
    gl_FragColor = vec4(sum, 1.0);
}
)GLSL";

static const char *fs_compose = R"GLSL(
#version 120
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomIntensity;
varying vec2 vUV;

vec3 tonemap_ACES(vec3 x) {
    // small ACES-ish curve approximation
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    vec3 scene = texture2D(uScene, vUV).rgb;
    vec3 bloom = texture2D(uBloom, vUV).rgb;
    // apply intensity then compress bloom contribution to avoid washout
    bloom *= uBloomIntensity;
    bloom = bloom / (1.0 + bloom); // simple Reinhard-style compression
    vec3 col = scene + bloom;
    col = tonemap_ACES(col);
    // gamma
    col = pow(col, vec3(1.0/2.2));
    gl_FragColor = vec4(col, 1.0);
}
)GLSL";

// GL objects
static GLuint prog_vs = 0;
static GLuint prog_extract = 0;
static GLuint prog_blur = 0;
static GLuint prog_compose = 0;

static GLuint quadVBO = 0;
static GLuint quadVAO = 0;

// FBOs and textures
static GLuint sceneTex = 0;
static GLuint sceneFBO = 0;
static GLuint pingTex[2] = {0,0};
static GLuint pingFBO[2] = {0,0};
static int ppWidth = 0;
static int ppHeight = 0;
static int downW = 0;
static int downH = 0;

// helper: compile and link
static GLuint compileShader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024]; GLsizei len = 0; glGetShaderInfoLog(s, 1024, &len, buf);
        std::fprintf(stderr, "Shader compile error: %s\n", buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint linkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "aPos");
    glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[1024]; GLsizei len = 0; glGetProgramInfoLog(p, 1024, &len, buf);
        std::fprintf(stderr, "Program link error: %s\n", buf);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

void ppInit() {
    // compile shaders
    GLuint vs = compileShader(GL_VERTEX_SHADER, vs_src);
    GLuint fsExt = compileShader(GL_FRAGMENT_SHADER, fs_extract);
    GLuint fsBlur = compileShader(GL_FRAGMENT_SHADER, fs_blur);
    GLuint fsComp = compileShader(GL_FRAGMENT_SHADER, fs_compose);
    if (!vs || !fsExt || !fsBlur || !fsComp) {
        std::fprintf(stderr, "Failed to compile postprocess shaders.\n");
        return;
    }
    prog_extract = linkProgram(vs, fsExt);
    prog_blur = linkProgram(vs, fsBlur);
    prog_compose = linkProgram(vs, fsComp);
    // we can keep vs attached to each program or delete here
    glDeleteShader(vs);
    glDeleteShader(fsExt);
    glDeleteShader(fsBlur);
    glDeleteShader(fsComp);

    // create screen quad VBO (two triangles covering clip space)
    GLfloat quadVerts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (GLEW_ARB_vertex_array_object || GLEW_VERSION_3_0) {
        glGenVertexArrays(1, &quadVAO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
}

static void createTexture(GLuint &tex, int w, int h, GLint internal=GL_RGBA, GLenum filter=GL_LINEAR) {
    if (tex != 0) glDeleteTextures(1, &tex);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void createFBO(GLuint &fbo, GLuint tex) {
    if (fbo != 0) glDeleteFramebuffers(1, &fbo);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "FBO incomplete: %u\n", st);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ppResize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    ppWidth = width; ppHeight = height;
    // downsample factor 2 for bloom
    downW = std::max(1, ppWidth / 2);
    downH = std::max(1, ppHeight / 2);

    // create scene texture and FBO at full res
    createTexture(sceneTex, ppWidth, ppHeight, GL_RGBA, GL_LINEAR);
    createFBO(sceneFBO, sceneTex);

    // create ping-pong textures at downsampled size
    createTexture(pingTex[0], downW, downH, GL_RGBA, GL_LINEAR);
    createFBO(pingFBO[0], pingTex[0]);
    createTexture(pingTex[1], downW, downH, GL_RGBA, GL_LINEAR);
    createFBO(pingFBO[1], pingTex[1]);
}

void ppBeginScene() {
    if (sceneFBO == 0) return;
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
    glViewport(0, 0, ppWidth, ppHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void ppEndScene() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void drawFullScreenQuad() {
    if (quadVAO != 0) {
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(2, GL_FLOAT, 0, nullptr);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableClientState(GL_VERTEX_ARRAY);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

// Run extract -> blur (separable) -> compose and present
void ppApplyAndPresent(float bloomThreshold, float bloomIntensity, int blurPasses) {
    // 1) extract bright areas into pingTex[0] (downsampled)
    glViewport(0,0,downW,downH);
    glBindFramebuffer(GL_FRAMEBUFFER, pingFBO[0]);
    glUseProgram(prog_extract);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTex);
    GLint loc = glGetUniformLocation(prog_extract, "uScene"); if (loc >= 0) glUniform1i(loc, 0);
    loc = glGetUniformLocation(prog_extract, "uThreshold"); if (loc >= 0) glUniform1f(loc, bloomThreshold);
    drawFullScreenQuad();

    // 2) blur: perform separable passes ping-ponging between pingTex[0] and pingTex[1]
    bool horizontal = true;
    int read = 0, write = 1;
    for (int i = 0; i < blurPasses; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingFBO[write]);
        glViewport(0,0,downW,downH);
        glUseProgram(prog_blur);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pingTex[read]);
        GLint locT = glGetUniformLocation(prog_blur, "uTex"); if (locT >= 0) glUniform1i(locT, 0);
        // set texel offset depending on horizontal/vertical
        float tx = horizontal ? (1.0f / (float)downW) : 0.0f;
        float ty = horizontal ? 0.0f : (1.0f / (float)downH);
        GLint locOff = glGetUniformLocation(prog_blur, "uTexelOffset"); if (locOff >= 0) glUniform2f(locOff, tx, ty);
        drawFullScreenQuad();
        // swap
        std::swap(read, write);
        horizontal = !horizontal;
    }

    // 3) composite: bind default framebuffer and draw combined image
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0,0,ppWidth,ppHeight);
    glUseProgram(prog_compose);
    // scene texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTex);
    GLint locS = glGetUniformLocation(prog_compose, "uScene"); if (locS >= 0) glUniform1i(locS, 0);
    // bloom texture is in 'read' (last rendered texture)
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pingTex[read]);
    GLint locB = glGetUniformLocation(prog_compose, "uBloom"); if (locB >= 0) glUniform1i(locB, 1);
    GLint locI = glGetUniformLocation(prog_compose, "uBloomIntensity"); if (locI >= 0) glUniform1f(locI, bloomIntensity);

    // disable lighting and other fixed-function state that might interfere
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
    drawFullScreenQuad();

    // unbind textures and program
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
}

void ppShutdown() {
    // cleanup omitted for brevity
}
