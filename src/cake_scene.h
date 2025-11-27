#pragma once

// Embedded cake scene API (call from main app to switch scenes)
void cake_init_embedded();
void cake_display();
void cake_reshape(int w, int h);
void cake_keyboard(unsigned char key, int x, int y);
void cake_special(int key, int x, int y);
void cake_idle();
