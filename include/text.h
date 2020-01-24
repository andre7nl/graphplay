// text.h
#ifndef TEXT_H
#define TEXT_H

extern void display_text(const unsigned __int32 timerange, const int frames, const int mode, int w, int h);
extern int init_text_resources(unsigned int program, unsigned int vbo);
extern void free_text_resources(unsigned int program);
//extern  void render_text(const char *text, atlas * a, float x, float y, float sx, float sy);
#endif