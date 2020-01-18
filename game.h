#ifndef __game_h__
#define __game_h__

#include "camera.h"
#include "common/data-structures.h"

typedef struct Level Level;

struct Level {
	float dt;
	Camera camera;
	PointerVector instances;
};

extern void bob_start(void);

#endif

