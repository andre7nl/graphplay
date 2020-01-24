#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>

#include <GL/glew.h>
#include "GL/wglew.h"
#include <SDL.h>

#include "common/shader_utils.h"

#include "res_texture.c"

#include <thread> // std::thread
#include <input.h>
#include <text.h>
#include <windows.h>

GLuint program;
GLint attribute_coord2d;
GLint uniform_xscale;
GLint uniform_yscale;
GLint uniform_sprite;

float yscale;
GLsync sync;

GLuint vbo;

int init_resources()
{
	program = create_program("graph.v.glsl", "graph.f.glsl");
	if (program == 0)
		return 0;

	attribute_coord2d = get_attrib(program, "coord");
	uniform_xscale = get_uniform(program, "xscale");
	uniform_yscale = get_uniform(program, "yscale");
	uniform_sprite = get_uniform(program, "sprite");
	yscale = 1.0;

	if (attribute_coord2d == -1 || uniform_xscale == -1 || uniform_sprite == -1)
		return 0;

#ifdef WIN32 // make sure vsync is turned off
	if (WGLEW_EXT_swap_control)
		wglSwapIntervalEXT(0);
#endif

	/* Enable blending */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* Enable point sprites (not necessary for true OpenGL ES 2.0) */
#ifndef GL_ES_VERSION_2_0
	glEnable(GL_POINT_SPRITE);
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif

	// Create the vertex buffer object
	//glGenBuffers(1, &vbo);
	GLuint *vboIds = new GLuint[2];
	glGenBuffers(2, vboIds);
	vbo = vboIds[0];
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	// Tell OpenGL to copy our array to the buffer object
	//glBufferData(GL_ARRAY_BUFFER, sizeof graph, graph, GL_STATIC_DRAW);
	glBufferData(GL_ARRAY_BUFFER, sizeof graph, graph, GL_STREAM_DRAW);
	//glBufferData(GL_ARRAY_BUFFER, sizeof graph, 0, GL_STREAM_DRAW);

	if (!init_text_resources(program, vboIds[1]))
		return 0;

	return 1;
}

void free_resources()
{
	glDeleteProgram(program);
}

void display(SDL_Window *window, int pos)
{
	int w, h;
	SDL_GL_GetDrawableSize(window, &w, &h);
	glUseProgram(program);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glUniform1f(uniform_sprite, 1);
	if (frames.mode == 0)
		glUniform1f(uniform_xscale, NSIZE - 1);
	else
		glUniform1f(uniform_xscale, frames.size - 1);

	glUniform1f(uniform_yscale, yscale);

	/* Draw using the vertices in our vertex buffer object */
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glEnableVertexAttribArray(attribute_coord2d);
	glVertexAttribPointer(attribute_coord2d, 4, GL_FLOAT, GL_FALSE, 0, 0);
	// glVertexAttribPointer(attribute_coord2d, 2, GL_UNSIGNED_INT, GL_FALSE, 0, 0);

	/* Push each element in buffer_vertices to the vertex shader */
	glUniform1f(uniform_sprite, 0);
	//glDrawArrays(GL_LINE_STRIP, 0, NSIZE);
	//printf("graph pos = %i\n", pos);
	glDrawArrays(GL_LINE_STRIP, 0, pos);
	if (pos < NSIZE && frames.mode == 0)
		glDrawArrays(GL_LINE_STRIP, pos, NSIZE - pos);

	glUniform1f(uniform_sprite, 2);
	display_text(timerange, frames.frames, frames.mode, w, h);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	SDL_GL_SwapWindow(window);
}

void keyDown(SDL_KeyboardEvent *ev)
{
	switch (ev->keysym.scancode)
	{
	case SDL_SCANCODE_F1:
		frames.queue_mode(0);
		printf("Now showing all frames.\n");
		break;
	case SDL_SCANCODE_F2:
		frames.queue_mode(1);
		printf("Now showing only first frame.\n");
		break;
	case SDL_SCANCODE_F3:
		break;
	case SDL_SCANCODE_F4:
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(graph), graph);
		break;
	case SDL_SCANCODE_LEFT:
		printf("toframes = %i\n", frames.queue_frames_down());
		break;
	case SDL_SCANCODE_RIGHT:
		printf("toframes = %i\n", frames.queue_frames_up());
		break;
	case SDL_SCANCODE_UP:
		yscale += 0.1;
		break;
	case SDL_SCANCODE_DOWN:
		yscale -= 0.1;
		break;
	case SDL_SCANCODE_HOME:
		break;
	default:
		break;
	}
}

void windowEvent(SDL_WindowEvent *ev)
{
	switch (ev->event)
	{
	case SDL_WINDOWEVENT_SIZE_CHANGED:
		glViewport(0, 0, ev->data1, ev->data2);
		break;
	default:
		break;
	}
}

void LockBuffer(GLsync &syncObj)
{
	if (syncObj)
		glDeleteSync(syncObj);

	//std::cout << "sync started\n";
	syncObj = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void WaitBuffer(GLsync &syncObj)
{
	// if (glIsSync(syncObj))
	// {
	// 	std::cout << "is sync";
	// }
	// else
	// {
	// 	std::cout << "is NOT sync";
	// }
	if (syncObj)
	{
		while (1)
		{
			GLenum waitReturn = glClientWaitSync(syncObj, GL_SYNC_FLUSH_COMMANDS_BIT, 1);
			if (waitReturn == GL_ALREADY_SIGNALED || waitReturn == GL_CONDITION_SATISFIED)
			{
				glwait = 0;
				//std::cout << "sync ended\n";
				return;
			};

			//std::cout << "sync waiting...\n";
		}
	}
}

void mainLoop(SDL_Window *window)
{
	while (true)
	{
		display(window, NSIZE);
		//SDL_GL_SwapWindow(window);
		bool redraw = false;

		while (!redraw)
		{
			SDL_Event ev;
			if (!SDL_WaitEvent(&ev))
				return;

			switch (ev.type)
			{
			case SDL_QUIT:
				return;
			case SDL_USEREVENT:
				if (ev.user.code == 9)
				{
					//std::cout << ev.user.code << "\n";
					glwait = 1;
					int *start = (int *)ev.user.data1;
					int *size1 = (int *)ev.user.data2;
					//printf("graph start= %i, size= %i\n", *start, *size1);
					//glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(graph), &graph);
					//glBufferSubData(GL_ARRAY_BUFFER, *start * sizeof(point), *size * sizeof(point), &graph);
					glBufferSubData(GL_ARRAY_BUFFER, *start * sizeof(point), *size1 * sizeof(point), &graph[*start]);
					display(window, *start + *size1);
					//SDL_GL_SwapWindow(window);
					LockBuffer(sync);
					WaitBuffer(sync);
					//redraw = true;
				};
				break;
			case SDL_KEYDOWN:
				keyDown(&ev.key);
				redraw = true;
				break;
			case SDL_WINDOWEVENT:
				windowEvent(&ev.window);
				redraw = true;
				break;
			default:
				break;
			}
		}
	}
}

void sendData(int *start, int *size)
{
	SDL_Event user_event;

	//printf("senddata start= %i, size= %i\n", *start, *size);
	user_event.type = SDL_USEREVENT;
	user_event.user.code = 9;
	user_event.user.data1 = start;
	user_event.user.data2 = size;
	SDL_PushEvent(&user_event);
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window *window = SDL_CreateWindow("My Graph",
										  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
										  1024, 480,
										  SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(window);

	/* This makes our buffer swap syncronized with the monitor's vertical refresh */
	//SDL_GL_SetSwapInterval(1);

	GLenum glew_status = glewInit();

	if (GLEW_OK != glew_status)
	{
		fprintf(stderr, "Error: %s\n", glewGetErrorString(glew_status));
		return 1;
	}

	if (!GLEW_VERSION_2_0)
	{
		fprintf(stderr, "No support for OpenGL 2.0 found\n");
		return 1;
	}

	GLfloat range[2];

	glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
	if (range[1] < res_texture.width)
		fprintf(stderr, "WARNING: point sprite range (%f, %f) too small\n", range[0], range[1]);

	printf("Use left/right to move horizontally.\n");
	printf("Use up/down to change the horizontal scale.\n");
	printf("Press home to reset the position and scale.\n");
	printf("Press F1 to draw lines.\n");
	printf("Press F2 to draw points.\n");
	printf("Press F3 to draw point sprites.\n");

	if (!init_resources())
		return EXIT_FAILURE;

	//simData.dataLoop(sendData);
	//std::thread t1(&Input::dataLoop, &input1, sendData);
	//std::thread t1(dataLoop,sendData);
	std::thread t1(dataLoop_ptr, sendData);

	mainLoop(window);

	free_resources();

	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return EXIT_SUCCESS;
}
