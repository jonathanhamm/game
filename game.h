#ifndef __game_h__
#define __game_h__

#include "camera.h"
#include "instance_plane.h"
#include "common/data-structures.h"

typedef struct Level Level;
typedef struct InstancePlane InstancePlane;

struct Level {
	double t0;
	Camera camera;
  vec3 ambient_gravity;
	PointerVector instances;
  PointerVector instance_planes;
  PointerVector gravityObjects;
};

extern void bob_start(void);

#endif

