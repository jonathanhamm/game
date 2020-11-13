#ifndef __game_h__
#define __game_h__

#include "camera.h"
#include "common/data-structures.h"

typedef struct Level Level;

struct Level {
	double t0;
	Camera camera;
	PointerVector instances;
  PointerVector gravityObjects;
};

extern void bob_start(void);

#endif

