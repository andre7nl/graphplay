// input.h
#ifndef INPUT_H
#define INPUT_H

#include <stdio.h>
#include <frames.h>

#ifndef NSIZE
#define NSIZE 1024
#endif

struct point {
	float x;
	float y;
	float s;
	float t;
};

// struct point
// {
//   unsigned int x;
//   unsigned int y;
// };

//extern point graph[NSIZE];
extern point graph[NSIZE];
extern unsigned __int32 timerange;
extern int glwait;

extern Frames frames;

typedef void (*sendFunc)(int* ,int*);

//extern int dataLoop(sendFunc);
extern int (*dataLoop_ptr)(sendFunc);// = &dataLoop; 


#endif