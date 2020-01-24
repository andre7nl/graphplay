#include <text.h>

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>

#include <GL/glew.h>
//#include <GL/freeglut.h>
#include <SDL.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "common/shader_utils.h"
// GLuint program;
GLint attribute_coord;
GLint uniform_tex;
GLint uniform_color;

struct point
{
	GLfloat x;
	GLfloat y;
	GLfloat s;
	GLfloat t;
};

GLuint vbo_txt;

FT_Library ft;
FT_Face face;

// Maximum texture width
#define MAXWIDTH 1024

const char *fontfilename = "FreeSans.ttf";

/**
 * The atlas struct holds a texture that contains the visible US-ASCII characters
 * of a certain font rendered with a certain character height.
 * It also contains an array that contains all the information necessary to
 * generate the appropriate vertex and texture coordinates for each character.
 *
 * After the constructor is run, you don't need to use any FreeType functions anymore.
 */
struct atlas
{
	GLuint tex; // texture object

	unsigned int w; // width of texture in pixels
	unsigned int h; // height of texture in pixels

	struct
	{
		float ax; // advance.x
		float ay; // advance.y

		float bw; // bitmap.width;
		float bh; // bitmap.height;

		float bl; // bitmap_left;
		float bt; // bitmap_top;

		float tx; // x offset of glyph in texture coordinates
		float ty; // y offset of glyph in texture coordinates
	} c[128];	 // character information

	atlas(FT_Face face, int height)
	{
		FT_Set_Pixel_Sizes(face, 0, height);
		FT_GlyphSlot g = face->glyph;

		unsigned int roww = 0;
		unsigned int rowh = 0;
		w = 0;
		h = 0;

		memset(c, 0, sizeof c);

		/* Find minimum size for a texture holding all visible ASCII characters */
		for (int i = 32; i < 128; i++)
		{
			if (FT_Load_Char(face, i, FT_LOAD_RENDER))
			{
				fprintf(stderr, "Loading character %c failed!\n", i);
				continue;
			}
			if (roww + g->bitmap.width + 1 >= MAXWIDTH)
			{
				w = std::max(w, roww);
				h += rowh;
				roww = 0;
				rowh = 0;
			}
			roww += g->bitmap.width + 1;
			rowh = std::max(rowh, g->bitmap.rows);
		}

		w = std::max(w, roww);
		h += rowh;

		/* Create a texture that will be used to hold all ASCII glyphs */
		glActiveTexture(GL_TEXTURE0);
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glUniform1i(uniform_tex, 0);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);

		/* We require 1 byte alignment when uploading texture data */
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		/* Clamping to edges is important to prevent artifacts when scaling */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		/* Linear filtering usually looks best for text */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		/* Paste all glyph bitmaps into the texture, remembering the offset */
		int ox = 0;
		int oy = 0;

		rowh = 0;

		for (int i = 32; i < 128; i++)
		{
			if (FT_Load_Char(face, i, FT_LOAD_RENDER))
			{
				fprintf(stderr, "Loading character %c failed!\n", i);
				continue;
			}

			if (ox + g->bitmap.width + 1 >= MAXWIDTH)
			{
				oy += rowh;
				rowh = 0;
				ox = 0;
			}

			glTexSubImage2D(GL_TEXTURE_2D, 0, ox, oy, g->bitmap.width, g->bitmap.rows, GL_ALPHA, GL_UNSIGNED_BYTE, g->bitmap.buffer);
			c[i].ax = g->advance.x >> 6;
			c[i].ay = g->advance.y >> 6;

			c[i].bw = g->bitmap.width;
			c[i].bh = g->bitmap.rows;

			c[i].bl = g->bitmap_left;
			c[i].bt = g->bitmap_top;

			c[i].tx = ox / (float)w;
			c[i].ty = oy / (float)h;

			rowh = std::max(rowh, g->bitmap.rows);
			ox += g->bitmap.width + 1;
		}

		fprintf(stderr, "Generated a %d x %d (%d kb) texture atlas\n", w, h, w * h / 1024);
	}

	~atlas()
	{
		glDeleteTextures(1, &tex);
	}
};

atlas *a48;
atlas *a24;
atlas *a12;

//int init_text_resources(GLuint vbo) {
int init_text_resources(unsigned int program, unsigned int vbo_in)
{
	/* Initialize the FreeType2 library */
	if (FT_Init_FreeType(&ft))
	{
		fprintf(stderr, "Could not init freetype library\n");
		return 0;
	}

	/* Load a font */
	if (FT_New_Face(ft, fontfilename, 0, &face))
	{
		fprintf(stderr, "Could not open font %s\n", fontfilename);
		return 0;
	}

	// program = create_program("text.v.glsl", "text.f.glsl");
	// if(program == 0)
	// 	return 0;

	attribute_coord = get_attrib(program, "coord");
	uniform_tex = get_uniform(program, "mytexture");
	uniform_color = get_uniform(program, "tx_color");

	if (attribute_coord == -1 || uniform_tex == -1 || uniform_color == -1)
		return 0;

	//glGenBuffers(1, &vbo_txt);
	vbo_txt = vbo_in;

	/* Create texture atlasses for several font sizes */
	a48 = new atlas(face, 48);
	a24 = new atlas(face, 24);
	a12 = new atlas(face, 12);

	return 1;
}

/**
 * Render text using the currently loaded font and currently set font size.
 * Rendering starts at coordinates (x, y), z is always 0.
 * The pixel coordinates that the FreeType2 library uses are scaled by (sx, sy).
 */
void render_text(const char *text, atlas *a, float x, float y, float sx, float sy)
{
	const uint8_t *p, *space, *digit;
	digit = (const uint8_t *)"8";
	float xdigit = a->c[*digit].ax;
	space = (const uint8_t *)" ";

	/* Use the texture containing the atlas */
	glBindTexture(GL_TEXTURE_2D, a->tex);
	glUniform1i(uniform_tex, 0);

	/* Set up the VBO for our vertex data */
	glEnableVertexAttribArray(attribute_coord);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_txt);
	glVertexAttribPointer(attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);

	//point coords[6 * strlen(text)];
	//point coords[500];
	point *coords = new point[6 * strlen(text)];
	//fprintf(stderr, "strlen(text) %i\n", (int)strlen(text));
	int c = 0, nc = 0;

	/* Loop through all characters */
	for (p = (const uint8_t *)text; *p; p++)
	{
		/* Calculate the vertex and texture coordinates */
		float x2 = x + a->c[*p].bl * sx;
		float y2 = -y - a->c[*p].bt * sy;
		float w = a->c[*p].bw * sx;
		float h = a->c[*p].bh * sy;

		/* Advance the cursor to the start of the next character */
		nc++;
		if (*p == *space && nc < 3)
		{
			x += xdigit * sx;
		}
		else
		{
			x += a->c[*p].ax * sx;
		};
		// x += a->c[*p].ax * sx;
		y += a->c[*p].ay * sy;

		/* Skip glyphs that have no pixels */
		if (!w || !h)
			continue;

		point p1{x2, -y2, a->c[*p].tx, a->c[*p].ty};
		coords[c++] = p1;
		point p2{x2 + w, -y2, a->c[*p].tx + a->c[*p].bw / a->w, a->c[*p].ty};
		coords[c++] = p2;
		point p3{x2, -y2 - h, a->c[*p].tx, a->c[*p].ty + a->c[*p].bh / a->h};
		coords[c++] = p3;
		point p4{x2 + w, -y2, a->c[*p].tx + a->c[*p].bw / a->w, a->c[*p].ty};
		coords[c++] = p4;
		point p5{x2, -y2 - h, a->c[*p].tx, a->c[*p].ty + a->c[*p].bh / a->h};
		coords[c++] = p5;
		point p6{x2 + w, -y2 - h, a->c[*p].tx + a->c[*p].bw / a->w, a->c[*p].ty + a->c[*p].bh / a->h};
		coords[c++] = p6;
	}

	/* Draw all the character on the screen in one go */
	glBufferData(GL_ARRAY_BUFFER, 16 * 6 * strlen(text), coords, GL_DYNAMIC_DRAW);
	//glBufferData(GL_ARRAY_BUFFER, sizeof coords, coords, GL_DYNAMIC_DRAW);
	glDrawArrays(GL_TRIANGLES, 0, c);

	glDisableVertexAttribArray(attribute_coord);
}

void display_text(const unsigned __int32 timerange, const int frames, const int mode, int w, int h)
{
	float sx = 2.0 / w; //1024; //glutGet(GLUT_WINDOW_WIDTH);
	float sy = 2.0 / h; //480;  //glutGet(GLUT_WINDOW_HEIGHT);

	GLfloat black[4] = {0, 0, 0, 1};
	GLfloat red[4] = {1, 0, 0, 1};
	GLfloat transparent_green[4] = {0, 1, 0, 0.5};

	char text[8] = "1234 ms";
	int j;
	if (timerange > 9999)
		j = sprintf_s(text, 8, "%3li ms", 9999);
	else
		j = sprintf_s(text, 8, "%3li ms", timerange);

	glUniform4fv(uniform_color, 1, transparent_green);
	render_text(text, a24, 1 - 100 * sx, -1 + 20 * sy, sx, sy);

	char ftext[12] = "1 frame";
	if (frames > 1)
		j = sprintf_s(ftext, 12, "%i frames", frames);

	glUniform4fv(uniform_color, 1, transparent_green);
	render_text(ftext, a24, -1 + 10 * sx, -1 + 20 * sy, sx, sy);

	if (mode == 1)
	{
		glUniform4fv(uniform_color, 1, red);
		render_text("Single frame mode", a24, -1 + 10 * sx, -1 + 50 * sy, sx, sy);
	}
}

void free_text_resources(GLuint program)
{
	glDeleteProgram(program);
}

// int main(int argc, char *argv[]) {
// 	glutInit(&argc, argv);
// 	glutInitContextVersion(2,0);
// 	glutInitDisplayMode(GLUT_RGB);
// 	glutInitWindowSize(640, 480);
// 	glutCreateWindow("Texture atlas text");

// 	if (argc > 1)
// 		fontfilename = argv[1];
// 	else
// 		fontfilename = "FreeSans.ttf";

// 	GLenum glew_status = glewInit();

// 	if (GLEW_OK != glew_status) {
// 		fprintf(stderr, "Error: %s\n", glewGetErrorString(glew_status));
// 		return 1;
// 	}

// 	if (!GLEW_VERSION_2_0) {
// 		fprintf(stderr, "No support for OpenGL 2.0 found\n");
// 		return 1;
// 	}

// 	if (init_resources()) {
// 		glutDisplayFunc(display);
// 		glutMainLoop();
// 	}

// 	free_resources();
// 	return 0;
// }
