#ifndef __game_h__
#define __game_h__

#include "camera.h"
#include "common/data-structures.h"

typedef struct Level Level;

struct Level {
	double t0;
	Camera camera;
	vec3 ambient_gravity;
	PointerVector instances;
	PointerVector ranges;
	PointerVector gravityObjects;
};

extern void bob_start(void);

#endif

