#include <GL/glew.h>
#include "utils.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include <windows.h>
#include <mmsystem.h>
#include "cake_scene.h"

/* BUILD COMMAND

g++ src/main.cpp src/utils.cpp -Ilibs/include -Ilibs/freeglut/include -Llibs/freeglut/lib/x64 -Llibs -lglew32 -lfreeglut -lopengl32 -lglu32 -o app.exe

*/

float mixf(float a, float b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return a + (b - a) * t;
}

void mixColor(const float a[3], const float b[3], float t, float out[3]) {
    for (int i = 0; i < 3; ++i) out[i] = mixf(a[i], b[i], t);
}

void drawCircle(float cx, float cy, float radius, const float color[3]) {
    glColor3fv(color);
    // 2D geometry lies on XY plane; set normal so fixed-function lighting affects it
    glNormal3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    const int segs = 48;
    for (int i = 0; i <= segs; ++i) {
        float a = (float)i / (float)segs * 2.0f * 3.14159265f;
        glVertex2f(cx + cosf(a) * radius, cy + sinf(a) * radius);
    }
    glEnd();
}

// Draw a persistent mesh
void drawMesh(const Mesh &m) {
    if (m.count == 0) return;
    if (m.vao != 0) {
        glBindVertexArray(m.vao);
        glDrawArrays(m.mode, 0, m.count);
        glBindVertexArray(0);
    } else if (m.vbo != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(2, GL_FLOAT, 0, nullptr);
        glDrawArrays(m.mode, 0, m.count);
        glDisableClientState(GL_VERTEX_ARRAY);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

// Draw the shared unit quad mesh transformed to position/size
void drawUnitQuad(float cx, float cy, float w, float h) {
    if (mesh_unit_quad.count == 0) return;
    glPushMatrix();
    glTranslatef(cx, cy, 0.0f);
    glScalef(w, h, 1.0f);
    drawMesh(mesh_unit_quad);
    glPopMatrix();
}

// ---------------------------
// Subtitle implementation
// ---------------------------
// playback start timestamp for subtitle sync (declared early so SubtitleImpl can reference it)
static std::chrono::steady_clock::time_point playbackStartTime = std::chrono::steady_clock::time_point();

class SubtitleImpl {
public:
    struct Entry { std::string text; float start; float end; };
    SubtitleImpl() : text(""), font(GLUT_BITMAP_HELVETICA_18), paddingX(12), paddingY(6), yOffset(36), enabled(false) {}
    void setText(const std::string &t) { text = t; }
    void setEntries(const std::vector<SubtitleEntry> &e) {
        entries.clear();
        for (const auto &en : e) entries.push_back({en.text, en.start, en.end});
    }
    void enable(bool e) { enabled = e; }
    void draw(int windowW, int windowH) {
        std::string drawText = text;
        if (!entries.empty()) {
            if (!enabled) return;
            using clock = std::chrono::steady_clock;
            auto now = clock::now();
            double elapsed = std::chrono::duration<double>(now - playbackStartTime).count();
            bool found = false;
            for (const auto &en : entries) {
                if (elapsed >= en.start && elapsed <= en.end) { drawText = en.text; found = true; break; }
            }
            if (!found) return;
        } else {
            if (text.empty()) return;
        }

        int textW = glutBitmapLength(font, (const unsigned char*)drawText.c_str());
        int textH = 18;
        int cx = windowW / 2;
        int left = cx - (textW / 2) - paddingX;
        int right = cx + (textW / 2) + paddingX;
        int bottom = yOffset - paddingY;
        int top = bottom + textH + paddingY * 2;

        glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, windowW, 0, windowH, -1, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        glColor4f(0.0f,0.0f,0.0f,0.6f);
        glBegin(GL_QUADS); glVertex2i(left,bottom); glVertex2i(right,bottom); glVertex2i(right,top); glVertex2i(left,top); glEnd();

        glColor3f(1.0f,1.0f,1.0f);
        glRasterPos2i(left + paddingX, bottom + paddingY);
        for (char c : drawText) glutBitmapCharacter(font, c);

        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
        glPopAttrib();
    }
private:
    std::string text;
    std::vector<Entry> entries;
    void *font;
    int paddingX, paddingY, yOffset;
    bool enabled;
} subtitleImpl;

// ---------------------------
// Audio and UI implementation
// ---------------------------
static bool audioPlaying = false;
static double audioDurationSeconds = 0.0;
static bool autoSunEnabled = true;
static int lastSceneIndex = 0;

// forward UI buttons
struct UIButton { int cx, cy, r; };
static UIButton btnPlay, btnRestart, btnNext;

// forward: compute WAV duration
static double getWavDurationSecondsImpl(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return 0.0;
    unsigned char buf[12]; if (fread(buf,1,12,f) != 12) { fclose(f); return 0.0; }
    if (memcmp(buf, "RIFF", 4) != 0 || memcmp(buf+8, "WAVE", 4) != 0) { fclose(f); return 0.0; }
    unsigned int dataBytes = 0; unsigned int sampleRate = 0; unsigned short channels = 0; unsigned short bitsPerSample = 0;
    while (!feof(f)) {
        unsigned char hdr[8]; if (fread(hdr,1,8,f) != 8) break;
        unsigned int chunkSize = *(unsigned int*)(hdr+4);
        if (memcmp(hdr, "fmt ", 4) == 0) {
            unsigned char *fmt = (unsigned char*)malloc(chunkSize);
            if (fread(fmt,1,chunkSize,f) != chunkSize) { free(fmt); break; }
            if (chunkSize >= 16) { channels = *(unsigned short*)(fmt+2); sampleRate = *(unsigned int*)(fmt+4); bitsPerSample = *(unsigned short*)(fmt+14); }
            free(fmt);
        } else if (memcmp(hdr, "data", 4) == 0) {
            dataBytes = chunkSize; fseek(f, chunkSize, SEEK_CUR);
        } else fseek(f, chunkSize, SEEK_CUR);
    }
    fclose(f);
    if (sampleRate == 0 || channels == 0 || bitsPerSample == 0 || dataBytes == 0) return 0.0;
    double bytesPerSec = (double)sampleRate * (double)channels * ((double)bitsPerSample/8.0);
    return (double)dataBytes / bytesPerSec;
}

// Exposed wrappers
double getWavDurationSeconds(const char *path) { return getWavDurationSecondsImpl(path); }

bool audio_isPlaying() { return audioPlaying; }
double audio_getDurationSeconds() { return audioDurationSeconds; }
std::chrono::steady_clock::time_point audio_getStartTime() { return playbackStartTime; }

void audio_setAutoSunEnabled(bool enable) { autoSunEnabled = enable; }
bool audio_isAutoSunEnabled() { return autoSunEnabled; }

void subtitle_setText(const std::string &t) { subtitleImpl.setText(t); }
void subtitle_setEntries(const std::vector<SubtitleEntry> &e) { subtitleImpl.setEntries(e); }
void subtitle_draw(int w, int h) { subtitleImpl.draw(w,h); }
void subtitle_enable(bool en) { subtitleImpl.enable(en); }

// audio play/restart
void audio_restartScene() {
    if (lastSceneIndex > 0) audio_playScene(lastSceneIndex);
}

void audio_playScene(int sceneIndex) {
    lastSceneIndex = sceneIndex;
    // stop any playback
    PlaySound(NULL, NULL, 0);
    mciSendStringA("close sceneaudio", NULL, 0, NULL);
    char path[512]; FILE *f = NULL;
    snprintf(path,sizeof(path),"audio\\scene%d.wav", sceneIndex); f = fopen(path,"rb");
    if (!f) { snprintf(path,sizeof(path),"scene%d.wav", sceneIndex); f = fopen(path,"rb"); }
    if (f) {
        fclose(f);
        audioDurationSeconds = getWavDurationSecondsImpl(path);
        char cmdOpen[1024]; snprintf(cmdOpen,sizeof(cmdOpen),"open \"%s\" type waveaudio alias sceneaudio", path);
        if (mciSendStringA(cmdOpen, NULL, 0, NULL) == 0) {
            if (mciSendStringA("play sceneaudio", NULL, 0, NULL) == 0) {
                audioPlaying = true; playbackStartTime = std::chrono::steady_clock::now(); autoSunEnabled = true; subtitleImpl.enable(true);
                return;
            }
            mciSendStringA("close sceneaudio", NULL, 0, NULL);
        }
        if (PlaySoundA(path, NULL, SND_FILENAME | SND_ASYNC)) { audioPlaying = true; playbackStartTime = std::chrono::steady_clock::now(); autoSunEnabled = true; subtitleImpl.enable(true); return; }
        audioPlaying = false;
    }
    // try MP3
    snprintf(path,sizeof(path),"audio\\scene%d.mp3", sceneIndex); f = fopen(path,"rb");
    if (f) { fclose(f); char cmd[1024]; snprintf(cmd,sizeof(cmd),"open \"%s\" type mpegvideo alias sceneaudio", path); if (mciSendStringA(cmd,NULL,0,NULL)==0) { if (mciSendStringA("play sceneaudio",NULL,0,NULL)==0) { char buf[128]={0}; if (mciSendStringA("status sceneaudio length", buf, sizeof(buf), NULL)==0) { long ms=atol(buf); if (ms>0) audioDurationSeconds=ms/1000.0; } audioPlaying=true; playbackStartTime=std::chrono::steady_clock::now(); autoSunEnabled=true; subtitleImpl.enable(true); return; } mciSendStringA("close sceneaudio",NULL,0,NULL);} audioPlaying=false; }
}

// UI drawing and mouse handling
void drawUIButtons(int windowW, int windowH) {
        int margin = 16, spacing = 12, radius = 20;
        int x = windowW - margin - radius; int y = margin + radius;
        btnNext = { x, y, radius };
        btnRestart = { x - (radius*2 + spacing), y, radius };
        btnPlay = { x - (radius*4 + spacing*2), y, radius };

        glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT);
        glDisable(GL_LIGHTING);
        // Ensure the UI overlay is drawn on top of 3D scene contents
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLineWidth(2.5f);

        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, windowW, 0, windowH, -1, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        // helper: draw filled background circle with alpha
        auto drawBg = [&](int cx,int cy,int r,float a){ glColor4f(0.06f,0.06f,0.06f,a); glBegin(GL_TRIANGLE_FAN); glVertex2i(cx,cy); for(int i=0;i<=48;i++){ float t=(float)i/48.0f*2.0f*3.14159265f; glVertex2f(cx+cosf(t)*r, cy+sinf(t)*r);} glEnd(); };
        // draw button backgrounds
        drawBg(btnPlay.cx, btnPlay.cy, btnPlay.r, 0.78f);
        drawBg(btnRestart.cx, btnRestart.cy, btnRestart.r, 0.72f);
        drawBg(btnNext.cx, btnNext.cy, btnNext.r, 0.72f);

        // draw clear white icons, larger and filled for visibility
        glColor3f(1.0f, 1.0f, 1.0f);

        // Play: single filled triangle
        glBegin(GL_TRIANGLES);
            glVertex2f(btnPlay.cx - 7.0f, btnPlay.cy - 9.0f);
            glVertex2f(btnPlay.cx - 7.0f, btnPlay.cy + 9.0f);
            glVertex2f(btnPlay.cx + 10.0f, btnPlay.cy);
        glEnd();

        // Restart: draw an outlined arc and a filled arrow tip 
        glLineWidth(3.0f);
        glBegin(GL_LINE_STRIP);
            for (int i = 30; i <= 300; i += 6) {
                float a = i * 3.14159265f / 180.0f;
                glVertex2f(btnRestart.cx + cosf(a) * (btnRestart.r - 7), btnRestart.cy + sinf(a) * (btnRestart.r - 7));
            }
        glEnd();
        glLineWidth(1.0f);
        // restart arrow tip (filled)
        {
            float a_end = 300.0f * 3.14159265f / 180.0f;
            float ex = btnRestart.cx + cosf(a_end) * (btnRestart.r - 7);
            float ey = btnRestart.cy + sinf(a_end) * (btnRestart.r - 7);
            float tri = 5.0f;
            glBegin(GL_TRIANGLES);
                glVertex2f(ex - tri, ey - tri);
                glVertex2f(ex - tri, ey + tri);
                glVertex2f(ex + tri, ey);
            glEnd();
        }

        // Next: two filled triangles (double chevron)
        glBegin(GL_TRIANGLES);
            // left chevron
            glVertex2f(btnNext.cx - 10.0f, btnNext.cy - 8.0f);
            glVertex2f(btnNext.cx - 10.0f, btnNext.cy + 8.0f);
            glVertex2f(btnNext.cx - 0.5f, btnNext.cy);
            // right chevron
            glVertex2f(btnNext.cx - 2.0f, btnNext.cy - 8.0f);
            glVertex2f(btnNext.cx - 2.0f, btnNext.cy + 8.0f);
            glVertex2f(btnNext.cx + 10.0f, btnNext.cy);
        glEnd();

        glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
        glPopAttrib();
}

void onMouseClick(int button, int state, int x, int y) {
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;
    int wy = glutGet(GLUT_WINDOW_HEIGHT) - y; // window height might be queried here
    auto hit = [&](const UIButton &b)->bool{ int dx = x - b.cx; int dy = wy - b.cy; return (dx*dx + dy*dy) <= (b.r*b.r); };
    if (hit(btnPlay)) { audio_playScene(1); subtitleImpl.enable(audioPlaying); }
    else if (hit(btnRestart)) { audio_restartScene(); subtitleImpl.enable(audioPlaying); }
    else if (hit(btnNext)) {
        // Switch to cake scene embedded in same window
        cake_init_embedded();
        // set GLUT callbacks to cake scene handlers
        glutDisplayFunc(cake_display);
        glutReshapeFunc(cake_reshape);
        glutKeyboardFunc(cake_keyboard);
        glutSpecialFunc(cake_special);
        glutIdleFunc(cake_idle);
        subtitleImpl.enable(false);
        // Immediately set projection for current window size
        int w = glutGet(GLUT_WINDOW_WIDTH);
        int h = glutGet(GLUT_WINDOW_HEIGHT);
        if (w > 0 && h > 0) cake_reshape(w, h);
        glutPostRedisplay();
    }
}

